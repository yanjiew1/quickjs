"""Typed diagnostic provider inputs for the one original timezone-link fixture.

No subprocess is launched here. The owning hosted runner/launcher supplies
closed command receipts. Expectations come only from the independent TZif
parser and the exact recorder bytes, never an engine offset result.
Copyright (c) 2026 Yan-Jie Wang. MIT license.
"""
import hashlib
import json
import os
import re
from datetime import datetime
from pathlib import Path
from selected_timezone_proof import EPOCH_NS, read_capture, compare_actual

TZ_FIXTURE = 'test262/test/intl402/Temporal/ZonedDateTime/links.js'
INPUT_SCHEMA = 'qjs-original-selected-provider-input-v1'
DIAG_SCHEMA = 'qjs-original-selected-provider-diagnostic-v1'


def require(value, reason):
    if not value:
        raise ValueError(reason)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return sha(json.dumps(value, sort_keys=True, ensure_ascii=True,
                          separators=(',', ':')).encode())


def file_sha(path):
    return sha(Path(path).read_bytes())


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def fresh_directory(path, owner):
    path, owner = Path(path), Path(owner).resolve()
    require(path.is_absolute() and path.parent.resolve().is_relative_to(owner),
            'Provider directory outside owned output')
    require(not path.exists() and not path.is_symlink(), 'Provider directory is not fresh')
    parent = path.parent
    while parent != owner:
        require(parent.is_dir() and not parent.is_symlink() and
                parent.stat().st_uid == os.getuid(), 'Unowned provider parent')
        parent = parent.parent
    require(owner.is_dir() and not owner.is_symlink() and
            owner.stat().st_uid == os.getuid(), 'Unowned provider output')
    path.mkdir(mode=0o700)
    return path


def primary_map(packet):
    data = json.loads((Path(packet) / 'PREMISE-DATA.json').read_bytes())
    records = data['timezoneRecords']
    labels = {name: row['primary'] for name, row in records.items()}
    require(len(labels) == 598 and len(set(labels.values())) == 450 and
            all(labels.get(primary) == primary for primary in labels.values()),
            'Original generated timezone identity inventory differs')
    return labels, records


def required_labels(packet, fixture_sha256):
    source = Path(packet) / 'immutable-original-inputs/test/intl402/Temporal/ZonedDateTime/links.js'
    raw = source.read_bytes()
    require(sha(raw) == fixture_sha256, 'Original link fixture differs')
    labels, _ = primary_map(packet)
    # This exact authenticated fixture has simple JSON string literals and
    # line comments. No arbitrary JavaScript expression is evaluated here.
    body = re.sub(r'//[^\n]*', '', raw.decode())
    found = {json.loads(token) for token in re.findall(r'"(?:\\.|[^"\\])*"', body)}
    found &= set(labels)
    require('UTC' in found and 'America/Rainy_River' in found and
            'America/Winnipeg' in found, 'Original finite timezone literals missing')
    return found


def validate_diagnostic(diagnostic, bindings):
    require(isinstance(diagnostic, dict) and diagnostic.get('schema') == DIAG_SCHEMA,
            'Missing typed diagnostic provider role')
    require(diagnostic.get('normal_engine_sha256') == bindings['engine_sha256'] and
            diagnostic.get('source_tree_sha256') == bindings['source_tree_sha256'] and
            diagnostic.get('profile_sha256') == bindings['profile_sha256'] and
            diagnostic.get('normal_policy') == 'system-first' and
            diagnostic.get('conformance_accepted') is False and
            diagnostic.get('semantic_adjudication_complete') is False,
            'Provider diagnostic changed ordinary source/profile/policy')
    qjs = Path(diagnostic['qjs'])
    require(qjs.is_absolute() and qjs.is_file() and not qjs.is_symlink() and
            file_sha(qjs) == diagnostic['engine_sha256'] and
            diagnostic['engine_sha256'] != bindings['engine_sha256'],
            'Diagnostic binary is not independently identified')
    require(Path(diagnostic['probe']).is_file() and
            file_sha(diagnostic['probe']) == diagnostic['probe_sha256'],
            'Preflight probe binary changed')
    compiler = json.loads(Path(diagnostic['compiler_configuration']).read_bytes())
    require(file_sha(diagnostic['compiler_configuration']) ==
            diagnostic['compiler_configuration_sha256'] and
            compiler.get('diagnostic_configuration') == {'CONFIG_TZ_CAPTURE_SNAPSHOT': 'y',
                'CONFIG_ICU': 'n', 'normal_policy': 'system-first'} and
            compiler.get('ordinary_engine_sha256') == bindings['engine_sha256'] and
            compiler.get('source_tree_sha256') == bindings['source_tree_sha256'] and
            compiler.get('diagnostic_engine_sha256') == diagnostic['engine_sha256'] and
            compiler.get('probe_sha256') == diagnostic['probe_sha256'] and
            compiler.get('make_args', []).count('CONFIG_TZ_CAPTURE_SNAPSHOT=y') == 1 and
            'CONFIG_ICU=n' in compiler.get('make_args', []),
            'Diagnostic compiler configuration changed')
    identity = json.loads(Path(diagnostic['execution_identity']).read_bytes())
    require(file_sha(diagnostic['execution_identity']) == diagnostic['execution_identity_sha256'] and
            identity['candidate_sha'] == bindings['actual_initialized_checkout_commit'] and
            identity['actual_production_source_map_sha256'] == bindings['source_tree_sha256'] and
            str(identity['run_id']) == diagnostic['hosted_identity']['run_id'] and
            str(identity['run_attempt']) == diagnostic['hosted_identity']['run_attempt'],
            'Provider diagnostic hosted attempt/source identity differs')
    return diagnostic


def prepare_input(packet, bindings, diagnostic, command, capture_directory):
    labels, embedded = primary_map(packet)
    capture_directory = Path(capture_directory)
    require(str(capture_directory) == diagnostic['preflight_directory'] and
            capture_directory.is_absolute() and capture_directory.is_dir() and
            not capture_directory.is_symlink() and capture_directory.stat().st_uid == os.getuid(),
            'Preflight provider directory changed')
    require(command['argv'][0] == diagnostic['probe'] and
            command['argv'][1:] == sorted(set(labels.values())),
            'Preflight primary arguments differ')
    require(command.get('observer_environment', {}).get('QJS_TZ_CAPTURE_DIRECTORY') ==
            str(Path(capture_directory).resolve()) and
            canonical(command['observer_environment']) == command['observer_environment_sha256'] and
            re.fullmatch(r'[0-9a-f]{64}', command['effective_env_sha256']) is not None,
            'Preflight observer environment differs')
    proof = read_capture(capture_directory, command, labels, diagnostic['system_root'])
    require(set(proof['selected_primaries']) == set(labels.values()),
            'Preflight does not cover all authenticated primaries')
    compare_actual(proof, proof, set(labels.values()), embedded)
    proof.update(role='preflight-selected-provider-input-proof-v1',
        fixture_sha256=diagnostic['fixture_sha256'],
        primary_metadata_sha256=canonical(labels),
        decoder_sha256=file_sha(Path(__file__).with_name('selected_timezone_proof.py')),
        source_tree_sha256=bindings['source_tree_sha256'],
        ordinary_engine_sha256=bindings['engine_sha256'],
        diagnostic_engine_sha256=diagnostic['engine_sha256'],
        profile_sha256=bindings['profile_sha256'],
        compiler_configuration_sha256=diagnostic['compiler_configuration_sha256'],
        execution_identity_sha256=diagnostic['execution_identity_sha256'],
        hosted_identity=diagnostic['hosted_identity'],
        system_root=diagnostic['system_root'], capture_directory=str(capture_directory),
        command_receipt=diagnostic['preflight_command'],
        command_file_sha256=file_sha(diagnostic['preflight_command']),
        conformance_accepted=False, semantic_adjudication_complete=False)
    return proof


def authenticated_input(packet, bindings):
    diagnostic = validate_diagnostic(bindings.get('selected_provider_diagnostic'), bindings)
    raw = Path(diagnostic['preflight_input_proof']).read_bytes()
    require(sha(raw) == diagnostic['preflight_input_proof_sha256'],
            'Selected provider proof changed after sealing')
    proof = json.loads(raw)
    command = json.loads(Path(diagnostic['preflight_command']).read_bytes())
    rebuilt = prepare_input(packet, bindings, diagnostic, command, proof['capture_directory'])
    require(proof == rebuilt, 'Preflight proof does not match exact selected raw bytes')
    labels, _ = primary_map(packet)
    fixture = next(f for f in json.loads((Path(packet) / 'HOSTED-RENDER-PLAN.json').read_bytes())['fixtures']
                   if f['path'] == TZ_FIXTURE)
    require(proof['fixture_sha256'] == fixture['official_original_sha256'] and
            proof['primary_metadata_sha256'] == canonical(labels),
            'Provider proof fixture or generated identity differs')
    records = {label: dict(proof['selected_primaries'][primary]) for label, primary in labels.items()}
    return dict(schema=INPUT_SCHEMA, fixture=TZ_FIXTURE,
        preflight_input_proof_sha256=sha(raw), diagnostic_engine_sha256=diagnostic['engine_sha256'],
        ordinary_engine_sha256=bindings['engine_sha256'],
        profile_sha256=bindings['profile_sha256'], source_tree_sha256=bindings['source_tree_sha256'],
        fixture_sha256=proof['fixture_sha256'], primary_metadata_sha256=canonical(labels),
        decoder_sha256=proof['decoder_sha256'],
        compiler_configuration_sha256=proof['compiler_configuration_sha256'],
        system_root=proof['system_root'], records=records,
        epoch_nanoseconds=list(EPOCH_NS), conformance_accepted=False,
        semantic_adjudication_complete=False)


def fixture_bindings(bindings, fixture):
    if fixture != TZ_FIXTURE:
        return bindings
    diagnostic = validate_diagnostic(bindings.get('selected_provider_diagnostic'), bindings)
    result = dict(bindings, engine_sha256=diagnostic['engine_sha256'])
    review = dict(bindings['premise_source_guard_review'], engine_sha256=diagnostic['engine_sha256'])
    result['premise_source_guard_review'] = review
    result['premise_source_guard_review_sha256'] = canonical(review)
    return result


def validate_config(config):
    selected = config.get('selected_provider_input')
    if selected is None:
        require(config.get('fixture') != TZ_FIXTURE,
                'Timezone-link fixture lacks authenticated selected provider input')
        return
    require(config['fixture'] == TZ_FIXTURE and selected['schema'] == INPUT_SCHEMA and
            selected['fixture'] == config['fixture'] and
            selected['fixture_sha256'] == config['fixture_sha256'] and
            selected['diagnostic_engine_sha256'] == config['engine_sha256'] and
            selected['source_tree_sha256'] == config['source_tree_sha256'] and
            selected['profile_sha256'] == config['profile_sha256'] and
            selected['epoch_nanoseconds'] == list(EPOCH_NS) and
            selected['conformance_accepted'] is False and
            selected['semantic_adjudication_complete'] is False,
            'Selected provider scope or diagnostic identity differs')
    static = config['premise_data']['timezoneRecords']
    records = selected['records']
    require(set(records) == set(static) and
            canonical({name: row['primary'] for name, row in static.items()}) ==
            selected['primary_metadata_sha256'], 'Generated primary identity changed')
    for label, row in records.items():
        require(row['primary'] == static[label]['primary'] and
                row['origin'] in ('system', 'embedded') and
                type(row.get('source_status')) is int and 0 <= row['source_status'] <= 4 and
                (row['origin'] == 'system') == (row['source_status'] == 0) and
                row['system_root'] == selected['system_root'] and
                set(row['offsets']) == set(EPOCH_NS) and
                all(type(value) is int and -86400 * 10**9 < value < 86400 * 10**9
                    for value in row['offsets'].values()) and
                isinstance(row['tzif_sha256'], str) and
                re.fullmatch(r'[0-9a-f]{64}', row['tzif_sha256']),
                'Malformed selected provider record')
        if row['origin'] == 'embedded':
            require(row['tzif_sha256'] == static[label]['tzif_sha256'] and
                    row['offsets'] == static[label]['offsets'],
                    'Selected fallback differs from pinned embedded bytes')


def verify_actual(packet, bindings, config, command, capture_directory, trace_path):
    validate_config(config)
    selected = authenticated_input(packet, bindings)
    require(config['selected_provider_input'] == selected, 'Rendered input differs from sealed preflight')
    diagnostic = bindings['selected_provider_diagnostic']
    require(command['argv'][0] == diagnostic['qjs'] and
            command['engine_sha256'] == diagnostic['engine_sha256'] and
            command['preflight_input_proof_sha256'] == diagnostic['preflight_input_proof_sha256'] and
            command.get('observer_environment', {}).get('QJS_TZ_CAPTURE_DIRECTORY') ==
                str(Path(capture_directory).resolve()) and
            canonical(command['observer_environment']) == command['observer_environment_sha256'] and
            re.fullmatch(r'[0-9a-f]{64}', command['effective_env_sha256']) is not None,
            'Actual observer command or environment differs')
    labels, embedded = primary_map(packet)
    capture_directory = Path(capture_directory)
    require(capture_directory.is_absolute() and capture_directory.is_dir() and
            not capture_directory.is_symlink() and capture_directory.stat().st_uid == os.getuid(),
            'Actual provider directory changed')
    actual = read_capture(capture_directory, command, labels, diagnostic['system_root'])
    probe = json.loads(Path(diagnostic['preflight_input_proof']).read_bytes())
    required = set()
    compare_actual(probe, actual, set(actual['selected_primaries']), embedded)
    # Revalidate the full native trace first in the caller. Then explicitly
    # bind every guarded original offset/identity constructor to a captured
    # primary. Name-only constructors do not read TZif bytes and need no
    # fictitious snapshot. No engine offset becomes an expectation.
    constructors = {}
    guarded_constructors = set()
    from trace_stream import EventLines
    from premise_trace_proof import decode
    trace = EventLines(Path(trace_path), config)
    for event in trace:
        site = config['sites'].get(event.get('site'), {})
        # An enclosing assertion source also contains its nested constructor
        # text. Only typed constructor exits carry constructor arguments.
        if (event.get('kind') == 'exit' and event.get('completion') == 'normal' and
                event.get('operation') == 'constructor' and
                site.get('kind') == 'constructor' and
                re.search(r'new\s+Temporal\.ZonedDateTime', site.get('source', ''))):
            arguments = event.get('arguments', [])
            require(len(arguments) == 2, 'Original constructor argument coverage differs')
            label = decode(arguments[1])
            require(label in labels, 'Original constructor label outside generated inventory')
            constructors[event['result_event']] = label
        if event.get('kind') == 'premise-guard' and event.get('guard_id') == 'tz-primary-identity':
            evidence = decode(event['evidence'])
            ids = evidence['proof']['original_arguments']
            require(len(ids) == 2, 'Missing original offset constructor references')
            for constructor, label in zip(ids, (evidence['link'], evidence['zone'])):
                require(constructors.get(constructor) == label and
                        labels[label] in actual['selected_primaries'],
                        'Original offset/identity constructor lacks retained-byte snapshot')
                required.add(labels[label])
                guarded_constructors.add(constructor)
    trace.verify(command['stdout_sha256'])
    require(constructors and guarded_constructors and required,
            'No original guarded timezone constructor receipts')
    compare_actual(probe, actual, required, embedded)
    return dict(schema='qjs-selected-provider-comparison-v1', fixture=TZ_FIXTURE,
        variant=config['variant'], preflight_input_proof_sha256=diagnostic['preflight_input_proof_sha256'],
        actual_finite_proof=actual, constructor_receipts=len(constructors),
        snapshot_bound_guarded_constructor_receipts=len(guarded_constructors),
        required_primaries=sorted(required), actual_input_matches_preflight=True,
        command_file_sha256=file_sha(command['command_receipt']),
        config_sha256=config['_config_sha256'], trace_sha256=file_sha(trace_path),
        diagnostic_engine_sha256=diagnostic['engine_sha256'],
        ordinary_engine_sha256=bindings['engine_sha256'],
        source_tree_sha256=bindings['source_tree_sha256'], profile_sha256=bindings['profile_sha256'],
        execution_identity_sha256=diagnostic['execution_identity_sha256'],
        hosted_identity=diagnostic['hosted_identity'],
        conformance_accepted=False, semantic_adjudication_complete=False)


def comparison_role(config, proof):
    selected = config['selected_provider_input']
    return dict(schema='qjs-selected-provider-trace-role-v1', fixture=TZ_FIXTURE,
        ordinary_engine_sha256=selected['ordinary_engine_sha256'],
        diagnostic_engine_sha256=selected['diagnostic_engine_sha256'],
        source_tree_sha256=selected['source_tree_sha256'],
        profile_sha256=selected['profile_sha256'], fixture_sha256=selected['fixture_sha256'],
        compiler_configuration_sha256=selected['compiler_configuration_sha256'],
        preflight_input_proof_sha256=selected['preflight_input_proof_sha256'],
        actual_comparison_canonical_sha256=canonical(proof),
        conformance_accepted=False, semantic_adjudication_complete=False)


def closed_observation(command):
    """A closed command receipt is an observation, not continuous monitoring."""
    require(command.get('lifecycle_closed') is True and
            command.get('returncode') == 0 and command.get('timed_out') is False and
            command.get('interrupted') is False and command.get('output_limited') is False and
            type(command.get('pid')) is int and command['pid'] > 0 and
            command.get('process_group') == command['pid'],
            'Provider observation command did not close cleanly')
    times = []
    for field in ('started_utc', 'finished_utc'):
        require(isinstance(command.get(field), str), 'Missing observation timestamp')
        timestamp = datetime.fromisoformat(command[field])
        require(timestamp.utcoffset() is not None, 'Unzoned observation timestamp')
        times.append(timestamp)
    require(times[0] <= times[1], 'Reversed observation interval')
    return times


def postflight_receipt(packet, bindings, postflight, capture_command_path):
    """Rebuild exact pre/post raw-byte proofs around one completed capture.

    The two selected-provider observations establish unchanged endpoints.
    They do not claim continuous filesystem monitoring or general conformance.
    """
    diagnostic = validate_diagnostic(bindings.get('selected_provider_diagnostic'), bindings)
    authenticated_input(packet, bindings)
    require(isinstance(postflight, dict) and set(postflight) ==
            {'capture_directory', 'command_receipt', 'input_proof', 'input_proof_sha256'},
            'Missing exact selected provider postflight binding')
    compiler = json.loads(Path(diagnostic['compiler_configuration']).read_bytes())
    require(compiler.get('system_root') == diagnostic['system_root'] and
            compiler.get('backend') == 'native' and
            isinstance(compiler.get('compiler_path'), str) and
            file_sha(compiler['compiler_path']) == compiler.get('compiler_sha256'),
            'Postflight system root or compiler bytes differ')
    before_command = json.loads(Path(diagnostic['preflight_command']).read_bytes())
    after_command = json.loads(Path(postflight['command_receipt']).read_bytes())
    capture_command = json.loads(Path(capture_command_path).read_bytes())
    before_times = closed_observation(before_command)
    after_times = closed_observation(after_command)
    capture_times = closed_observation(capture_command)
    require(before_times[1] <= capture_times[0] <= capture_times[1] <= after_times[0],
            'Provider observations do not bracket original capture')
    for command in (before_command, after_command, capture_command):
        require(command.get('source_tree_sha256') == bindings['source_tree_sha256'] and
                command.get('profile_sha256') == bindings['profile_sha256'],
                'Observation source map or profile differs')
    require(before_command.get('engine_sha256') == after_command.get('engine_sha256') ==
            diagnostic['engine_sha256'] and
            capture_command.get('engine_sha256') == bindings['engine_sha256'] and
            capture_command.get('ordinary_engine_sha256') == bindings['engine_sha256'],
            'Observation ordinary or diagnostic engine differs')
    argv = capture_command.get('argv', [])
    require(argv.count('--qjs') == 1 and argv.index('--qjs') + 1 < len(argv),
            'Missing original ordinary engine command argument')
    ordinary = Path(argv[argv.index('--qjs') + 1])
    require(ordinary.is_absolute() and ordinary.is_file() and not ordinary.is_symlink() and
            file_sha(ordinary) == bindings['engine_sha256'], 'Ordinary engine bytes changed')
    require(after_command.get('cwd') == before_command.get('cwd') and
            isinstance(before_command.get('effective_profile_environment'), dict) and
            after_command.get('effective_profile_environment') ==
            capture_command.get('effective_profile_environment') ==
            before_command['effective_profile_environment'] and
            isinstance(before_command.get('provider_environment_sha256'), str) and
            re.fullmatch(r'[0-9a-f]{64}', before_command['provider_environment_sha256']) and
            after_command.get('provider_environment_sha256') ==
            capture_command.get('provider_environment_sha256') ==
            before_command['provider_environment_sha256'],
            'Postflight compiled tree or effective profile environment differs')
    before = json.loads(Path(diagnostic['preflight_input_proof']).read_bytes())
    derived = dict(diagnostic, preflight_directory=postflight['capture_directory'],
                   preflight_command=postflight['command_receipt'])
    after = prepare_input(packet, bindings, derived, after_command, postflight['capture_directory'])
    after['role'] = 'postflight-selected-provider-input-proof-v1'
    raw = Path(postflight['input_proof']).read_bytes()
    require(sha(raw) == postflight['input_proof_sha256'] and json.loads(raw) == after,
            'Postflight proof differs from retained actual bytes')
    labels, embedded = primary_map(packet)
    require(len(set(labels.values())) == 450 and
            set(before['selected_primaries']) == set(after['selected_primaries']) == set(labels.values()),
            'Pre/post observations lack complete primary coverage')
    compare_actual(before, after, set(labels.values()), embedded)
    return dict(schema='qjs-selected-provider-postflight-comparison-v1',
        preflight_input_proof_sha256=diagnostic['preflight_input_proof_sha256'],
        postflight_input_proof_sha256=postflight['input_proof_sha256'],
        preflight_command_sha256=file_sha(diagnostic['preflight_command']),
        postflight_command_sha256=file_sha(postflight['command_receipt']),
        original_capture_command_sha256=file_sha(capture_command_path),
        original_capture_started_utc=capture_command['started_utc'],
        original_capture_finished_utc=capture_command['finished_utc'],
        preflight_finished_utc=before_command['finished_utc'],
        postflight_started_utc=after_command['started_utc'],
        ordinary_engine_sha256=bindings['engine_sha256'],
        diagnostic_engine_sha256=diagnostic['engine_sha256'],
        probe_sha256=diagnostic['probe_sha256'], source_tree_sha256=bindings['source_tree_sha256'],
        profile_sha256=bindings['profile_sha256'],
        provider_environment_sha256=before_command['provider_environment_sha256'],
        effective_profile_environment=before_command['effective_profile_environment'],
        compiler_configuration_sha256=diagnostic['compiler_configuration_sha256'],
        execution_identity_sha256=diagnostic['execution_identity_sha256'],
        hosted_identity=diagnostic['hosted_identity'], system_root=diagnostic['system_root'],
        primary_metadata_sha256=canonical(labels), decoder_sha256=after['decoder_sha256'],
        compared_primary_count=450, selected_provider_endpoints_equal=True,
        original_capture_completed=True, continuous_monitoring=False,
        conformance_accepted=False, semantic_adjudication_complete=False)


def authenticated_postflight(packet, bindings, postflight_path, capture_command_path):
    document = json.loads(Path(postflight_path).read_bytes())
    require(isinstance(document, dict) and set(document) == {'binding', 'comparison'},
            'Missing selected provider postflight receipt')
    rebuilt = postflight_receipt(packet, bindings, document['binding'], capture_command_path)
    require(document['comparison'] == rebuilt, 'Changed selected provider postflight comparison')
    return rebuilt
