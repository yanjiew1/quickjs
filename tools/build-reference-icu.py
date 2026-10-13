#!/usr/bin/env python3
"""Build the optional pinned ICU SDK from an already downloaded archive.

Run under the invoking job's process owner and timeout. Child commands inherit
that owner's session; this script starts no detached sessions or network work.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tarfile

HERE = Path(__file__).resolve().parent
PACKAGE = HERE / 'icu-reference'


def digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def files(directory):
    result = {}
    for path in sorted(directory.rglob('*')):
        name = str(path.relative_to(directory))
        if path.is_symlink():
            result[name] = {'symlink': os.readlink(path)}
        elif path.is_file():
            result[name] = {'sha256': digest(path)}
    return result


def save(path, value):
    temporary = path.with_name(path.name + '.tmp')
    with temporary.open('w') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def shared_linkage(log, prefix, installed):
    """Require each ICU dependency to resolve to the just-installed SDK bytes."""
    text = log.read_text()
    if 'not found' in text:
        raise ValueError('Unresolved capability dependency: ' + str(log))
    expected = {'libicui18n.so', 'libicuuc.so', 'libicudata.so'}
    resolved = {}
    dependencies = []
    library_root = (prefix / 'lib').resolve(strict=True)
    for line in text.splitlines():
        match = re.fullmatch(r'\s*(\S+)\s+=>\s+(/\S+)\s+\(0x[0-9a-fA-F]+\)\s*', line)
        if not match:
            if 'libicu' in line:
                raise ValueError('Unrecognized ICU linkage line: ' + line)
            continue
        soname, path = match.groups()
        actual = Path(path).resolve(strict=True)
        dependencies.append(dict(soname=soname, path=path,
                                 resolved_path=str(actual), sha256=digest(actual)))
        if not soname.startswith('libicu'):
            continue
        stem = re.sub(r'\.so(?:\.\d+)*$', '.so', soname)
        if stem not in expected or stem in resolved:
            raise ValueError('Unexpected or duplicate ICU dependency: ' + soname)
        wanted = (prefix / 'lib' / stem).resolve(strict=True)
        if actual != wanted or not actual.is_relative_to(library_root):
            raise ValueError('Capability resolved outside installed ICU SDK: ' + path)
        relative = str(actual.relative_to(prefix.resolve(strict=True)))
        if installed.get(relative) != {'sha256': digest(actual)}:
            raise ValueError('Capability dependency differs from installed manifest: ' + path)
        resolved[stem] = dependencies[-1]
    if set(resolved) != expected:
        raise ValueError('Missing installed ICU dependencies: ' + str(expected - set(resolved)))
    return dict(icu=resolved, dependencies=dependencies, log_sha256=digest(log))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--cc', required=True, type=Path)
    parser.add_argument('--cxx', required=True, type=Path)
    parser.add_argument('--ar', default='/usr/bin/ar', type=Path)
    parser.add_argument('--ranlib', default='/usr/bin/ranlib', type=Path)
    parser.add_argument('--nm', default='/usr/bin/nm', type=Path)
    parser.add_argument('--ld', default='/usr/bin/ld', type=Path)
    parser.add_argument('--as', dest='assembler', default='/usr/bin/as', type=Path)
    parser.add_argument('--ldd', default='/usr/bin/ldd', type=Path)
    parser.add_argument('--shell', default='/bin/sh', type=Path)
    parser.add_argument('--make', default='/usr/bin/make', type=Path)
    parser.add_argument('--patch', default='/usr/bin/patch', type=Path)
    parser.add_argument('--profile', required=True, choices=('shared', 'static'))
    parser.add_argument('--sanitizer', default='none', choices=('none', 'address', 'undefined'))
    parser.add_argument('--jobs', default=4, type=int, choices=range(1, 5))
    parser.add_argument('--runtime-library-path', default='')
    parser.add_argument('--runner-identity', required=True,
                        help='Immutable runner image/libc/toolchain image identity')
    parser.add_argument('--check', action='store_true', help='Run the ICU test suite')
    args = parser.parse_args()
    if platform.system() != 'Linux':
        raise ValueError('This SDK builder supports native Linux GCC with external GNU binutils only')
    provenance = json.loads((PACKAGE / 'PROVENANCE.json').read_text())
    if digest(args.archive) != provenance['archive_sha256']:
        raise ValueError('ICU archive digest mismatch')
    for name, expected in provenance['packaged_inputs'].items():
        if digest(PACKAGE / name) != expected:
            raise ValueError('Packaged ICU input mismatch: ' + name)
    root = args.output.resolve()
    if not re.fullmatch(r'/[A-Za-z0-9_./-]+', str(root)):
        raise ValueError('Output path must use simple ASCII path characters (ICU makefile constraint)')
    if root.exists() or root.is_symlink():
        raise ValueError('Output must be a fresh directory; failed builds are preserved')
    tools = {}
    for name in ('cc', 'cxx', 'ar', 'ranlib', 'nm', 'ld', 'assembler', 'ldd', 'shell', 'make', 'patch'):
        path = getattr(args, name)
        if not path.is_absolute() or not path.is_file() or not os.access(path, os.X_OK):
            raise ValueError('Tool must be an absolute file: ' + name)
        if not re.fullmatch(r'/[A-Za-z0-9_./+-]+', str(path)):
            raise ValueError('Tool path must use simple ASCII path characters: ' + name)
        tools[name] = {'path': str(path), 'resolved_path': str(path.resolve(strict=True)),
                       'sha256': digest(path)}
    flags = '-O2 -fPIC'
    if args.sanitizer != 'none':
        flags += ' -g -fno-omit-frame-pointer -fsanitize=' + args.sanitizer
        if args.sanitizer == 'undefined':
            flags += ' -fno-sanitize-recover=all'
    prefix = root / 'install'
    configure = ['--prefix=' + str(prefix),
                 '--enable-tests' if args.check else '--disable-tests', '--disable-samples',
                 '--disable-static' if args.profile == 'shared' else '--enable-static',
                 '--enable-shared' if args.profile == 'shared' else '--disable-shared',
                 '--with-data-packaging=' + ('library' if args.profile == 'shared' else 'static')]
    identity = dict(provenance=provenance, script_sha256=digest(__file__), tools=tools,
                    runner_identity=args.runner_identity, architecture=platform.machine(),
                    libc=platform.libc_ver(), flags=flags, profile=args.profile,
                    sanitizer=args.sanitizer, configure_arguments=[x for x in configure if not x.startswith('--prefix=')])
    identity['driver_selection'] = {'supported': 'native Linux GCC; external GNU assembler and BFD linker',
                                    'options': ['-B<private traced tool directory>/', '-fuse-ld=bfd'],
                                    'assembler': tools['assembler'], 'linker': tools['ld']}
    if args.runtime_library_path:
        runtime_directory = Path(args.runtime_library_path)
        if (not runtime_directory.is_absolute() or not runtime_directory.is_dir() or
                ':' in str(runtime_directory)):
            raise ValueError('Runtime library path must be one absolute directory')
        identity['runtime_library_path'] = str(runtime_directory)
        identity['runtime_library_manifest'] = files(runtime_directory)
    # The caller must supplement runtime/toolchain image identity when compiler
    # launchers load mutable libraries. No broad or release-only cache keys.
    cache_key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    root.mkdir(parents=True)
    (root / 'tmp').mkdir()
    build = root / 'build'
    build.mkdir()
    selected = root / 'selected-tools'
    selected.mkdir()
    trace = root / 'selected-tools.log'
    trace.touch()
    # These wrappers use only the already-declared shell. A real invocation
    # records its role and execs the exact hashed absolute tool, never PATH.
    for name, role in (('as', 'assembler'), ('ld', 'ld'), ('ld.bfd', 'ld')):
        wrapper = selected / name
        wrapper.write_text('#!' + str(args.shell) + '\nset -eu\n' +
            "printf '%s %s\\n' " + shlex.quote(role) + ' "$$" >> ' + shlex.quote(str(trace)) + '\n' +
            'exec ' + shlex.quote(str(getattr(args, role))) + ' "$@"\n')
        wrapper.chmod(0o755)
    selection_flags = ['-B' + str(selected) + '/', '-fuse-ld=bfd']
    environment = dict(PATH=str(selected) + ':' + str(args.cc.parent) + ':/usr/bin:/bin', LANG='C.UTF-8',
                       LC_ALL='C.UTF-8', TZ='UTC', TMPDIR=str(root / 'tmp'),
                       CC=shlex.join([str(args.cc), *selection_flags]),
                       CXX=shlex.join([str(args.cxx), *selection_flags]), AR=str(args.ar),
                       RANLIB=str(args.ranlib), NM=str(args.nm), LD=str(selected / 'ld'),
                       AS=str(selected / 'as'), SHELL=str(args.shell), CONFIG_SHELL=str(args.shell),
                       CFLAGS=flags, CXXFLAGS=flags, LDFLAGS=flags,
                       LD_LIBRARY_PATH=args.runtime_library_path,
                       ASAN_OPTIONS='halt_on_error=1:detect_leaks=1',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    state = dict(status='building', identity=identity, cache_key=cache_key,
                 environment=environment, commands=[], capability_pass=False,
                 full_icu_check_pass=False, engine_acceptance=False,
                 sanitizer_acceptance=False, selected_tool_wrappers=files(selected),
                 effective_driver_tools={}, shared_linkage_pass=False)
    save(root / 'sdk-manifest.json', state)

    def run(name, argv, cwd=build, env=environment):
        item = dict(name=name, argv=list(map(str, argv)), cwd=str(cwd), environment=env)
        state['commands'].append(item)
        save(root / 'sdk-manifest.json', state)
        with (root / (name + '.log')).open('xb') as output:
            value = subprocess.run(argv, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                   stdout=output, stderr=subprocess.STDOUT, check=False)
        item.update(returncode=value.returncode, log_sha256=digest(root / (name + '.log')))
        save(root / 'sdk-manifest.json', state)
        value.check_returncode()

    try:
        for role, marker in (('assembler', 'GNU assembler'), ('ld', 'GNU ld')):
            run('selected-' + role + '-version', [str(getattr(args, role)), '--version'])
            if marker not in (root / ('selected-' + role + '-version.log')).read_text():
                raise ValueError('Unsupported external tool: ' + role)
        probe = root / 'driver-probe.c'
        probe.write_text('int main(void) { return 0; }\n')
        for name, language in (('cc', 'c'), ('cxx', 'c++')):
            driver = [str(getattr(args, name)), *selection_flags]
            run(name + '-target', [*driver, '-dumpmachine'])
            target = (root / (name + '-target.log')).read_text().strip()
            if target.split('-')[0] != platform.machine() or '-linux-' not in target:
                raise ValueError('Unsupported compiler target: native Linux architecture is required: ' + target)
            run(name + '-macros', [*driver, '-dM', '-E', '-x', language, str(probe)])
            macros = (root / (name + '-macros.log')).read_text()
            if '#define __GNUC__ ' not in macros or '#define __clang__ ' in macros:
                raise ValueError('Unsupported compiler driver: native GCC is required')
            for program, role in (('as', 'assembler'), ('ld.bfd', 'ld')):
                log_name = name + '-selected-' + program
                run(log_name, [*driver, '-print-prog-name=' + program])
                reported = Path((root / (log_name + '.log')).read_text().strip())
                if reported != selected / program:
                    raise ValueError('Compiler driver did not select pinned tool wrapper: ' + name + '/' + program)
                state['effective_driver_tools'][name + '/' + program] = dict(
                    wrapper=str(reported), wrapper_sha256=digest(reported), **tools[role])
            before = trace.read_text().splitlines()
            run(name + '-tool-probe', [*driver, '-v', '-x', language, str(probe),
                                      '-o', str(root / (name + '-tool-probe'))])
            invocations = trace.read_text().splitlines()[len(before):]
            if not any(x.startswith('assembler ') for x in invocations) or not any(x.startswith('ld ') for x in invocations):
                raise ValueError('Compiler probe bypassed selected assembler or linker: ' + name)
            run(name + '-tool-probe-run', [str(root / (name + '-tool-probe'))], cwd=root)
        save(root / 'sdk-manifest.json', state)
        with tarfile.open(args.archive) as archive:
            for member in archive.getmembers():
                path = Path(member.name)
                if path.is_absolute() or '..' in path.parts or path.parts[0] != 'icu':
                    raise ValueError('Unexpected ICU archive path')
                if not (member.isfile() or member.isdir()):
                    raise ValueError('Unexpected ICU archive entry type')
            archive.extractall(root, filter='data')
        source = root / 'icu'
        for name, expected in provenance['original_files'].items():
            if digest(source / name) != expected:
                raise ValueError('Pinned source mismatch: ' + name)
        original_source_manifest = files(source)
        run('patch', [str(args.patch), '--batch', '--forward', '--fuzz=0', '-p1',
                      '-i', str(PACKAGE / 'exact-plural.patch')], cwd=source)
        run('patch-calendar', [str(args.patch), '--batch', '--forward', '--fuzz=0', '-p1',
                      '-i', str(PACKAGE / 'calendar-integer.patch')], cwd=source)
        run('patch-chinese-pmo', [str(args.patch), '--batch', '--forward', '--fuzz=0', '-p1',
                      '-i', str(PACKAGE / 'chinese-pmo.patch')], cwd=source)
        for name, expected in provenance['patched_files'].items():
            if digest(source / name) != expected:
                raise ValueError('Patched source mismatch: ' + name)
        state['patched_source_manifest'] = files(source)
        changed = {name for name in set(original_source_manifest) | set(state['patched_source_manifest'])
                   if original_source_manifest.get(name) != state['patched_source_manifest'].get(name)}
        if changed != set(provenance['patched_files']):
            raise ValueError('Unexpected ICU patch file delta')
        expected_source = json.loads((PACKAGE / 'icu-post-source-sha256.json').read_text())
        if state['patched_source_manifest'] != expected_source:
            raise ValueError('Complete patched ICU source manifest mismatch')
        expected_public_headers = json.loads((PACKAGE / 'icu-public-headers-sha256.json').read_text())
        for name, expected in expected_public_headers.items():
            if (original_source_manifest.get(name) != expected or
                    state['patched_source_manifest'].get(name) != expected):
                raise ValueError('Public ICU Unicode header changed: ' + name)
        actual_public_headers = {name for name in state['patched_source_manifest']
                                 if '/unicode/' in name and name.endswith('.h')}
        if actual_public_headers != set(expected_public_headers):
            raise ValueError('Public ICU Unicode header set changed')
        state['public_headers_source_equal'] = True
        state['public_abi_runtime_accepted'] = False
        run('configure', [str(args.shell), str(source / 'source/configure'), *configure])
        run('build', [str(args.make), '-j' + str(args.jobs)])
        run('install', [str(args.make), 'install'])
        installed = files(prefix)
        libraries = ['-L' + str(prefix / 'lib'), '-licui18n', '-licuuc', '-licudata']
        if args.profile == 'static':
            libraries = [str(prefix / 'lib' / name) for name in
                         ('libicui18n.a', 'libicuuc.a', 'libicudata.a')]
        runtime = dict(environment, LD_LIBRARY_PATH=str(prefix / 'lib') +
                       (':' + args.runtime_library_path if args.runtime_library_path else ''))
        for name in ('capability', 'icu-hebrew-epoch', 'icu-indian-negative-julian'):
            run(name + '-build', [str(args.cc), *selection_flags, '-std=c11', *flags.split(),
                '-I' + str(prefix / 'include'), str(PACKAGE / (name + '.c')),
                *libraries, '-lstdc++', '-lm', '-ldl', '-lpthread', '-o', str(root / name)])
            if args.profile == 'shared':
                run(name + '-linkage', [str(args.ldd), str(root / name)], cwd=root, env=runtime)
                state.setdefault('capability_linkage', {})[name] = shared_linkage(
                    root / (name + '-linkage.log'), prefix, installed)
                save(root / 'sdk-manifest.json', state)
            run(name, [str(root / name)], cwd=root, env=runtime)
        state['shared_linkage_pass'] = args.profile == 'shared'
        state['capability_pass'] = True
        if args.check:
            run('icu-check', [str(args.make), '-j' + str(args.jobs), 'check'], env=runtime)
            state['full_icu_check_pass'] = True
        if files(source) != state['patched_source_manifest']:
            raise ValueError('ICU build changed the source tree')
        if files(prefix) != installed:
            raise ValueError('Installed SDK changed during qualification')
        for name, tool in tools.items():
            if (digest(tool['path']) != tool['sha256'] or
                    str(Path(tool['path']).resolve(strict=True)) != tool['resolved_path']):
                raise ValueError('Tool bytes changed during build: ' + name)
        if files(selected) != state['selected_tool_wrappers']:
            raise ValueError('Selected tool wrappers changed during build')
        state.update(status='built capability-qualified SDK', installed_files=installed,
                     selected_tool_trace_sha256=digest(trace), tool_identities_unchanged=True)
        save(root / 'sdk-manifest.json', state)
    except BaseException as error:
        state.update(status='failed', error=repr(error))
        save(root / 'sdk-manifest.json', state)
        raise


if __name__ == '__main__':
    main()
