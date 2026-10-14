#!/usr/bin/env python3
"""Scratch hosted orchestration. No official Test262 or acceptance decision.

Command limits: 30 minutes normally, two hours for packaged ICU --check.
Existing run.py owns command journals and process group closure.
"""
import importlib.util
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import sys

HERE = Path(__file__).resolve().parent
# The reviewed runner now imports its two pure gate/parser helpers.
sys.path.insert(0, str(HERE.parent / 'native-prefix'))
from reconstruction_scope import required_paths
spec = importlib.util.spec_from_file_location('reviewed_prefix', HERE.parent / 'native-prefix/run.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
ROW = os.environ['QUALIFICATION_ROW']
r.require(ROW in ('packaged-static', 'packaged-shared', 'system', 'm32', 'macos', 'mingw', 'msys2'), 'Unknown row')
r.require(os.environ.get('GITHUB_ACTIONS') == 'true', 'Hosted only')
r.require(r.BACKEND == ('icu' if ROW in ('packaged-static', 'packaged-shared', 'system') else 'native') and
          r.CC == ('clang' if ROW == 'macos' else 'gcc') and r.SAN == 'normal', 'Wrong backend/compiler/profile')
r.require(os.environ['GITHUB_REF'].startswith('refs/heads/scratch/native-prefix/qualification/'), 'Scratch branch only')
# Bind the exact committed current checkout without an obsolete derivative parent.
BINDING = None

# Do not inherit the pinned static ICU environment from run.py.
r.ENV = dict(os.environ, MAKEFLAGS='', LC_ALL='C.UTF-8', TZ='UTC',
             PKG_CONFIG_PATH='', PKG_CONFIG_LIBDIR='', PKG_CONFIG_SYSROOT_DIR='')
if ROW == 'system':
    for key in ('PKG_CONFIG_PATH', 'PKG_CONFIG_LIBDIR', 'PKG_CONFIG_SYSROOT_DIR'):
        r.ENV.pop(key, None)
MAKE = "gmake" if ROW == "macos" else "make"
COMMAND_LIMIT = 1800
original_run = r.run

def command(name, argv, **kw):
    kw.setdefault('timeout', COMMAND_LIMIT)
    return original_run(name, argv, **kw)

r.run = command

def current_checkout_source_map(committed, historical, required, derivation):
    """Separate the preserved old baseline from exact current required sources."""
    r.require(r.canonical(historical) ==
              'c71443f857120b4ece57b2d4c2ce635e9b54bacf97f33f5eb4b6177c839a3b70',
              'Changed historical baseline production source manifest')
    expected_paths = required_paths(historical, derivation)
    r.require(set(required) == expected_paths and
              all(committed.get(path) == digest for path, digest in required.items()),
              'Changed exact current required production or identity source')
    # committed_sources authenticates every current Git blob, checkout byte and
    # path. Additional current tracked files remain in this complete identity.
    return r.canonical(committed)


def initialize():
    global BINDING
    actual = os.environ['GITHUB_SHA']
    r.verify_host_default_locale_fixture()
    r.require(command('checkout-head', ['git', 'rev-parse', 'HEAD']).strip() == actual, 'Wrong SHA')
    r.require(not command('checkout-diff', ['git', 'diff', 'HEAD', '--']).strip(), 'Changed checkout')
    command('checkout-tree', ['git', 'ls-tree', '-r', 'HEAD'])
    checkout = r.inspect_checkout()
    entries = checkout['entries']
    r.require('tools/ci/native-prefix/binding.json' not in entries and
              'tools/ci/stop39b-qualification/binding.json' not in entries,
              'Generated runtime binding must be untracked')
    committed = r.committed_sources(entries)
    historical = json.loads((r.HERE / 'production-source-sha256.json').read_text())
    required = json.loads((HERE / 'current-required-source-sha256.json').read_text())
    current_source_map = current_checkout_source_map(committed, historical, required,
        json.loads((r.HERE / 'source-path-derivation.json').read_bytes()))
    scope = json.loads((r.HERE / 'activation-scope.json').read_text())
    BINDING = dict(stage='automatic-enabled-checkout', official_source_commit=actual,
         source_map_sha256=r.canonical(committed), source_sha256=committed,
         git_modes=checkout['modes'], gitlinks=checkout['gitlinks'],
         mode_capabilities=checkout['mode_capabilities'], adapter_git_modes={},
         adapter_allowlist=[], adapter_sha256={}, native_frontends_enabled=True,
         source_only=False, runtime_acceptance=False, hosted_acceptance=False,
         publication_authorized=False, binding_mode='actual-committed-checkout',
         production_source_map_sha256=current_source_map,
         historical_baseline_production_source_map_sha256=r.canonical(historical),
         shared_timezone_snapshot_extension_adopted=
             committed['tests/test_intl_native_timezone_runtime.c'] == scope['shared_timezone_snapshot_source_sha256'])
    (r.HERE / 'binding.json').write_text(json.dumps(BINDING, indent=2) + '\n')
    r.save('binding.json', BINDING)
    r.save('checkout-mode-capabilities.json', checkout['mode_capabilities'])
    r.save('actual-source-sha256.json', committed)
    r.save('actual-git-source-modes.json', BINDING['git_modes'])
    r.save('execution-identity.json', dict(candidate_sha=actual,
           production_source_map_sha256=current_source_map,
           historical_baseline_production_source_map_sha256=r.canonical(historical),
           source_map_sha256=BINDING['source_map_sha256'], gitlinks=checkout['gitlinks'],
           mode_capabilities=checkout['mode_capabilities'], run_id=os.environ['GITHUB_RUN_ID'],
           run_attempt=os.environ['GITHUB_RUN_ATTEMPT'], row=ROW,
           command_timeout_seconds=COMMAND_LIMIT, sdk_timeout_seconds=7200))
    command('environment-uname', ['uname', '-a'])
    command('environment-compiler', [r.CC, '--version'])
    compilers = [r.CC, 'clang++' if r.CC == 'clang' else 'g++']
    if ROW == 'mingw':
        compilers += ['x86_64-w64-mingw32-gcc', 'x86_64-w64-mingw32-g++']
    r.save('compiler-identity.json', {c: dict(path=shutil.which(c),
           resolved_path=str(Path(shutil.which(c)).resolve()), executable_sha256=r.sha(shutil.which(c))) for c in compilers})
    command('environment-make', [MAKE, '--version'])
    if Path('/etc/os-release').is_file():
        shutil.copyfile('/etc/os-release', r.OUT / 'os-release.txt')
    if ROW in ('packaged-static', 'packaged-shared', 'system', 'm32', 'mingw'):
        command('environment-packages', ['dpkg-query', '-W'])
    else:
        command('environment-platform', ['sw_vers'] if ROW == 'macos' else ['pacman', '-Q'])

def make_args():
    args = [MAKE, 'CONFIG_WERROR=y', 'CONFIG_LTO=', 'CONFIG_ASAN=', 'CONFIG_UBSAN=',
            'CONFIG_INTL=y', 'CONFIG_TEMPORAL=y', 'CONFIG_INTL_LEGACY=y',
            'CONFIG_INTL_BACKEND=' + r.BACKEND, 'CONFIG_ICU=' + ('y' if r.BACKEND == 'icu' else 'n')]
    if ROW == 'macos':
        args += ['CC=clang', 'HOST_CC=clang', 'CONFIG_CLANG=y']
    elif ROW == 'mingw':
        args += ['CONFIG_WIN32=y', 'WINE=/usr/lib/wine/wine64']
    else:
        args += ['CC=gcc', 'HOST_CC=gcc', 'CONFIG_CLANG=']
    if ROW == 'm32':
        args += ['CONFIG_M32=y']
    if ROW == 'system':
        args += ['PKG_CONFIG=/usr/bin/pkg-config', 'HOST_PKG_CONFIG=/usr/bin/pkg-config', 'ICU_STATIC=']
    elif ROW.startswith('packaged-'):
        prefix = Path(os.environ['QUALIFICATION_SDK']) / 'install'
        libraries = ([str(prefix / 'lib' / p) for p in ('libicui18n.a', 'libicuuc.a', 'libicudata.a')]
                     if ROW == 'packaged-static' else ['-L' + str(prefix / 'lib'), '-licui18n', '-licuuc', '-licudata'])
        libs = ' '.join(libraries + ['-lstdc++', '-lm', '-ldl', '-lpthread'])
        flags = '-I' + str(prefix / 'include')
        args += ['ICU_STATIC=' + ('y' if ROW == 'packaged-static' else ''),
                 'ICU_CFLAGS=' + flags, 'HOST_ICU_CFLAGS=' + flags,
                 'ICU_LIBS=' + libs, 'HOST_ICU_LIBS=' + libs]
        r.ENV['LD_LIBRARY_PATH'] = str(prefix / 'lib')
    return args

def sdk():
    root = Path(os.environ['QUALIFICATION_SDK'])
    archive = Path(os.environ['RUNNER_TEMP']) / 'icu4c-78.3-sources.tgz'
    provenance = json.loads(Path('tools/icu-reference/PROVENANCE.json').read_text())
    command('sdk-download', ['curl', '--fail', '--location', '--retry', '3', '--output', str(archive), provenance['archive_url']])
    r.require(r.sha(archive) == provenance['archive_sha256'], 'Wrong archive digest')
    runtime = str(Path(command('compiler-libstdcxx', ['/usr/bin/g++', '-print-file-name=libstdc++.so']).strip()).resolve().parent)
    # Exact image version plus recorded tool/libc manifests, no invented image digest.
    image = 'github-actions:' + os.environ['ImageOS'] + ':' + os.environ['ImageVersion']
    command('packaged-sdk-build', ['python3', 'tools/build-reference-icu.py', '--archive', str(archive),
            '--output', str(root), '--cc', '/usr/bin/gcc', '--cxx', '/usr/bin/g++',
            '--profile', ROW.split('-', 1)[1], '--runner-identity', image,
            '--runtime-library-path', runtime, '--check'], timeout=7200)
    shutil.copyfile(root / 'sdk-manifest.json', r.OUT / 'sdk-manifest.json')
    stock = Path(os.environ['RUNNER_TEMP']) / 'stock-icu-headers'
    stock.mkdir()
    command('stock-header-extract', ['tar', '-xzf', str(archive), '-C', str(stock), 'icu/source/common/unicode', 'icu/source/i18n/unicode'])
    args = make_args()
    libs = next(a.split('=', 1)[1] for a in args if a.startswith('ICU_LIBS='))
    executable = r.OUT / 'stock-header-client'
    command('stock-header-client-build', ['/usr/bin/gcc', '-std=c11', '-I' + str(stock / 'icu/source/common'),
            '-I' + str(stock / 'icu/source/i18n'), 'tools/icu-reference/capability.c',
            *shlex.split(libs), '-o', str(executable)])
    command('stock-header-client', [str(executable)])
    for library in ('icui18n', 'icuuc', 'icudata'):
        path = root / 'install/lib' / ('lib' + library + ('.a' if ROW == 'packaged-static' else '.so'))
        command('sdk-symbols-' + library, ['nm', '-g', '--defined-only', *([] if ROW == 'packaged-static' else ['-D']), str(path)])

def icu_identity(args):
    if ROW == 'system':
        flags = command('system-cflags', ['/usr/bin/pkg-config', '--cflags', 'icu-i18n', 'icu-uc']).strip()
        libs = command('system-libs', ['/usr/bin/pkg-config', '--libs', 'icu-i18n', 'icu-uc']).strip() + ' -lstdc++'
    else:
        flags = next(a.split('=', 1)[1] for a in args if a.startswith('ICU_CFLAGS='))
        libs = next(a.split('=', 1)[1] for a in args if a.startswith('ICU_LIBS='))
    for fixture in ('icu_versions_probe', 'test_icu_version_contract'):
        executable = r.OUT / fixture
        command(fixture + '-build', ['/usr/bin/gcc', '-std=c11', '-DCONFIG_ICU', *shlex.split(flags),
                'tests/' + fixture + '.c', *shlex.split(libs), '-o', str(executable)])
        command(fixture, [str(executable)])
    linked = command('frontend-linkage', ['ldd', './qjs'])
    r.require('not found' not in linked, 'Unresolved frontend dependency')
    identities = {}
    for soname, path in re.findall(r'(libicu\S+)\s+=>\s+(/\S+)', linked):
        resolved = Path(path).resolve(strict=True)
        identities[soname] = dict(path=path, resolved_path=str(resolved), sha256=r.sha(resolved))
        if ROW == 'packaged-shared':
            r.require(resolved.is_relative_to((Path(os.environ['QUALIFICATION_SDK']) / 'install/lib').resolve()),
                      'Frontend linked outside selected packaged SDK')
        elif ROW == 'system':
            r.require(str(resolved).startswith('/usr/lib/') or str(resolved).startswith('/lib/'),
                      'Frontend linked outside system library directories')
    if ROW == 'packaged-static':
        r.require(not identities, 'Static frontend linked shared ICU')
    else:
        r.require(len(identities) == 3, 'Missing ICU frontend shared libraries')
    r.save('selected-icu-linkage.json', dict(row=ROW, libraries=identities,
           installed_versions_receipt='icu_versions_probe.log', stock_equivalence_claim=False,
           binary_ABI_equality_accepted=False))

def preserve():
    binding = json.loads((r.HERE / 'binding.json').read_text())
    checkout = r.inspect_checkout()
    initialized = json.loads((r.OUT / 'execution-identity.json').read_text())
    r.require(checkout['mode_capabilities'] == binding['mode_capabilities'] ==
              initialized['mode_capabilities'],
              'Changed initialized filesystem mode capabilities')
    unchanged = all(Path(p).is_file() and not Path(p).is_symlink() and r.sha(p) == digest
                    for p, digest in binding['source_sha256'].items())
    tracked_diff = command('source-after-diff', ['git', 'diff', 'HEAD', '--'])
    unchanged = unchanged and not tracked_diff.strip()
    r.save('source-after.json', dict(current_checkout_sources_unchanged=unchanged,
           source_map_sha256=binding['source_map_sha256'],
           production_source_map_sha256=binding['production_source_map_sha256']))
    r.save('scope.json', dict(row=ROW, backend=r.BACKEND, intl_enabled=True, temporal_enabled=True,
           full_official_Test262_owner='current native-enabled 12-row workflow',
           frontend_only_sanitizer=False, MSan_pending=True, binary_ABI_equality_pending=True,
           broader_enabled_platform_census_pending=True, runtime_accepted=False, hosted_accepted=False,
           publication_authorized=False, zero_error_conformance_claim=False,
           conformance_accepted=False, failure_waiver=False))
    r.require(unchanged, 'Qualification changed the bound current checkout sources')

phase = sys.argv[1]
if phase == 'initialize':
    initialize()
elif phase == 'sdk':
    sdk()
elif phase == 'preserve':
    preserve()
else:
    args = make_args()
    r.make_args = make_args
    if phase == 'timezone-inputs':
        r.main(phase)
    elif phase == 'build':
        scope = json.loads((r.HERE / 'activation-scope.json').read_text())
        suffix = '.exe' if ROW in ('mingw', 'msys2') else ''
        # Platform Make owns its guarded native targets; do not impose Linux census.
        targets = ([t + suffix for t in scope[r.BACKEND]['C_targets']] if r.BACKEND == 'icu' else [])
        command('build', args + ['-j' + str(r.available_cpus()), 'all'] + targets)
        stamp = Path('.obj/temporal-build-config').read_text().splitlines()
        r.require(stamp[:6] == ['y', 'y' if r.BACKEND == 'icu' else 'n', 'y', 'y', r.BACKEND,
                  'n' if r.BACKEND == 'icu' else 'y'], 'Wrong enabled stamp')
        r.save('candidate-build-policy.json', dict(make_args=args, intl_build_config=stamp))
        if r.BACKEND == 'icu':
            icu_identity(args)
        else:
            command('target-binary', ['file', './qjs' + suffix])
    elif phase == 'repository-tests':
        command('repository-tests', args + ['test'])
    elif phase == 'public-api':
        r.require(ROW in ('packaged-static', 'packaged-shared', 'system'), 'Linux ICU API helper only')
        r.public_api(args)
    elif phase == 'selection-units':
        command('icu-selection-units', ['python3', 'tests/test_icu_build_config.py'])
        if ROW == 'system':
            command('system-icu-floor', ['/usr/bin/pkg-config', '--atleast-version=74', 'icu-i18n', 'icu-uc'])
            command('system-icu-version', ['/usr/bin/pkg-config', '--modversion', 'icu-i18n', 'icu-uc'])
            command('system-icu-prefix', ['/usr/bin/pkg-config', '--variable=prefix', 'icu-i18n'])
    else:
        raise ValueError('Unknown phase')
