"""Synthetic binding tests; no engine output supplies normative expectations."""
import copy
import hashlib
import json
import unittest

from native_normative_scope import (IDENTITY_HELPERS, ORACLE_PATH, POLICY_PATH, SCOPE_PATH,
    STATIC_FIELDS, bind_static_scopes, canonical, current_mandatory_review)
from native_profile_gate import MANDATORY_OBLIGATIONS


class StaticScopeTests(unittest.TestCase):
    def setUp(self):
        self.files = {'src/mock-engine.c': b'independently reviewed mock implementation'}
        self.files.update({path: b'independently reviewed mock helper' for path in IDENTITY_HELPERS})
        self.context = {field: 'a' * 64 for field in (*STATIC_FIELDS, 'qjs_sha256')}
        self.context.update(actual_initialized_checkout_commit='b' * 40,
                            mandatory_repository_gates_passed=True)
        stable_fields = {field: self.context[field] for field in STATIC_FIELDS}
        self.proof_path = 'tools/ci/native-prefix/mock-mandatory-review.json'
        self.oracle_review_path = 'tools/ci/native-prefix/mock-oracle-review.json'
        self.documents = {
            self.proof_path: dict(stable_fields, production_source_map_sha256=None,
                mandatory_semantics_satisfied=True, independently_reviewed_duties=['mock-duty']),
            self.oracle_review_path: dict(effective_profile_sha256='a' * 64,
                production_source_map_sha256=None, mandatory_semantics_satisfied=True,
                independently_expected_numeric_parts=['〇二', '三五', '〇六']),
        }
        self.policy = dict(stable_fields, schema='native-exact-profile-policy-v2',
            production_source_map_sha256=None, profile_diagnostic_authorized=True,
            failure_waiver=False, conformance_accepted=False,
            no_new_skips_exclusions_expected_errors=True,
            witness_source_bindings=[self.binding('src/mock-engine.c')],
            adjudicated_diagnostic_entries=[dict(mock_exact_independent_review=True)])
        self.oracle = dict(schema='native-exact-oracle-profile-policy-v2',
            production_source_map_sha256=None, effective_profile_sha256='a' * 64,
            profile_diagnostic_authorized=True, failure_waiver=False,
            conformance_accepted=False,
            adjudicated_diagnostic_entries=[dict(mock_exact_independent_oracle_review=True)],
            exact_witness=dict(mock_independent_literal='\u202f'))
        self.scope = dict(stable_fields, schema='native-static-normative-scope-v1',
            source_review_complete=True, normative_review_complete=True,
            failure_waiver=False, conformance_accepted=False,
            production_source_map_sha256=None,
            source_bindings=[self.binding(path) for path in sorted(IDENTITY_HELPERS | {'src/mock-engine.c'})],
            document_bindings=[], stable_observation_expectations={}, historical_capture_bindings={})
        self.traces, self.fixture_hashes = {}, {}
        for index in range(118):
            path = 'test262/test/intl402/mock/fixture-%03d.js' % index
            kind = 'script-variants' if index < 105 else 'rtf-variant-neutral'
            trace = self.trace(kind)
            trace['fixture'] = path
            for item in trace.get('variant_summaries', {}).values():
                item['fixture'] = path
            self.traces[path] = trace
            self.fixture_hashes[path] = trace['fixture_sha256']
            self.scope['stable_observation_expectations'][path] = copy.deepcopy(trace['stable_observation'])
            self.scope['historical_capture_bindings'][path] = copy.deepcopy(trace['current_capture_binding'])
        self.validated = dict(schema='native-original-assertion-capture-summary-v2',
            full_capture_complete=True, independent_revalidation_complete=True,
            complete_original_source_count=118, complete_original_default_variant_count=210,
            current_RTF_recaptured=True, conformance_accepted=False, failure_waiver=False,
            fixture_summaries=self.traces, profile_sha256='a' * 64,
            raw_full_evidence_sha256='c' * 64)
        self.raw = dict(raw_parser_complete=True, raw_collection_complete=True,
            sealed_inputs_unchanged=True, blocking_tool_records=[], unknown_feature_records=[])
        self.persist()

    def binding(self, path):
        return dict(path=path, sha256=hashlib.sha256(self.files[path]).hexdigest())

    def trace(self, kind):
        capture = dict(capture_kind=kind)
        trace = dict(fixture_capture_complete=True, original_eof_reached=True,
            mandatory_failed_site_ids=[], mandatory_failures=[], fixture_sha256='d' * 64,
            profile_sha256='a' * 64, engine_sha256='a' * 64,
            trace_sha256='e' * 64, generated_sha256='f' * 64, config_sha256='1' * 64)
        if kind == 'script-variants':
            variants = {}
            for name in ('strict', 'nonstrict'):
                variants[name] = dict(trace, variant=name, totalized_original_data_obligations=0,
                    all_original_expressions_evaluated=True, continued_data_site_ids=[],
                    totalized_original_data_site_ids=[])
            variants['strict'].update(trace_sha256='8' * 64, generated_sha256='9' * 64,
                                      config_sha256='0' * 64)
            trace.update(variant_summaries=variants, variant='nonstrict',
                raw_failure_identity=None, continued_data_site_ids=[],
                totalized_original_data_obligations=[], totalized_original_data_obligation_count=0,
                all_original_expressions_evaluated=True)
            capture['variant_bindings'] = {name: {field: item[field] for field in
                ('engine_sha256', 'trace_sha256', 'generated_sha256', 'config_sha256')}
                for name, item in variants.items()}
            observations = {name: dict(events_sha256='2' * 64, event_count=7,
                stable_configuration_sha256='3' * 64) for name in variants}
        else:
            trace['strict_variant_neutral_capture'] = True
            capture.update(engine_sha256='a' * 64, trace_sha256='e' * 64,
                sampler_sha256='f' * 64, command_sha256='4' * 64, current_bindings_sha256='5' * 64)
            observations = dict(whole_parsed_rtf_sha256='6' * 64)
        trace['current_capture_binding'] = capture
        trace['stable_observation'] = dict(schema='native-complete-stable-observation-v2',
            capture_kind=kind, source_bindings={'mock-witness.js': '7' * 64}, observations=observations)
        return trace

    def persist(self):
        for path, document in self.documents.items():
            self.files[path] = json.dumps(document, sort_keys=True).encode()
        proof_binding = self.binding(self.proof_path)
        self.policy['mandatory_obligations'] = [dict(id=name, status='SATISFIED',
            proof_bindings=[proof_binding]) for name in MANDATORY_OBLIGATIONS]
        self.oracle['semantic_review_binding'] = self.binding(self.oracle_review_path)
        self.files[POLICY_PATH] = json.dumps(self.policy, sort_keys=True).encode()
        self.files[ORACLE_PATH] = json.dumps(self.oracle, sort_keys=True).encode()
        self.scope['document_bindings'] = [dict(role=role, binding=self.binding(path)) for role, path in (
            ('diagnostic-policy', POLICY_PATH), ('oracle-policy', ORACLE_PATH),
            ('mandatory-review', self.proof_path), ('oracle-semantic-review', self.oracle_review_path))]
        self.files[SCOPE_PATH] = json.dumps(self.scope, sort_keys=True).encode()
        self.sources = {path: hashlib.sha256(value).hexdigest() for path, value in self.files.items()}
        self.context['production_source_map_sha256'] = canonical(self.sources)
        self.context['actual_full_tracked_source_map_sha256'] = canonical(self.sources)
        self.rebind_current_identity()

    def rebind_current_identity(self):
        self.raw['input_identity'] = {field: self.context[field] for field in (
            *STATIC_FIELDS[:-1], 'production_source_map_sha256',
            'actual_full_tracked_source_map_sha256', 'actual_initialized_checkout_commit', 'qjs_sha256')}
        self.validated['source_tree_sha256'] = self.context['production_source_map_sha256']
        self.validated['engine_sha256'] = self.context['qjs_sha256']
        for trace in self.traces.values():
            trace['source_tree_sha256'] = self.context['production_source_map_sha256']
            trace['engine_sha256'] = self.context['qjs_sha256']
            if trace['current_capture_binding']['capture_kind'] == 'rtf-variant-neutral':
                trace['current_capture_binding']['engine_sha256'] = self.context['qjs_sha256']
            else:
                for item in trace['variant_summaries'].values():
                    item['source_tree_sha256'] = self.context['production_source_map_sha256']
                    item['engine_sha256'] = self.context['qjs_sha256']
                for item in trace['current_capture_binding']['variant_bindings'].values():
                    item['engine_sha256'] = self.context['qjs_sha256']

    def bind(self):
        return bind_static_scopes(self.binding(SCOPE_PATH), context=self.context,
            sources=self.sources, fixture_hashes=self.fixture_hashes, validated=self.validated,
            raw=self.raw, raw_sha256='c' * 64, load_bytes=self.files.__getitem__)

    def blocked(self):
        with self.assertRaises(ValueError):
            self.bind()

    def test_passing_original_uses_nonstrict_without_invented_failure(self):
        trace = next(iter(self.traces.values()))
        self.assertIsNone(trace['raw_failure_identity'])
        historical = copy.deepcopy(self.scope['historical_capture_bindings'])
        receipt = self.bind().receipt()
        self.assertEqual(len(receipt['current_semantic_row_evidence']), 118)
        self.assertIsNone(trace['raw_failure_identity'])
        self.assertEqual(trace['variant'], 'nonstrict')
        self.assertEqual(self.scope['historical_capture_bindings'], historical)

    def test_passing_original_cannot_select_strict_or_its_hashes(self):
        trace = next(iter(self.traces.values()))
        trace['variant'] = 'strict'
        self.blocked()
        trace['variant'] = 'nonstrict'
        for field in ('trace_sha256', 'generated_sha256', 'config_sha256'):
            with self.subTest(field=field):
                old = trace[field]
                trace[field] = trace['variant_summaries']['strict'][field]
                self.blocked()
                trace[field] = old

    def test_passing_original_still_requires_complete_opposite_variant(self):
        trace = next(iter(self.traces.values()))
        opposite = trace['variant_summaries'].pop('strict')
        self.blocked()
        trace['variant_summaries']['strict'] = opposite
        opposite['original_eof_reached'] = False
        self.blocked()

    def test_passing_original_mandatory_failure_is_not_reclassified(self):
        trace = next(iter(self.traces.values()))
        for target, field, value in (
                (trace, 'mandatory_failures', ['mock-site']),
                (trace, 'mandatory_failed_site_ids', ['mock-site']),
                (trace['variant_summaries']['strict'], 'mandatory_failed_site_ids', ['mock-site']),
                (trace['variant_summaries']['nonstrict'], 'mandatory_failed_site_ids', ['mock-site'])):
            with self.subTest(variant=target.get('variant'), field=field):
                old = target[field]; target[field] = value
                self.blocked(); target[field] = old
        self.assertIsNone(trace['raw_failure_identity'])

    def test_genuine_failure_identity_selects_its_strict_variant(self):
        trace = next(iter(self.traces.values()))
        trace['raw_failure_identity'] = dict(path=trace['fixture'], strict=True,
                                            line=7, signature='synthetic failing assertion')
        trace['variant'] = 'strict'
        for field in ('engine_sha256', 'trace_sha256', 'generated_sha256', 'config_sha256'):
            trace[field] = trace['variant_summaries']['strict'][field]
        self.bind()
        trace['variant'] = 'nonstrict'
        self.blocked()

    def test_present_failure_requires_complete_well_typed_identity(self):
        trace = next(iter(self.traces.values()))
        for value in ({'strict': False}, False, [], '',
                dict(path=trace['fixture'], strict=0, line=7, signature='mock'),
                dict(path=trace['fixture'], strict=False, line=0, signature='mock'),
                dict(path='../unsafe.js', strict=False, line=7, signature='mock'),
                dict(path=trace['fixture'], strict=False, line=7, signature=None)):
            with self.subTest(value=value):
                trace['raw_failure_identity'] = value
                self.blocked()

    def test_missing_failure_identity_field_is_not_a_passing_capture(self):
        del next(iter(self.traces.values()))['raw_failure_identity']
        self.blocked()

    def selected_provider_trace(self):
        old_path = next(iter(self.traces))
        path = 'test262/test/intl402/Temporal/ZonedDateTime/links.js'
        trace = self.traces.pop(old_path)
        self.traces[path] = trace
        self.fixture_hashes[path] = self.fixture_hashes.pop(old_path)
        for key in ('stable_observation_expectations', 'historical_capture_bindings'):
            self.scope[key][path] = self.scope[key].pop(old_path)
        trace['fixture'] = path
        for item in trace['variant_summaries'].values():
            item['fixture'] = path
        self.persist()
        diagnostic = dict(schema='qjs-original-selected-provider-diagnostic-v1',
            fixture_sha256=trace['fixture_sha256'], normal_policy='system-first',
            normal_engine_sha256=self.context['qjs_sha256'], engine_sha256='b' * 64,
            source_tree_sha256=self.context['production_source_map_sha256'],
            profile_sha256=self.context['effective_profile_sha256'],
            compiler_configuration_sha256='c' * 64, preflight_input_proof_sha256='d' * 64,
            conformance_accepted=False, semantic_adjudication_complete=False)
        self.context['selected_provider_diagnostic'] = diagnostic
        role = dict(schema='qjs-selected-provider-trace-role-v1', fixture=path,
            fixture_sha256=trace['fixture_sha256'],
            ordinary_engine_sha256=self.context['qjs_sha256'],
            diagnostic_engine_sha256=diagnostic['engine_sha256'],
            source_tree_sha256=diagnostic['source_tree_sha256'],
            profile_sha256=diagnostic['profile_sha256'],
            compiler_configuration_sha256=diagnostic['compiler_configuration_sha256'],
            preflight_input_proof_sha256=diagnostic['preflight_input_proof_sha256'],
            actual_comparison_canonical_sha256='e' * 64,
            conformance_accepted=False, semantic_adjudication_complete=False)
        for item in (trace, *trace['variant_summaries'].values()):
            item['engine_sha256'] = diagnostic['engine_sha256']
            item['selected_provider_diagnostic_role'] = copy.deepcopy(role)
        for item in trace['current_capture_binding']['variant_bindings'].values():
            item['engine_sha256'] = diagnostic['engine_sha256']
        return trace

    def test_passing_selected_provider_keeps_separate_authenticated_engine(self):
        trace = self.selected_provider_trace()
        self.assertIsNone(trace['raw_failure_identity'])
        self.bind()
        self.assertNotEqual(trace['engine_sha256'], self.context['qjs_sha256'])

    def test_selected_provider_role_mutations_block_passing_capture(self):
        trace = self.selected_provider_trace()
        for target in (trace, *trace['variant_summaries'].values()):
            for field in ('diagnostic_engine_sha256', 'ordinary_engine_sha256',
                          'compiler_configuration_sha256', 'preflight_input_proof_sha256'):
                with self.subTest(variant=target.get('variant'), field=field):
                    role = target['selected_provider_diagnostic_role']
                    old = role[field]; role[field] = 'f' * 64
                    self.blocked(); role[field] = old
        del trace['variant_summaries']['strict']['selected_provider_diagnostic_role']
        self.blocked()

    def test_witness_identity_derives_only_reviewed_sources_and_preserves_policy(self):
        historical = copy.deepcopy(self.policy)
        old_bindings = copy.deepcopy(historical['witness_source_bindings'])
        self.files['src/mock-engine.c'] = b'reviewed reconstructed mock implementation'
        for binding in self.scope['source_bindings']:
            if binding['path'] == 'src/mock-engine.c':
                binding['sha256'] = self.binding(binding['path'])['sha256']
        self.persist()
        original_bytes = self.files[POLICY_PATH]
        bound = self.bind()
        derived = bound.document_for('diagnostic-policy')
        self.assertEqual(derived['witness_source_bindings'],
                         [self.binding('src/mock-engine.c')])
        self.assertEqual({k: v for k, v in derived.items() if k not in
                          ('witness_source_bindings', 'production_source_map_sha256')},
                         {k: v for k, v in self.policy.items() if k not in
                          ('witness_source_bindings', 'production_source_map_sha256')})
        receipt = bound.receipt()['witness_source_derivation']
        self.assertEqual(receipt['historical_policy_binding'], self.binding(POLICY_PATH))
        self.assertEqual(receipt['approved_static_scope_binding'], self.binding(SCOPE_PATH))
        self.assertEqual(receipt['production_source_map_sha256'], canonical(self.sources))
        self.assertEqual(receipt['ordered_bindings'], [dict(
            historical=old_bindings[0], current=self.binding('src/mock-engine.c'))])
        self.assertTrue(receipt['identity_only'])
        self.assertFalse(receipt['observations_changed'])
        self.assertFalse(receipt['conformance_accepted'])
        self.assertEqual(self.files[POLICY_PATH], original_bytes)
        self.assertEqual(self.policy['witness_source_bindings'], old_bindings)

    def test_witness_order_is_preserved_in_document_and_receipt(self):
        helper = sorted(IDENTITY_HELPERS)[-1]
        self.policy['witness_source_bindings'] = [self.binding(helper),
                                                 self.binding('src/mock-engine.c')]
        self.persist()
        bound = self.bind()
        self.assertEqual(bound.document_for('diagnostic-policy')['witness_source_bindings'],
                         self.policy['witness_source_bindings'])
        self.assertEqual([item['historical'] for item in
                         bound.receipt()['witness_source_derivation']['ordered_bindings']],
                         self.policy['witness_source_bindings'])

    def test_witness_unknown_or_duplicate_paths_are_rejected(self):
        original = copy.deepcopy(self.policy['witness_source_bindings'])
        self.files['src/unreviewed.c'] = b'present in checkout but not approved'
        for bindings in ([self.binding('src/unreviewed.c')], original + original, [],
                         [dict(path='../unsafe.c', sha256='a' * 64)]):
            with self.subTest(bindings=bindings):
                self.policy['witness_source_bindings'] = bindings
                self.persist()
                self.blocked()

    def test_witness_revalidates_current_source_policy_and_static_scope(self):
        for path in ('src/mock-engine.c', POLICY_PATH, SCOPE_PATH):
            with self.subTest(path=path):
                bound = self.bind()
                old = self.files[path]
                self.files[path] += b'changed after authentication'
                with self.assertRaises(ValueError):
                    bound.document_for('diagnostic-policy')
                self.files[path] = old

    def test_witness_current_identity_requires_review_not_only_checkout_map(self):
        self.files['src/mock-engine.c'] = b'unreviewed replacement'
        self.persist()
        self.blocked()

    def test_current_map_advances_without_rewriting_static_expectations(self):
        historical = copy.deepcopy(self.scope['historical_capture_bindings'])
        expected = copy.deepcopy(self.scope['stable_observation_expectations'])
        old = self.bind().document_for('diagnostic-policy')
        self.files['docs/mock-unrelated.txt'] = b'additional tracked source'
        self.persist()
        bound = self.bind()
        fresh = bound.document_for('diagnostic-policy')
        self.assertNotEqual(old['production_source_map_sha256'], fresh['production_source_map_sha256'])
        self.assertEqual({key: value for key, value in old.items() if key != 'production_source_map_sha256'},
                         {key: value for key, value in fresh.items() if key != 'production_source_map_sha256'})
        self.assertEqual(self.scope['historical_capture_bindings'], historical)
        self.assertEqual(self.scope['stable_observation_expectations'], expected)
        self.assertIsNone(json.loads(self.files[POLICY_PATH])['production_source_map_sha256'])
        receipt = bound.receipt()
        self.assertEqual(receipt['complete_current_source_files'], self.sources)
        self.assertFalse(receipt['conformance_accepted'])
        self.assertFalse(receipt['failure_waiver'])

    def test_compiler_engine_change_retains_exact_semantics_and_historical_identity(self):
        old = self.scope['historical_capture_bindings']
        self.context['qjs_sha256'] = '8' * 64
        self.rebind_current_identity()
        receipt = self.bind().receipt()
        first = next(iter(receipt['current_semantic_row_evidence'].values()))
        self.assertEqual(first['historical_capture_binding'], next(iter(old.values())))
        self.assertNotEqual(first['historical_capture_binding'], first['current_capture_binding'])
        self.assertEqual(receipt['current_identity']['qjs_sha256'], '8' * 64)

    def test_mandatory_document_preserves_reviewed_duties(self):
        bound = self.bind()
        context = dict(self.context, native_normative_scopes=bound)
        proof = current_mandatory_review(self.binding(self.proof_path), context, self.files.__getitem__)
        self.assertEqual(proof['independently_reviewed_duties'], ['mock-duty'])
        self.assertTrue(proof['mandatory_semantics_satisfied'])
        self.assertEqual(proof['production_source_map_sha256'], canonical(self.sources))

    def test_unapproved_scope_blocks_even_after_map_is_refreshed(self):
        for key in ('source_review_complete', 'normative_review_complete'):
            with self.subTest(key=key):
                self.scope[key] = False; self.persist(); self.blocked(); self.scope[key] = True

    def test_static_scope_cannot_embed_full_map_or_future_fields(self):
        self.scope['production_source_map_sha256'] = '8' * 64
        self.persist(); self.blocked()
        self.scope['production_source_map_sha256'] = None
        self.scope['runtime_accepted'] = True
        self.persist(); self.blocked()

    def test_unapproved_and_empty_policy_block(self):
        self.policy['profile_diagnostic_authorized'] = False
        self.persist(); self.blocked()
        self.policy['profile_diagnostic_authorized'] = True
        self.policy['adjudicated_diagnostic_entries'] = []
        self.persist(); self.blocked()

    def test_unresolved_mandatory_review_blocks(self):
        self.documents[self.proof_path]['mandatory_semantics_satisfied'] = False
        self.persist(); self.blocked()

    def test_policy_cannot_embed_source_identity_or_grant_acceptance(self):
        for key in ('production_source_map_sha256', 'runtime_accepted', 'hosted_accepted',
                    'failure_waiver', 'conformance_accepted'):
            with self.subTest(key=key):
                prior = copy.deepcopy(self.policy)
                self.policy[key] = '8' * 64 if key.endswith('sha256') else True
                self.persist(); self.blocked(); self.policy = prior

    def test_all_stable_binding_fields_remain_exact(self):
        for field in STATIC_FIELDS:
            with self.subTest(field=field):
                prior = self.scope[field]; self.scope[field] = '8' * 64
                self.persist(); self.blocked(); self.scope[field] = prior

    def test_source_scope_hash_change_blocks_even_with_fresh_complete_map(self):
        self.files['src/mock-engine.c'] += b' changed semantics'
        self.persist(); self.blocked()

    def test_identity_helper_change_or_unreviewed_helper_blocks(self):
        helper = sorted(IDENTITY_HELPERS)[0]
        self.files[helper] += b' changed binder'
        self.persist(); self.blocked()
        self.scope['source_bindings'] = [binding for binding in self.scope['source_bindings']
                                         if binding['path'] != helper]
        self.persist(); self.blocked()

    def test_raw_source_bytes_and_binding_hash_tampering_block(self):
        self.files[POLICY_PATH] += b' '
        self.blocked()

    def test_scope_binding_must_be_member_of_complete_map(self):
        binding = self.binding(SCOPE_PATH); binding['sha256'] = '8' * 64
        with self.assertRaises(ValueError):
            bind_static_scopes(binding, context=self.context, sources=self.sources,
                fixture_hashes=self.fixture_hashes, validated=self.validated, raw=self.raw,
                raw_sha256='c' * 64, load_bytes=self.files.__getitem__)

    def test_complete_source_map_identity_cannot_be_truncated(self):
        del self.sources['src/mock-engine.c']; self.blocked()

    def test_unknown_document_or_unsafe_path_cannot_receive_identity(self):
        bound = self.bind()
        for binding in (dict(path='../outside.json', sha256='a' * 64),
                        dict(path=POLICY_PATH, sha256='8' * 64)):
            with self.subTest(binding=binding), self.assertRaises(ValueError):
                bound.document(binding, 'diagnostic-policy')

    def test_static_document_rechecked_on_every_read(self):
        bound = self.bind()
        binding = self.binding(self.proof_path)
        self.files[self.proof_path] += b' '
        with self.assertRaises(ValueError):
            bound.document(binding, 'mandatory-review')

    def test_changed_semantic_hash_count_configuration_or_source_blocks(self):
        trace = next(iter(self.traces.values()))
        for field in ('events_sha256', 'event_count', 'stable_configuration_sha256'):
            with self.subTest(field=field):
                observation = trace['stable_observation']['observations']['strict']
                old = observation[field]; observation[field] = 8 if field == 'event_count' else '8' * 64
                self.blocked(); observation[field] = old
        trace['stable_observation']['source_bindings']['mock-witness.js'] = '8' * 64
        self.blocked()

    def test_changed_whole_rtf_semantics_block(self):
        trace = list(self.traces.values())[-1]
        trace['stable_observation']['observations']['whole_parsed_rtf_sha256'] = '8' * 64
        self.blocked()

    def test_missing_original_or_opposite_variant_is_not_a_pass(self):
        del next(iter(self.traces.values()))['variant_summaries']['strict']
        self.blocked()

    def test_original_scope_counts_and_rtf_receipt_are_mandatory(self):
        for key, value in (('complete_original_source_count', 117),
                           ('complete_original_default_variant_count', 208),
                           ('current_RTF_recaptured', False)):
            with self.subTest(key=key):
                prior = self.validated[key]; self.validated[key] = value
                self.blocked(); self.validated[key] = prior

    def test_unknown_or_disappeared_expected_fixture_blocks(self):
        del self.scope['stable_observation_expectations'][next(iter(self.traces))]
        self.persist(); self.blocked()

    def test_mandatory_failure_missing_eof_or_provider_engine_substitution_blocks(self):
        trace = next(iter(self.traces.values()))
        for key, value in (('mandatory_failed_site_ids', ['mock-site']),
                           ('original_eof_reached', False), ('engine_sha256', '8' * 64)):
            with self.subTest(key=key):
                prior = trace[key]; trace[key] = value
                self.blocked(); trace[key] = prior

    def test_missing_or_changed_raw_and_repository_receipts_block(self):
        self.validated['raw_full_evidence_sha256'] = '8' * 64
        self.blocked()
        self.validated['raw_full_evidence_sha256'] = 'c' * 64
        self.context['mandatory_repository_gates_passed'] = False
        self.blocked()

    def test_raw_baseline_change_is_not_rebound(self):
        self.raw['input_identity']['baseline_sha256'] = '8' * 64
        self.blocked()

    def test_runtime_fabricated_scope_object_is_rejected(self):
        with self.assertRaises(ValueError):
            current_mandatory_review(self.binding(self.proof_path),
                dict(self.context, native_normative_scopes={'approved': True}), self.files.__getitem__)

    def test_returned_documents_and_receipts_do_not_mutate_static_approvals(self):
        bound = self.bind()
        document = bound.document_for('diagnostic-policy')
        document['profile_diagnostic_authorized'] = False
        receipt = bound.receipt(); receipt['failure_waiver'] = True
        self.assertTrue(bound.document_for('diagnostic-policy')['profile_diagnostic_authorized'])
        self.assertFalse(bound.receipt()['failure_waiver'])


if __name__ == '__main__':
    unittest.main()
