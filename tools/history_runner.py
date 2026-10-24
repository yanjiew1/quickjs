#!/usr/bin/env python3
"""Run one immutable historical revision under its fullest supported profile.

The controller lives outside candidate history. Each phase has its own bounded
process group and receipt; independent suites can still run after another fails.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def stamp():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def write_json(path, value):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
    temp.replace(path)


def command(args, cwd, env, log, seconds):
    started = stamp()
    print(json.dumps(dict(started_at=started, phase_log=str(log), command=[str(a) for a in args], timeout_seconds=seconds)), flush=True)
    with log.open('wb') as stream:
        child = subprocess.Popen([str(a) for a in args], cwd=str(cwd), env=env,
                                 stdout=stream, stderr=subprocess.STDOUT,
                                 start_new_session=True)
        timed_out = False
        interrupted = False
        try:
            code = child.wait(timeout=seconds)
        except (subprocess.TimeoutExpired, KeyboardInterrupt, SystemExit) as error:
            timed_out = isinstance(error, subprocess.TimeoutExpired)
            interrupted = not timed_out
            try:
                os.killpg(child.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                code = child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                code = child.wait(timeout=10)
    result = dict(command=[str(a) for a in args], cwd=str(cwd), started_at=started,
                  completed_at=stamp(), returncode=code, timed_out=timed_out,
                  interrupted=interrupted, log=str(log), log_sha256=digest(log.read_bytes()))
    write_json(log.with_suffix('.json'), result)
    print(json.dumps(result), flush=True)
    print(log.read_text(errors='replace')[-12000:], flush=True)
    if code or timed_out or interrupted:
        raise RuntimeError('phase command failed: ' + log.name)
    return result


def declared_options(source):
    path = source / 'meson_options.txt'
    return set(re.findall(r"\boption\(\s*['\"]([^'\"]+)['\"]", path.read_text())) if path.exists() else set()


def settings(source, report, mode):
    opts = declared_options(source)
    features = {name: value for name, value in
                [('temporal', 'true'), ('intl', 'native'), ('calendars', 'native'),
                 ('timezone_backend', 'native'), ('system_timezone_policy', 'utc')]
                if name in opts}
    make = not (source / 'meson.build').exists()
    build_source = report / 'make-source' if make else source
    build = report / 'build'
    compiler = 'clang' if mode == 'msan' else 'gcc'
    sanitizer = {'asan-ubsan': 'address,undefined', 'asan': 'address',
                 'ubsan': 'undefined', 'msan': 'memory'}[mode]
    runner_source = next(p for p in [source / 'tools/run-test262.c', source / 'run-test262.c'] if p.exists())
    runner_text = runner_source.read_text()
    sha = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], timeout=30).decode().strip()
    tree = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD^{tree}'], timeout=30).decode().strip()
    return dict(revision=sha, tree=tree, mode=mode, compiler=compiler, sanitizer=sanitizer,
                features=features, build_system='make' if make else 'meson',
                source=str(source), build_source=str(build_source), build=str(build),
                threads=os.cpu_count() or 1, assertions=True, lto=False,
                timing='--timing-log' in runner_text, progress='--progress-log' in runner_text,
                intl_config='intl' in features and (source / 'tests/intl-locale-test262.conf').is_file(),
                documentation='documentation' in opts)


def compiler_wrapper(report, state):
    """Keep sanitizer linking for historical qjsc-generated executables too."""
    path = report / 'cc-wrapper'
    path.write_text('''#!/usr/bin/env python3
import os, sys
args = sys.argv[1:]
flags = ['-fsanitize=SANITIZER', '-fno-omit-frame-pointer']
if '-shared' in args:
    flags += ['-fPIC']
elif '-c' in args or '-S' in args or '-E' in args:
    if '-fPIC' not in args and '-fpic' not in args:
        flags += ['-fPIE']
else:
    flags += ['-fPIE', '-pie']
os.execvp('COMPILER', ['COMPILER'] + args + flags)
'''.replace('SANITIZER', state['sanitizer']).replace('COMPILER', state['compiler']))
    path.chmod(0o755)
    return path


def make_args(state, wrapper):
    values = ['make', 'CONFIG_WERROR=y', 'CONFIG_LTO=', 'CC=' + str(wrapper)]
    if state['mode'] == 'msan':
        values += ['CONFIG_CLANG=y', 'CONFIG_MSAN=y']
    else:
        if 'address' in state['sanitizer']:
            values += ['CONFIG_ASAN=y']
        if 'undefined' in state['sanitizer']:
            values += ['CONFIG_UBSAN=y']
    return values


def main():
    def terminate(signum, frame):
        raise SystemExit(128 + signum)
    signal.signal(signal.SIGTERM, terminate)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--mode', choices=['asan-ubsan', 'asan', 'ubsan', 'msan'], required=True)
    parser.add_argument('--phase', choices=['configure', 'build', 'unit', 'integration', 'bootstrap', 'core', 'intl'], required=True)
    args = parser.parse_args()
    source, report = args.source.resolve(), args.report.resolve()
    report.mkdir(parents=True, exist_ok=True)
    state = settings(source, report, args.mode)
    saved = report / 'configuration.json'
    if saved.exists():
        assert json.loads(saved.read_text()) == state, 'Configuration drift between phases'
    else:
        write_json(saved, state)
    env = dict(os.environ, ASAN_OPTIONS='halt_on_error=1:detect_leaks=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1', MSAN_OPTIONS='halt_on_error=1')
    cwd, build = Path(state['build_source']), Path(state['build'])
    wrapper = compiler_wrapper(report, state)
    env['CC'] = str(wrapper)
    make = make_args(state, wrapper)
    phase = args.phase
    log = report / (phase + '.log')
    if phase == 'configure':
        if state['build_system'] == 'make':
            command(['git', '-C', source, 'worktree', 'add', '--detach', cwd, state['revision']], source, env, log, 120)
        else:
            options = ['-Dwerror=true', '-Db_ndebug=false', '-Db_lto=false', '-Db_sanitize=' + state['sanitizer']]
            if state['mode'] == 'msan':
                options += ['-Db_pie=true']
            if state['documentation']:
                options += ['-Ddocumentation=disabled']
            options += ['-D' + name + '=' + value for name, value in state['features'].items()]
            command(['meson', 'setup', build, source] + options, source, env, log, 300)
    elif phase == 'build':
        cmd = make + ['-j', str(state['threads']), 'all'] if state['build_system'] == 'make' else ['meson', 'compile', '-C', build, '-j', str(state['threads'])]
        command(cmd, cwd, env, log, 1200)
    elif phase in ('unit', 'integration'):
        if state['build_system'] == 'make':
            if phase == 'integration':
                unit = report / 'unit.json'
                complete = unit.exists() and json.loads(unit.read_text())['returncode'] == 0
                write_json(report / 'integration.json', {'status': 'historical make test owns the combined suite', 'coverage_complete': complete})
                if not complete:
                    raise RuntimeError('Historical make test did not complete successfully')
                return
            cmd = make + ['-k', 'test']
        else:
            cmd = ['meson', 'test', '-C', build, '--no-rebuild', '--suite', phase,
                   '--logbase', 'repo-' + phase, '--print-errorlogs', '--num-processes', str(state['threads']), '--timeout-multiplier', '3']
        command(cmd, cwd, env, log, 1800)
    elif phase == 'bootstrap':
        cmd = ['python3', source / 'tools/build/test262.py', 'bootstrap'] if state['build_system'] == 'meson' else make + ['test2-bootstrap']
        command(cmd, cwd, env, log, 600)
        checkout = cwd / 'test262'
        assert (checkout / 'harness/assert.js').is_file(), 'Test262 bootstrap did not install tests'
        pin = subprocess.check_output(['git', '-C', str(checkout), 'rev-parse', 'HEAD'], timeout=30).decode().strip()
        patches = {str(p.relative_to(cwd)): digest(p.read_bytes()) for p in (cwd / 'tests').glob('test262*.patch')}
        write_json(report / 'test262-source.json', dict(pin=pin, patches=patches))
    else:
        if phase == 'intl' and not state['intl_config']:
            write_json(report / 'intl.json', {'status': 'not applicable; no enabled Intl API option/config at this revision'})
            return
        config = 'test262.conf' if phase == 'core' else 'tests/intl-locale-test262.conf'
        execute = cwd
        runner = cwd / 'run-test262' if state['build_system'] == 'make' else build / 'run-test262'
        cmd = [runner, '-T', str(state['threads']), '-t', '-m', '-c', config]
        if phase == 'core':
            cmd += ['-a']
        if state['timing']:
            cmd += ['--timing-log', report / (phase + '-timings.jsonl')]
        if state['progress']:
            cmd += ['--progress-log', report / (phase + '-progress.jsonl')]
        command(cmd, execute, env, log, 3600)


if __name__ == '__main__':
    main()
