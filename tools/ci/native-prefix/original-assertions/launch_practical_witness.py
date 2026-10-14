"""Root-owned bounded hosted launcher. Source preparation executes none of this module."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import signal
import subprocess
import sys
import time


STOP_REQUESTED = False
_DEFER_STOP = True


def request_stop(signum, frame):
    """Latch cancellation once; defer exceptions while acquiring or closing a child."""
    global STOP_REQUESTED
    first = not STOP_REQUESTED
    STOP_REQUESTED = True
    if first and not _DEFER_STOP:
        raise KeyboardInterrupt('Diagnostic launcher cancellation requested')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def run_owned(argv, cwd, prefix, bindings, generated_sha256, config_sha256=None, fixture_config=None, child_env=None):
    from trace_stream import DEFAULT_OUTPUT_LIMIT, hash_file, trace_limits
    global _DEFER_STOP, STOP_REQUESTED
    if STOP_REQUESTED:
        raise KeyboardInterrupt('Diagnostic launcher already stopped')
    effective_env = dict(os.environ) if child_env is None else dict(child_env)
    prefix = Path(prefix)
    stdout = prefix.with_suffix('.stdout.log')
    stderr = prefix.with_suffix('.stderr.log')
    timeout = 180
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
            _DEFER_STOP = True
            try:
                proc = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.DEVNULL, stdout=out, stderr=err, env=effective_env, start_new_session=True)
            finally:
                _DEFER_STOP = False
            if STOP_REQUESTED:
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
            _DEFER_STOP = True
            if interrupted:
                STOP_REQUESTED = True
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
    interrupted = interrupted or STOP_REQUESTED
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
    write_json(prefix.with_suffix('.command.json'), record)
    _DEFER_STOP = False
    return record, stdout


def main():
    global _DEFER_STOP, STOP_REQUESTED
    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)
    parser = argparse.ArgumentParser()
    parser.add_argument('--packet', required=True)
    parser.add_argument('--bindings', required=True)
    parser.add_argument('--qjs', required=True)
    parser.add_argument('--raw-full-run', required=True)
    parser.add_argument('--output-root', required=True)
    parser.add_argument('--selection')
    args = parser.parse_args()
    packet = Path(args.packet).resolve()
    bindings_raw = Path(args.bindings).read_bytes()
    raw_receipt_bytes = Path(args.raw_full_run).read_bytes()
    bindings = json.loads(bindings_raw)
    raw_full_run = json.loads(raw_receipt_bytes)
    # Authenticate all files before adding this isolated packet to the import path.
    if digest((packet / 'SEAL.json').read_bytes()) != bindings['packet_seal_sha256']:
        raise ValueError('Packet seal differs')
    for rel, expected in json.loads((packet / 'SEAL.json').read_text())['files'].items():
        path = (packet / rel).resolve()
        if not path.is_relative_to(packet) or not path.is_file() or digest(path.read_bytes()) != expected:
            raise ValueError('Authenticated packet file changed: ' + rel)
    sys.path.insert(0, str(packet))
    from trace_stream import DEFAULT_OUTPUT_LIMIT, hash_file, trace_limits
    from selected_timezone_orchestration import (TZ_FIXTURE, fixture_bindings,
        fresh_directory, authenticated_input, verify_actual, file_sha, comparison_role)
    from render_practical_witness import render
    from validate_practical_witness import validate_fixture
    from validate_current_rtf import validate_current_rtf
    qjs = Path(args.qjs).resolve()
    if not qjs.is_file() or digest(qjs.read_bytes()) != bindings['engine_sha256']:
        raise ValueError('Current qjs identity differs')
    output = Path(args.output_root).resolve()
    if output.exists():
        raise ValueError('Diagnostic output must be fresh and isolated')
    output.mkdir(parents=True)
    write_json(output / 'CURRENT-BINDINGS.json', bindings)
    (output / 'NORMAL-RAW-FULL-RUN-RECEIPT.json').write_bytes(raw_receipt_bytes)
    plan = json.loads((packet / 'HOSTED-RENDER-PLAN.json').read_text())
    prepared = {f['path'] for f in plan['fixtures'] + plan.get('oracle_fixtures', [])}
    rtf_paths = set(plan['existing_relative_time_capture']['paths'])
    selected = set(json.loads(Path(args.selection).read_text())) if args.selection else prepared | rtf_paths
    unknown = selected - prepared - rtf_paths
    if not selected or unknown:
        raise ValueError('Current selection contains newly unclassified source: ' + repr(sorted(unknown)))
    ordinary = selected & prepared
    manifest = []
    summaries = {}
    rtf_summaries = {}
    errors = []
    try:
        _DEFER_STOP = False
        if STOP_REQUESTED:
            raise KeyboardInterrupt('Cancellation before capture')
        manifest = render(packet, bindings, output / 'rendered', sorted(ordinary)) if ordinary else []
        write_json(output / 'GENERATED-MANIFEST.json', manifest)
        budget_started = time.monotonic()
        for index, item in enumerate(manifest):
            if STOP_REQUESTED:
                break
            if time.monotonic() - budget_started > 3600:
                errors.append(dict(reason='Bounded one-hour diagnostic launch budget exceeded'))
                break
            path = output / 'rendered' / item['generated_path']
            if digest(path.read_bytes()) != item['generated_sha256']:
                raise ValueError('Generated source changed before launch')
            child_bindings = fixture_bindings(bindings, item['path'])
            child_engine = qjs
            child_env = None
            provider_directory = None
            if item['path'] == TZ_FIXTURE:
                authenticated_input(packet, bindings)
                diagnostic = bindings['selected_provider_diagnostic']
                child_engine = Path(diagnostic['qjs'])
                provider_directory = fresh_directory(output / ('selected-provider-' + str(index)), output)
                child_env = dict(os.environ, QJS_TZ_CAPTURE_DIRECTORY=str(provider_directory))
            command, stdout = run_owned([str(child_engine), str(path)], output, output / ('fixture-' + str(index)), child_bindings,
                item['generated_sha256'], item['config']['_config_sha256'], child_env=child_env,
                **({'fixture_config': item['config']} if trace_limits(item['config'])[0] != DEFAULT_OUTPUT_LIMIT else {}))
            if provider_directory is not None:
                command['preflight_input_proof_sha256'] = diagnostic['preflight_input_proof_sha256']
                command['command_receipt'] = str(output / ('fixture-' + str(index) + '.command.json'))
                write_json(Path(command['command_receipt']), command)
            try:
                if command['output_limited']:
                    raise ValueError('Output bound exceeded')
                if command['stderr_sha256'] != digest(b''):
                    raise ValueError('Unknown diagnostic stderr')
                summary = validate_fixture(stdout, item['config'], command, raw_full_run)
                if provider_directory is not None:
                    proof = verify_actual(packet, bindings, item['config'], command, provider_directory, stdout)
                    write_json(output / ('selected-provider-' + str(index) + '-comparison.json'), proof)
                    summary['selected_provider_diagnostic_role'] = comparison_role(item['config'], proof)
                summaries.setdefault(item['path'], {})[item['variant']] = summary
            except (ValueError, UnicodeError) as exc:
                errors.append(dict(fixture=item['path'], variant=item['variant'], diagnostic_capture_complete=False,
                    reason=str(exc), command_receipt='fixture-' + str(index) + '.command.json'))
            if digest(path.read_bytes()) != item['generated_sha256']:
                errors.append(dict(fixture=item['path'], variant=item['variant'], reason='Generated source changed after launch'))
        if not STOP_REQUESTED and selected & rtf_paths and time.monotonic() - budget_started <= 3600:
            sampler = packet / 'rtf-reviewed-sampler' / 'capture-relative-diagnostic.js'
            command, stdout = run_owned([str(qjs), str(sampler)], output, output / 'rtf-current', bindings, digest(sampler.read_bytes()))
            try:
                if command['output_limited'] or command['stderr_sha256'] != digest(b''):
                    raise ValueError('RTF output incomplete or unknown stderr')
                rtf_summaries = validate_current_rtf(packet, stdout.read_text(), command, bindings, raw_full_run)
            except (ValueError, UnicodeError) as exc:
                errors.append(dict(fixture='RTF13', reason=str(exc), diagnostic_capture_complete=False))
    except KeyboardInterrupt as exc:
        STOP_REQUESTED = True
        errors.append(dict(reason=str(exc), diagnostic_capture_complete=False, interrupted=True))
    finally:
        # Preserve completed and partial summaries without another signal unwind.
        _DEFER_STOP = True
    if STOP_REQUESTED:
        errors.append(dict(reason='Diagnostic launcher stopped; remaining captures were not launched',
            diagnostic_capture_complete=False, interrupted=True))
    projected = {}
    for path, variants in summaries.items():
        expected = next(f.get('variants', ['nonstrict', 'strict']) for f in plan['fixtures'] + plan.get('oracle_fixtures', []) if f['path'] == path)
        if set(variants) != set(expected):
            continue
        first = next(iter(variants.values()))
        projected[path] = dict(first, variant='both-default-script-variants', variant_summaries=variants,
            fixture_capture_complete=True, full_original_assertion_capture_complete=True,
            continued_data_site_ids=sorted({id for v in variants.values() for id in v['continued_data_site_ids']}),
            executed_site_ids=sorted({id for v in variants.values() for id in v['executed_site_ids']}),
            totalized_original_data_site_ids=sorted({id for v in variants.values() for id in v['totalized_original_data_site_ids']}),
            totalized_original_data_obligations=sum(v['totalized_original_data_obligations'] for v in variants.values()),
            all_original_expressions_evaluated=all(v['all_original_expressions_evaluated'] for v in variants.values()))
    projected.update({path: dict(v, full_original_assertion_capture_complete=True) for path, v in rtf_summaries.items() if path in selected})
    if digest(qjs.read_bytes()) != bindings['engine_sha256'] or Path(args.bindings).read_bytes() != bindings_raw or Path(args.raw_full_run).read_bytes() != raw_receipt_bytes:
        errors.append(dict(reason='Engine, current bindings or preserved raw full receipt changed'))
    covered = set(projected)
    complete = not STOP_REQUESTED and not errors and covered == selected
    summary = dict(schema='native-original-assertion-capture-summary-v2', prepared_prior_observed_new_scope=116,
        current_selected_paths=sorted(selected), prepared_non_rtf_source_scope=103, separate_oracle_source_scope=2,
        interrupted=STOP_REQUESTED, partial_variant_summaries=summaries,
        fixture_summaries=projected, current_raw_full_run=raw_full_run, current_raw_full_run_receipt_sha256=digest(raw_receipt_bytes),
        full_capture_complete=complete, errors=errors, engine_sha256=bindings['engine_sha256'],
        source_tree_sha256=bindings['source_tree_sha256'], profile_sha256=bindings['profile_sha256'], packet_seal_sha256=bindings['packet_seal_sha256'],
        semantic_adjudication_complete=False, conformance_accepted=False)
    write_json(output / 'CAPTURE-SUMMARY.json', summary)
    print(json.dumps(dict(full_capture_complete=complete, complete_files=len(projected), blocked_commands=len(errors), summary=str(output / 'CAPTURE-SUMMARY.json'))))
    return 0 if complete else 1


if __name__ == '__main__':
    raise SystemExit(main())
