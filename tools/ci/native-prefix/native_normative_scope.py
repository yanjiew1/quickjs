"""Bind reviewed static semantics to authenticated current capture identities.

The caller authenticates checkout, raw command and projector receipts first.
This module never approves observations, changes expectations or writes sources.
Committed review documents leave their current production map field null;
only the returned in-memory documents receive that identity. The receipt keeps
the complete map, immutable review bindings and historical capture bindings.
"""
import copy
import hashlib
import json
import re
from core_baseline_transition import authenticate_transition, review_baseline_matches


SCOPE_PATH = 'tools/ci/native-prefix/native-normative-static-scope.json'
POLICY_PATH = 'tools/ci/native-prefix/native-profile-diagnostic-policy.json'
ORACLE_PATH = 'tools/ci/native-prefix/native-oracle-profile-policy.json'
IDENTITY_HELPERS = {'tools/ci/native-prefix/' + name for name in (
    'native_normative_scope.py', 'run.py', 'native_profile_gate.py',
    'project_original_assertions.py', 'core_baseline_transition.py')}
STATIC_FIELDS = ('fixture_source_map_sha256', 'config_sha256',
                 'baseline_sha256', 'effective_profile_sha256')
ROLES = {'diagnostic-policy', 'oracle-policy', 'mandatory-review',
         'oracle-semantic-review'}
SCOPE_FIELDS = {'schema', 'source_review_complete', 'normative_review_complete',
                'failure_waiver', 'conformance_accepted',
                'production_source_map_sha256', 'source_bindings',
                'document_bindings', 'stable_observation_expectations',
                'historical_capture_bindings', *STATIC_FIELDS}


def require(value, reason):
    if not value:
        raise ValueError(reason)


def canonical(value):
    # Match the complete-source-map canonicalization used by run.py.
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def digest(value):
    require(isinstance(value, bytes), 'Evidence loader must return source bytes')
    return hashlib.sha256(value).hexdigest()


def is_sha(value):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value)


def safe_path(value):
    return (isinstance(value, str) and value and not value.startswith('/') and
            '\\' not in value and
            all(part not in ('', '.', '..') for part in value.split('/')))


def check_binding(binding):
    require(isinstance(binding, dict) and set(binding) == {'path', 'sha256'} and
            safe_path(binding['path']) and is_sha(binding['sha256']),
            'Missing safe exact static evidence binding')


def authenticate(binding, sources, load_bytes):
    check_binding(binding)
    require(sources.get(binding['path']) == binding['sha256'],
            'Review binding is outside the complete current source map')
    raw = load_bytes(binding['path'])
    require(digest(raw) == binding['sha256'], 'Changed reviewed source bytes')
    document = json.loads(raw)
    require(isinstance(document, dict), 'Static review must be an object')
    return document


def historical_capture(binding, kind):
    require(isinstance(binding, dict) and binding.get('capture_kind') == kind,
            'Missing unchanged historical capture identity')
    if kind == 'script-variants':
        require(set(binding) == {'capture_kind', 'variant_bindings'} and
                isinstance(binding['variant_bindings'], dict) and
                set(binding['variant_bindings']) == {'strict', 'nonstrict'},
                'Historical capture must bind both original variants')
        for variant in binding['variant_bindings'].values():
            require(isinstance(variant, dict) and set(variant) == {
                'engine_sha256', 'trace_sha256', 'generated_sha256', 'config_sha256'} and
                all(is_sha(value) for value in variant.values()),
                'Missing historical original variant hashes')
    else:
        require(kind == 'rtf-variant-neutral' and set(binding) == {
            'capture_kind', 'engine_sha256', 'trace_sha256', 'sampler_sha256',
            'command_sha256', 'current_bindings_sha256'} and
            all(is_sha(value) for name, value in binding.items() if name != 'capture_kind'),
            'Missing historical complete RTF capture identity')


class BoundNormativeScopes:
    """Only authenticated static documents can receive a current identity."""

    def __init__(self, documents, roles, load_bytes, source_map, receipt,
                 transition=None, baseline_context=None, helper_derivation=None,
                 witness_derivation=None, correction=None):
        self._documents = copy.deepcopy(documents)
        self._roles = copy.deepcopy(roles)
        self._load_bytes = load_bytes
        self._source_map = source_map
        self._receipt = copy.deepcopy(receipt)
        self._transition = transition
        self._baseline_context = copy.deepcopy(baseline_context)
        self._helper_derivation = helper_derivation
        self._witness_derivation = copy.deepcopy(witness_derivation)
        self._correction = correction

    def document(self, binding, role):
        check_binding(binding)
        key = binding['path']
        require(key in self._roles and self._roles[key] == (role, binding['sha256']),
                'Document is outside the independently reviewed normative scope')
        require(digest(self._load_bytes(key)) == binding['sha256'],
                'Static review changed after scope authentication')
        document = copy.deepcopy(self._documents[key])
        if role == 'diagnostic-policy' and self._witness_derivation is not None:
            derivation = self._witness_derivation
            scope = derivation['approved_static_scope_binding']
            require(digest(self._load_bytes(scope['path'])) == scope['sha256'],
                    'Witness source review changed after scope authentication')
            require(binding == derivation['historical_policy_binding'] and
                    document['witness_source_bindings'] ==
                    [item['historical'] for item in derivation['ordered_bindings']],
                    'Historical witness policy or ordered binding list changed')
            current = [item['current'] for item in derivation['ordered_bindings']]
            require(all(digest(self._load_bytes(item['path'])) == item['sha256']
                        for item in current),
                    'Reviewed witness source changed after scope authentication')
            document['witness_source_bindings'] = copy.deepcopy(current)
        if self._helper_derivation is not None:
            document = self._helper_derivation.derive_review(document)
        if self._transition is not None:
            document = self._transition.derive_document(
                document, role, self._baseline_context)
        if role == 'diagnostic-policy' and self._correction is not None:
            document = self._correction.derive_policy(document)
        document['production_source_map_sha256'] = self._source_map
        return document

    def document_for(self, role):
        candidates = [(path, pin) for path, (kind, pin) in self._roles.items() if kind == role]
        require(len(candidates) == 1, 'Missing or ambiguous static normative role')
        path, pin = candidates[0]
        return self.document(dict(path=path, sha256=pin), role)

    def helper_derivation(self):
        return self._helper_derivation

    def baseline_transition(self):
        return self._transition

    def correction(self):
        return self._correction

    def receipt(self):
        return copy.deepcopy(self._receipt)


def bind_static_scopes(scope_binding, *, context, sources, fixture_hashes,
                       validated, raw, raw_sha256, load_bytes):
    """Authenticate static approvals, then derive identity fields only.

    Current source/hash fields originate in previously authenticated run.py
    receipts. Expectations originate exclusively in the approved static scope.
    Exact equality is required; observed engine data never becomes an expectation.
    """
    from native_profile_gate import (MANDATORY_OBLIGATIONS, exact_current_capture,
                                     expected_trace_engine, semantic_row_evidence)

    require(isinstance(sources, dict) and sources and
            all(safe_path(path) and is_sha(pin) for path, pin in sources.items()),
            'Missing complete current source map')
    current_map = canonical(sources)
    require(current_map == context.get('production_source_map_sha256') ==
            context.get('actual_full_tracked_source_map_sha256') and
            isinstance(context.get('actual_initialized_checkout_commit'), str) and
            re.fullmatch(r'[0-9a-f]{40}', context['actual_initialized_checkout_commit']) and
            all(is_sha(context.get(field)) for field in (*STATIC_FIELDS, 'qjs_sha256')),
            'Current checkout/engine/profile identity differs')
    require(all(digest(load_bytes(path)) == pin for path, pin in sources.items()),
            'Complete current source bytes changed')
    require(raw.get('raw_parser_complete') is True and raw.get('raw_collection_complete') is True and
            raw.get('sealed_inputs_unchanged') is True and not raw.get('blocking_tool_records') and
            not raw.get('unknown_feature_records') and
            all(raw.get('input_identity', {}).get(field) == context[field] for field in
                (*STATIC_FIELDS[:-1], 'production_source_map_sha256',
                 'actual_full_tracked_source_map_sha256', 'actual_initialized_checkout_commit',
                 'qjs_sha256')) and context.get('mandatory_repository_gates_passed') is True,
            'Missing authenticated current raw or mandatory repository evidence')
    require(validated.get('schema') == 'native-original-assertion-capture-summary-v2' and
            validated.get('full_capture_complete') is True and
            validated.get('independent_revalidation_complete') is True and
            validated.get('complete_original_source_count') == 118 and
            validated.get('complete_original_default_variant_count') == 210 and
            validated.get('current_RTF_recaptured') is True and
            validated.get('conformance_accepted') is False and
            validated.get('failure_waiver') is False and is_sha(raw_sha256) and
            validated.get('raw_full_evidence_sha256') == raw_sha256 and
            validated.get('source_tree_sha256') == current_map and
            validated.get('engine_sha256') == context['qjs_sha256'] and
            validated.get('profile_sha256') == context['effective_profile_sha256'],
            'Missing exact complete current original assertion capture')
    from helper_source_derivation import authenticate_helper_derivation
    helper_derivation = authenticate_helper_derivation(sources, current_map, load_bytes)
    from native_correction_transition import authenticate_correction
    correction = authenticate_correction(sources, load_bytes, fixture_hashes)
    transition = authenticate_transition(sources, load_bytes, context)
    review_context = dict(context)
    if transition is not None:
        review_context['core_baseline_transition'] = transition
    scope = authenticate(scope_binding, sources, load_bytes)
    require(scope_binding['path'] == SCOPE_PATH and set(scope) == SCOPE_FIELDS and
            scope.get('schema') == 'native-static-normative-scope-v1' and
            scope.get('source_review_complete') is True and
            scope.get('normative_review_complete') is True and
            scope.get('failure_waiver') is False and scope.get('conformance_accepted') is False,
            'Static normative scope remains unreviewed or unauthorized')
    require(scope['production_source_map_sha256'] is None and
            all(review_baseline_matches(scope.get(field), review_context)
                if field == 'baseline_sha256' else scope.get(field) == context[field]
                for field in STATIC_FIELDS),
            'Static scope embeds a stale production identity or changed stable bindings')
    require(isinstance(scope['source_bindings'], list) and scope['source_bindings'],
            'Missing independently reviewed implementation source bindings')
    reviewed_sources = {}
    for binding in scope['source_bindings']:
        check_binding(binding)
        require(binding['path'] not in reviewed_sources and
                binding['path'] != SCOPE_PATH and
                sources.get(binding['path']) == binding['sha256'],
                'Changed or duplicate independently reviewed implementation source')
        reviewed_sources[binding['path']] = binding['sha256']
    require(IDENTITY_HELPERS <= reviewed_sources.keys(),
            'Identity/projector helper source is outside the independently reviewed scope')
    summaries = validated.get('fixture_summaries')
    expectations = scope['stable_observation_expectations']
    historical = scope['historical_capture_bindings']
    require(isinstance(summaries, dict) and len(summaries) == 118 and
            isinstance(expectations, dict) and set(expectations) == set(summaries) and
            isinstance(historical, dict) and set(historical) == set(summaries),
            'Missing exact approved complete original fixture scope')
    evidence, script_count, rtf_count = {}, 0, 0
    for path, trace in sorted(summaries.items()):
        require(safe_path(path) and path.startswith('test262/test/') and
                trace.get('fixture_capture_complete') is True and
                trace.get('original_eof_reached') is True and
                trace.get('mandatory_failed_site_ids') == [] and
                trace.get('mandatory_failures') == [] and
                trace.get('fixture_sha256') == fixture_hashes.get(path) and
                is_sha(trace.get('fixture_sha256')) and
                trace.get('source_tree_sha256') == current_map and
                trace.get('profile_sha256') == context['effective_profile_sha256'] and
                trace.get('engine_sha256') == expected_trace_engine(trace, context),
                'Changed fixture/engine/profile or incomplete mandatory original capture')
        capture = exact_current_capture(trace, context)
        historical_capture(historical[path], capture['capture_kind'])
        review = dict(stable_observation_expectation=expectations[path],
                      current_capture_binding=historical[path])
        evidence[path] = semantic_row_evidence(trace, scope_binding, review,
                                             helper_derivation, correction)
        script_count += capture['capture_kind'] == 'script-variants'
        rtf_count += capture['capture_kind'] == 'rtf-variant-neutral'
    require((script_count, rtf_count) == (105, 13),
            'Missing 210 complete original variants or 13 complete RTF captures')
    require(isinstance(scope['document_bindings'], list) and scope['document_bindings'],
            'Missing approved static policy and mandatory review documents')
    documents, roles = {}, {}
    for record in scope['document_bindings']:
        require(isinstance(record, dict) and set(record) == {'role', 'binding'} and
                record['role'] in ROLES, 'Unsupported normative document role')
        binding, role = record['binding'], record['role']
        check_binding(binding)
        path = binding['path']
        require(path not in roles, 'Duplicate approved normative document')
        document = authenticate(binding, sources, load_bytes)
        require(document.get('production_source_map_sha256', 'absent') is None and
                document.get('failure_waiver', False) is False and
                document.get('conformance_accepted', False) is False and
                document.get('runtime_accepted', False) is False and
                document.get('hosted_accepted', False) is False,
                'Static document embeds current identity, waiver or future acceptance')
        fields = STATIC_FIELDS if role in ('diagnostic-policy', 'mandatory-review') else ('effective_profile_sha256',)
        require(all(review_baseline_matches(document.get(field), review_context)
                    if field == 'baseline_sha256' else document.get(field) == context[field]
                    for field in fields),
                'Static normative document fixture/config/baseline/profile differs')
        if role.endswith('policy'):
            require(document.get('profile_diagnostic_authorized') is True,
                    'Static policy remains an unapproved REQUIRED draft')
            schema = ('native-exact-profile-policy-v2' if role == 'diagnostic-policy'
                      else 'native-exact-oracle-profile-policy-v2')
            require(document.get('schema') == schema and
                    isinstance(document.get('adjudicated_diagnostic_entries'), list) and
                    document['adjudicated_diagnostic_entries'],
                    'Static policy is empty or has an unsupported schema')
        else:
            require(document.get('mandatory_semantics_satisfied') is True,
                    'Static mandatory semantics remain unresolved')
        roles[path] = (role, binding['sha256'])
        documents[path] = document
    require(roles.get(POLICY_PATH, (None,))[0] == 'diagnostic-policy' and
            roles.get(ORACLE_PATH, (None,))[0] == 'oracle-policy' and
            sum(role == 'diagnostic-policy' for role, _ in roles.values()) == 1 and
            sum(role == 'oracle-policy' for role, _ in roles.values()) == 1 and
            sum(role == 'oracle-semantic-review' for role, _ in roles.values()) == 1,
            'Missing or ambiguous static policy/semantic review roles')
    obligations = documents[POLICY_PATH].get('mandatory_obligations', [])
    require(isinstance(obligations, list) and len(obligations) == len(MANDATORY_OBLIGATIONS) and
            {item.get('id') for item in obligations} == set(MANDATORY_OBLIGATIONS),
            'Missing complete mandatory normative obligation scope')
    required_documents = {POLICY_PATH, ORACLE_PATH}
    for item in obligations:
        require(item.get('status') == 'SATISFIED' and item.get('proof_bindings'),
                'Unresolved approved mandatory obligation')
        for binding in item['proof_bindings']:
            check_binding(binding)
            require(roles.get(binding['path']) == ('mandatory-review', binding['sha256']),
                    'Mandatory proof is outside the approved static scope')
            required_documents.add(binding['path'])
    binding = documents[ORACLE_PATH].get('semantic_review_binding')
    check_binding(binding)
    require(roles.get(binding['path']) == ('oracle-semantic-review', binding['sha256']),
            'Oracle semantic proof is outside the approved static scope')
    required_documents.add(binding['path'])
    require(set(documents) == required_documents, 'Unreferenced normative scope document')
    # The immutable diagnostic policy retains its original ordered witness
    # list. Only sources separately approved in the current static scope can
    # replace those identities; observed checkout bytes alone never suffice.
    witness_bindings = documents[POLICY_PATH].get('witness_source_bindings')
    require(isinstance(witness_bindings, list) and witness_bindings,
            'Missing historical witness source bindings')
    seen, ordered = set(), []
    for binding in witness_bindings:
        check_binding(binding)
        path = binding['path']
        require(path not in seen and path in reviewed_sources and
                reviewed_sources[path] == sources.get(path),
                'Duplicate or unreviewed witness source identity')
        seen.add(path)
        ordered.append(dict(historical=copy.deepcopy(binding),
                            current=dict(path=path, sha256=reviewed_sources[path])))
    witness_derivation = dict(schema='native-witness-source-identity-derivation-v1',
        historical_policy_binding=dict(path=POLICY_PATH, sha256=roles[POLICY_PATH][1]),
        approved_static_scope_binding=copy.deepcopy(scope_binding),
        production_source_map_sha256=current_map, ordered_bindings=ordered,
        identity_only=True, observations_changed=False,
        conformance_accepted=False, failure_waiver=False)
    receipt = dict(schema='native-current-normative-scope-envelope-v1',
        approved_static_scope_binding=copy.deepcopy(scope_binding),
        current_identity={field: context[field] for field in
            (*STATIC_FIELDS, 'production_source_map_sha256',
             'actual_full_tracked_source_map_sha256', 'actual_initialized_checkout_commit', 'qjs_sha256')},
        complete_current_source_files=copy.deepcopy(sources),
        current_raw_full_evidence_sha256=raw_sha256,
        static_document_bindings=copy.deepcopy(scope['document_bindings']),
        witness_source_derivation=copy.deepcopy(witness_derivation),
        current_semantic_row_evidence=evidence,
        conformance_accepted=False, failure_waiver=False,
        basis='Exact independently reviewed static semantics; current identity derivation only')
    if helper_derivation is not None:
        receipt['helper_source_derivation'] = helper_derivation.receipt()
    if transition is not None:
        receipt['core_baseline_transition'] = transition.receipt()
    if correction is not None:
        receipt['semantic_correction'] = correction.receipt()
        receipt['basis'] = ('Exact historical static semantics plus separately reviewed '
                            'correction deltas and current identity derivation')
    return BoundNormativeScopes(documents, roles, load_bytes, current_map, receipt,
        transition, {name: context[name] for name in
            ('baseline_sha256', 'production_source_map_sha256')}, helper_derivation,
        witness_derivation, correction)


def current_mandatory_review(binding, context, load_bytes):
    """Legacy exact-current reviews remain supported; no implicit static fallback."""
    scopes = context.get('native_normative_scopes')
    if scopes is None:
        from native_profile_gate import bound_json
        return bound_json(binding, load_bytes)
    require(type(scopes) is BoundNormativeScopes, 'Untrusted current normative scope object')
    return scopes.document(binding, 'mandatory-review')
