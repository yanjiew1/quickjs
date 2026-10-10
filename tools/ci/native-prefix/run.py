#!/usr/bin/env python3
"""Scratch CI adapter. Native frontend conformance remains deferred until 25."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys

HERE = Path(__file__).resolve().parent
OUT = Path('ci-native-prefix-diagnostics').resolve()
OUT.mkdir(exist_ok=True)
BACKEND = os.environ['PREFIX_BACKEND']
CC = os.environ['PREFIX_COMPILER']
SAN = os.environ['PREFIX_SANITIZER']
ICU = Path(os.environ['PREFIX_ICU'])
REVISION = 'c8c798898646638cd0c24879f8e0374e847e7d74'
PATCHES = {'tests/test262.patch': '64d8f5b124afae386c0b2b154a147f2f8daf087dd4b5f602d1948e85c7f9cbba',
           'tests/test262-unicode18.patch': '0ca6149a256e331f3fd4a5ca4fa31536dab1de577bc759bcd523ceaac8545698'}
FIXTURE_MAP = '808f46746cde3b35db59c83dc7ecb9b91df7970bcae65572ffd4ee126547896d'
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


def run(name, argv, cwd=None, env=None, timeout=5400):
    """Record actual argv/status/log; terminate this command's group on timeout."""
    record = dict(argv=list(map(str, argv)), cwd=str(cwd or Path.cwd()), returncode=None)
    log = OUT / (name + '.log')
    process = None
    try:
        with log.open('wb') as stream:
            process = subprocess.Popen(argv, cwd=cwd, env=env or ENV,
                                       stdout=stream, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            record['returncode'] = process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        record['timed_out'] = True
        raise
    finally:
        if process is not None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
        record['log_sha256'] = sha(log)
        save(name + '.command.json', record)
    require(record['returncode'] == 0, 'Failed command: ' + name)
    return log.read_text(errors='replace')


def make_args():
    require(BACKEND in ('native', 'icu') and CC in ('gcc', 'clang') and
            SAN in ('normal', 'ASan', 'UBSan'), 'Unsupported matrix row')
    args = ['make', 'CONFIG_WERROR=y', 'CONFIG_LTO=', 'CC=' + CC, 'HOST_CC=' + CC,
            'CONFIG_CLANG=' + ('y' if CC == 'clang' else ''),
            'CONFIG_ASAN=' + ('y' if SAN == 'ASan' else ''),
            'CONFIG_UBSAN=' + ('y' if SAN == 'UBSan' else ''),
            'CONFIG_INTL_BACKEND=' + BACKEND,
            'CONFIG_INTL=' + ('y' if BACKEND == 'icu' else 'n'),
            'CONFIG_TEMPORAL=' + ('y' if BACKEND == 'icu' else 'n'),
            'CONFIG_INTL_LEGACY=y']
    if BACKEND == 'icu':
        flags = run('icu-cflags', ['pkg-config', '--cflags', 'icu-i18n', 'icu-uc']).strip()
        libs = run('icu-libs', ['pkg-config', '--static', '--libs', 'icu-i18n', 'icu-uc']).strip() + ' -lstdc++'
        args += ['ICU_STATIC=y', 'ICU_CFLAGS=' + flags, 'ICU_LIBS=' + libs,
                 'HOST_ICU_CFLAGS=' + flags, 'HOST_ICU_LIBS=' + libs]
    return args


def native_units():
    binding = json.loads((HERE / 'binding.json').read_text())
    inventory = json.loads((HERE / 'library-inventory.json').read_text())[binding['stage']]
    require(inventory['source_map'] == binding['source_map_sha256'], 'Wrong stage inventory')
    directory = Path('.obj') / ('asan' if SAN == 'ASan' else 'ubsan' if SAN == 'UBSan' else '')
    unitdir = directory / 'hosted-native-prefix'
    unitdir.mkdir(parents=True, exist_ok=True)
    flags = ['-std=c99', '-O2', '-g', '-Wall', '-Wextra', '-Werror', '-Wpedantic',
             '-D_GNU_SOURCE', '-Iinclude', '-Isrc', '-Isrc/intl', '-Isrc/intl/data',
             '-Isrc/cutils', '-Isrc/dtoa', '-Isrc/unicode', '-Isrc/regexp']
    if SAN != 'normal':
        flags += ['-fsanitize=' + ('address' if SAN == 'ASan' else 'undefined'), '-fno-omit-frame-pointer']
    calendar = ['-DQJS_CAL_USE_PERSIAN_AUTHORITY_TABLE', '-DQJS_CAL_ENABLE_LUNISOLAR_CANDIDATE',
                '-DQJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA', '-DQJS_CAL_PERSIAN_AUTHORITY_VERIFIED',
                '-DQJS_CAL_CHINESE_AUTHORITY_VERIFIED', '-DQJS_CAL_DANGI_AUTHORITY_VERIFIED']
    if Path('tools/timezone/pin.json').exists():
        run('timezone-library-build', make_args() + ['-j2', 'libqjstimezone.a'])
    objects = []
    for index, source in enumerate(inventory['sources']):
        extra = calendar if source.startswith('src/calendar/') else []
        if source.startswith(('src/cutils/', 'src/dtoa/', 'src/unicode/')):
            extra = extra + inventory['support_flags'][CC]
        if source.startswith('src/timezone/'):
            extra = extra + ['-I' + str(directory / 'src/timezone')]
        obj = unitdir / (source.replace('/', '_') + '.o')
        run('native-object-' + str(index), [CC] + flags + extra + ['-c', source, '-o', str(obj)])
        objects.append(str(obj))
    archive = unitdir / 'libnative.a'
    run('native-archive', ['ar', 'rcs', str(archive)] + objects)
    symbols = run('native-undefined-symbols', ['nm', '-u', 'qjs', 'qjsc', 'libquickjs.a', str(archive)])
    require(not re.search(r'\b(?:u_|uloc_|unum_|ucal_|udat_|ubrk_|ucol_|ures_)\w+|_ZN\d+icu_\d+', symbols), 'Native prefix gained ICU symbols')
    definitions = run('native-data-owner', ['nm', '-g', '--defined-only', str(archive)])
    for symbol in ('qjs_intl_locale_metadata_blob', 'qjs_intl_locale_metadata_blob_size'):
        require(len(re.findall(r'[ \t]' + symbol + r'$', definitions, re.M)) == 1, 'Invalid data owner: ' + symbol)
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
        extra = calendar if unit['name'].startswith('test_intl_calendar_') else []
        run('native-unit-compile-' + str(index), [CC] + flags + extra +
            [test, str(archive), '-lm', '-ldl', '-lpthread', '-o', str(target)])
        run('native-unit-run-' + str(index), ['./' + str(target)] +
            ([str(binary)] if unit['binary_argument'] else []))
    run('native-selector-rejection', ['python3', 'tests/test_intl_native_selector.py'])
    if Path('tools/timezone/pin.json').exists():
        targets = ['tests/test_tzif_decoder', 'tests/test_timezone_system_reader', 'tests/test_timezone']
        run('timezone-units-build', make_args() + targets)
        for index, target in enumerate(targets):
            run('timezone-unit-' + str(index), ['./' + target])
    save('native-unit-scope.json', dict(stage=binding['stage'], units=inventory['units'],
         native_frontends_enabled=False, native_frontend_conformance='deferred until activation25'))


def main(phase):
    require(os.environ.get('GITHUB_ACTIONS') == 'true', 'Hosted runner only')
    if phase == 'preserve':
        for path in ('test262_report.txt', 'test262_intl_report.txt', 'test262_errors.txt', '.obj/intl-build-config'):
            if Path(path).is_file():
                shutil.copyfile(path, OUT / Path(path).name)
        save('scope.json', dict(backend=BACKEND, compiler=CC, sanitizer=SAN,
             native_frontends_enabled=False, dependency_sanitizer_instrumented=False,
             native_frontend_Test262='deferred until activation25',
             ICU_frontend_Test262='executed only if corresponding command receipt succeeds'))
        return
    if phase == 'initialize':
        binding = json.loads((HERE / 'binding.json').read_text())
        require(run('checkout-head', ['git', 'rev-parse', 'HEAD']).strip() == os.environ['GITHUB_SHA'], 'Wrong checked out SHA')
        parents = run('checkout-parent', ['git', 'rev-list', '--parents', '-n', '1', 'HEAD']).split()
        require(parents == [os.environ['GITHUB_SHA'], binding['official_source_commit']], 'Wrong sole official source parent')
        require(canonical(binding['source_sha256']) == binding['source_map_sha256'], 'Wrong source map digest')
        for path, expected in binding['source_sha256'].items():
            require(Path(path).is_file() and not Path(path).is_symlink() and sha(path) == expected, 'Changed source: ' + path)
        for path, mode in binding['git_modes'].items():
            require(bool(Path(path).stat().st_mode & 0o111) == (mode == '100755'), 'Changed source mode: ' + path)
        # Parent tree equality independently checks additions, deletions, modes and blobs.
        diff = run('official-parent-diff', ['git', 'diff', '--name-only', binding['official_source_commit'], 'HEAD']).splitlines()
        require(set(diff) == set(binding['adapter_allowlist']), 'Unexpected derivative path set')
        require(all(sha(path) == expected for path, expected in binding['adapter_sha256'].items()), 'Changed adapter bytes')
        shutil.copyfile(HERE / 'binding.json', OUT / 'binding.json')
        save('execution-identity.json', dict(candidate_sha=os.environ['GITHUB_SHA'],
             run_id=os.environ['GITHUB_RUN_ID'], run_attempt=os.environ['GITHUB_RUN_ATTEMPT'],
             source_map_sha256=binding['source_map_sha256'], binding_sha256=sha(HERE / 'binding.json')))
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
        run('icu-build', ['make', '-j2'], cwd=build, env=env)
        run('icu-install', ['make', 'install'], cwd=build, env=env)
        installed = {str(p.relative_to(ICU / 'install')): sha(p) for p in (ICU / 'install').rglob('*') if p.is_file()}
        (ICU / 'install/CI-IDENTITY.json').write_text(json.dumps(dict(overlay_sha256=sha(HERE / 'icu-overlay.json'), installed_files=installed), indent=2) + '\n')
        return
    if phase == 'inspect-icu':
        identity = json.loads((ICU / 'install/CI-IDENTITY.json').read_text())
        require(identity['overlay_sha256'] == sha(HERE / 'icu-overlay.json'), 'Wrong cached ICU overlay')
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
    if phase in ('build', 'repository-tests', 'microbench'):
        run(phase, args + (['-j2'] if phase == 'build' else ['test' if phase == 'repository-tests' else 'microbench']))
    elif phase == 'bootstrap-test262':
        require(all(sha(path) == expected for path, expected in PATCHES.items()), 'Reviewed Test262 overlays changed')
        run(phase, args + ['TEST262_COMMIT=' + REVISION, 'test2-bootstrap'])
        require(run('test262-head', ['git', '-C', 'test262', 'rev-parse', 'HEAD']).strip() == REVISION, 'Wrong fixture revision')
        mapping = {str(p.relative_to('test262')): sha(p) for p in Path('test262').rglob('*')
                   if p.is_file() and '.git' not in p.relative_to('test262').parts}
        require(canonical(mapping) == FIXTURE_MAP, 'Latest fixture differs from reviewed overlay tree')
        save('test262-fixture.json', dict(revision=REVISION, source_map_sha256=canonical(mapping), files=len(mapping), patches=PATCHES))
        run('test262-overlays', ['git', '-C', 'test262', 'diff', '--binary'])
    elif phase == 'test262':
        config = 'test262-intl.conf' if BACKEND == 'icu' else 'test262.conf'
        for path in (config, 'test262_errors.txt'):
            shutil.copyfile(path, OUT / ('input-' + path))
        run(phase, ['./run-test262', '-T', '1', '-t', '-m', '-c', config, '-a'])
    else:
        raise RuntimeError('Unknown phase: ' + phase)


if __name__ == '__main__':
    main(sys.argv[1])
