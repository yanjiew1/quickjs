#!/usr/bin/env python3
"""Independently rerender and revalidate practical traces for the existing gate.

This projection preserves both variants and binds a gate view to each actual
raw failure tuple. Per-file normative and semantic review remains separate.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import sys

# Isolated Python omits the script directory from its import search path.
# Load only helpers next to this authenticated validator.
sys.path.insert(0, str(Path(__file__).resolve().parent))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(value, reason):
    if not value:
        raise ValueError(reason)


def read_json(path):
    return json.loads(Path(path).read_bytes())



# These are TOP-LEVEL envelope exclusions only. Every other field and array/key
# order is retained, including unknown nested data and typed scalar encodings.
EVENT_ENVELOPE_FIELDS = {'engine_sha256', 'source_tree_sha256', 'config_sha256'}
CONFIG_ENVELOPE_FIELDS = {'engine_sha256', 'source_tree_sha256', '_config_sha256'}


def strict_json(text):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'Duplicate JSON field in complete observation')
            result[key] = value
        return result
    def invalid_constant(value):
        raise ValueError('Non-finite JSON scalar in complete observation: ' + value)
    return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid_constant)


def stable_digest(value, sort_keys=False):
    return digest(json.dumps(value, ensure_ascii=True, separators=(',', ':'),
                             sort_keys=sort_keys, allow_nan=False).encode())


def stable_guard_envelopes(records, config, bindings):
    """After complete fixture validation, separate exact guard row identity only.

    This uses authenticated CURRENT-BINDINGS and preserves all API/proof data.
    The source-file hash map is an explicit row identity envelope, authenticated
    in full before exclusion. Unknown properties elsewhere stay in the hash.
    """
    guarded = {sid: site['premise_guard'] for sid, site in config['sites'].items()
               if site.get('premise_guard')}
    if not guarded:
        return records, config
    binding = config.get('premise_bindings')
    require(isinstance(binding, dict), 'Missing guard identity envelope')
    review = binding.get('source_guard_review')
    require(isinstance(review, dict) and
            review == bindings.get('premise_source_guard_review') and
            review.get('schema') == 'native-premise-root-source-guard-review-v1' and
            review.get('source_review_complete') is True and
            review.get('semantic_adjudication_complete') is False and
            review.get('conformance_accepted') is False,
            'Guard source scope is not the authenticated current review')
    actual_review_hash = stable_digest(review, sort_keys=True)
    require(actual_review_hash == binding.get('root_guard_review_sha256') ==
            bindings.get('premise_source_guard_review_sha256'),
            'Guard current review digest differs')
    for key in ('engine_sha256', 'source_tree_sha256', 'profile_sha256'):
        require(isinstance(config.get(key), str) and
                re.fullmatch(r'[0-9a-f]{64}', config[key]) and
                config[key] == binding.get(key) == review.get(key) == bindings.get(key),
                'Guard current identity differs: ' + key)
    require(config.get('profile') == bindings.get('profile') and
            review.get('profile_canonical_sha256') ==
            stable_digest(config['profile'], sort_keys=True),
            'Guard complete effective profile differs')
    approved = review.get('approved_source_scope_sha256')
    require(isinstance(approved, str) and re.fullmatch(r'[0-9a-f]{64}', approved),
            'Missing authenticated static source scope identity')
    files = binding.get('current_source_files')
    require(isinstance(files, dict) and files and
            all(isinstance(name, str) and name and isinstance(pin, str) and
                re.fullmatch(r'[0-9a-f]{64}', pin) for name, pin in files.items()) and
            files == bindings.get('current_source_files') and
            digest(json.dumps(files, sort_keys=True).encode()) == config['source_tree_sha256'],
            'Guard complete current source identity map differs')
    # Precise known fields only. Profile, scope, static pins, helper hashes,
    # every proof, and every unknown review/config/binding property remain.
    stable_review = dict(review)
    stable_review['engine_sha256'] = None
    stable_review['source_tree_sha256'] = None
    stable_review_hash = stable_digest(stable_review, sort_keys=True)
    stable_binding = dict(binding)
    stable_binding.pop('engine_sha256')
    stable_binding.pop('source_tree_sha256')
    stable_binding.pop('current_source_files')
    stable_binding['source_guard_review'] = stable_review
    stable_binding['root_guard_review_sha256'] = stable_review_hash
    stable_config = dict(config)
    stable_config['premise_bindings'] = stable_binding
    def stable_records():
        for event in records:
            transformed = dict(event)
            if event.get('kind') == 'premise-guard':
                require(event.get('schema') == 'native-original-assertion-trace-v2' and
                        event.get('site') in guarded and
                        event.get('guard_id') == guarded[event['site']] and
                        event.get('root_guard_review_sha256') == actual_review_hash,
                        'Guard event current review authentication differs')
                transformed['root_guard_review_sha256'] = stable_review_hash
            yield transformed
    return stable_records(), stable_config


def stable_provider_envelope(config, bindings):
    """Separate authenticated provider row identities from semantic inputs.

    The caller has already validated the complete selected-provider preflight,
    trace and postflight proof. Keep those exact receipts in the capture; only
    the stable comparison removes the five identities that vary by CI row.
    Every TZif record, offset, decoder and metadata pin remains significant.
    """
    fixture = 'test262/test/intl402/Temporal/ZonedDateTime/links.js'
    selected = config.get('selected_provider_input')
    if selected is None:
        require(config.get('fixture') != fixture, 'Missing selected provider input')
        return config
    require(isinstance(selected, dict), 'Invalid selected provider input')
    diagnostic = bindings.get('selected_provider_diagnostic')
    require(config.get('fixture') == selected.get('fixture') == fixture and
            selected.get('schema') == 'qjs-original-selected-provider-input-v1' and
            isinstance(diagnostic, dict) and
            diagnostic.get('schema') == 'qjs-original-selected-provider-diagnostic-v1' and
            diagnostic.get('normal_policy') == 'system-first' and
            diagnostic.get('conformance_accepted') is False and
            diagnostic.get('semantic_adjudication_complete') is False and
            selected.get('fixture_sha256') == config.get('fixture_sha256') == diagnostic.get('fixture_sha256') and
            selected.get('profile_sha256') == bindings.get('profile_sha256') == diagnostic.get('profile_sha256') and
            selected.get('conformance_accepted') is False and
            selected.get('semantic_adjudication_complete') is False,
            'Unauthenticated selected provider stable envelope')
    identities = {
        'ordinary_engine_sha256': diagnostic.get('normal_engine_sha256'),
        'diagnostic_engine_sha256': diagnostic.get('engine_sha256'),
        'source_tree_sha256': diagnostic.get('source_tree_sha256'),
        'compiler_configuration_sha256': diagnostic.get('compiler_configuration_sha256'),
        'preflight_input_proof_sha256': diagnostic.get('preflight_input_proof_sha256'),
    }
    require(identities['source_tree_sha256'] == bindings.get('source_tree_sha256') == config.get('source_tree_sha256') and
            identities['diagnostic_engine_sha256'] == bindings.get('engine_sha256') == config.get('engine_sha256') and
            all(isinstance(pin, str) and re.fullmatch(r'[0-9a-f]{64}', pin) and
                selected.get(key) == pin for key, pin in identities.items()),
            'Selected provider current row identity changed')
    stable = dict(selected)
    for key in identities:
        stable[key] = None
    result = dict(config)
    result['selected_provider_input'] = stable
    return result


def validated_semantic_observation(source, config, bindings, expected_trace_sha256=None):
    """Called ONLY after validate_fixture succeeds on this exact full stdout.

    Preserve the byte-exact canonical JSON array digest without retaining the
    whole trace. EventLines enforces the same authenticated finite transport
    bounds on this second pass; all event order, keys and values remain.
    """
    # The transitive helper is authenticated by the complete runtime packet.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from premise_source_compatibility import stable_effective_config
    config = stable_effective_config(config, bindings)
    sys.path.insert(0, str(Path(__file__).resolve().parent / 'original-assertions'))
    from trace_stream import EventLines
    lines = EventLines(source, config, parser=strict_json)
    def original_events():
        for event in lines:
            require(isinstance(event, dict),
                    'Missing complete ordered semantic observations')
            yield event
    records, config = stable_guard_envelopes(original_events(), config, bindings)
    event_hash = hashlib.sha256(b'[')
    count = 0
    for event in records:
        if count:
            event_hash.update(b',')
        stable = {key: value for key, value in event.items()
                  if key not in EVENT_ENVELOPE_FIELDS}
        event_hash.update(json.dumps(stable, ensure_ascii=True,
                          separators=(',', ':'), allow_nan=False).encode())
        count += 1
    require(count > 0 and lines.exhausted,
            'Missing complete ordered semantic observations')
    if expected_trace_sha256 is not None:
        require(isinstance(expected_trace_sha256, str) and
                re.fullmatch(r'[0-9a-f]{64}', expected_trace_sha256),
                'Missing authenticated semantic trace identity')
        lines.verify(expected_trace_sha256)
    event_hash.update(b']')
    config = stable_provider_envelope(config, bindings)
    stable_config = {key: value for key, value in config.items()
                     if key not in CONFIG_ENVELOPE_FIELDS}
    return dict(events_sha256=event_hash.hexdigest(), event_count=count,
                stable_configuration_sha256=stable_digest(stable_config))


def current_row_evidence(trace, review_binding, load_bytes, helper_derivation=None,
                         correction=None):
    """Derive a row receipt; never rewrite or manufacture an independent review."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from native_profile_gate import bound_json, semantic_row_evidence
    review = bound_json(review_binding, load_bytes)
    return semantic_row_evidence(trace, review_binding, review, helper_derivation, correction)


def project_variants(variants, raw_identity):
    """Keep raw-selected identity while covering every validated original variant."""
    require(set(variants) == {'strict', 'nonstrict'}, 'Missing original variant')
    selected = 'strict' if raw_identity and raw_identity['strict'] else 'nonstrict'
    for name, item in variants.items():
        require(item.get('variant') == name and item.get('fixture_capture_complete') is True and
                item.get('original_eof_reached') is True and item.get('mandatory_failed_site_ids') == [],
                'Original variant incomplete or mandatory failure')
    trace = dict(variants[selected], variant_summaries=variants)
    trace.update(
        continued_data_site_ids=sorted({site for item in variants.values()
                                       for site in item['continued_data_site_ids']}),
        totalized_original_data_site_ids=sorted({site for item in variants.values()
                                                for site in item['totalized_original_data_site_ids']}),
        totalized_original_data_obligations=sum(item['totalized_original_data_obligations']
                                                for item in variants.values()),
        data_mismatches=sum(item['data_mismatches'] for item in variants.values()),
        all_original_expressions_evaluated=all(item['all_original_expressions_evaluated'] is True
                                               for item in variants.values()),
        current_capture_binding=dict(capture_kind='script-variants',
            variant_bindings={name: {key: item[key] for key in
                ('engine_sha256', 'trace_sha256', 'generated_sha256', 'config_sha256')}
                for name, item in sorted(variants.items())}))
    return trace


def require_projection_identity(config, bindings, raw_identity):
    """Bind a pinned historical map or the exact authenticated current checkout."""
    mode = config.get('source_binding_mode', 'pinned-production-map')
    require(mode in ('pinned-production-map', 'current-checkout'),
            'Unsupported projection source binding mode')
    source_map = bindings.get('source_tree_sha256')
    require(isinstance(source_map, str) and re.fullmatch(r'[0-9a-f]{64}', source_map),
            'Missing current projection source map')
    if mode == 'current-checkout':
        historical = config.get('historical_baseline_production_source_map_sha256')
        require(config.get('production_source_map_sha256') is None and
                isinstance(historical, str) and re.fullmatch(r'[0-9a-f]{64}', historical),
                'Invalid current-checkout projection template')
        require(isinstance(raw_identity, dict) and
                raw_identity.get('production_source_map_sha256') == source_map and
                raw_identity.get('actual_full_tracked_source_map_sha256') == source_map and
                bindings.get('production_source_map_sha256') == source_map and
                bindings.get('actual_full_tracked_source_map_sha256') == source_map and
                isinstance(raw_identity.get('actual_initialized_checkout_commit'), str) and
                re.fullmatch(r'[0-9a-f]{40}', raw_identity['actual_initialized_checkout_commit']) and
                all(bindings.get(key) == value for key, value in raw_identity.items()),
                'Current projection identity differs from authenticated raw checkout')
    else:
        require(source_map == config.get('production_source_map_sha256'),
                'Current projection source differs from pinned production map')
    require(bindings.get('engine_sha256') == raw_identity.get('qjs_sha256') and
            source_map == raw_identity.get('production_source_map_sha256'),
            'Current production/engine differs from raw full run')


def main():
    def review_bytes(name):
        path = Path(name)
        require(not path.is_absolute() and '..' not in path.parts and path.is_file()
                and not path.is_symlink(), 'Unsafe stable review source')
        return path.read_bytes()

    parser = argparse.ArgumentParser()
    for option in ('config', 'bindings', 'raw-full-run', 'capture', 'summary'):
        parser.add_argument('--' + option, required=True, type=Path)
    args = parser.parse_args()
    config = read_json(args.config)
    bindings = read_json(args.bindings)
    from helper_source_derivation import authenticate_helper_derivation
    helper_derivation = authenticate_helper_derivation(
        bindings.get('current_source_files', {}), bindings['source_tree_sha256'], review_bytes)
    from native_correction_transition import pending_capture
    correction_pending = pending_capture(bindings)
    correction = None
    if not correction_pending:
        from native_correction_transition import authenticate_correction
        correction = authenticate_correction(bindings.get('current_source_files', {}), review_bytes)
    parsed = read_json(args.raw_full_run)
    root = args.capture.resolve()
    packet = Path(config['packet_root']).resolve()
    require(config.get('packet_seal_sha256') == bindings.get('packet_seal_sha256') ==
            digest((packet / 'SEAL.json').read_bytes()), 'Changed practical packet seal')
    for name, expected in read_json(packet / 'SEAL.json')['files'].items():
        path = (packet / name).resolve()
        require(path.is_relative_to(packet) and path.is_file() and
                digest(path.read_bytes()) == expected, 'Changed authenticated packet file: ' + name)
    require(read_json(root / 'CURRENT-BINDINGS.json') == bindings, 'Capture current bindings differ')
    require_projection_identity(config, bindings, parsed['input_identity'])
    require(parsed.get('raw_collection_complete') is True and parsed.get('raw_parser_complete') is True and
            parsed.get('sealed_inputs_unchanged') is True and not parsed.get('blocking_tool_records') and
            not parsed.get('unknown_feature_records'), 'Incomplete raw full evidence')
    normal = read_json(root / 'NORMAL-RAW-FULL-RUN-RECEIPT.json')
    require(normal['raw_preserved'] is True and normal['returncode'] == parsed['raw_runner_returncode'] and
            normal['counters'] == parsed['counters'] and
            normal['parsed_raw_evidence_sha256'] == digest(args.raw_full_run.read_bytes()),
            'Normal full receipt differs from exact parsed raw evidence')
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from premise_source_compatibility import install
    install(packet)
    sys.path.insert(0, str(packet))
    from selected_timezone_orchestration import (TZ_FIXTURE, fixture_bindings,
        authenticated_input, verify_actual, comparison_role)
    from render_practical_witness import render
    from validate_practical_witness import validate_fixture
    from validate_current_rtf import validate_current_rtf
    plan = read_json(packet / 'HOSTED-RENDER-PLAN.json')
    for fixture in plan['fixtures'] + plan.get('oracle_fixtures', []):
        require(digest(Path(fixture['path']).read_bytes()) == fixture['official_original_sha256'],
                'Current original fixture differs from authenticated witness')
        for harness in fixture['harness']:
            require(digest((Path('test262/harness') / harness['name']).read_bytes()) == harness['sha256'],
                    'Current original harness differs from authenticated witness')
    ordinary = {f['path'] for f in plan['fixtures'] + plan.get('oracle_fixtures', [])}
    rtf = set(plan['existing_relative_time_capture']['paths'])
    selected = ordinary | rtf
    require(len(ordinary) == 105 and len(rtf) == 13 and len(selected) == 118,
            'Complete original103 + oracle2 + RTF13 scope differs')
    regenerated = render(packet, bindings, root / 'independent-rerender', sorted(ordinary))
    manifest = read_json(root / 'GENERATED-MANIFEST.json')
    require(manifest == regenerated and len(manifest) == 210,
            'Complete independently rendered original variants differ')
    source_bindings = {name: digest((packet / name).read_bytes()) for name in (
        'render_practical_witness.py', 'launch_practical_witness.py',
        'validate_practical_witness.py', 'validate_current_rtf.py', 'trace_stream.py',
        'selected_timezone_orchestration.py', 'selected_timezone_proof.py',
        'witness-runtime.js', 'HOSTED-RENDER-PLAN.json',
        'rtf-reviewed-sampler/capture-relative-diagnostic.js')}
    source_bindings.update({name: digest((Path(__file__).resolve().parent / name).read_bytes())
        for name in ('project_original_assertions.py', 'native_profile_gate.py', 'run.py',
                     'premise_source_compatibility.py', 'premise-runner-compatibility.json',
                     'launch_current_premise_witness.py')})
    for fixture in plan['fixtures'] + plan.get('oracle_fixtures', []):
        source_bindings[fixture['path']] = digest(Path(fixture['path']).read_bytes())
        for harness in fixture['harness']:
            name = 'test262/harness/' + harness['name']
            source_bindings[name] = digest(Path(name).read_bytes())
    from launch_current_premise_witness import load_execution_budget, require_budget_receipt
    execution_budget = load_execution_budget(bindings)
    summaries, semantic_observations = {}, {}
    for i, item in enumerate(regenerated):
        generated = root / 'rendered' / item['generated_path']
        require(generated.read_bytes() == (root / 'independent-rerender' / item['generated_path']).read_bytes(),
                'Generated practical source differs from independent render')
        command = read_json(root / ('fixture-' + str(i) + '.command.json'))
        require(command.get('output_limited') is False and command.get('error') is None and
                command['stderr_sha256'] == digest(b'') and
                (root / ('fixture-' + str(i) + '.stderr.log')).read_bytes() == b'',
                'Unknown, limited or incomplete original variant output')
        require_budget_receipt(command, execution_budget, item['config'])
        child_bindings = fixture_bindings(bindings, item['path'])
        engine = bindings['selected_provider_diagnostic']['qjs'] if item['path'] == TZ_FIXTURE else config['qjs']
        require(command['argv'] == [str(Path(engine).resolve()), str(generated.resolve())] and
                command['generated_sha256'] == item['generated_sha256'],
                'Current qjs/generated argv differs')
        trace_path = root / ('fixture-' + str(i) + '.stdout.log')
        result = validate_fixture(trace_path, item['config'], command, normal)
        if item['path'] == TZ_FIXTURE:
            proof = verify_actual(packet, bindings, item['config'], command, root / ('selected-provider-' + str(i)), trace_path)
            require(read_json(root / ('selected-provider-' + str(i) + '-comparison.json')) == proof,
                    'Actual selected provider comparison proof differs')
            result['selected_provider_diagnostic_role'] = comparison_role(item['config'], proof)
        summaries.setdefault(item['path'], {})[item['variant']] = result
        require(result['trace_sha256'] == command['stdout_sha256'],
                'Validated trace digest differs from command')
        semantic_observations.setdefault(item['path'], {})[item['variant']] = \
            validated_semantic_observation(trace_path, item['config'], child_bindings,
                                           command['stdout_sha256'])
    command = read_json(root / 'rtf-current.command.json')
    require_budget_receipt(command, execution_budget)
    require(command.get('output_limited') is False and command.get('error') is None and
            command['stderr_sha256'] == digest(b'') and (root / 'rtf-current.stderr.log').read_bytes() == b'' and
            command['argv'] == [str(Path(config['qjs']).resolve()),
                                str(packet / 'rtf-reviewed-sampler/capture-relative-diagnostic.js')],
            'Unknown or changed current RTF command')
    rtf_summaries = validate_current_rtf(packet, (root / 'rtf-current.stdout.log').read_text(),
                                         command, bindings, normal)
    # validate_current_rtf above still compares the WHOLE parsed RTF object.
    complete_rtf_sha256 = stable_digest(strict_json((root / 'rtf-current.stdout.log').read_text()),
                                       sort_keys=True)
    review_bindings = config.get('stable_review_bindings', {})
    require(isinstance(review_bindings, dict) and set(review_bindings) <= selected,
            'Unknown stable original review fixture')
    failures = [r for r in parsed['raw_observations'] if r['status'] == 'observed-failure']
    oracle = read_json(args.raw_full_run.parent / 'native-oracle-actual.json')
    require(oracle['source'] == parsed['input_identity'] and
            oracle['profile_sha256'] == bindings['profile_sha256'], 'Oracle current source/profile differs')
    for evidence in oracle['parsed'].values():
        require(evidence['raw_collection_complete'] is True and evidence['raw_parser_complete'] is True and
                evidence['sealed_inputs_unchanged'] is True and not evidence['blocking_tool_records'] and
                not evidence['unknown_feature_records'], 'Incomplete original oracle raw evidence')
        failures.extend(r for r in evidence['raw_observations'] if r['status'] == 'observed-failure')
    by_path = {}
    for record in failures:
        require(record['path'] in selected and record['path'] not in by_path,
                'New or duplicate unclassified original raw failure')
        require(type(record.get('strict')) is bool and type(record.get('line')) is int and
                record['line'] > 0 and isinstance(record.get('signature'), str),
                'Missing exact original raw failure tuple')
        by_path[record['path']] = {k: record[k] for k in ('path', 'strict', 'line', 'signature')}
    projected = {}
    for path in sorted(selected):
        raw_identity = by_path.get(path)
        if path in summaries:
            trace = project_variants(summaries[path], raw_identity)
        else:
            trace = dict(rtf_summaries[path])
            trace['current_capture_binding'] = dict(capture_kind='rtf-variant-neutral',
                engine_sha256=trace['engine_sha256'], trace_sha256=trace['trace_sha256'],
                sampler_sha256=trace['generated_sha256'],
                command_sha256=digest((root / 'rtf-current.command.json').read_bytes()),
                current_bindings_sha256=digest((root / 'CURRENT-BINDINGS.json').read_bytes()))
        require(trace['mandatory_failed_site_ids'] == [] and trace['original_eof_reached'] is True,
                'Original mandatory failures or incomplete EOF')
        trace.update(raw_failure_identity=raw_identity, mandatory_failures=[],
            totalized_original_data_obligation_count=trace['totalized_original_data_obligations'],
            totalized_original_data_obligations=trace['totalized_original_data_site_ids'])
        trace['stable_observation'] = dict(
            schema='native-complete-stable-observation-v2',
            capture_kind=trace['current_capture_binding']['capture_kind'],
            source_bindings=dict(source_bindings),
            observations=(semantic_observations[path] if path in summaries else
                          dict(whole_parsed_rtf_sha256=complete_rtf_sha256)))
        if path in review_bindings and not correction_pending:
            trace['current_row_evidence'] = current_row_evidence(trace, review_bindings[path],
                review_bytes, helper_derivation, correction)
        projected[path] = trace
    require(set(projected) == selected, 'Incomplete original scope projection')
    result = dict(schema='native-original-assertion-capture-summary-v2',
        full_capture_complete=True, independent_revalidation_complete=True,
        fixture_summaries=projected, complete_original_source_count=118,
        complete_original_default_variant_count=210, current_RTF_recaptured=True,
        engine_sha256=bindings['engine_sha256'], source_tree_sha256=bindings['source_tree_sha256'],
        profile_sha256=bindings['profile_sha256'], packet_seal_sha256=bindings['packet_seal_sha256'],
        raw_full_evidence_sha256=digest(args.raw_full_run.read_bytes()),
        semantic_adjudication_complete=False, conformance_accepted=False, failure_waiver=False)
    if correction_pending:
        result['correction_observation_review_pending'] = True
    args.summary.write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
