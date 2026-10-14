"""Pure offline validation of authenticated actual trace; root owns execution."""
import hashlib
import json
from collections import Counter
from guard_binding import validate_premise_config
from premise_trace_proof import PremiseProofInvalid, UNDEFINED, decode, validate_branch
from pathlib import Path
import sqlite3
from trace_stream import (EventLines, EventSet, ReceiptSet, ReceiptStore,
                          TraceBoundInvalid, MAX_STACK_DEPTH)


class WitnessInvalid(ValueError):
    pass


def sha(data):
    return hashlib.sha256(data).hexdigest()


def config_digest(config):
    return sha(json.dumps({k: v for k, v in config.items() if k != '_config_sha256'}, sort_keys=True, ensure_ascii=True, separators=(',', ':')).encode())


def validate_snapshot(value, prior_values):
    if not isinstance(value, dict):
        raise WitnessInvalid('Typed value missing')
    kind = value.get('kind')
    if kind == 'string':
        text = value.get('value')
        if not isinstance(text, str):
            raise WitnessInvalid('Full string missing')
        encoded = text.encode('utf-16-le', errors='surrogatepass')
        units = [int.from_bytes(encoded[i:i+2], 'little') for i in range(0, len(encoded), 2)]
        if value.get('utf16') != units or value.get('codepoints') != [ord(c) for c in text]:
            raise WitnessInvalid('UTF16 or codepoints differ')
    elif kind in ('array', 'object'):
        props = value.get('properties')
        if not isinstance(props, list):
            raise WitnessInvalid('Full properties missing')
        for p in props:
            if not isinstance(p, dict) or not {'key', 'value', 'enumerable', 'configurable', 'writable'} <= p.keys():
                raise WitnessInvalid('Descriptor missing')
            validate_snapshot(p['key'], prior_values)
            validate_snapshot(p['value'], prior_values)
    elif kind == 'call-reference':
        if value.get('call_event') not in prior_values:
            raise WitnessInvalid('Unbound native result reference')
    elif kind in ('number', 'bigint', 'symbol', 'function'):
        if not isinstance(value.get('value'), str):
            raise WitnessInvalid('Scalar representation missing')
    elif kind == 'boolean':
        if not isinstance(value.get('value'), bool):
            raise WitnessInvalid('Boolean missing')
    elif kind not in ('null', 'undefined', 'cycle', 'accessor'):
        raise WitnessInvalid('Unknown snapshot kind')


# These roles are scoped to the exact guards that emit them. Older branch roles
# retain their existing validation; new roles cannot be attached to another guard.
NEW_AUXILIARY_GUARDS = {
    'unmatched-hc-supported': 'unmatched-locale-hc-default',
    'selected-list-supported': 'unsupported-es-selected-unit-list',
    'selected-list-format-join': 'unsupported-es-selected-unit-list',
}


def data_property(snapshot, name):
    matches = [p['value'] for p in snapshot.get('properties', [])
               if p.get('key', {}).get('kind') == 'string' and p['key']['value'] == name]
    if len(matches) != 1:
        raise WitnessInvalid('Missing or duplicate typed original proof property')
    return matches[0]


def list_leaf_context(event, stack, config, sid):
    if not stack or stack[-1]['operation'] != 'aggregate':
        raise WitnessInvalid('Original list leaf missing active helper aggregate')
    aggregate = stack[-1]
    adapter = config.get('aggregate_adapters', {}).get(aggregate['site'], {})
    leaf = event.get('original_part_leaf')
    if not isinstance(leaf, dict) or set(leaf) != {'aggregate_entry', 'index', 'field'} or type(leaf['index']) is not int or leaf['aggregate_entry'] != aggregate['sequence'] or leaf['field'] != 'value':
        raise WitnessInvalid('Original list leaf loop/helper lineage differs')
    if adapter.get('mode') != 'list-mandatory-elements' or aggregate.get('adapter_mode') != adapter['mode'] or aggregate.get('original_helper_body_sha256') != adapter.get('original_helper_body_sha256') or not isinstance(adapter.get('original_helper_body_sha256'), str) or len(adapter['original_helper_body_sha256']) != 64 or {'site': sid, 'field': 'value'} not in adapter.get('fields', []):
        raise WitnessInvalid('Original list helper adapter differs')
    return aggregate, leaf['index']


def guard_operands(guard, guard_kind):
    evidence = guard['evidence']
    if guard_kind in ('tz-primary-identity', 'unmatched-locale-hc-default'):
        pair = data_property(evidence, 'actual' if guard_kind == 'tz-primary-identity' else 'compared')
        return [data_property(pair, str(i)) for i in range(2)]
    index = guard['_branch_evidence']['index']
    return [data_property(data_property(data_property(evidence, name), str(index)), 'value')
            for name in ('actual_parts', 'expected_parts')]


def full_data(v):
    return v.get('complete') is not False and v['kind'] not in ('call-reference', 'function', 'symbol', 'cycle', 'accessor') and all(full_data(p['key']) and full_data(p['value']) for p in v.get('properties', []))


def validate_fixture(lines, config, command, raw_full_run):
    directory = str(lines.parent) if isinstance(lines, Path) else None
    try:
        with ReceiptStore(UNDEFINED, directory) as receipts:
            return _validate_fixture(lines, config, command, raw_full_run, receipts)
    except (TraceBoundInvalid, sqlite3.Error, OSError) as exc:
        raise WitnessInvalid('Bounded streaming witness rejected: ' + str(exc)) from exc


def _validate_fixture(lines, config, command, raw_full_run, receipts):
    try:
        validate_premise_config(config)
    except (ValueError, KeyError, TypeError) as exc:
        raise WitnessInvalid('Premise source/profile guard binding rejected') from exc
    if command.get('returncode') != 0 or command.get('timed_out') is not False or command.get('interrupted') is not False or command.get('lifecycle_closed') is not True or command.get('output_limited', False) is not False or command.get('cleanup_failed', False) is not False or command.get('stderr_sha256', sha(b'')) != sha(b''):
        raise WitnessInvalid('Command lifecycle failed or incomplete')
    if raw_full_run.get('raw_preserved') is not True or raw_full_run.get('returncode') not in (0, 1) or not isinstance(raw_full_run.get('counters'), dict):
        raise WitnessInvalid('Exact normal full run receipt missing')
    for key in ('engine_sha256', 'source_tree_sha256', 'profile_sha256'):
        if not config.get(key) or command.get(key) != config[key]:
            raise WitnessInvalid('Actual engine/source/profile binding differs')
    if config_digest(config) != config.get('_config_sha256') or command.get('config_sha256') != config['_config_sha256']:
        raise WitnessInvalid('Runtime configuration binding differs')
    if not isinstance(command.get('generated_sha256'), str) or len(command['generated_sha256']) != 64:
        raise WitnessInvalid('Exact generated source binding missing')
    trace = EventLines(lines, config)
    first_kind = None
    stack = []
    counts = Counter()
    continued = set()
    data_mismatches = 0
    totalized = 0
    prior_values = EventSet(trace.event_limit)
    expected_throw_entries = EventSet(trace.event_limit)
    pending_expected_throws = EventSet(trace.event_limit)
    totalized_keys = ReceiptSet(receipts, "totalized-keys")
    completion = None
    premise_guards = receipts.mapping("premise-guards")
    consumed_guards = EventSet(trace.event_limit)
    premise_invariants = {}
    unavailable_site_ids = set()
    native_receipts = receipts.mapping("native-receipts")
    native_pending = receipts.mapping("native-pending")
    last_native_pending = {}
    auxiliary_receipts = receipts.mapping("auxiliary-receipts")
    previous_guard = {}
    unavailable_receipts = receipts.mapping("unavailable-receipts")
    for i, event in enumerate(trace, 1):
        if completion is not None:
            raise WitnessInvalid('Output follows original EOF completion')
        if event.get('sequence') != i or event.get('schema') != 'native-original-assertion-trace-v2':
            raise WitnessInvalid('Sequence or schema differs')
        for key in ('fixture', 'fixture_sha256', 'variant', 'profile_sha256', 'engine_sha256', 'source_tree_sha256'):
            if event.get(key) != config.get(key):
                raise WitnessInvalid('Event identity differs: ' + key)
        if event.get('config_sha256') != config['_config_sha256'] or event.get('conformance_accepted') is not False or event.get('semantic_adjudication_complete') is not False:
            raise WitnessInvalid('False event qualification')
        kind = event.get('kind')
        if i == 1:
            first_kind = kind
        sid = event.get('site')
        if sid is not None:
            if sid not in config['sites'] or event.get('site_source_sha256') != config['sites'][sid]['source_sha256']:
                raise WitnessInvalid('Unknown or changed exact source site')
        elif kind not in ('start', 'completion'):
            raise WitnessInvalid('Site identity missing')
        s = config['sites'].get(sid, {})
        for key in ('arguments',):
            for v in event.get(key, []):
                validate_snapshot(v, prior_values)
        for key in ('value', 'result', 'actual', 'expected', 'message', 'detail', 'receiver', 'original_pass_return', 'evidence', 'original_component'):
            if key in event and event[key] is not None:
                validate_snapshot(event[key], prior_values)
        if kind == 'start':
            if i != 1 or event.get('original_entry_reached') is not True:
                raise WitnessInvalid('Original entry missing or repeated')
        elif kind == 'enter':
            if i == 1 or event.get('parent') != (stack[-1]['sequence'] if stack else None):
                raise WitnessInvalid('Parent or entry nesting differs')
            op = event.get('operation')
            if op not in ('assertion', 'call', 'constructor', 'helper', 'aggregate') or (op == 'aggregate' and s.get('kind') != 'helper') or (op != 'aggregate' and s.get('kind') != op):
                raise WitnessInvalid('Operation kind differs from source site')
            if op == 'assertion' and s.get('premise_guard') and event.get('mandatory_override') is True:
                if s['premise_guard'] != 'unsupported-es-selected-unit-list' or event.get('premise_guard_event') is not None:
                    raise WitnessInvalid('Only exact mandatory original list elements may omit a premise token')
                aggregate, index = list_leaf_context(event, stack, config, sid)
                try:
                    parts = [decode(aggregate[name]) for name in ('actual', 'expected')]
                    typed_parts = [data_property(aggregate[name], str(index)) for name in ('actual', 'expected')]
                    if not 0 <= index < len(parts[0]) or not index < len(parts[1]) or any(p[index]['type'] != 'element' for p in parts) or len(event.get('arguments', [])) < 2 or event['arguments'][:2] != [data_property(p, 'value') for p in typed_parts] or any(p.get('kind') != 'string' for p in event['arguments'][:2]) or event['arguments'][0] != event['arguments'][1]:
                        raise WitnessInvalid('Mandatory original list element input differs')
                    element_indices = [i for i, part in enumerate(parts[0]) if part['type'] == 'element']
                    indices = aggregate.setdefault('_guarded_element_indices', [])
                    if indices != element_indices[:len(indices)] or len(indices) >= len(element_indices) or index != element_indices[len(indices)]:
                        raise WitnessInvalid('Mandatory original list element index replay or order differs')
                    indices.append(index)
                except (ValueError, KeyError, TypeError, IndexError) as exc:
                    raise WitnessInvalid('Mandatory original list element lineage rejected') from exc
            if op == 'assertion' and s.get('premise_guard') and event.get('mandatory_override') is not True:
                guard = premise_guards.get(event.get('premise_guard_event'))
                if not guard or guard['site'] != sid or guard['sequence'] in consumed_guards:
                    raise WitnessInvalid('Original assertion missing exact single-use premise guard')
                guard_kind = s['premise_guard']
                if guard_kind in ('tz-primary-identity', 'unmatched-locale-hc-default', 'unsupported-es-selected-unit-list'):
                    if len(event.get('arguments', [])) < 2 or event['arguments'][:2] != guard_operands(guard, guard_kind):
                        raise WitnessInvalid('Original assertion operands differ from typed premise proof')
                if guard_kind == 'unsupported-es-selected-unit-list':
                    aggregate, index = list_leaf_context(event, stack, config, sid)
                    evidence = guard['_branch_evidence']
                    if index != evidence['index']:
                        raise WitnessInvalid('Original list literal loop index differs from proof')
                    if aggregate.get('actual') != data_property(guard['evidence'], 'actual_parts') or aggregate.get('expected') != data_property(guard['evidence'], 'expected_parts'):
                        raise WitnessInvalid('Original list helper actual/expected arguments differ from proof')
                    indices = aggregate.setdefault('_guarded_literal_indices', [])
                    literal_indices = [i for i, part in enumerate(evidence['actual_parts']) if part['type'] == 'literal']
                    if indices != literal_indices[:len(indices)] or len(indices) >= len(literal_indices) or index != literal_indices[len(indices)]:
                        raise WitnessInvalid('Original list literal index replay or order differs')
                    indices.append(index)
                consumed_guards.add(guard['sequence'])
                premise_invariants[sid] = set()
            if len(stack) >= MAX_STACK_DEPTH:
                raise WitnessInvalid('Finite operation stack bound exceeded')
            stack.append(event)
        elif kind == 'exit':
            if not stack or event.get('entry') != stack[-1]['sequence'] or sid != stack[-1]['site'] or event.get('operation') != stack[-1]['operation']:
                raise WitnessInvalid('Exit nesting differs')
            entry = stack.pop()
            counts[sid] += 1
            op = event['operation']
            if op == 'aggregate':
                adapter = config.get('aggregate_adapters', {}).get(sid, {})
                guarded_list = any(config['sites'].get(field.get('site'), {}).get('premise_guard') == 'unsupported-es-selected-unit-list' for field in adapter.get('fields', []))
                if guarded_list:
                    try:
                        literal_indices = [i for i, part in enumerate(decode(entry['actual'])) if part['type'] == 'literal']
                    except (ValueError, KeyError, TypeError) as exc:
                        raise WitnessInvalid('Original list aggregate snapshot malformed') from exc
                    element_indices = [i for i, part in enumerate(decode(entry['actual'])) if part['type'] == 'element']
                    if entry.get('_guarded_literal_indices', []) != literal_indices or entry.get('_guarded_element_indices', []) != element_indices:
                        raise WitnessInvalid('Original list literal/element obligations missing or replayed')
            if event.get('completion') not in ('normal', 'throw'):
                raise WitnessInvalid('Original completion missing')
            if op == 'assertion':
                passed = event.get('original_assertion_passed')
                if passed is True:
                    if event['completion'] != 'normal' or event.get('continued') is not False:
                        raise WitnessInvalid('Passed assertion shape differs')
                    if s.get('assertion_method') == 'throws':
                        expected_throw_entries.add(entry['sequence'])
                elif passed is False:
                    if event.get('continued') is not True or entry.get('mandatory_override') is True or s.get('continue_exact_data_assertion') is not True or event.get('error', {}).get('test262_error') is not True or not all(full_data(v) for v in entry.get('arguments', [])):
                        raise WitnessInvalid('Mandatory or unreviewed assertion failure')
                    if s.get('premise_guard') and premise_guards[entry['premise_guard_event']].get('permit_snapshot_continuation') is not True:
                        raise WitnessInvalid('Conditional premise did not permit continuation')
                    continued.add(sid)
                    data_mismatches += 1
                else:
                    raise WitnessInvalid('Assertion result missing')
            elif event['completion'] == 'throw':
                # Original assert.throws must subsequently pass. No generic abrupt completion is waived.
                ancestor = next((e for e in reversed(stack) if e['operation'] == 'assertion' and config['sites'][e['site']].get('assertion_method') == 'throws'), None)
                if not ancestor:
                    raise WitnessInvalid('Unexpected native/helper throw')
                pending_expected_throws.add(ancestor['sequence'])
            if op in ('call', 'constructor') and event['completion'] == 'normal':
                if event.get('result_event') not in prior_values:
                    raise WitnessInvalid('Native result capture missing')
                receipt = native_pending.get(event['result_event'])
                if receipt is None:
                    raise WitnessInvalid('Original native receipt missing')
                receipt.update(arguments=event.get('arguments', []), receiver=event.get('receiver'))
                native_pending[event['result_event']] = receipt
                native_receipts[event['result_event']] = receipt
        elif kind == 'value':
            if event.get('role') in ('native-result', 'constructor-resolved-options', 'auxiliary-fractional-range-parts'):
                if not stack or event.get('operation_entry') != stack[-1]['sequence'] or sid != stack[-1]['site']:
                    raise WitnessInvalid('Native result binding differs')
                prior_values.add(i)
                if event.get('role') == 'native-result':
                    native_pending[i] = dict(site=sid, source=s.get('source',''), value=event['value'])
                    last_native_pending[sid] = i
                elif event.get('role') == 'constructor-resolved-options':
                    pending_id = last_native_pending.get(sid)
                    pending = native_pending.get(pending_id)
                    if pending is None:
                        raise WitnessInvalid('Unbound constructor selection')
                    pending['resolved'] = event['value']
                    native_pending[pending_id] = pending
            elif event.get('role') == 'auxiliary-premise-data':
                if not s.get('premise_guard') or event.get('auxiliary') is not True or event.get('auxiliary_role') not in (
                    'dayperiod-parts','dayperiod-format','calendar-format-join','lunisolar-format-join',
                    'hourcycle-resolved-probe','ar-EG-supportedLocalesOf','extreme-plain-date-parts',
                    'collator-original-locale-support','locale-collator-prefix-support',
                    *NEW_AUXILIARY_GUARDS) or not full_data(event['value']):
                    raise WitnessInvalid('Unbound or incomplete auxiliary premise data')
                if event.get('auxiliary_role') in NEW_AUXILIARY_GUARDS and s.get('premise_guard') != NEW_AUXILIARY_GUARDS[event['auxiliary_role']]:
                    raise WitnessInvalid('Auxiliary role differs from exact premise guard kind')
                auxiliary_receipts[i] = event
            elif event.get('role') in ('original-operand', 'auxiliary-semantic-baseline') and s.get('kind') == 'value':
                counts[sid] += 1
            else:
                raise WitnessInvalid('Unknown value event')
        elif kind == 'invariant':
            if event.get('mandatory') is not True or event.get('passed') is not True:
                raise WitnessInvalid('Mandatory semantic invariant failed')
            if s.get('premise_guard'):
                premise_invariants.setdefault(sid,set()).add(event.get('name'))
        elif kind == 'premise-guard':
            required = {'premise-root-binding','premise-exact-production-source','premise-effective-profile','premise-context-kind'}
            if not s.get('premise_guard') or event.get('guard_id') != s['premise_guard'] or event.get('passed') is not True or not isinstance(event.get('permit_snapshot_continuation'),bool) or event.get('root_guard_review_sha256') != config['premise_bindings']['root_guard_review_sha256'] or not required <= premise_invariants.get(sid,set()) or event.get('evidence',{}).get('kind') != 'object' or not full_data(event['evidence']):
                raise WitnessInvalid('Unbound or incomplete source premise guard')
            event['_previous_guard_sequence'] = previous_guard.get(sid,0)
            try:
                branch_evidence = validate_branch(event, s, premise_invariants.get(sid,set()), config, native_receipts, auxiliary_receipts, stack)
                if s['premise_guard'] == 'lunisolar-selected-U' and s.get('kind') == 'assertion' and not s.get('unavailable_premise') and branch_evidence['selected_U_without_r']:
                    lineage = unavailable_receipts.get(branch_evidence['unavailable_read_event'])
                    if not lineage or lineage['adapter_kind'] != 'year-read' or lineage['_original_parts_event'] != branch_evidence['proof']['parts_event']:
                        raise WitnessInvalid('Dependent unavailable year assertion missing exact prior read receipt')
            except (ValueError, KeyError, TypeError, IndexError, AttributeError) as exc:
                raise WitnessInvalid('Substantive source premise proof rejected') from exc
            event['_branch_evidence'] = branch_evidence
            event['_premise_invariants'] = premise_invariants.get(sid,set()).copy()
            premise_invariants[sid] = set()
            premise_guards[i] = event
            previous_guard[sid] = i
        elif kind == 'unavailable-obligation':
            guard = premise_guards.get(event.get('guard_event'))
            if not stack or stack[-1]['operation'] != 'helper' or event.get('helper_entry') != stack[-1]['sequence'] or not guard or guard['site'] != sid or guard['sequence'] in consumed_guards or guard.get('permit_snapshot_continuation') is not True:
                raise WitnessInvalid('Unavailable obligation helper/guard binding differs')
            if event.get('prerequisite') != s.get('unavailable_premise') or event.get('adapter_kind') != s.get('unavailable_adapter_kind') or event.get('adapter_kind') not in ('year-read','assertion-operands') or event.get('source_expression') != s['source'] or event.get('original_evaluation_performed') is not True or event.get('original_assertion_invoked') is not False or event.get('original_assertion_passed') is not False or event.get('original_abrupt_premise') is not True or event.get('continued') is not True or event.get('all_original_operand_bytes_preserved') is not True or event.get('error_constructor') != 'TypeError' or event.get('original_error',{}).get('test262_error') is not False or not isinstance(event.get('original_error',{}).get('message'),str) or event.get('original_component') != {'kind':'undefined'} or event.get('actual') != {'kind':'undefined'} or event.get('expected') != {'kind':'undefined'}:
                raise WitnessInvalid('Unavailable original dereference evidence differs')
            required = {'premise-exact-unavailable-obligation-allowlist','premise-unavailable-expression-prerequisite','premise-unavailable-original-helper-binding','premise-original-undefined-dereference-threw-TypeError'}
            # The allowlist precedes the guard; later dereference invariants must still be visible.
            recent_names = guard['_premise_invariants'] | premise_invariants.get(sid,set())
            if not required <= recent_names:
                raise WitnessInvalid('Unavailable dereference invariant proof missing')
            event['_original_parts_event'] = next(p['value'] for p in guard['evidence']['properties'] if p['key'].get('value') == 'proof')
            event['_original_parts_event'] = next(p['value']['value'] for p in event['_original_parts_event']['properties'] if p['key'].get('value') == 'parts_event')
            event['_original_parts_event'] = int(event['_original_parts_event'])
            unavailable_receipts[i] = event
            consumed_guards.add(guard['sequence'])
            premise_invariants[sid] = set()
            key = (event['helper_entry'],sid,i,event['adapter_kind'])
            totalized_keys.add(key)
            counts[sid] += 1
            continued.add(sid)
            unavailable_site_ids.add(sid)
            data_mismatches += 1
            totalized += 1
        elif kind == 'original-obligation':
            if not stack or stack[-1]['operation'] != 'aggregate' or event.get('aggregate_entry') != stack[-1]['sequence']:
                raise WitnessInvalid('Aggregate obligation binding differs')
            spec = config.get('aggregate_adapters', {}).get(stack[-1]['site'])
            if not spec or {'site': sid, 'field': event.get('field')} not in spec['fields'] or event.get('totalized_reviewed_data_comparison') is not True or event.get('continued') is not True or s.get('continue_exact_data_assertion') is not True:
                raise WitnessInvalid('Unreviewed totalized data obligation')
            key = (event['aggregate_entry'], sid, event.get('index'), event.get('field'))
            if key in totalized_keys:
                raise WitnessInvalid('Duplicate aggregate obligation')
            totalized_keys.add(key)
            counts[sid] += 1
            continued.add(sid)
            data_mismatches += 1
            totalized += 1
        elif kind == 'completion':
            if stack or completion is not None or event.get('original_eof_reached') is not True or event.get('fixture_capture_complete') is not True:
                raise WitnessInvalid('Original EOF or dynamic trace closure missing')
            if event.get('mandatory_failed_site_ids') != [] or event.get('site_counts') != dict(counts) or event.get('data_mismatches') != data_mismatches or set(event.get('continued_data_site_ids', [])) != continued or event.get('totalized_original_data_obligations') != totalized or event.get('all_original_expressions_evaluated') is not (totalized == 0):
                raise WitnessInvalid('Completion contradicts actual trace')
            completion = event
        else:
            raise WitnessInvalid('Unknown trace event')
    trace.verify(command.get('stdout_sha256'))
    if first_kind != 'start' or completion is None:
        raise WitnessInvalid('Incomplete trace')
    if len(premise_guards) != len(consumed_guards) or any(key not in consumed_guards for key in premise_guards):
        raise WitnessInvalid('Unused or replayed premise guard')
    for entry in pending_expected_throws:
        if entry not in expected_throw_entries:
            raise WitnessInvalid('Original expected throw assertion did not pass')
    return dict(schema='native-original-assertion-trace-v2', fixture=config['fixture'], variant=config['variant'],
        fixture_sha256=config['fixture_sha256'], fixture_capture_complete=True, original_eof_reached=True,
        unavailable_original_obligation_site_ids=sorted(unavailable_site_ids), premise_guard_events=len(premise_guards),
        mandatory_failed_site_ids=[], continued_data_site_ids=sorted(continued), executed_site_ids=sorted(counts),
        data_mismatches=data_mismatches, totalized_original_data_obligations=totalized,
        totalized_original_data_site_ids=sorted({key[1] for key in totalized_keys}), all_original_expressions_evaluated=totalized == 0,
        site_counts=dict(counts), trace_sha256=command['stdout_sha256'], generated_sha256=command['generated_sha256'],
        config_sha256=config['_config_sha256'], engine_sha256=config['engine_sha256'],
        source_tree_sha256=config['source_tree_sha256'], profile_sha256=config['profile_sha256'],
        semantic_adjudication_complete=False, conformance_accepted=False)
