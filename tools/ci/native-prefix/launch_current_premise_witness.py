#!/usr/bin/env python3
"""Current bounded executor for the immutable original-assertion launcher.

The historical renderer, assertion stream and validators remain byte-pinned.
Only the exact ASan lunisolar capture gets extra wall-time headroom.
"""
import argparse
import functools
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from premise_source_compatibility import install, validate_bindings
from execution_threads import available_cpus

LUNISOLAR_FIXTURE = ('test262/test/intl402/DateTimeFormat/prototype/'
                     'formatToParts/compare-to-temporal-lunisolar.js')
LUNISOLAR_SHA256 = 'a2aaeb68a67e1f4e95c81a7eec113474f4ee4e730b804e8df09a1387d446489e'


def require(value, reason):
    if not value:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def budget_digest(policy):
    return digest(json.dumps(policy, sort_keys=True, separators=(',', ':')).encode())


def load_execution_budget(bindings):
    """Bind the budget to actual closed build receipts, not an environment flag."""
    policy = bindings.get('witness_execution_budget')
    require(isinstance(policy, dict) and set(policy) == {
        'schema', 'backend', 'compiler', 'sanitizer', 'source_tree_sha256',
        'engine_sha256', 'checkout_commit', 'build_policy', 'build_command'},
        'Missing or unknown witness execution budget fields')
    require(policy['schema'] == 'native-current-witness-execution-budget-v1' and
            policy['backend'] == 'native' and policy['compiler'] in ('gcc', 'clang') and
            policy['sanitizer'] in ('normal', 'ASan', 'UBSan') and
            policy['source_tree_sha256'] == bindings['source_tree_sha256'] and
            policy['engine_sha256'] == bindings['engine_sha256'] and
            policy['checkout_commit'] == bindings['actual_initialized_checkout_commit'],
            'Witness budget source, engine or row identity differs')
    documents = {}
    parents = set()
    for field, filename in [('build_policy', 'candidate-build-policy.json'),
                            ('build_command', 'build.command.json')]:
        ref = policy[field]
        require(isinstance(ref, dict) and set(ref) == {'path', 'sha256'},
                'Invalid witness budget document binding')
        path = Path(ref['path'])
        require(path.is_absolute() and path.name == filename and path.is_file() and
                not path.is_symlink() and digest(path.read_bytes()) == ref['sha256'],
                'Witness build document identity differs')
        parents.add(path.parent.resolve())
        documents[field] = json.loads(path.read_bytes())
    require(len(parents) == 1, 'Witness build documents have different owners')
    compiler, sanitizer = policy['compiler'], policy['sanitizer']
    args = ['make', 'CONFIG_WERROR=y', 'CONFIG_LTO=', 'CC=' + compiler,
            'HOST_CC=' + compiler, 'CONFIG_CLANG=' + ('y' if compiler == 'clang' else ''),
            'CONFIG_ASAN=' + ('y' if sanitizer == 'ASan' else ''),
            'CONFIG_UBSAN=' + ('y' if sanitizer == 'UBSan' else ''),
            'CONFIG_INTL_BACKEND=native', 'CONFIG_ICU=n', 'CONFIG_INTL=y',
            'CONFIG_TEMPORAL=y', 'CONFIG_INTL_LEGACY=y']
    scope = json.loads((HERE / 'activation-scope.json').read_bytes())['native']
    targets = scope['C_targets'] + [unit['target'] for unit in scope['special_C_units']]
    build = documents['build_policy']
    command = documents['build_command']
    require(build.get('make_args') == args and build.get('unicode_backend') == 'native' and
            build.get('intl_build_config', [])[:6] == ['y', 'n', 'y', 'y', 'native', 'y'] and
            build.get('available_worker_count') == available_cpus() and
            command.get('argv') == args + ['-j' + str(available_cpus()), 'all'] + targets and
            command.get('returncode') == 0 and command.get('timed_out') is False and
            command.get('interrupted') is False and command.get('lifecycle_closed') is True and
            command.get('effective_profile_environment', {}).get('PREFIX_BACKEND') == 'native' and
            command['effective_profile_environment'].get('PREFIX_COMPILER') == compiler and
            command['effective_profile_environment'].get('PREFIX_SANITIZER') == sanitizer,
            'Witness budget does not match a closed build of the selected row')
    return policy


def child_timeout(policy, fixture_config=None):
    config = fixture_config or {}
    if (policy['sanitizer'] == 'ASan' and config.get('fixture') == LUNISOLAR_FIXTURE and
            config.get('fixture_sha256') == LUNISOLAR_SHA256):
        return 600
    return 180


def require_budget_receipt(command, policy, fixture_config=None):
    require(command.get('execution_budget_sha256') == budget_digest(policy) and
            command.get('timeout_seconds') == child_timeout(policy, fixture_config),
            'Original capture receipt differs from its authenticated timeout policy')


def run_owned(launcher, policy, argv, cwd, prefix, bindings, generated_sha256, config_sha256=None, fixture_config=None, child_env=None):
    from trace_stream import DEFAULT_OUTPUT_LIMIT, hash_file, trace_limits
    if launcher.STOP_REQUESTED:
        raise KeyboardInterrupt('Diagnostic launcher already stopped')
    effective_env = dict(os.environ) if child_env is None else dict(child_env)
    prefix = Path(prefix)
    stdout = prefix.with_suffix('.stdout.log')
    stderr = prefix.with_suffix('.stderr.log')
    timeout = child_timeout(policy, fixture_config)
    limit = trace_limits(fixture_config or {})[0]
    stderr_limit = DEFAULT_OUTPUT_LIMIT
    started = time.monotonic()
    interrupted = False
    timed_out = False
    output_limited = False
    proc = None
    returncode = None
    error = None
    cleanup_failed = False
    with stdout.open('wb') as out, stderr.open('wb') as err:
        try:
            # A signal inside Popen must not unwind before its handle is assigned.
            # Deferring the exception leaves child signal masks unchanged.
            launcher._DEFER_STOP = True
            try:
                proc = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.DEVNULL, stdout=out, stderr=err, env=effective_env, start_new_session=True)
            finally:
                launcher._DEFER_STOP = False
            if launcher.STOP_REQUESTED:
                raise KeyboardInterrupt('Cancellation during child creation')
            while proc.poll() is None:
                if time.monotonic() - started > timeout:
                    timed_out = True
                    break
                if stdout.stat().st_size > limit or stderr.stat().st_size > stderr_limit:
                    output_limited = True
                    break
                time.sleep(0.05)
        except BaseException as exc:
            interrupted = isinstance(exc, KeyboardInterrupt)
            error = repr(exc)
        finally:
            # Repeated cancellation cannot interrupt bounded cleanup or the receipt.
            launcher._DEFER_STOP = True
            if interrupted:
                launcher.STOP_REQUESTED = True
            if proc is not None:
                # Attempt KILL even when TERM or its wait reported an OS error.
                for signum in (signal.SIGTERM, signal.SIGKILL):
                    try:
                        os.killpg(proc.pid, signum)
                    except ProcessLookupError:
                        pass
                    except OSError as exc:
                        cleanup_failed = True
                        error = repr(exc)
                    try:
                        returncode = proc.wait(timeout=1)
                    except subprocess.TimeoutExpired as exc:
                        if signum == signal.SIGKILL:
                            cleanup_failed = True
                            error = repr(exc)
                    except OSError as exc:
                        cleanup_failed = True
                        error = repr(exc)
    group_closed = False
    if proc is not None:
        try:
            os.killpg(proc.pid, 0)
        except ProcessLookupError:
            group_closed = True
        except OSError as exc:
            cleanup_failed = True
            error = repr(exc)
    output_limited = output_limited or stdout.stat().st_size > limit or stderr.stat().st_size > stderr_limit
    interrupted = interrupted or launcher.STOP_REQUESTED
    record = dict(argv=argv, cwd=str(cwd), pid=proc.pid if proc else None, process_group=proc.pid if proc else None,
        returncode=returncode, timed_out=timed_out, interrupted=interrupted, output_limited=output_limited,
        lifecycle_closed=group_closed and not cleanup_failed, cleanup_failed=cleanup_failed, elapsed_seconds=time.monotonic() - started, timeout_seconds=timeout,
        generated_sha256=generated_sha256, config_sha256=config_sha256,
        stdout_sha256=hash_file(stdout), stderr_sha256=hash_file(stderr),
        stdout_limit_bytes=limit, stderr_limit_bytes=stderr_limit,
        engine_sha256=bindings['engine_sha256'], source_tree_sha256=bindings['source_tree_sha256'], profile_sha256=bindings['profile_sha256'],
        effective_env_sha256=digest(json.dumps(effective_env, sort_keys=True, separators=(',', ':')).encode()),
        observer_environment={k: effective_env[k] for k in ('QJS_TZ_CAPTURE_DIRECTORY',) if k in effective_env},
        observer_environment_sha256=digest(json.dumps({k: effective_env[k] for k in ('QJS_TZ_CAPTURE_DIRECTORY',) if k in effective_env}, sort_keys=True, separators=(',', ':')).encode()),
        locale_environment={k: effective_env.get(k) for k in ('TZ', 'LANG', 'LC_ALL', 'LC_TIME', 'LC_NUMERIC')}, error=error)
    record['execution_budget_sha256'] = budget_digest(policy)
    launcher.write_json(prefix.with_suffix('.command.json'), record)
    launcher._DEFER_STOP = False
    return record, stdout


def main():
    packet = HERE / 'original-assertions'
    install(packet)
    path = packet / 'launch_practical_witness.py'
    spec = importlib.util.spec_from_file_location('sealed_practical_launcher', path)
    launcher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(launcher)
    if '-h' in sys.argv[1:] or '--help' in sys.argv[1:]:
        return launcher.main()
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--bindings', required=True)
    args, _ = parser.parse_known_args()
    raw = Path(args.bindings).read_bytes()
    bindings = json.loads(raw)
    validate_bindings(bindings, packet)
    policy = load_execution_budget(bindings)
    launcher.run_owned = functools.partial(run_owned, launcher, policy)
    result = launcher.main()
    require(Path(args.bindings).read_bytes() == raw, 'Witness bindings changed during capture')
    return result


if __name__ == '__main__':
    raise SystemExit(main())
