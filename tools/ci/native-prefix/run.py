#!/usr/bin/env python3
"""Enabled native/ICU hosted CI; acceptance requires actual command receipts."""
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time
from datetime import datetime, timezone
from native_test262_diagnostic import reconcile, classify_tool_records
from native_profile_gate import qualify_native, reviewed_entry, identity
from native_normative_scope import bind_static_scopes, SCOPE_PATH
from core_baseline_transition import authenticate_transition
from checkout_identity import inspect_checkout
from execution_threads import available_cpus, test262_workers
from slow_test_selection import (EXCLUSION_PATH,
                                 authenticate_slow_selection)

HERE = Path(__file__).resolve().parent
OUT = Path('ci-native-prefix-diagnostics').resolve()
OUT.mkdir(exist_ok=True)
BACKEND = os.environ['PREFIX_BACKEND']
CC = os.environ['PREFIX_COMPILER']
SAN = os.environ['PREFIX_SANITIZER']
ICU = Path(os.environ['PREFIX_ICU'])
REVISION = 'c8c798898646638cd0c24879f8e0374e847e7d74'
def test262_data_profile(backend):
    """Bind data-specific fixtures independently of the error baseline."""
    patches = {'tests/test262.patch': '64d8f5b124afae386c0b2b154a147f2f8daf087dd4b5f602d1948e85c7f9cbba'}
    if backend == 'native':
        patches['tests/test262-unicode18.patch'] = '0ca6149a256e331f3fd4a5ca4fa31536dab1de577bc759bcd523ceaac8545698'
        version = [18, 0, 0, 0]
        fixture_map = '808f46746cde3b35db59c83dc7ecb9b91df7970bcae65572ffd4ee126547896d'
        count = 53617
    elif backend == 'icu':
        version = [17, 0, 0, 0]
        fixture_map = '994760adcc776f52a7a7fe0c04a7d12307117789bad2e06d501defbbdf8adf40'
        count = 53611
    else:
        raise ValueError('Unsupported Test262 data backend')
    return dict(backend=backend, unicode_data_version=version, patches=patches,
                fixture_source_map_sha256=fixture_map, runner_distinct_source_count=count,
                zero_error_conformance_claim=False,
                latest_unicode_conformance_claim=False)


TEST262_PROFILE = test262_data_profile(BACKEND)
PATCHES = TEST262_PROFILE['patches']
FIXTURE_MAP = TEST262_PROFILE['fixture_source_map_sha256']
HISTORICAL_BASELINE_PRODUCTION_MAP = 'c71443f857120b4ece57b2d4c2ce635e9b54bacf97f33f5eb4b6177c839a3b70'
ENV = dict(os.environ, MAKEFLAGS='', LC_ALL='C.UTF-8', TZ='UTC',
           PKG_CONFIG_PATH=str(ICU / 'install/lib/pkgconfig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def canonical(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n')


def require(value, reason):
    if not value:
        raise RuntimeError(reason)


def process_cpu_seconds(pid):
    """Read direct subprocess CPU time when Linux exposes it; otherwise omit it."""
    try:
        fields = Path('/proc/{}/stat'.format(pid)).read_text().rsplit(')', 1)[1].split()
        ticks = int(fields[11]) + int(fields[12])
        return ticks / os.sysconf('SC_CLK_TCK')
    except (OSError, ValueError, IndexError, AttributeError):
        return None


def wait_with_progress(name, process, timeout, started, stdout_path, stderr_path,
                       raw_streams):
    """Report progress outside the raw logs without extending the command timeout."""
    deadline = time.monotonic() + timeout
    while True:
        remaining = max(0, deadline - time.monotonic())
        try:
            return process.wait(timeout=min(60, remaining))
        except subprocess.TimeoutExpired:
            if time.monotonic() >= deadline:
                raise subprocess.TimeoutExpired(process.args, timeout)
            elapsed = time.monotonic() - started
            cpu = process_cpu_seconds(process.pid)
            cpu_text = 'unavailable' if cpu is None else '{:.1f}'.format(cpu)
            if raw_streams:
                output = 'stdout_bytes={} stderr_bytes={}'.format(
                    stdout_path.stat().st_size, stderr_path.stat().st_size)
            else:
                output = 'merged_output_bytes={}'.format(stdout_path.stat().st_size)
            print('[progress] {} elapsed_seconds={:.1f} process_cpu_seconds={} {}'.format(
                name, elapsed, cpu_text, output), flush=True)


def run(name, argv, cwd=None, env=None, timeout=5400, check=True, raw_streams=False):
    """Retain command status and exact raw streams; prove process-group closure."""
    effective_env = env or ENV
    record = dict(argv=list(map(str, argv)), cwd=str(cwd or Path.cwd()), returncode=None,
                  started_utc=datetime.now(timezone.utc).isoformat(), timeout_seconds=timeout,
                  effective_environment_sha256=canonical(effective_env),
                  effective_profile_environment={key: effective_env.get(key) for key in
                      ('LC_ALL', 'LANG', 'TZ', 'ASAN_OPTIONS', 'UBSAN_OPTIONS',
                       'PREFIX_BACKEND', 'PREFIX_COMPILER', 'PREFIX_SANITIZER')},
                  timed_out=False, interrupted=False, lifecycle_closed=False,
                  stdout_stderr_separate=raw_streams)
    if name == 'native-original-assertion-capture':
        # Bind the actual environment selected for this subprocess before it
        # starts, using the same observer-independent encoding as the probes.
        record['provider_environment_sha256'] = hashlib.sha256(json.dumps(
            {key: value for key, value in effective_env.items()
             if key != 'QJS_TZ_CAPTURE_DIRECTORY'}, sort_keys=True, ensure_ascii=True,
            separators=(',', ':')).encode()).hexdigest()
    log = OUT / (name + '.log')
    stdout_path, stderr_path = OUT / (name + '.stdout.log'), OUT / (name + '.stderr.log')
    process = None
    started = time.monotonic()
    try:
        with (stdout_path if raw_streams else log).open('wb') as stream:
            with stderr_path.open('wb') as errors:
                process = subprocess.Popen(argv, cwd=cwd, env=effective_env,
                                           stdout=stream, stderr=errors if raw_streams else subprocess.STDOUT,
                                           start_new_session=True)
                record['pid'] = process.pid
                record['process_group'] = process.pid
                record['returncode'] = wait_with_progress(
                    name, process, timeout, started,
                    stdout_path if raw_streams else log, stderr_path, raw_streams)
    except subprocess.TimeoutExpired:
        record['timed_out'] = True
        raise
    except BaseException:
        record['interrupted'] = True
        raise
    finally:
        if process is not None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            record['returncode'] = process.wait()
            close_deadline = time.monotonic() + 2
            while True:
                try:
                    os.killpg(process.pid, 0)
                except ProcessLookupError:
                    record['lifecycle_closed'] = True
                    break
                if time.monotonic() >= close_deadline:
                    break
                time.sleep(0.01)
        record['finished_utc'] = datetime.now(timezone.utc).isoformat()
        record['elapsed_seconds'] = time.monotonic() - started
        if raw_streams:
            # .log is a convenient stdout-then-stderr view; the two raw files are authoritative.
            raw_stdout, raw_stderr = stdout_path.read_bytes(), stderr_path.read_bytes()
            separator = b'\n' if raw_stdout and not raw_stdout.endswith(b'\n') else b''
            log.write_bytes(raw_stdout + separator + raw_stderr)
            record.update(stdout_sha256=sha(stdout_path), stderr_sha256=sha(stderr_path),
                          combined_view_is_original_stream_order=False)
        record['log_sha256'] = sha(log)
        save(name + '.command.json', record)
    require(record['lifecycle_closed'] and not record['timed_out'] and not record['interrupted'],
            'Incomplete command lifecycle: ' + name)
    if check:
        clean = record['returncode'] == 0 and not classify_tool_records(log.read_text(errors='replace'), '')
        if not clean:
            print('[failure] {}: retained command output tail'.format(name), flush=True)
            print(log.read_text(errors='replace')[-8192:], flush=True)
        require(clean, 'Failed command or SAN/infra diagnostic: ' + name)
    return log.read_text(errors='replace')


def committed_sources(entries):
    """Hash raw committed Git blob bytes, then compare every checked-out source."""
    argv = ['git', 'cat-file', '--batch']
    result = subprocess.run(argv, input=('\n'.join(blob for _, blob in entries.values()) + '\n').encode(),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    receipt = dict(argv=argv, cwd=str(Path.cwd()), returncode=result.returncode,
                   stdout_sha256=hashlib.sha256(result.stdout).hexdigest(),
                   stderr_sha256=hashlib.sha256(result.stderr).hexdigest())
    (OUT / 'checkout-committed-blobs.stderr.log').write_bytes(result.stderr)
    save('checkout-committed-blobs.command.json', receipt)
    require(result.returncode == 0, 'Cannot read exact committed source blobs')
    stream, mapping = io.BytesIO(result.stdout), {}
    for path, (_mode, blob) in entries.items():
        header = stream.readline().decode().strip().split()
        require(len(header) == 3 and header[:2] == [blob, 'blob'], 'Unexpected committed blob header')
        data = stream.read(int(header[2]))
        require(len(data) == int(header[2]) and stream.read(1) == b'\n', 'Truncated committed blob')
        mapping[path] = hashlib.sha256(data).hexdigest()
        require(Path(path).is_file() and not Path(path).is_symlink() and sha(path) == mapping[path],
                'Checkout bytes differ from committed blob: ' + path)
    require(not stream.read(), 'Unexpected extra committed blob output')
    save('actual-git-blob-identity.json', {path: dict(mode=mode, blob=blob)
         for path, (mode, blob) in entries.items()})
    checkout = inspect_checkout()
    require(entries == checkout['entries'] and all(
            checkout['sources'].get(name) == value for name, value in mapping.items()),
            'Committed top-level census differs from authenticated checkout')
    save('actual-gitlink-identity.json', checkout['gitlinks'])
    return checkout['sources']


def make_args():
    require(BACKEND in ('native', 'icu') and CC in ('gcc', 'clang') and
            SAN in ('normal', 'ASan', 'UBSan'), 'Unsupported matrix row')
    args = ['make', 'CONFIG_WERROR=y', 'CONFIG_LTO=', 'CC=' + CC, 'HOST_CC=' + CC,
            'CONFIG_CLANG=' + ('y' if CC == 'clang' else ''),
            'CONFIG_ASAN=' + ('y' if SAN == 'ASan' else ''),
            'CONFIG_UBSAN=' + ('y' if SAN == 'UBSan' else ''),
            'CONFIG_INTL_BACKEND=' + BACKEND,
            'CONFIG_ICU=' + ('y' if BACKEND == 'icu' else 'n'),
            'CONFIG_INTL=y', 'CONFIG_TEMPORAL=y',
            'CONFIG_INTL_LEGACY=y']
    if BACKEND == 'icu':
        flags = run('icu-cflags', ['pkg-config', '--cflags', 'icu-i18n', 'icu-uc']).strip()
        libs = run('icu-libs', ['pkg-config', '--static', '--libs', 'icu-i18n', 'icu-uc']).strip() + ' -lstdc++'
        args += ['ICU_STATIC=y', 'ICU_CFLAGS=' + flags, 'ICU_LIBS=' + libs,
                 'HOST_ICU_CFLAGS=' + flags, 'HOST_ICU_LIBS=' + libs]
    return args


def native_units():
    binding = json.loads((HERE / 'binding.json').read_text())
    inventory = json.loads((HERE / 'library-inventory.json').read_text())['enabled-suffix']
    require(inventory['source_map'] is None, 'Inventory must be a committed scope template')
    directory = Path('.obj') / ('asan' if SAN == 'ASan' else 'ubsan' if SAN == 'UBSan' else '')
    unitdir = directory / 'hosted-native-prefix'
    unitdir.mkdir(parents=True, exist_ok=True)
    flags = ['-std=c99', '-O2', '-g', '-Wall', '-Wextra', '-Werror', '-Wpedantic',
             '-D_GNU_SOURCE', '-DCONFIG_UNICODE_NATIVE', '-Iinclude', '-Isrc', '-Isrc/intl', '-Isrc/intl/data',
             '-Isrc/calendar', '-Isrc/cutils', '-Isrc/dtoa', '-Isrc/unicode', '-Isrc/regexp']
    if SAN != 'normal':
        flags += ['-fsanitize=' + ('address' if SAN == 'ASan' else 'undefined'), '-fno-omit-frame-pointer']
    calendar = ['-DQJS_CAL_USE_PERSIAN_AUTHORITY_TABLE', '-DQJS_CAL_ENABLE_LUNISOLAR_CANDIDATE',
                '-DQJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA', '-DQJS_CAL_PERSIAN_AUTHORITY_VERIFIED',
                '-DQJS_CAL_CHINESE_AUTHORITY_VERIFIED', '-DQJS_CAL_DANGI_AUTHORITY_VERIFIED']
    if Path('tools/timezone/pin.json').exists():
        run('timezone-library-build', make_args() + ['-j' + str(available_cpus()), 'libqjstimezone.a'])
    objects = []
    for index, source in enumerate(inventory['sources']):
        extra = calendar if source.startswith('src/calendar/') else []
        if source.startswith(('src/cutils/', 'src/dtoa/', 'src/unicode/')):
            extra = extra + inventory['support_flags'][CC]
        if source.startswith('src/timezone/'):
            extra = extra + ['-I' + str(directory / 'src/timezone')]
        if source == 'src/intl/provider-native.c':
            extra = extra + ['-std=c11']
        obj = unitdir / (source.replace('/', '_') + '.o')
        run('native-object-' + str(index), [CC] + flags + extra + ['-c', source, '-o', str(obj)])
        objects.append(str(obj))
    archive = unitdir / 'libnative.a'
    run('native-archive', ['ar', 'rcs', str(archive)] + objects)
    symbols = run('native-undefined-symbols', ['nm', '-u', 'qjs', 'qjsc', 'libquickjs.a', str(archive)])
    require(not re.search(r'\b(?:u_|uloc_|unum_|ucal_|udat_|ubrk_|ucol_|ures_)\w+|_ZN\d+icu_\d+', symbols), 'Native prefix gained ICU symbols')
    definitions = run('native-data-owner', ['nm', '-g', '--defined-only', str(archive)])
    for symbol in ('qjs_intl_locale_metadata_blob', 'qjs_intl_locale_metadata_blob_size',
                   'qjs_calendar_lunisolar_provider', 'qjs_tz_embedded_data',
                   'qjs_tz_embedded_size', 'qjs_tz_record_count', 'qjs_tz_records'):
        require(len(re.findall(r'[ \t]' + symbol + r'$', definitions, re.M)) == 1, 'Invalid direct-library owner: ' + symbol)
    require('qjs_intl_counted_data_open' not in symbols,
            'Production archive refers to the counted test reader')
    production = run('native-production-owners', ['nm', '-g', '--defined-only', 'libquickjs.a'])
    for symbol in ('qjs_intl_locale_metadata_blob', 'qjs_intl_locale_metadata_blob_size',
                   'qjs_calendar_lunisolar_provider', 'qjs_tz_embedded_data',
                   'qjs_tz_embedded_size', 'qjs_tz_record_count', 'qjs_tz_records'):
        require(len(re.findall(r'[ \t]' + symbol + r'$', production, re.M)) == 1, 'Invalid production owner: ' + symbol)
    dynamic = run('native-direct-dependencies', ['readelf', '-d', 'qjs', 'qjsc'])
    require(not re.search(r'libicu|libstdc\+\+', dynamic), 'Native direct ICU/C++ dependency')
    loaded = run('native-loaded-dependencies', ['ldd', 'qjs', 'qjsc'])
    require('libicu' not in loaded and 'not found' not in loaded, 'Invalid native loader dependencies')
    if SAN == 'normal':
        require('libstdc++' not in loaded, 'Normal native prefix loads C++')
    binary = unitdir / 'intl-data.bin'
    body = Path('src/intl/data/locale-metadata.c').read_text()
    blob = re.search(r'qjs_intl_locale_metadata_blob\[\]\s*=\s*\{([^}]+)\}', body).group(1)
    require(not re.sub(r'0x[0-9a-fA-F]{2}|[,\s]', '', blob), 'Unexpected embedded data syntax')
    binary.write_bytes(bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', blob)))
    require(sha(binary) == inventory['binary_sha256'], 'Embedded fixture differs from stage data')
    for index, unit in enumerate(inventory['units']):
        test = 'tests/' + unit['name'] + '.c'
        body = Path(test).read_text()
        require('defined(CONFIG_INTL_NATIVE)' not in body and '#ifdef CONFIG_INTL_NATIVE' not in body,
                'Guarded frontend unit requires activation')
        target = unitdir / unit['name']
        extra = calendar if unit['name'].startswith(('test_intl_calendar_', 'test_calendar_')) else []
        # This unit includes the provider owner, including its C11 atomics.
        if unit['name'] == 'test_intl_native_date_bank_data':
            extra = extra + ['-std=c11']
        run('native-unit-compile-' + str(index), [CC] + flags + extra +
            [test, str(archive), '-lm', '-ldl', '-lpthread', '-o', str(target)])
        run('native-unit-run-' + str(index), ['./' + str(target)] +
            ([str(binary)] if unit['binary_argument'] else []))
    run('native-activation-selector', ['python3', 'tests/test_intl_native_selector.py'])
    if Path('tools/timezone/pin.json').exists():
        targets = ['tests/test_tzif_decoder', 'tests/test_timezone_system_reader', 'tests/test_timezone']
        run('timezone-units-build', make_args() + targets)
        for index, target in enumerate(targets):
            run('timezone-unit-' + str(index), ['./' + target])
    special = special_c_units(json.loads(
        (HERE / 'activation-scope.json').read_text())[BACKEND])
    require(inventory['make_owned_units'] == special,
            'Special unit inventory differs from repository scope')
    require(all(unit['name'] != 'test_intl_native_compiled_cache'
                for unit in inventory['units']),
            'Counted provider fixture entered direct-library units')
    save('native-special-unit-census.json', dict(
        make_owned_units=special, source_fixtures=len(special),
        mode_commands=sum(len(unit['modes']) for unit in special),
        execution_basis='Required separate repository receipts'))
    save('native-unit-scope.json', dict(stage=binding['stage'], units=inventory['units'],
         native_frontends_enabled=True, direct_units_completed=True,
         frontend_conformance='See separate actual repository and Test262 command receipts'))



def special_c_units(row):
    units = row['special_C_units']
    expected = ['retry', 'provider-failure', 'concurrent', 'mutable',
                'number', 'duration', 'number-mutable']
    require(len(units) == (1 if BACKEND == 'native' else 0),
            'Wrong special source fixture census')
    for unit in units:
        require(unit['target'] ==
                'tests/test_intl_native_compiled_cache' and
                unit['source'] == unit['target'] + '.c' and
                unit['make_target'] == 'test-intl-native-compiled-cache'
                and unit['modes'] == expected,
                'Wrong counted provider fixture or mode census')
        require(unit['target'] not in row['C_targets'],
                'Special fixture entered argument-free targets')
        require(sha(unit['source']) == unit['source_sha256'],
                'Changed special fixture source')
    return units


def repository_tests(args):
    scope = json.loads((HERE / 'activation-scope.json').read_text())
    binding = json.loads((HERE / 'binding.json').read_text())
    require(scope['source_map'] is None and sha('Makefile') == scope['make_source_sha256'],
            'Makefile changed: review and update the committed invocation census')
    row = scope[BACKEND]
    special = special_c_units(row)
    binary_targets = sorted(set(row['C_targets']) |
        {unit['target'] for unit in special} |
        {entry['target'] for entry in row['extra_C_invocations']})
    before = {name: sha(name) for name in binary_targets}
    save('repository-binaries-before.json', before)
    log = run('repository-tests', args + ['test'], check=False)
    repository_receipt = json.loads((OUT / 'repository-tests.command.json').read_text())
    lines = log.splitlines()
    records = {}
    source_paths = row.get('C_source_paths', {})
    require(isinstance(source_paths, dict) and set(source_paths) <= set(row['C_targets']),
            'Unknown C fixture source ownership')
    expected = [(name, ['./' + name], source_paths.get(name, name + '.c'))
                for name in row['C_targets']]
    expected += [(name, ['./run-test262', '-N', name] if name.endswith('test_intl_bound_function_realms.js')
                  else ['./qjs', name], name) for name in row['JS_files']]
    special_targets = {}
    for unit in special:
        for mode in unit['modes']:
            label = unit['target'] + ':' + mode
            argv = ['./' + unit['target'], mode]
            expected.append((label, argv, unit['source']))
            special_targets[label] = unit['target']
    for index, (name, argv, source) in enumerate(expected):
        count = lines.count(' '.join(argv))
        binary_target = special_targets.get(name, name)
        if name in special_targets:
            require(count == 1,
                    'Missing or repeated Make-owned mode: ' + name)
        record = dict(argv=argv, source_sha256=sha(source), repository_invocation_count=count)
        if binary_target in before:
            require(sha(binary_target) == before[binary_target],
                    'Repository unit binary changed: ' + name)
            record['binary_sha256_before'] = before[binary_target]
            record['binary_sha256_after'] = before[binary_target]
        if count and repository_receipt['returncode'] == 0:
            record['execution_basis'] = 'Actual passed repository command log invocation'
            record['repository_log_line_1based'] = lines.index(' '.join(argv)) + 1
        else:
            label = 'activation-invocation-' + str(index)
            run(label, argv, timeout=900, check=False)
            record['returncode'] = json.loads((OUT / (label + '.command.json')).read_text())['returncode']
            record['execution_basis'] = 'Independent actual hosted command receipt'
            record['command_receipt'] = label + '.command.json'
            if binary_target in before:
                require(
                    sha(binary_target) == before[binary_target],
                    'Fallback unit binary changed: ' + name)
        if name in special_targets:
            record['category'] = 'Make-owned special repository unit'
            record['target'] = binary_target
        records[name] = record
        save('repository-invocation-coverage.json', dict(backend=BACKEND, records=records,
             expected_C_targets=row['C_targets'],
             special_C_units=special,
             special_C_source_fixtures=len(special),
             special_C_mode_count=len(special_targets),
             expected_JS_files=row['JS_files'],
             completed=len(records) == len(expected), generic_bodies_skipped=False,
             shared_timezone_snapshot_extension_adopted=binding['shared_timezone_snapshot_extension_adopted']))
    for index, entry in enumerate(row.get('special_JS_invocations', [])):
        require(isinstance(entry, dict) and set(entry) == {'source', 'argv'} and
                Path(entry['source']).is_file() and entry['argv'],
                'Missing special JS invocation contract')
        argv = entry['argv']
        count = lines.count(' '.join(argv))
        label = 'special-js-' + str(index)
        record = dict(argv=argv, source_sha256=sha(entry['source']),
                      repository_invocation_count=count)
        if count == 1 and repository_receipt['returncode'] == 0:
            record['execution_basis'] = 'Actual passed fresh-process repository invocation'
            record['repository_log_line_1based'] = lines.index(' '.join(argv)) + 1
        else:
            run(label, argv, timeout=900, check=False)
            record['returncode'] = json.loads((OUT / (label + '.command.json')).read_text())['returncode']
            record['execution_basis'] = 'Independent actual hosted fresh-process command receipt'
            record['command_receipt'] = label + '.command.json'
        records[label] = record
    for index, entry in enumerate(row['extra_C_invocations']):
        label = 'unicode-provider-oracle-' + str(index)
        run(label, entry['argv'], timeout=3600, check=False)
        records[label] = dict(entry, source_sha256=sha(entry['target'] + '.c'),
                              binary_sha256_before=before[entry['target']],
                              binary_sha256_after=sha(entry['target']),
                              command_receipt=label + '.command.json',
                              returncode=json.loads((OUT / (label + '.command.json')).read_text())['returncode'])
        require(records[label]['binary_sha256_before'] == records[label]['binary_sha256_after'],
                'Unicode oracle binary changed')
    save('repository-invocation-coverage.json', dict(backend=BACKEND, records=records,
         expected_C_targets=row['C_targets'],
             special_C_units=special,
             special_C_source_fixtures=len(special),
             special_C_mode_count=len(special_targets),
             expected_JS_files=row['JS_files'],
         extra_C_invocations=row['extra_C_invocations'],
         special_JS_invocations=row.get('special_JS_invocations', []),
         completed=len(records) ==
             len(expected) + len(row['extra_C_invocations']) +
             len(row.get('special_JS_invocations', [])),
         generic_bodies_skipped=False,
         repository_returncode=repository_receipt['returncode'],
         shared_timezone_snapshot_extension_adopted=binding['shared_timezone_snapshot_extension_adopted']))
    require(repository_receipt['returncode'] == 0 and
            all(record.get('returncode', 0) == 0 for record in records.values()),
            'Repository or independent scope invocation failed; retained all receipts')


def public_api(args, cwd=None, prefix='public-api'):
    tree = Path(cwd or Path.cwd())
    cxx = 'g++' if CC == 'gcc' else 'clang++'
    for name in ('utf16', 'instant', 'default_time_zone'):
        for language, compiler, standard in (('c', CC, 'c11'), ('c++', cxx, 'c++17')):
            for suffix, checked in (('normal', []), ('checked', ['-DCONFIG_CHECK_JSVALUE'])):
                run(prefix + '-' + name + '-' + language + '-' + suffix,
                    [compiler, '-x', language, '-std=' + standard, '-O2', '-Wall', '-Wextra',
                     '-Wno-unused-parameter', '-Werror', '-Iinclude'] + checked +
                    ['-fsyntax-only', str(HERE / (name + '_abi_contract.c'))], cwd=tree)
    for name, extra in (('abi_contract', ['-DCHECK_INTL_API']),
                        ('new_api_abi_contract', ['-DCHECK_UTF16_API'])):
        for suffix, checked in (('normal', []), ('checked', ['-DCONFIG_CHECK_JSVALUE'])):
            run(prefix + '-' + name + '-' + suffix,
                [CC, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-parameter',
                 '-Werror', '-Iinclude'] + checked + extra +
                ['-fsyntax-only', str(HERE / (name + '.c'))], cwd=tree)
    for compiler, standard in ((CC, 'c99'), (cxx, 'c++11')):
        for order in ('1', '2'):
            run(prefix + '-private-headers-' + compiler + '-' + order,
                [compiler, '-x', 'c++' if compiler == cxx else 'c', '-std=' + standard,
                 '-Wall', '-Wextra', '-Werror', '-Iinclude', '-Isrc', '-Isrc/quickjs',
                 '-DHEADER_ORDER=' + order, '-fsyntax-only', str(HERE / 'private_header_order.c')], cwd=tree)
    run(prefix + '-units-build', args + ['tests/test_api', 'tests/test_temporal_api',
                                       'tests/test_intl_default_locale_api', 'tests/test_default_time_zone_api'], cwd=tree)
    artifacts = {name: sha(tree / name) for name in ('qjs', 'qjsc', 'libquickjs.a',
                 'tests/test_api', 'tests/test_temporal_api', 'tests/test_intl_default_locale_api', 'tests/test_default_time_zone_api')}
    save(prefix + '-binaries-before.json', artifacts)
    for name in ('utf16-public-api', 'utf16-public-api-oom'):
        run(prefix + '-' + name, ['./tests/test_api', name], cwd=tree, timeout=900)
    run(prefix + '-instant', ['./tests/test_temporal_api'], cwd=tree, timeout=900)
    run(prefix + '-default-time-zone', ['./tests/test_default_time_zone_api'], cwd=tree, timeout=900)
    run(prefix + '-default-locale', ['./tests/test_intl_default_locale_api'], cwd=tree, timeout=900)
    definitions = run(prefix + '-archive-exports', ['nm', '-g', '--defined-only', 'libquickjs.a'], cwd=tree)
    symbols = ['JS_NewStringUTF16', 'JS_ToCStringLenUTF16', 'JS_FreeCStringRT',
               'JS_FreeCStringUTF16', 'JS_FreeCStringRT_UTF16', 'JS_NewTemporalInstant',
               'JS_GetTemporalInstantEpochNanoseconds', 'JS_SetIntlDefaultLocale', 'JS_GetIntlDefaultLocale',
               'JS_SetDefaultTimeZone', 'JS_GetDefaultTimeZone']
    for symbol in symbols:
        require(len(re.findall(r'[ \t]' + symbol + r'$', definitions, re.M)) == 1,
                'Invalid public API owner: ' + symbol)
    require(artifacts == {name: sha(tree / name) for name in artifacts}, 'API runtime unit binary changed')
    save(prefix + '-scope.json', dict(backend=BACKEND, make_args=args, cwd=str(tree),
         checked_C_and_CXX_header_signatures=True, focused_runtime_units_completed=True,
         binary_sha256_before_and_after=artifacts, archive_export_symbols=symbols))


def api_variants(args):
    binding = json.loads((HERE / 'binding.json').read_text())
    variants = [('intl-y-temporal-n', 'y', 'n'), ('intl-n-temporal-n', 'n', 'n')]
    if BACKEND == 'native':
        variants += [('intl-n-temporal-y', 'n', 'y')]
    for label, intl, temporal in variants:
        prefix = 'variant-' + label
        tree = OUT / 'feature-variants' / label
        tree.mkdir(parents=True)
        for name in sorted(set(binding['source_sha256']) | set(binding['adapter_allowlist'])):
            destination = tree / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(name, destination)
        overrides = {'CONFIG_INTL': intl, 'CONFIG_TEMPORAL': temporal}
        alternate = [a.split('=', 1)[0] + '=' + overrides[a.split('=', 1)[0]]
                     if '=' in a and a.split('=', 1)[0] in overrides else a for a in args]
        targets = ['all', 'tests/test_api', 'tests/test_temporal_api', 'tests/test_intl_default_locale_api', 'tests/test_default_time_zone_api']
        if BACKEND == 'native' and intl == 'y':
            targets += ['tests/test_intl_native_timezone_runtime']
        if temporal == 'y':
            targets += ['tests/test_temporal_native_calendars', 'tests/test_temporal_calendars']
        run(prefix + '-build', alternate + ['-j' + str(available_cpus())] + targets, cwd=tree)
        public_api(alternate, tree, prefix + '-public-api')
        if BACKEND == 'native' and intl == 'y':
            run(prefix + '-shared-timezone-snapshot', ['./tests/test_intl_native_timezone_runtime'], cwd=tree, timeout=900)
            run(prefix + '-duration-js', ['./qjs', 'tests/test_intl_native_duration.js'], cwd=tree, timeout=900)
        if temporal == 'y':
            run(prefix + '-calendars', ['./tests/test_temporal_native_calendars'], cwd=tree, timeout=900)
            for name in ('tests/test_temporal_native_calendars.js', 'tests/test_temporal_native_zones.js'):
                run(prefix + '-' + Path(name).stem, ['./qjs', name], cwd=tree, timeout=900)
        run(prefix + '-repository-tests', alternate + ['test'], cwd=tree)


def summary(name, text):
    found = re.findall(r'^Result: (\d+)/(\d+) errors?([^\n]*)$', text, re.M)
    require(len(found) == 1, 'Missing or ambiguous Test262 summary: ' + name)
    failures, executed, tail = found[0]
    record = dict(error_source_count=int(failures), executed_source_count=int(executed),
                  runner_summary=found[0], excluded_source_count=sum(int(n) for n in re.findall(r'(\d+) excluded', tail)),
                  skipped_source_count=sum(int(n) for n in re.findall(r'(\d+) skipped', tail)),
                  known_error_changes={label: sum(int(n) for n in re.findall(r'(\d+) ' + label, tail))
                                       for label in ('new', 'changed', 'fixed')}, zero_error_conformance_claim=False)
    save(name + '-summary.json', record)
    return record


def input_identity():
    binding = json.loads((HERE / 'binding.json').read_text())
    initialized = json.loads((OUT / 'execution-identity.json').read_text())
    checkout = inspect_checkout()
    require(checkout['gitlinks'] == binding.get('gitlinks') == initialized.get('gitlinks'),
            'Changed initialized submodule commit or blob census')
    require(checkout['mode_capabilities'] == binding.get('mode_capabilities') ==
            initialized.get('mode_capabilities'),
            'Changed initialized filesystem mode capabilities')
    production = json.loads((OUT / 'actual-production-source-sha256.json').read_text())
    expected = dict(binding['source_sha256'], **binding['adapter_sha256'])
    require(checkout['head'] == initialized['candidate_sha'] and
            checkout['sources'] == production and checkout['modes'] ==
            dict(binding['git_modes'], **binding['adapter_git_modes']),
            'Changed initialized complete checkout commit, source or mode census')
    require(sha(HERE / 'binding.json') == sha(OUT / 'binding.json') == initialized['binding_sha256'] and
            initialized['candidate_sha'] == os.environ['GITHUB_SHA'] and
            initialized['actual_production_source_map_sha256'] == canonical(production) and
            production == expected, 'Current production map differs from initialized authenticated checkout')
    require(all(Path(path).is_file() and not Path(path).is_symlink() and sha(path) == digest
                for path, digest in production.items()), 'Changed actual initialized production source')
    fixture = {str(path.relative_to('test262')): sha(path) for path in Path('test262').rglob('*')
               if path.is_file() and '.git' not in path.relative_to('test262').parts}
    require(canonical(fixture) == FIXTURE_MAP, 'Changed complete fixture bytes')
    binding = json.loads((HERE / 'binding.json').read_text())
    require(all(Path(path).is_file() and not Path(path).is_symlink() and sha(path) == digest
                for path, digest in binding['source_sha256'].items()), 'Changed tracked final source')
    return dict(production_source_map_sha256=canonical(production),
                actual_initialized_checkout_commit=initialized['candidate_sha'],
                historical_baseline_production_source_map_sha256=HISTORICAL_BASELINE_PRODUCTION_MAP,
                actual_full_tracked_source_map_sha256=canonical(production),
                fixture_source_map_sha256=canonical(fixture),
                config_sha256=sha('test262-intl.conf'), baseline_sha256=sha('test262_errors.txt'),
                qjs_sha256=sha('qjs'), runner_sha256=sha('run-test262'))


def baseline_records(checkpoint):
    records = []
    for literal in checkpoint['known_error_baseline_lines']:
        match = re.fullmatch(r'(test262/test/.*?\.js):([0-9]+): (.*)', literal)
        require(match is not None, 'Malformed unchanged baseline reference')
        path, line, body = match.groups()
        strict = body.startswith('strict mode: ')
        records.append(dict(path=path, line=int(line), strict=strict,
                            body=body[len('strict mode: '):] if strict else body))
    return records


def parse_raw(name, distinct_source_count, baseline, before, after):
    receipt = json.loads((OUT / (name + '.command.json')).read_text())
    stdout = (OUT / (name + '.stdout.log')).read_text(encoding='utf-8', errors='strict')
    stderr = (OUT / (name + '.stderr.log')).read_text(encoding='utf-8', errors='strict')
    # Only absolute checkout-root prefixes are removed from parser views.
    # Both unchanged original streams remain authoritative artifacts.
    prefix = str(Path.cwd()) + '/'
    def view(content):
        return '\n'.join(line[len(prefix):] if line.startswith(prefix + 'test262/test/') else line
                         for line in content.splitlines())
    runnable = {str(path) for path in Path('test262/test').rglob('*.js')}
    parsed = reconcile(view(stdout), view(stderr), receipt['returncode'], baseline,
                       runnable, distinct_source_count)
    parsed.update(raw_collection_complete=receipt.get('lifecycle_closed') is True and
                  not receipt.get('timed_out') and not receipt.get('interrupted'),
                  sealed_inputs_unchanged=before == after,
                  parser_absolute_path_prefix_only=prefix,
                  stdout_sha256=receipt['stdout_sha256'], stderr_sha256=receipt['stderr_sha256'],
                  command_sha256=sha(OUT / (name + '.command.json')),
                  input_identity=after)
    save(name + '-raw-evidence.json', parsed)
    summary_path = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary_path:
        with Path(summary_path).open('a') as stream:
            stream.write('\n### ' + name + ' raw ' + BACKEND + ' result\n\n')
            stream.write('Runner returncode: ' + str(receipt['returncode']) + '\n\n')
            stream.write('Counters: `' + json.dumps(parsed['counters'], sort_keys=True) + '`\n\n')
            stream.write('Each original failing path/variant/signature is retained in `' +
                         name + '-raw-evidence.json`; matching baseline references are inferred.\n')
    return parsed


def require_collection(parsed):
    require(parsed['raw_parser_complete'] and parsed['raw_collection_complete'] and
            parsed['sealed_inputs_unchanged'], 'Incomplete raw run or SAN/infra/unknown/unreconciled input')


def test262_telemetry(name):
    return ['--progress-log', str(OUT / (name + '-execution.jsonl')),
            '--timing-log', str(OUT / (name + '-timings.jsonl'))]


def full_test262():
    checkpoint = json.loads((HERE / 'integration-checkpoint.json').read_text())
    baseline = Path('test262_errors.txt')
    transition = authenticate_transition(
        json.loads((OUT / 'actual-production-source-sha256.json').read_bytes()),
        evidence_bytes, input_identity())
    if transition is not None:
        proof = transition.receipt()
        save('core-baseline-transition-evidence.json', proof)
        checkpoint = dict(checkpoint,
            known_error_baseline_sha256=proof['current_baseline_sha256'],
            known_error_baseline_lines=proof['current_baseline_lines'],
            known_failing_source_count=proof['remaining_source_count'])
    require(sha(baseline) == checkpoint['known_error_baseline_sha256'] and
            baseline.read_text().splitlines() == checkpoint['known_error_baseline_lines'],
            'Reviewed known-error baseline changed')
    for path in ('test262-intl.conf', 'test262_errors.txt'):
        shutil.copyfile(path, OUT / ('input-' + path))
    before = input_identity()
    save('test262-inputs-before.json', before)
    log = run('test262', test262_workers(checkpoint['argv']) + test262_telemetry('test262') +
              ['-x', EXCLUSION_PATH],
              timeout=14400, check=False, raw_streams=True)
    result = summary('test262', log)
    receipt = json.loads((OUT / 'test262.command.json').read_text())
    after = input_identity()
    save('test262-inputs-after.json', after)
    parsed = parse_raw('test262', TEST262_PROFILE['runner_distinct_source_count'],
                       baseline_records(checkpoint), before, after)
    selection_transition = authenticate_slow_selection(
        json.loads((OUT / 'actual-production-source-sha256.json').read_bytes()),
        evidence_bytes, after, (OUT / 'test262.command.json').read_bytes(),
        parsed, (), OUT)
    save('test262-slow-selection-receipt.json', selection_transition.receipt())
    result.update(backend=BACKEND, compiler=CC, sanitizer=SAN, data_profile=TEST262_PROFILE,
                  selection='Full configured core, Intl and Temporal; eight slow DST sources additionally excluded',
                  authorized_slow_exclusions=selection_transition.receipt(),
                  known_error_baseline_sha256=sha(baseline),
                  baseline_matching_signatures_are_inferred_not_fresh_observations=True,
                  baseline_unchanged=before == after, returncode=receipt['returncode'],
                  raw_failures_preserved=True, native_profile_qualification_pending=BACKEND == 'native')
    save('test262-summary.json', result)
    require_collection(parsed)
    require(sha(baseline) == checkpoint['known_error_baseline_sha256'], 'Test262 mutated baseline')
    if BACKEND == 'icu':
        require(receipt['returncode'] == 0 and not any(result['known_error_changes'].values()) and
                result['error_source_count'] == checkpoint['known_failing_source_count'],
                'Independent ICU core baseline gate failed; native policy cannot admit ICU deltas')
    # Native collection success is not qualification. A later required workflow
    # step validates complete originals and exact per-file policy before any green job.


def effective_profile():
    run('effective-profile', ['./qjs', str(HERE / 'effective-profile.js')], timeout=900, raw_streams=True)
    profile = json.loads((OUT / 'effective-profile.stdout.log').read_text())
    require(profile.get('schema') == 'intl-effective-profile-v2' and
            not (OUT / 'effective-profile.stderr.log').read_bytes(), 'Invalid effective profile capture')
    save('effective-profile.json', profile)
    return canonical(profile)


def evidence_bytes(path):
    target = Path(path)
    require(not target.is_absolute() and '..' not in target.parts and
            target.is_file() and not target.is_symlink(), 'Unsafe or missing evidence source: ' + path)
    return target.read_bytes()


def attach_premise_source_guard_review(packet, context, invocation):
    """Authenticate approved source scope, then bind fresh row identities only."""
    review_name = invocation.get('premise_source_guard_review')
    require(isinstance(review_name, str) and review_name.startswith('tools/ci/native-prefix/') and
            not Path(review_name).is_absolute() and '..' not in Path(review_name).parts and
            Path(review_name).is_file() and not Path(review_name).is_symlink() and
            sha(review_name) == invocation.get('premise_source_guard_review_sha256'),
            'Unbound premise source guard review')
    scope = json.loads(Path(review_name).read_bytes())
    require(scope.get('schema') == 'native-premise-root-source-guard-review-v1' and
            scope.get('source_review_complete') is True and
            scope.get('conformance_accepted') is False and
            scope.get('semantic_adjudication_complete') is False,
            'Actual source guard review remains required')
    dynamic = ('engine_sha256', 'source_tree_sha256', 'profile_sha256', 'profile_canonical_sha256')
    require(all(scope.get(key) is None for key in dynamic),
            'Static source scope must not contain stale row identities')
    files = json.loads((OUT / 'actual-production-source-sha256.json').read_bytes())
    require(canonical(files) == context.get('source_tree_sha256') ==
            context.get('production_source_map_sha256') ==
            context.get('actual_full_tracked_source_map_sha256') and
            all(Path(name).is_file() and not Path(name).is_symlink() and sha(name) == expected
                for name, expected in files.items()),
            'Premise current full source identity differs')
    packet = Path(packet)
    from premise_source_compatibility import APPROVAL, ADAPTER, LAUNCHER, THREADS
    current_helpers = {name: sha(name) for name in (APPROVAL, ADAPTER, LAUNCHER, THREADS)}
    require(invocation.get('current_compatibility_sources') == current_helpers and
            invocation.get('capture_launcher') == LAUNCHER and
            invocation.get('capture_launcher_sha256') == current_helpers[LAUNCHER] and
            invocation.get('historical_capture_launcher') ==
                str(packet / 'launch_practical_witness.py') and
            invocation.get('historical_capture_launcher_sha256') ==
                sha(packet / 'launch_practical_witness.py'),
            'Current compatibility invocation differs from reviewed sources')
    static_files = dict(guard_policy_sha256='PREMISE-GUARD-POLICY.json',
        runtime_sha256='witness-runtime.js', source_pins_sha256='PREMISE-SOURCE-PINS.json',
        premise_data_sha256='PREMISE-DATA.json', trace_proof_sha256='premise_trace_proof.py')
    require(all(scope.get(key) == sha(packet / name) for key, name in static_files.items()),
            'Premise reviewed guard source hashes differ')
    helpers = {name: sha(packet / name) for name in
        ('guard_binding.py', 'render_practical_witness.py', 'validate_practical_witness.py', 'premise_trace_proof.py')}
    require(scope.get('helper_sha256') == helpers, 'Premise reviewed helper hashes differ')
    policy = json.loads((packet / 'PREMISE-GUARD-POLICY.json').read_bytes())
    pins = json.loads((packet / 'PREMISE-SOURCE-PINS.json').read_bytes())
    from premise_source_compatibility import authenticated_receipt
    compatibility = authenticated_receipt(files, packet,
        invocation['premise_source_guard_review_sha256'])
    require(scope.get('reviewed_guard_sites') == policy.get('guarded_sites') and
            all(files.get(name) == compatibility['subjects'].get(name, {}).get(
                'current_sha256', expected) for name, expected in pins.items()),
            'Premise exact reviewed sites or current production pins differ')
    fresh = dict(scope)
    fresh.update({key: context[key] for key in dynamic if key != 'profile_canonical_sha256'})
    fresh['profile_canonical_sha256'] = hashlib.sha256(json.dumps(context['profile'],
        sort_keys=True, ensure_ascii=True, separators=(',', ':')).encode()).hexdigest()
    fresh['approved_source_scope_sha256'] = invocation['premise_source_guard_review_sha256']
    context['premise_source_guard_review'] = fresh
    context['premise_source_guard_review_sha256'] = hashlib.sha256(json.dumps(fresh,
        sort_keys=True, ensure_ascii=True, separators=(',', ':')).encode()).hexdigest()
    context['current_source_files'] = files
    context['premise_source_compatibility'] = compatibility
    save('native-premise-current-runner-compatibility.json', compatibility)
    save('native-premise-current-source-guard-review.json', fresh)
    return context


def selected_provider_preflight(packet, context):
    """Isolated diagnostic binary; ordinary repo/API/raw/benchmark artifacts stay bound."""
    sys.path.insert(0, str(packet.resolve()))
    from selected_timezone_orchestration import (primary_map, fresh_directory,
        prepare_input, canonical as compact_sha, DIAG_SCHEMA)
    labels, _ = primary_map(packet)
    plan = json.loads((packet / 'HOSTED-RENDER-PLAN.json').read_bytes())
    fixture = next(f for f in plan['fixtures']
                   if f['path'] == 'test262/test/intl402/Temporal/ZonedDateTime/links.js')
    tree = OUT / 'selected-provider-diagnostic-build'
    require(not tree.exists(), 'Diagnostic build must be fresh')
    tree.mkdir()
    files = context['current_source_files']
    for name, expected in sorted(files.items()):
        require(Path(name).is_file() and not Path(name).is_symlink() and sha(name) == expected,
                'Diagnostic source differs: ' + name)
        destination = tree / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(name, destination)
    # Reassert the approved release input census within the authenticated
    # submodule file map, without copying its Git metadata.
    tzfiles = {
        "LICENSE": "0613408568889f5739e5ae252b722a2659c02002839ad970a63dc5e9174b27cf",
        "africa": "f2851d4be4a4925cbdc9d56e10d780bccadb89d6ffb9aed78c3e35f97c200aed",
        "antarctica": "e410ad71c9450828c592d21419301d41ac79ce50159fd0ac2d6c5031cb6bdfe6",
        "asia": "02b69728085f6b0c999da70de2f1f9a2d8952b533a450d53bb41e03eb7d0e92e",
        "australasia": "846ba455578e3e0f9eb850f05be3cc06d02ff51bbe4b726f755be9dc765f16c9",
        "backward": "b8296b9ef1ece72d813878132c0f17e2c52b26af6004eeb9c37d049b64cd9401",
        "backzone": "9c965661692c07d241c758c41c6f1e9c082d0f77a193d9240228b995e2f1666f",
        "etcetera": "7281f095b42c13c4ae36b8bcba884e81dbb38127221fc1d9805c4dbf852487db",
        "europe": "fec11bf6c6bd0619a8777bbdaa2ecd184fde291cb4b97efa87d334c7145ba3c2",
        "factory": "ae2ec1d36dabf79a69cb7dd4fb6fd9168d05fc8cfd31aee2dd19e4f18beb9885",
        "northamerica": "c0c5da0dc337444d80709cf9f8d8011719b789df160ce107a5c38c90e73dc54c",
        "private.h": "dcf3a0041cebf13bebdf4c8e4d1163b7fb4751a16b9f9ebde2cbf79e63b3b4ff",
        "southamerica": "13be56a242da3c1a4a6d12050e5fd48b1753c8c1dc237cbc2edb40eac636be7f",
        "tzfile.h": "449eceb0318327d7e885c9ab537a88084c98f03735758e4cf3e9ad1ebae79a1c",
        "zic.c": "1e6c712d271b8da7c62c358fdc5c2d978b176cb258951b6da599e6b1a503e0ed",
        "ziguard.awk": "d01a09ded697379bc6df555fa6ce9b976dfb502df01c3741fca69fbd07f27569",
        "zone.tab": "e2e8e31c4e10f44576c79345e76060591dbb3eef6ce14e006afc8a373caa9593"
    }
    for name, expected in tzfiles.items():
        source = Path('third_party/tz') / name
        require(source.is_file() and not source.is_symlink() and sha(source) == expected,
                'Pinned timezone generator input differs: ' + name)
        destination = tree / 'third_party/tz' / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    save('selected-provider-generator-fixture.json', dict(files_sha256=tzfiles,
         source='Same pinned release prerequisites as ordinary generated provider'))
    args = make_args() + ['CONFIG_TZ_CAPTURE_SNAPSHOT=y']
    run('selected-provider-diagnostic-build', args + ['-j' + str(available_cpus()), 'all', 'selected-timezone-probe'], cwd=tree)
    objdir = '.obj/asan' if SAN == 'ASan' else '.obj/ubsan' if SAN == 'UBSan' else '.obj'
    probe = tree / objdir / 'tools/ci/native-prefix/probe-selected-timezones'
    require(probe.is_file(), 'Missing diagnostic preflight probe')
    run('selected-provider-effective-profile', ['./qjs', str(HERE / 'effective-profile.js')],
        cwd=tree, timeout=900, raw_streams=True)
    require(json.loads((OUT / 'selected-provider-effective-profile.stdout.log').read_bytes()) == context['profile']
            and not (OUT / 'selected-provider-effective-profile.stderr.log').read_bytes(),
            'Diagnostic effective profile differs from ordinary profile')
    config_path = OUT / 'selected-provider-compiler-configuration.json'
    # This source path has the fixed native default root. A future override
    # must become an explicit configuration input and regenerate this role.
    require('#define QJS_TZ_SYSTEM_DIR "/usr/share/zoneinfo"' in Path('src/timezone/timezone.c').read_text()
            and not any('QJS_TZ_SYSTEM_DIR' in arg for arg in args), 'Unbound compiled system root')
    save(config_path.name, dict(make_args=args, build_command_sha256=sha(OUT / 'selected-provider-diagnostic-build.command.json'),
         diagnostic_configuration={'CONFIG_TZ_CAPTURE_SNAPSHOT':'y','CONFIG_ICU':'n','normal_policy':'system-first'},
         source_tree_sha256=context['source_tree_sha256'], ordinary_engine_sha256=context['engine_sha256'],
         diagnostic_engine_sha256=sha(tree / 'qjs'), probe_sha256=sha(probe),
         generated_fixture_sha256=sha(OUT / 'selected-provider-generator-fixture.json'),
         backend=BACKEND, compiler=CC, compiler_path=shutil.which(CC),
         compiler_sha256=sha(shutil.which(CC)), sanitizer=SAN, system_root='/usr/share/zoneinfo'))
    diagnostic = dict(schema=DIAG_SCHEMA, normal_engine_sha256=context['engine_sha256'],
         engine_sha256=sha(tree / 'qjs'), qjs=str((tree / 'qjs').resolve()), probe=str(probe.resolve()),
         probe_sha256=sha(probe), source_tree_sha256=context['source_tree_sha256'],
         profile_sha256=context['profile_sha256'], normal_policy='system-first', system_root='/usr/share/zoneinfo',
         compiler_configuration=str(config_path), compiler_configuration_sha256=sha(config_path),
         fixture_sha256=fixture['official_original_sha256'],
         execution_identity=str(OUT / 'execution-identity.json'),
         execution_identity_sha256=sha(OUT / 'execution-identity.json'),
         hosted_identity=dict(workflow=os.environ['GITHUB_WORKFLOW'], job=os.environ['GITHUB_JOB'],
             run_id=os.environ['GITHUB_RUN_ID'], run_attempt=os.environ['GITHUB_RUN_ATTEMPT']),
         preflight_command=str(OUT / 'selected-provider-preflight.command.json'),
         conformance_accepted=False, semantic_adjudication_complete=False)
    directory = fresh_directory(OUT / 'selected-provider-preflight', OUT)
    diagnostic['preflight_directory'] = str(directory)
    env = dict(ENV, QJS_TZ_CAPTURE_DIRECTORY=str(directory))
    run('selected-provider-preflight', [str(probe)] + sorted(set(labels.values())), cwd=tree,
        env=env, timeout=900, raw_streams=True)
    command_path = Path(diagnostic['preflight_command'])
    command = json.loads(command_path.read_bytes())
    require(not (OUT / 'selected-provider-preflight.stderr.log').read_bytes() and
            not (OUT / 'selected-provider-preflight.stdout.log').read_bytes(), 'Preflight unexpected output')
    command.update(output_limited=False, engine_sha256=diagnostic['engine_sha256'],
        source_tree_sha256=context['source_tree_sha256'], profile_sha256=context['profile_sha256'],
        effective_env_sha256=compact_sha(env),
        provider_environment_sha256=compact_sha({key: value for key, value in env.items()
            if key != 'QJS_TZ_CAPTURE_DIRECTORY'}),
        observer_environment={'QJS_TZ_CAPTURE_DIRECTORY': str(directory)},
        observer_environment_sha256=compact_sha({'QJS_TZ_CAPTURE_DIRECTORY': str(directory)}))
    save(command_path.name, command)
    proof = prepare_input(packet, context, diagnostic, command, directory)
    proof_path = OUT / 'selected-provider-preflight-input-proof.json'
    save(proof_path.name, proof)
    diagnostic.update(preflight_input_proof=str(proof_path), preflight_input_proof_sha256=sha(proof_path))
    context['selected_provider_diagnostic'] = diagnostic
    save('selected-provider-diagnostic-binding.json', diagnostic)
    require(context['engine_sha256'] == sha('qjs'), 'Diagnostic build altered ordinary engine')
    return context


def selected_provider_postflight(packet, context):
    """Observe the same compiled system-first provider after successful capture."""
    sys.path.insert(0, str(packet.resolve()))
    from selected_timezone_orchestration import (primary_map, fresh_directory,
        prepare_input, postflight_receipt, canonical as compact_sha, closed_observation)
    diagnostic = context['selected_provider_diagnostic']
    capture_path = OUT / 'native-original-assertion-capture.command.json'
    capture = json.loads(capture_path.read_bytes())
    # This runs only after the launcher returns cleanly; a failed capture never
    # gains a postflight receipt or an equality claim.
    capture['output_limited'] = False
    closed_observation(capture)
    capture.update(engine_sha256=context['engine_sha256'],
        ordinary_engine_sha256=context['engine_sha256'],
        source_tree_sha256=context['source_tree_sha256'], profile_sha256=context['profile_sha256'])
    save(capture_path.name, capture)
    require(context['engine_sha256'] == sha('qjs') and
            input_identity() == {key: context[key] for key in input_identity()},
            'Capture changed ordinary source/fixture/engine/config inputs')
    labels, _ = primary_map(packet)
    directory = fresh_directory(OUT / 'selected-provider-postflight', OUT)
    env = dict(ENV, QJS_TZ_CAPTURE_DIRECTORY=str(directory))
    run('selected-provider-postflight', [diagnostic['probe']] + sorted(set(labels.values())),
        cwd=Path(diagnostic['qjs']).parent, env=env, timeout=900, raw_streams=True)
    require(not (OUT / 'selected-provider-postflight.stdout.log').read_bytes() and
            not (OUT / 'selected-provider-postflight.stderr.log').read_bytes(),
            'Postflight unexpected output')
    command_path = OUT / 'selected-provider-postflight.command.json'
    command = json.loads(command_path.read_bytes())
    command.update(output_limited=False, engine_sha256=diagnostic['engine_sha256'],
        source_tree_sha256=context['source_tree_sha256'], profile_sha256=context['profile_sha256'],
        effective_env_sha256=compact_sha(env),
        provider_environment_sha256=compact_sha({key: value for key, value in env.items()
            if key != 'QJS_TZ_CAPTURE_DIRECTORY'}),
        observer_environment={'QJS_TZ_CAPTURE_DIRECTORY': str(directory)},
        observer_environment_sha256=compact_sha({'QJS_TZ_CAPTURE_DIRECTORY': str(directory)}))
    save(command_path.name, command)
    derived = dict(diagnostic, preflight_directory=str(directory), preflight_command=str(command_path))
    proof = prepare_input(packet, context, derived, command, directory)
    proof['role'] = 'postflight-selected-provider-input-proof-v1'
    proof_path = OUT / 'selected-provider-postflight-input-proof.json'
    save(proof_path.name, proof)
    binding = dict(capture_directory=str(directory), command_receipt=str(command_path),
                   input_proof=str(proof_path), input_proof_sha256=sha(proof_path))
    comparison = postflight_receipt(packet, context, binding, capture_path)
    save('selected-provider-postflight-comparison.json', dict(binding=binding, comparison=comparison))
    require(context['engine_sha256'] == sha('qjs') and
            input_identity() == {key: context[key] for key in input_identity()},
            'Postflight changed ordinary source/fixture/engine/config inputs')


def native_assertion_witness():
    require(BACKEND == 'native', 'Native original assertions only')
    profile_sha256 = effective_profile()
    context = dict(input_identity(), effective_profile_sha256=profile_sha256)
    context.update(source_tree_sha256=context['production_source_map_sha256'],
                   engine_sha256=context['qjs_sha256'], profile_sha256=profile_sha256,
                   integrated_source_fix_ids=['L1', 'T1', 'F1', 'F2', 'H1'])
    invocation = json.loads((HERE / 'native-witness-invocation.json').read_text())
    require(invocation.get('authorized') is True, 'REQUIRED practical witness integration is incomplete')
    require(isinstance(invocation.get('packet_root'), str) and
            invocation['packet_root'].startswith('tools/ci/native-prefix/') and
            sha(Path(invocation['packet_root']) / 'SEAL.json') == invocation.get('packet_seal_sha256'),
            'Unbound practical witness packet')
    context['packet_seal_sha256'] = invocation['packet_seal_sha256']
    context['profile'] = json.loads((OUT / 'effective-profile.json').read_text())
    context = attach_premise_source_guard_review(Path(invocation['packet_root']), context, invocation)
    context = selected_provider_preflight(Path(invocation['packet_root']), context)
    context['witness_execution_budget'] = dict(
        schema='native-current-witness-execution-budget-v1', backend=BACKEND,
        compiler=CC, sanitizer=SAN, source_tree_sha256=context['source_tree_sha256'],
        engine_sha256=context['engine_sha256'],
        checkout_commit=context['actual_initialized_checkout_commit'],
        build_policy=dict(path=str((OUT / 'candidate-build-policy.json').resolve()),
                          sha256=sha(OUT / 'candidate-build-policy.json')),
        build_command=dict(path=str((OUT / 'build.command.json').resolve()),
                           sha256=sha(OUT / 'build.command.json')))
    save('native-witness-current-bindings.json', context)
    for field in ('capture_launcher', 'validator', 'configuration'):
        path = invocation.get(field)
        require(isinstance(path, str) and path.startswith('tools/ci/native-prefix/') and
                sha(path) == invocation.get(field + '_sha256'), 'Unbound practical witness input')
    # The practical capture runs authenticated originals for both variants.
    # A REQUIRED independently reviewed adapter merges/revalidates current traces
    # and the dependency-bound RTF receipt into the gate's summary projection.
    policy = json.loads((HERE / 'native-profile-diagnostic-policy.json').read_text())
    raw = json.loads((OUT / 'test262-raw-evidence.json').read_text())
    packet = Path(invocation['packet_root'])
    plan = json.loads((packet / 'HOSTED-RENDER-PLAN.json').read_text())
    ordinary = {fixture['path'] for fixture in plan['fixtures'] + plan.get('oracle_fixtures', [])}
    relative = set(plan['existing_relative_time_capture']['paths'])
    official = ordinary | relative
    require(len(ordinary) == 105 and len(relative) == 13 and len(official) == 118,
            'Complete original103 + oracle2 + RTF13 selection required')
    observed = {record['path'] for record in raw['raw_observations']
                if record['status'] == 'observed-failure'}
    require(observed <= official, 'New unclassified native original assertion source')
    selection = OUT / 'native-original-assertion-selection.json'
    selection.write_text(json.dumps(sorted(official), indent=2) + '\n')
    receipt = OUT / 'native-original-normal-raw-receipt.json'
    save(receipt.name, dict(raw_preserved=True, returncode=raw['raw_runner_returncode'],
         counters=raw['counters'], parsed_raw_evidence_sha256=sha(OUT / 'test262-raw-evidence.json'),
         raw_stdout_sha256=sha(OUT / 'test262.stdout.log'), raw_stderr_sha256=sha(OUT / 'test262.stderr.log'),
         raw_command_sha256=sha(OUT / 'test262.command.json'), input_identity=raw['input_identity']))
    run('native-original-assertion-capture', [sys.executable, '-I', '-B', invocation['capture_launcher'],
        '--packet', invocation['packet_root'], '--bindings', str(OUT / 'native-witness-current-bindings.json'),
        '--qjs', str(Path('qjs').resolve()), '--raw-full-run', str(receipt),
        '--output-root', str(OUT / 'original-assertions'), '--selection', str(selection)],
        timeout=14400, raw_streams=True)
    selected_provider_postflight(packet, context)
    run('native-original-assertion-revalidate', [sys.executable, '-I', '-B', invocation['validator'],
        '--config', invocation['configuration'], '--bindings', str(OUT / 'native-witness-current-bindings.json'),
        '--raw-full-run', str(OUT / 'test262-raw-evidence.json'), '--capture', str(OUT / 'original-assertions'),
        '--summary', str(OUT / 'original-assertions-validated.json')], timeout=900, raw_streams=True)
    save('original-assertions-validation-binding.json', dict(
        summary_sha256=sha(OUT / 'original-assertions-validated.json'),
        projector_sha256=sha(invocation['validator']),
        configuration_sha256=sha(invocation['configuration']),
        command_sha256=sha(OUT / 'native-original-assertion-revalidate.command.json')))
    require(input_identity() == {key: context[key] for key in input_identity()},
            'Original assertion capture changed source/fixture/engine/config inputs')


def exact_profile_gate():
    if BACKEND == 'icu':
        require(json.loads((OUT / 'phase-test262.json').read_text()).get('success') is True,
                'Missing independent ICU full-suite gate')
        return
    required_phases = ('repository-tests', 'native-units', 'public-api', 'api-variants',
                       'oracle-diagnostics', 'required-test262', 'test262', 'native-assertion-witness')
    for phase in required_phases:
        receipt = json.loads((OUT / ('phase-' + phase + '.json')).read_text())
        require(receipt.get('success') is True and receipt.get('backend') == BACKEND and
                receipt.get('compiler') == CC and receipt.get('sanitizer') == SAN and
                receipt.get('qjs_sha256') == sha('qjs'), 'Missing current mandatory phase: ' + phase)
    run('native-normative-scope-unit-tests', [sys.executable, '-B', '-m', 'unittest',
        'discover', '-s', str(HERE), '-p', 'test_native_normative_scope.py'], timeout=900)
    policy = json.loads((HERE / 'native-profile-diagnostic-policy.json').read_text())
    parsed = json.loads((OUT / 'test262-raw-evidence.json').read_text())
    context = dict(input_identity(), effective_profile_sha256=canonical(
                   json.loads((OUT / 'effective-profile.json').read_text())),
                   mandatory_repository_gates_passed=True)
    require(parsed['input_identity'] == input_identity(), 'Gate source differs from original full run')
    invocation = json.loads((HERE / 'native-witness-invocation.json').read_text())
    projection = json.loads((OUT / 'original-assertions-validation-binding.json').read_text())
    require(projection == dict(summary_sha256=sha(OUT / 'original-assertions-validated.json'),
        projector_sha256=sha(invocation['validator']),
        configuration_sha256=sha(invocation['configuration']),
        command_sha256=sha(OUT / 'native-original-assertion-revalidate.command.json')) and
        projection['projector_sha256'] == invocation['validator_sha256'] and
        projection['configuration_sha256'] == invocation['configuration_sha256'],
        'Changed output of authenticated current projector invocation')
    validated = json.loads((OUT / 'original-assertions-validated.json').read_text())
    require(validated.get('engine_sha256') == context['qjs_sha256'] and
            validated.get('source_tree_sha256') == context['production_source_map_sha256'] and
            validated.get('profile_sha256') == context['effective_profile_sha256'] and
            validated.get('packet_seal_sha256') == invocation['packet_seal_sha256'] and
            validated.get('raw_full_evidence_sha256') == sha(OUT / 'test262-raw-evidence.json'),
            'Current projector summary source/profile/raw binding differs')
    current_bindings = json.loads((OUT / 'native-witness-current-bindings.json').read_bytes())
    require(current_bindings['engine_sha256'] == context['qjs_sha256'] and
            current_bindings['source_tree_sha256'] == context['production_source_map_sha256'] and
            current_bindings['profile_sha256'] == context['effective_profile_sha256'],
            'Diagnostic role ordinary identity differs from raw context')
    packet = Path(invocation['packet_root']).resolve()
    sys.path.insert(0, str(packet))
    from selected_timezone_orchestration import authenticated_input, authenticated_postflight
    authenticated_input(packet, current_bindings)
    context['selected_provider_postflight'] = authenticated_postflight(packet, current_bindings,
        OUT / 'selected-provider-postflight-comparison.json',
        OUT / 'native-original-assertion-capture.command.json')
    context['selected_provider_diagnostic'] = current_bindings['selected_provider_diagnostic']
    fixture_hashes = {'test262/' + path: digest for path, digest in
                      json.loads((OUT / 'test262-fixture-source-sha256.json').read_text()).items()}
    sources = json.loads((OUT / 'actual-production-source-sha256.json').read_bytes())
    scopes = bind_static_scopes(dict(path=SCOPE_PATH, sha256=sha(SCOPE_PATH)),
        context=context, sources=sources, fixture_hashes=fixture_hashes,
        validated=validated, raw=parsed, raw_sha256=sha(OUT / 'test262-raw-evidence.json'),
        load_bytes=evidence_bytes)
    context['native_normative_scopes'] = scopes
    if scopes.baseline_transition() is not None:
        context['core_baseline_transition'] = scopes.baseline_transition()
    policy = scopes.document_for('diagnostic-policy')
    selection_transition = authenticate_slow_selection(
        sources, evidence_bytes, context,
        (OUT / 'test262.command.json').read_bytes(), parsed,
        validated['fixture_summaries'], OUT)
    context['slow_test_selection'] = selection_transition
    policy = selection_transition.derive_policy(policy, context)
    save('native-current-slow-selection-receipt.json', selection_transition.receipt())
    save('native-current-normative-scope-envelope.json', scopes.receipt())
    native_oracle_gate(context, validated, fixture_hashes)
    result = qualify_native(parsed, policy, context, fixture_hashes, validated, evidence_bytes)
    save('native-profile-qualification.json', result)
    selections = json.loads((HERE / 'selections.json').read_text())
    require([item['distinct_source_count'] for item in selections] == [18, 2, 114],
            'Required 18/2/114 selections changed')
    for item in selections:
        name = 'required-' + item['name']
        selected = json.loads((OUT / (name + '-raw-evidence.json')).read_text())
        require_collection(selected)
        counts = selected['counters']
        require(counts['executed_source_count'] == item['distinct_source_count'] and
                counts['excluded'] == 0 and counts['skipped'] == 0, 'Required selection was excluded/skipped')
        if counts['failed_source_count'] == 0 and selected['raw_runner_returncode'] == 0:
            require(not any(counts[key] for key in ('new', 'changed', 'fixed')), 'Unexpected required events')
            save(name + '-qualification.json', dict(strict_original_selection_passed=True))
        else:
            save(name + '-qualification.json', qualify_native(selected, policy, context,
                 fixture_hashes, validated, evidence_bytes, selection=item['name']))


def selection_configuration(source, empty, selected):
    """Keep production features/exclusions; normalize only selection paths/outputs."""
    output, section = [], None
    fixture = Path('test262').resolve()
    for line in source.splitlines():
        stripped = line.strip()
        if stripped.startswith('['):
            section = stripped
        elif section == '[tests]' and stripped and not stripped.startswith('#'):
            require(False, 'Production configuration adds unexpected positional cases')
        elif section == '[config]' and '=' in line:
            key, value = line.split('=', 1)
            replacements = {'harnessdir': str(fixture / 'harness'), 'testdir': str(empty),
                            'errorfile': '/dev/null', 'reportfile': 'none'}
            if key.strip() in replacements:
                line = key + '=' + replacements[key.strip()]
        elif section == '[exclude]' and stripped and not stripped.startswith('#'):
            require(stripped.startswith('test262/'), 'Unexpected exclusion path')
            relative = stripped[len('test262/'):]
            require(not any(name.startswith(relative) if relative.endswith('/') else name == relative
                            for name in selected), 'Required case violates production exclusion: ' + relative)
            line = str(fixture / relative) + ('/' if relative.endswith('/') else '')
        output.append(line)
    return '\n'.join(output) + '\n'


def required_test262():
    empty = OUT / 'empty-required-selection'
    empty.mkdir()
    parsed_results = []
    selections = json.loads((HERE / 'selections.json').read_text())
    require([item['distinct_source_count'] for item in selections] == [18, 2, 114],
            'Required source census changed')
    for item in selections:
        source = HERE / item['path']
        require(sha(source) == item['sha256'], 'Changed required selection')
        names = source.read_text().splitlines()
        require(len(names) == len(set(names)) == item['distinct_source_count'], 'Changed selection cardinality')
        config = OUT / (item['name'] + '-selection.conf')
        config.write_text(selection_configuration(Path('test262-intl.conf').read_text(), empty, names))
        selected = OUT / (item['name'] + '-cases.txt')
        selected.write_text(''.join(str((Path('test262') / name).resolve()) + '\n' for name in names))
        name = 'required-' + item['name']
        before = input_identity()
        log = run(name, ['./run-test262', '-T', str(available_cpus()), '-q', '-d', str(empty), '-c', str(config),
                  '-a', '--count_skipped_features', '-e', '/dev/null', '-r', 'none'] +
                  test262_telemetry(name) + [str(selected)],
                  check=False, raw_streams=True)
        result = summary(name, log)
        result.update(selection_sha256=sha(source), actual_source_count=len(names),
                      actual_case_list_sha256=sha(selected), known_error_file='/dev/null',
                      source_config_sha256=sha('test262-intl.conf'), selection_config_sha256=sha(config),
                      raw_failures_preserved=True, native_profile_qualification_pending=BACKEND == 'native')
        save(name + '-summary.json', result)
        parsed_results.append((item, parse_raw(name, len(names), [], before, input_identity())))
    # Collect all 18+2+114 original sources before applying any gate.
    for item, parsed in parsed_results:
        require_collection(parsed)
        counts = parsed['counters']
        require(counts['executed_source_count'] == item['distinct_source_count'] and
                counts['excluded'] == 0 and counts['skipped'] == 0,
                'Required original selection source was excluded/skipped')
        if BACKEND == 'icu':
            require(parsed['raw_runner_returncode'] == 0 and counts['failed_source_count'] == 0,
                    'Independent ICU required selection failed')


def oracle_diagnostics():
    policy = json.loads((HERE / 'oracle-policy.json').read_text())
    require(json.loads(Path('tests/intl-date-time-oracle-policy.json').read_text()) == policy,
            'Source oracle policy changed')
    native_policy = json.loads((HERE / 'native-oracle-profile-policy.json').read_text()) if BACKEND == 'native' else None
    paths = policy['fixture_sha256']
    parsed_oracles = {}
    source = Path('test262-intl.conf').read_text()
    section, removed, lines = None, [], []
    for line in source.splitlines():
        stripped = line.strip()
        if stripped.startswith('['):
            section = stripped
        if section == '[exclude]' and stripped in paths:
            removed.append(stripped)
        else:
            lines.append(line)
    require(sorted(removed) == sorted(paths), 'Reviewed oracle exclusions changed')
    empty = OUT / 'empty-oracle-selection'
    empty.mkdir()
    config = OUT / 'raw-original-oracles.conf'
    config.write_text(selection_configuration('\n'.join(lines) + '\n', empty,
                      [path[len('test262/'):] for path in paths]))
    outcomes = {}
    for path, expected in paths.items():
        require(sha(path) == expected, 'Raw original oracle bytes changed')
        selected = OUT / (Path(path).stem + '-oracle-cases.txt')
        fixture = Path(path).resolve()
        selected.write_text(str(fixture) + '\n')
        name = 'raw-original-' + Path(path).stem
        before = input_identity()
        log = run(name, ['./run-test262', '-T', str(available_cpus()), '-q', '-d', str(empty), '-c', str(config),
                  '-a', '--count_skipped_features', '-e', '/dev/null', '-r', 'none'] +
                  test262_telemetry(name) + [str(selected)], check=False, raw_streams=True)
        result = summary(name, log)
        rc = json.loads((OUT / (name + '.command.json')).read_text())['returncode']
        parsed_oracles[path] = parse_raw(name, 1, [], before, input_identity())
        signature = policy['raw_original_observed']['failure_signatures'][0 if path.endswith('hourCycle-default.js') else 1]
        failure_line = str(fixture) + ':30: unexpected error: ' + signature
        meaningful = [line for line in log.splitlines() if line and not line.startswith('Result: ') and
                      not re.fullmatch(re.escape(str(config)) + r':\d+: ignoring testdir=' + re.escape(str(empty)), line)]
        passed = rc == 0 and result['runner_summary'] == ('0', '1', '') and not meaningful
        pinned = rc == 1 and result['runner_summary'] == ('1', '1', ', 1 new') and meaningful == [failure_line]
        outcomes[path] = dict(returncode=rc, summary=result, fixture_sha256=expected,
                              outcome='PASS' if passed else 'ONLY_PINNED_DATA_ORACLE_ASSERTION' if pinned and BACKEND == 'icu' else 'REVIEW_REQUIRED')
        save('oracle-diagnostics.json', outcomes)
    run('oracle-exact-witness', ['./qjs', str(HERE / 'diagnostic-witness.js')], timeout=900, raw_streams=True)
    expected = json.loads((HERE / 'diagnostic-witness-expected.log').read_text())
    for path, outcome in outcomes.items():
        if outcome['outcome'] != 'PASS':
            continue
        if path.endswith('hourCycle-default.js'):
            expected['japaneseHour12']['hourCycle'] = 'h11'
        else:
            expected['hanidec']['formatted'] = expected['hanidec']['formatted'].replace('\u202f', ' ')
            expected['hanidec']['codePoints'][8] = 'U+0020'
            expected['hanidec']['parts'][5]['value'] = ' '
            expected['hanidec']['officialAsciiSpaceIncludes'] = True
    actual_witness = json.loads((OUT / 'oracle-exact-witness.stdout.log').read_text())
    if BACKEND == 'native':
        save('native-oracle-actual.json', dict(outcomes=outcomes, parsed=parsed_oracles,
             exact_witness=actual_witness, source=input_identity(), profile_sha256=effective_profile(),
             raw_failures_preserved=True, exact_native_oracle_review_pending=True))
    for companion in policy['normative_companions']:
        run('oracle-companion-' + Path(companion).stem, ['./qjs', companion], timeout=900)
    require(not (OUT / 'oracle-exact-witness.stderr.log').read_bytes(), 'Oracle witness emitted stderr')
    if BACKEND == 'icu':
        expected_bytes = (json.dumps(expected, ensure_ascii=False, separators=(',', ':')) + '\n').encode()
        require((OUT / 'oracle-exact-witness.stdout.log').read_bytes() == expected_bytes, 'Changed ICU oracle witness')
    else:
        save('native-oracle-actual.json', dict(outcomes=outcomes, parsed=parsed_oracles,
             exact_witness=actual_witness, source=input_identity(), profile_sha256=effective_profile(),
             raw_failures_preserved=True, exact_native_oracle_review_pending=True))
    for path, parsed in parsed_oracles.items():
        require_collection(parsed)
        if BACKEND == 'icu':
            require(outcomes[path]['outcome'] in ('PASS', 'ONLY_PINNED_DATA_ORACLE_ASSERTION'),
                    'Unadjudicated original ICU oracle result: ' + path)
    save('oracle-scope.json', dict(outcomes=outcomes, source_map=json.loads((HERE / 'binding.json').read_text())['source_map_sha256'],
         raw_original_actual_count=2, accepted_errorfile_entries=False, exact_witness_sha256=sha(OUT / 'oracle-exact-witness.log'),
         mandatory_companions_completed=True, official_zero_error_claim=False))


def native_oracle_gate(context, validated, fixture_hashes):
    scopes = context['native_normative_scopes']
    policy = scopes.document_for('oracle-policy')
    actual = json.loads((OUT / 'native-oracle-actual.json').read_text())
    require(policy.get('schema') == 'native-exact-oracle-profile-policy-v2' and
            policy.get('failure_waiver') is False and policy.get('conformance_accepted') is False,
            'Invalid native oracle policy schema, failure waiver or conformance authority')
    require(policy.get('profile_diagnostic_authorized') is True,
            'REQUIRED actual native fallback/hour-cycle/hanidec per-file review')
    require(policy.get('production_source_map_sha256') == context['production_source_map_sha256'] and
            policy.get('effective_profile_sha256') == context['effective_profile_sha256'] and
            actual['source'] == input_identity() and actual['profile_sha256'] == context['effective_profile_sha256'],
            'Native oracle source/profile differs from review')
    require(canonical(actual['outcomes']) == canonical(policy.get('actual_original_outcomes')) and
            actual['exact_witness'] == policy.get('exact_witness'), 'Changed native original oracle identity/witness')
    witness = actual['exact_witness']
    hanidec = witness['hanidec']
    parts = hanidec['parts']
    require(''.join(part['value'] for part in parts) == hanidec['formatted'], 'Oracle parts/string mismatch')
    numeric = [(part['type'], part['value']) for part in parts if part['type'] in ('hour', 'minute', 'second')]
    require(numeric == [('hour', '〇二'), ('minute', '三五'), ('second', '〇六')],
            'Mandatory hanidec digits/order/2-digit padding failed')
    require(hanidec['resolvedOptions']['numberingSystem'] == 'hanidec' and
            witness['japaneseHour12']['hourCycle'] in ('h11', 'h12') and
            witness['japaneseHour12']['hour12'] is True, 'Mandatory selected cycle/digit semantics failed')
    require(['U+' + format(ord(c), '04X') for c in hanidec['formatted']] == hanidec['codePoints'],
            'Oracle codepoint/string mismatch')
    review_binding = policy.get('semantic_review_binding')
    require(isinstance(review_binding, dict) and
            sha(review_binding['path']) == review_binding.get('sha256'), 'Missing native semantic review')
    review = scopes.document(review_binding, 'oracle-semantic-review')
    require(review.get('mandatory_semantics_satisfied') is True and
            review.get('effective_profile_sha256') == context['effective_profile_sha256'] and
            review.get('production_source_map_sha256') == context['production_source_map_sha256'],
            'Native selected fallback/hour-cycle/digits semantics unresolved')
    entries = policy.get('adjudicated_diagnostic_entries', [])
    index = {identity(entry): entry for entry in entries}
    require(len(index) == len(entries), 'Duplicate original oracle review')
    failures = []
    for path, parsed in actual['parsed'].items():
        require_collection(parsed)
        counts = parsed['counters']
        require(counts['executed_source_count'] == 1 and counts['excluded'] == counts['skipped'] == 0,
                'Original oracle excluded/skipped')
        failures.extend(record for record in parsed['raw_observations']
                        if record['status'] == 'observed-failure')
    require({identity(record) for record in failures} == set(index), 'Unclassified original oracle failure')
    for entry in entries:
        reviewed_entry(entry, context, fixture_hashes, validated['fixture_summaries'], evidence_bytes)
    save('native-oracle-qualification.json', dict(exact_profile_diagnostic_qualified=True,
         actual_original_outcomes=actual['outcomes'], raw_failures_preserved=True,
         conformance_accepted=False, failure_waiver=False))


def verify_host_default_locale_fixture():
    scope = json.loads((HERE / 'activation-scope.json').read_text())
    require(sha('tests/test_intl_default_locale_api.c') ==
            scope['host_default_locale_fixture_sha256'],
            'Changed current H1 host-default-locale fixture')


def main(phase):
    require(os.environ.get('GITHUB_ACTIONS') == 'true', 'Hosted runner only')
    if phase == 'preserve':
        if (HERE / 'binding.json').is_file():
            binding = json.loads((HERE / 'binding.json').read_text())
            current_sources = {path: sha(path) for path in binding['source_sha256']}
            unchanged = current_sources == binding['source_sha256']
            save('actual-source-after.json', dict(source_map_sha256=canonical(current_sources),
                 source_unchanged=unchanged, files_sha256=current_sources))
            require(unchanged, 'Hosted commands changed committed candidate source bytes')
        variant_root = OUT / 'feature-variants'
        if variant_root.exists():
            shutil.rmtree(variant_root)
        directory = Path('.obj') / ('asan' if SAN == 'ASan' else 'ubsan' if SAN == 'UBSan' else '')
        for path in ('test262_report.txt', 'test262_intl_report.txt', 'test262_errors.txt',
                     str(directory / 'temporal-build-config')):
            if Path(path).is_file():
                shutil.copyfile(path, OUT / Path(path).name)
        generated = directory / 'src/timezone'
        if generated.exists():
            files = {str(path.relative_to(generated)): sha(path) for path in generated.rglob('*')
                     if path.is_file() and path.name in ('timezone-data.c', 'timezone-data.h',
                                                        'timezone-data.json', 'timezone-data.bin',
                                                        'timezone-data.inc')}
            save('timezone-generated-source-identity.json', dict(files_sha256=files,
                 compiled_eligible_inventory_acceptance=False, eligible_fallback_acceptance=False))
            if (generated / 'timezone-data.json').is_file():
                shutil.copyfile(generated / 'timezone-data.json', OUT / 'timezone-data.json')
        if (OUT / 'test262-fixture-source-sha256.json').is_file():
            mapping = {str(path.relative_to('test262')): sha(path) for path in Path('test262').rglob('*')
                       if path.is_file() and '.git' not in path.relative_to('test262').parts}
            save('test262-fixture-after.json', dict(source_map_sha256=canonical(mapping),
                 fixture_unchanged=canonical(mapping) == FIXTURE_MAP))
            require(canonical(mapping) == FIXTURE_MAP, 'Hosted tests changed the reviewed complete fixture')
        save('scope.json', dict(backend=BACKEND, compiler=CC, sanitizer=SAN,
             native_frontends_enabled=BACKEND == 'native', intl_enabled=True, temporal_enabled=True,
             dependency_sanitizer_instrumented=False, zero_error_conformance_claim=False,
             frontend_Test262='Original raw status/counts retained; ICU authenticated current baseline and native exact per-file full-assertion qualification separately required',
             shared_timezone_snapshot_extension_adopted=json.loads((HERE / 'binding.json').read_text())['shared_timezone_snapshot_extension_adopted'],
             timezone_inventory_and_fallback_followon_pending=True, final_MSan_pending=True))
        return
    if phase == 'initialize':
        verify_host_default_locale_fixture()
        actual_sha = os.environ['GITHUB_SHA']
        require(run('checkout-head', ['git', 'rev-parse', 'HEAD']).strip() == actual_sha, 'Wrong checked out SHA')
        run('checkout-tree', ['git', 'ls-tree', '-r', 'HEAD'])
        checkout = inspect_checkout()
        tree, entries = checkout['modes'], checkout['entries']
        if os.environ.get('PREFIX_BINDING_MODE') == 'checkout':
            require('tools/ci/native-prefix/binding.json' not in tree, 'Generated binding must be untracked')
            require(not run('checkout-tracked-diff', ['git', 'diff', 'HEAD', '--']).strip(), 'Changed tracked checkout')
            source_map = committed_sources(entries)
            scope = json.loads((HERE / 'activation-scope.json').read_text())
            binding = dict(stage='automatic-enabled-checkout', official_source_commit=actual_sha,
                           source_map_sha256=canonical(source_map), source_sha256=source_map,
                           git_modes=tree, gitlinks=checkout['gitlinks'], adapter_git_modes={}, adapter_allowlist=[], adapter_sha256={},
                           native_frontends_enabled=True, source_only=False, runtime_acceptance=False,
                           hosted_acceptance=False, binding_mode='actual-committed-checkout',
                           shared_timezone_snapshot_extension_adopted=source_map['tests/test_intl_native_timezone_runtime.c'] == scope['shared_timezone_snapshot_source_sha256'])
            (HERE / 'binding.json').write_text(json.dumps(binding, indent=2) + '\n')
        else:
            binding = json.loads((HERE / 'binding.json').read_text())
            parents = run('checkout-parent', ['git', 'rev-list', '--parents', '-n', '1', 'HEAD']).split()
            require(parents == [actual_sha, binding['official_source_commit']], 'Wrong sole official source parent')
            require(binding['stage'] in ('37-enabled-sanitizer-hosted-ci', '38-shared-timezone-snapshot-unit') and
                    binding['native_frontends_enabled'] is True, 'Reviewed suffix binding required')
        require(canonical(binding['source_sha256']) == binding['source_map_sha256'], 'Wrong source map digest')
        expected_modes = dict(binding['git_modes'], **binding['adapter_git_modes'])
        require(tree == expected_modes, 'Checkout Git path set or exact modes differ from binding')
        for path, expected in binding['source_sha256'].items():
            require(Path(path).is_file() and not Path(path).is_symlink() and sha(path) == expected, 'Changed source: ' + path)
        # inspect_checkout enforces stat bits per repository where supported;
        # exact committed Git modes remain mandatory on every platform.
        binding['mode_capabilities'] = checkout['mode_capabilities']
        (HERE / 'binding.json').write_text(json.dumps(binding, indent=2) + '\n')
        save('checkout-mode-capabilities.json', checkout['mode_capabilities'])
        # Parent tree equality independently checks additions, deletions, modes and blobs.
        diff = run('official-parent-diff', ['git', 'diff', '--name-only', binding['official_source_commit'], 'HEAD']).splitlines()
        require(set(diff) == set(binding['adapter_allowlist']), 'Unexpected derivative path set')
        save('actual-source-sha256.json', binding['source_sha256'])
        save('actual-git-source-modes.json', binding['git_modes'])
        require(all(sha(path) == expected for path, expected in binding['adapter_sha256'].items()), 'Changed adapter bytes')
        shutil.copyfile(HERE / 'binding.json', OUT / 'binding.json')
        # The current production binding covers EVERY actual committed checkout
        # source, including additions, deletions and adapters. The old 881-file
        # map stays an explicit historical baseline, never a current source pin.
        current_production = (source_map if os.environ.get('PREFIX_BINDING_MODE') == 'checkout'
                              else committed_sources(entries))
        require(current_production == dict(binding['source_sha256'], **binding['adapter_sha256']),
                'Actual committed checkout differs from initialized source/adapter binding')
        save('actual-production-source-sha256.json', current_production)
        save('execution-identity.json', dict(candidate_sha=os.environ['GITHUB_SHA'],
             run_id=os.environ['GITHUB_RUN_ID'], run_attempt=os.environ['GITHUB_RUN_ATTEMPT'],
             source_map_sha256=binding['source_map_sha256'], binding_sha256=sha(HERE / 'binding.json'),
             gitlinks=checkout['gitlinks'], mode_capabilities=checkout['mode_capabilities'],
             actual_production_source_map_sha256=canonical(current_production),
             historical_baseline_production_source_map_sha256=HISTORICAL_BASELINE_PRODUCTION_MAP))
        cxx = 'g++' if CC == 'gcc' else 'clang++'
        compiler_identity = {name: dict(resolved_path=str(Path(shutil.which(name)).resolve()),
                                       executable_sha256=sha(shutil.which(name)))
                             for name in (CC, cxx)}
        save('compiler-identity.json', compiler_identity)
        compiler_key = canonical({name: value['executable_sha256']
                                  for name, value in compiler_identity.items()})
        with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
            stream.write('compiler_key=' + compiler_key + '\n')
        for label, argv in [('system', ['uname', '-a']), ('compiler', [CC, '--version']),
                            ('cxx', [cxx, '--version']),
                            ('make', ['make', '--version']), ('python', ['python3', '--version']),
                            ('software', ['dpkg-query', '-W'])]:
            run('environment-' + label, argv)
        shutil.copyfile('/etc/os-release', OUT / 'os-release.txt')
        return
    if phase == 'setup-icu':
        pin = json.loads((HERE / 'icu-overlay.json').read_text())
        ICU.mkdir(parents=True, exist_ok=True)
        archive = ICU / 'release.tgz'
        run('icu-download', ['curl', '--fail', '--location', '--retry', '3', '--max-time', '300', pin['archive_url'], '-o', str(archive)])
        require(sha(archive) == pin['archive_sha256'], 'ICU archive digest mismatch')
        run('icu-extract', ['tar', '-xzf', str(archive), '-C', str(ICU)])
        source = ICU / 'icu'
        for path, hashes in pin['files'].items():
            target = source / path
            require((sha(target) if target.exists() else None) == hashes['preimage_sha256'], 'Changed ICU preimage: ' + path)
            overlay = HERE / 'icu-overlay' / path
            require(sha(overlay) == hashes['postimage_sha256'], 'Changed ICU overlay: ' + path)
            shutil.copyfile(overlay, target)
        build = ICU / 'build'
        build.mkdir(exist_ok=True)
        env = dict(ENV, CC=CC, CXX='g++' if CC == 'gcc' else 'clang++', CFLAGS='-O2 -fPIC', CXXFLAGS='-O2 -fPIC')
        run('icu-configure', [str(source / 'source/runConfigureICU'), 'Linux', '--prefix=' + str(ICU / 'install'),
            '--enable-static', '--disable-shared', '--with-data-packaging=static', '--disable-tests', '--disable-samples'], cwd=build, env=env)
        run('icu-build', ['make', '-j' + str(available_cpus())], cwd=build, env=env)
        run('icu-install', ['make', 'install'], cwd=build, env=env)
        installed = {str(p.relative_to(ICU / 'install')): sha(p) for p in (ICU / 'install').rglob('*') if p.is_file()}
        compiler = json.loads((OUT / 'compiler-identity.json').read_text())
        (ICU / 'install/CI-IDENTITY.json').write_text(json.dumps(dict(
            overlay_sha256=sha(HERE / 'icu-overlay.json'), archive_sha256=pin['archive_sha256'],
            accepted_modified_ICU=True, private_overlay_file_count=len(pin['files']),
            stock_ICU_equivalence_claim=False, compiler_identity=compiler,
            dependency_sanitizer_instrumented=False, installed_files=installed), indent=2) + '\n')
        return
    if phase == 'inspect-icu':
        identity = json.loads((ICU / 'install/CI-IDENTITY.json').read_text())
        require(identity['overlay_sha256'] == sha(HERE / 'icu-overlay.json'), 'Wrong cached ICU overlay')
        pin = json.loads((HERE / 'icu-overlay.json').read_text())
        require(identity['archive_sha256'] == pin['archive_sha256'] and
                identity['private_overlay_file_count'] == len(pin['files']) == 10 and
                identity['accepted_modified_ICU'] is True and identity['stock_ICU_equivalence_claim'] is False,
                'Wrong cached modified ICU provenance')
        require(identity['compiler_identity'] == json.loads((OUT / 'compiler-identity.json').read_text()),
                'Wrong cached ICU compiler identity')
        installed = {str(p.relative_to(ICU / 'install')): sha(p)
                     for p in (ICU / 'install').rglob('*')
                     if p.is_file() and p.relative_to(ICU / 'install') != Path('CI-IDENTITY.json')}
        require(installed == identity['installed_files'], 'Changed cached ICU install file set or bytes')
        run('icu-version', ['pkg-config', '--exact-version=78.3', 'icu-i18n', 'icu-uc'])
        require(run('icu-prefix', ['pkg-config', '--variable=prefix', 'icu-i18n']).strip() == str(ICU / 'install'), 'Wrong pkg-config provider')
        shutil.copyfile(ICU / 'install/CI-IDENTITY.json', OUT / 'icu-install-identity.json')
        return
    if phase == 'timezone-inputs':
        if not Path('tools/timezone/pin.json').exists():
            save('timezone-inputs.json', dict(applicable=False))
            return
        pin = json.loads(Path('tools/timezone/pin.json').read_text())
        source = OUT / 'tz-source'
        run('tz-clone', ['git', 'clone', '--depth', '1', '--branch', pin['tag'], pin['url'], str(source)])
        require(run('tz-revision', ['git', '-C', str(source), 'rev-parse', 'HEAD']).strip() == pin['commit'], 'Wrong tz release commit')
        target = Path('third_party/tz')
        target.mkdir(parents=True, exist_ok=True)
        for name, expected in pin['files'].items():
            require(sha(source / name) == expected, 'Changed tz release source: ' + name)
            shutil.copyfile(source / name, target / name)
        shutil.copyfile(source / 'LICENSE', target / 'LICENSE')
        shutil.rmtree(source)
        save('timezone-inputs.json', pin)
        return
    if phase == 'native-units':
        native_units()
        return
    args = make_args()
    if phase == 'build':
        row = json.loads(
            (HERE / 'activation-scope.json').read_text())[BACKEND]
        targets = row['C_targets'] + [
            unit['target'] for unit in special_c_units(row)]
        run(phase, args + ['-j' + str(available_cpus()), 'all'] + targets)
        directory = Path('.obj') / ('asan' if SAN == 'ASan' else 'ubsan' if SAN == 'UBSan' else '')
        stamp_path = directory / 'temporal-build-config'
        stamp = stamp_path.read_text().splitlines()
        require(stamp[:6] == ['y', 'y' if BACKEND == 'icu' else 'n', 'y', 'y', BACKEND,
                            'n' if BACKEND == 'icu' else 'y'], 'Wrong enabled build stamp')
        save('candidate-build-policy.json', dict(make_args=args, intl_build_config=stamp,
             intl_build_config_sha256=sha(stamp_path), available_worker_count=available_cpus(),
             unicode_backend='icu' if BACKEND == 'icu' else 'native',
             unicode_versions='See actual tests/test_unicode_version invocation and provider logs',
             dependency_sanitizer_instrumented=False))
    elif phase == 'h2-lunisolar-guard-tests':
        runtime = HERE / 'original-assertions/witness-runtime.js'
        mock = HERE / 'original-assertions/test_h2_lunisolar_guard_runtime.js'
        payload = OUT / 'h2-lunisolar-guard-runtime.js'
        require(not payload.exists(), 'Guard mock payload must be fresh')
        payload.write_bytes(runtime.read_bytes() + b'\n' + mock.read_bytes())
        run(phase, [str(Path('qjs').resolve()), str(payload.resolve())],
            timeout=900, raw_streams=True)
        require((OUT / (phase + '.stdout.log')).read_text().splitlines().count(
            'H2 lunisolar guard: 31 positive and 25 rejection scenarios.') == 1
            and not (OUT / (phase + '.stderr.log')).read_bytes(),
            'Guard mock must execute all positive and rejection scenarios')
        save('h2-lunisolar-guard-runtime-binding.json', dict(
            runtime_sha256=sha(runtime), mock_sha256=sha(mock),
            payload_sha256=sha(payload), engine_sha256=sha('qjs'),
            command_sha256=sha(OUT / (phase + '.command.json')),
            conformance_accepted=False, semantic_adjudication_complete=False))
    elif phase == 'repository-tests':
        repository_tests(args)
    elif phase == 'public-api':
        public_api(args)
    elif phase == 'api-variants':
        api_variants(args)
    elif phase == 'native-date-exhaustive':
        require(BACKEND == 'native' and CC == 'gcc' and SAN == 'normal',
                'Exhaustive Date matrix belongs to one normal native row')
        run(phase, args + ['test-intl-native-date-exhaustive'])
    elif phase == 'microbench':
        run(phase, args + ['microbench'])
    elif phase == 'bootstrap-test262':
        require(all(sha(path) == expected for path, expected in PATCHES.items()), 'Reviewed Test262 overlays changed')
        version_binary = Path('tests/test_unicode_version')
        version_sha = sha(version_binary)
        version_text = run('test262-provider-version', ['./tests/test_unicode_version'], timeout=900)
        match = re.search(r'^Unicode backend=(native|icu) data=(\d+)\.(\d+)\.(\d+)\.(\d+) ', version_text, re.M)
        require(match is not None and match[1] == BACKEND and
                [int(value) for value in match.groups()[1:]] == TEST262_PROFILE['unicode_data_version'] and
                sha(version_binary) == version_sha,
                'Actual Unicode provider differs from reviewed Test262 data profile')
        save('test262-data-profile.json', dict(TEST262_PROFILE, version_binary_sha256=version_sha))
        run(phase, args + ['TEST262_COMMIT=' + REVISION,
            'TEST262_UNICODE18_OVERLAY=' + ('y' if BACKEND == 'native' else 'n'), 'test2-bootstrap'])
        require(run('test262-head', ['git', '-C', 'test262', 'rev-parse', 'HEAD']).strip() == REVISION, 'Wrong fixture revision')
        mapping = {str(p.relative_to('test262')): sha(p) for p in Path('test262').rglob('*')
                   if p.is_file() and '.git' not in p.relative_to('test262').parts}
        require(canonical(mapping) == FIXTURE_MAP, 'Latest fixture differs from reviewed overlay tree')
        save('test262-fixture.json', dict(revision=REVISION, source_map_sha256=canonical(mapping), files=len(mapping), patches=PATCHES))
        save('test262-fixture-source-sha256.json', mapping)
        run('test262-overlays', ['git', '-C', 'test262', 'diff', '--binary'])
    elif phase == 'test262':
        full_test262()
    elif phase == 'required-test262':
        required_test262()
    elif phase == 'oracle-diagnostics':
        oracle_diagnostics()
    elif phase == 'native-assertion-witness':
        native_assertion_witness()
    elif phase == 'test262-profile-gate':
        exact_profile_gate()
    else:
        raise RuntimeError('Unknown phase: ' + phase)


if __name__ == '__main__':
    phase = sys.argv[1]
    succeeded = False
    try:
        main(phase)
        succeeded = True
    finally:
        save('phase-' + phase + '.json', dict(phase=phase, success=succeeded,
             backend=BACKEND, compiler=CC, sanitizer=SAN,
             qjs_sha256=sha('qjs') if Path('qjs').is_file() else None,
             conformance_accepted=False, failure_waiver=False))
