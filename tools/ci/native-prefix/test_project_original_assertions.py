"""Pure mocked projection regressions; execution belongs to root."""
import unittest
import copy
import json
import hashlib
from pathlib import Path
from project_original_assertions import validated_semantic_observation, strict_json
from project_original_assertions import project_variants


class VariantProjectionTests(unittest.TestCase):
    def variants(self):
        return {name: dict(variant=name, fixture_capture_complete=True, original_eof_reached=True,
            mandatory_failed_site_ids=[], continued_data_site_ids=[], totalized_original_data_site_ids=[],
            totalized_original_data_obligations=0, data_mismatches=0,
            all_original_expressions_evaluated=True, engine_sha256="a"*64, trace_sha256=pin*64,
            generated_sha256="c"*64, config_sha256="d"*64)
            for name, pin in (("strict", "e"), ("nonstrict", "f"))}

    def test_opposite_variant_obligations_aggregate_for_either_raw_identity(self):
        for raw_strict in (False, True):
            with self.subTest(raw_strict=raw_strict):
                variants = self.variants()
                selected = "strict" if raw_strict else "nonstrict"
                other = "nonstrict" if raw_strict else "strict"
                variants[other].update(continued_data_site_ids=["later", "array"],
                    totalized_original_data_site_ids=["array"], totalized_original_data_obligations=3,
                    data_mismatches=4, all_original_expressions_evaluated=False)
                projected = project_variants(variants, dict(strict=raw_strict))
                self.assertEqual(projected["variant"], selected)
                self.assertEqual(projected["trace_sha256"], variants[selected]["trace_sha256"])
                self.assertEqual(projected["continued_data_site_ids"], ["array", "later"])
                self.assertEqual(projected["totalized_original_data_site_ids"], ["array"])
                self.assertEqual(projected["totalized_original_data_obligations"], 3)
                self.assertEqual(projected["data_mismatches"], 4)
                self.assertFalse(projected["all_original_expressions_evaluated"])
                self.assertEqual(projected["variant_summaries"], variants)

    def test_overlap_has_unique_sites_and_combined_occurrence_count(self):
        variants = self.variants()
        for item in variants.values():
            item.update(continued_data_site_ids=["array"], totalized_original_data_site_ids=["array"],
                        totalized_original_data_obligations=2, data_mismatches=2,
                        all_original_expressions_evaluated=False)
        projected = project_variants(variants, dict(strict=False))
        self.assertEqual(projected["totalized_original_data_site_ids"], ["array"])
        self.assertEqual(projected["totalized_original_data_obligations"], 4)
        self.assertEqual(projected["data_mismatches"], 4)

    def test_opposite_variant_mandatory_failure_blocks(self):
        variants = self.variants()
        variants["strict"]["mandatory_failed_site_ids"] = ["mandatory"]
        with self.assertRaisesRegex(ValueError, "mandatory failure"):
            project_variants(variants, dict(strict=False))

    def test_both_current_variant_capture_hashes_are_preserved(self):
        variants = self.variants()
        projected = project_variants(variants, dict(strict=True))
        self.assertEqual(projected["current_capture_binding"]["variant_bindings"], {
            name: {key: item[key] for key in
                ("engine_sha256", "trace_sha256", "generated_sha256", "config_sha256")}
            for name, item in variants.items()})



class CompleteStableObservationTests(unittest.TestCase):
    """Pure MOCK inputs; these tests confer no actual observation authority."""
    def setUp(self):
        self.events = [dict(sequence=1, kind='start', engine_sha256='a'*64,
            source_tree_sha256='b'*64, config_sha256='c'*64, variant='strict',
            profile_sha256='d'*64, source_site='mock-source'),
            dict(sequence=2, kind='value', engine_sha256='a'*64,
                source_tree_sha256='b'*64, config_sha256='c'*64,
                value=dict(kind='number', value='-0', unknown=dict(engine_sha256='nested-pin')),
                bigint=dict(kind='bigint', value='9007199254740993'),
                utf=dict(value='𝄞', utf16=[55348, 56606], codepoints=[119070]),
                descriptor=dict(writable=False, enumerable=True, configurable=False),
                error=dict(name='Test262Error', message='exact mocked later operand'),
                mandatory=True, adapter='exact-mock', operands=[1, 2]),
            dict(sequence=3, kind='completion', site_counts={'mock-source':1},
                original_eof_reached=True, unknown_future_field={'ordered':[1, 2]})]
        self.config = dict(engine_sha256='a'*64, source_tree_sha256='b'*64,
            _config_sha256='c'*64, fixture='test262/test/mock.js', variant='strict',
            profile={'numberingSystem':'latn'}, guards={'H1':True},
            sites={'mock-source':{'source_sha256':'d'*64, 'mandatory':True}},
            aggregate_adapters={'array':{'fields':['first', 'later']}}, unknown={'order':[1, 2]})

    def observation(self):
        return validated_semantic_observation(
            '\n'.join(json.dumps(event, ensure_ascii=True) for event in self.events), self.config, {})

    def test_only_top_level_build_envelopes_are_removed(self):
        original = self.observation()
        for event in self.events:
            for field in ('engine_sha256', 'source_tree_sha256', 'config_sha256'):
                event[field] = 'e'*64
        for field in ('engine_sha256', 'source_tree_sha256', '_config_sha256'):
            self.config[field] = 'e'*64
        self.assertEqual(self.observation(), original)
        self.events[1]['value']['unknown']['engine_sha256'] = 'changed-nested-pin'
        self.assertNotEqual(self.observation(), original)

    def test_every_non_envelope_event_field_and_order_remains_significant(self):
        mutations = [('value', {'kind':'number', 'value':'0'}),
            ('bigint', {'kind':'bigint', 'value':'9007199254740992'}),
            ('utf', {'value':'𝄞', 'utf16':[0], 'codepoints':[119070]}),
            ('descriptor', {'writable':True}), ('error', {'message':'changed-later'}),
            ('mandatory', False), ('adapter', 'changed'), ('operands', [2, 1]),
            ('unknown_future_field', {'new':True}), ('site_source_sha256', 'changed')]
        for field, value in mutations:
            with self.subTest(field=field):
                self.setUp(); original = self.observation()
                self.events[1][field] = value
                self.assertNotEqual(self.observation(), original)
        self.setUp(); original = self.observation(); self.events.reverse()
        self.assertNotEqual(self.observation(), original)
        self.setUp(); original = self.observation(); self.events.pop()
        self.assertNotEqual(self.observation(), original)

    def test_configuration_retains_unknowns_profile_guards_sites_adapters_and_order(self):
        for field in ('fixture', 'variant', 'profile', 'guards', 'sites', 'aggregate_adapters', 'unknown'):
            with self.subTest(field=field):
                self.setUp(); original = self.observation()
                self.config[field] = {'changed':{'mandatory':True}} if field == 'sites' else {'changed':True}
                self.assertNotEqual(self.observation(), original)
        self.setUp(); original = self.observation()
        self.config['config_sha256'] = 'retained-not-an-authorized-config-exclusion'
        self.assertNotEqual(self.observation(), original)

    def test_ambiguous_and_non_finite_json_blocks(self):
        for text in ('{"x":1,"x":2}', '{"nested":{"x":1,"x":2}}', '{"x":NaN}', '{"x":Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                strict_json(text)


if __name__ == "__main__":
    unittest.main()


class ProjectionSourceIdentityTests(unittest.TestCase):
    def setUp(self):
        from project_original_assertions import require_projection_identity
        self.check = require_projection_identity
        self.raw = dict(production_source_map_sha256='a'*64,
            actual_full_tracked_source_map_sha256='a'*64,
            actual_initialized_checkout_commit='b'*40, qjs_sha256='c'*64,
            fixture_source_map_sha256='d'*64)
        self.bindings = dict(self.raw, source_tree_sha256='a'*64, engine_sha256='c'*64)
        self.current = dict(source_binding_mode='current-checkout',
            production_source_map_sha256=None,
            historical_baseline_production_source_map_sha256='e'*64)

    def test_matching_current_checkout_identity(self):
        self.check(self.current, self.bindings, self.raw)

    def test_wrong_or_missing_current_identity_fails(self):
        for key in ('production_source_map_sha256', 'actual_full_tracked_source_map_sha256',
                    'actual_initialized_checkout_commit', 'qjs_sha256'):
            for remove in (False, True):
                with self.subTest(key=key, missing=remove):
                    raw = dict(self.raw)
                    if remove:
                        raw.pop(key)
                    else:
                        raw[key] = 'f' * len(raw[key])
                    with self.assertRaises(ValueError):
                        self.check(self.current, self.bindings, raw)
        for key in ('source_tree_sha256', 'production_source_map_sha256',
                    'actual_full_tracked_source_map_sha256', 'engine_sha256'):
            with self.subTest(binding=key):
                binding = dict(self.bindings)
                binding.pop(key)
                with self.assertRaises(ValueError):
                    self.check(self.current, binding, self.raw)

    def test_unsupported_or_incomplete_template_fails(self):
        for change in (dict(source_binding_mode='unsupported'),
                       dict(production_source_map_sha256='a'*64),
                       dict(historical_baseline_production_source_map_sha256=None)):
            with self.subTest(change=change):
                with self.assertRaises(ValueError):
                    self.check(dict(self.current, **change), self.bindings, self.raw)

    def test_legacy_pinned_mode_behavior(self):
        for mode in (None, 'pinned-production-map'):
            with self.subTest(mode=mode):
                config = dict(production_source_map_sha256='a'*64)
                if mode is not None:
                    config['source_binding_mode'] = mode
                self.check(config, self.bindings, self.raw)
                with self.assertRaises(ValueError):
                    self.check(dict(config, production_source_map_sha256='f'*64),
                               self.bindings, self.raw)



class SelectedProviderEnvelopeTests(unittest.TestCase):
    """Mock identity tests confer no provider or hosted acceptance authority."""
    def setUp(self):
        self.fixture = 'test262/test/intl402/Temporal/ZonedDateTime/links.js'
        self.diag = dict(schema='qjs-original-selected-provider-diagnostic-v1',
            normal_engine_sha256='a'*64, engine_sha256='b'*64,
            source_tree_sha256='c'*64, profile_sha256='d'*64,
            fixture_sha256='e'*64, compiler_configuration_sha256='f'*64,
            preflight_input_proof_sha256='1'*64, normal_policy='system-first',
            conformance_accepted=False, semantic_adjudication_complete=False)
        selected = dict(schema='qjs-original-selected-provider-input-v1',
            fixture=self.fixture, fixture_sha256='e'*64, profile_sha256='d'*64,
            ordinary_engine_sha256='a'*64, diagnostic_engine_sha256='b'*64,
            source_tree_sha256='c'*64, compiler_configuration_sha256='f'*64,
            preflight_input_proof_sha256='1'*64, primary_metadata_sha256='2'*64,
            decoder_sha256='3'*64, system_root='/mock/zoneinfo',
            records={'America/New_York': {'offsets': {'0': -18000000000000},
                'tzif_sha256': '4'*64, 'origin': 'system'}},
            conformance_accepted=False, semantic_adjudication_complete=False)
        self.config = dict(fixture=self.fixture, fixture_sha256='e'*64,
            engine_sha256='b'*64, source_tree_sha256='c'*64,
            selected_provider_input=selected)
        self.bindings = dict(engine_sha256='b'*64, source_tree_sha256='c'*64,
            profile_sha256='d'*64, selected_provider_diagnostic=self.diag)

    def stable(self):
        from project_original_assertions import stable_provider_envelope
        return stable_provider_envelope(self.config, self.bindings)

    def test_authenticated_build_identities_are_row_neutral(self):
        original = copy.deepcopy(self.config)
        before = self.stable()
        self.assertEqual(self.config, original)
        selected = self.config['selected_provider_input']
        for field, diagnostic_field in (
                ('ordinary_engine_sha256', 'normal_engine_sha256'),
                ('diagnostic_engine_sha256', 'engine_sha256'),
                ('source_tree_sha256', 'source_tree_sha256'),
                ('compiler_configuration_sha256', 'compiler_configuration_sha256'),
                ('preflight_input_proof_sha256', 'preflight_input_proof_sha256')):
            self.setUp()
            selected = self.config['selected_provider_input']
            selected[field] = self.diag[diagnostic_field] = '7'*64
            if field == 'diagnostic_engine_sha256':
                self.config['engine_sha256'] = self.bindings['engine_sha256'] = '7'*64
            if field == 'source_tree_sha256':
                self.config['source_tree_sha256'] = self.bindings['source_tree_sha256'] = '7'*64
            after = self.stable()
            self.assertEqual(after['selected_provider_input'], before['selected_provider_input'])

    def test_identity_substitution_without_current_proof_is_rejected(self):
        for field in ('ordinary_engine_sha256', 'diagnostic_engine_sha256',
                'source_tree_sha256', 'compiler_configuration_sha256',
                'preflight_input_proof_sha256'):
            with self.subTest(field=field):
                self.setUp(); self.config['selected_provider_input'][field] = '7'*64
                with self.assertRaises(ValueError): self.stable()

    def test_provider_semantics_and_unknown_fields_remain_significant(self):
        for field in ('records', 'primary_metadata_sha256', 'decoder_sha256',
                'system_root', 'unknown_future_data'):
            with self.subTest(field=field):
                self.setUp(); before = self.stable()
                self.config['selected_provider_input'][field] = {'changed': True}
                self.assertNotEqual(self.stable(), before)

    def test_missing_input_wrong_fixture_and_wrong_schema_are_rejected(self):
        for change in ('missing', 'fixture', 'schema', 'type'):
            with self.subTest(change=change):
                self.setUp()
                if change == 'missing': del self.config['selected_provider_input']
                elif change == 'fixture': self.config['fixture'] = 'another.js'
                elif change == 'schema': self.config['selected_provider_input']['schema'] = 'unknown'
                else: self.config['selected_provider_input'] = []
                with self.assertRaises(ValueError): self.stable()

    def test_acceptance_flags_and_system_first_policy_are_preserved(self):
        for owner in ('input', 'diagnostic'):
            for field in ('conformance_accepted', 'semantic_adjudication_complete'):
                self.setUp()
                target = self.config['selected_provider_input'] if owner == 'input' else self.diag
                target[field] = True
                with self.assertRaises(ValueError): self.stable()
        self.setUp(); self.diag['normal_policy'] = 'embedded-only'
        with self.assertRaises(ValueError): self.stable()

    def test_unrelated_config_is_not_normalized(self):
        from project_original_assertions import stable_provider_envelope
        config = {'fixture': 'another.js', 'unknown': {'source_tree_sha256': 'x'}}
        self.assertIs(stable_provider_envelope(config, {}), config)


class CurrentProjectionSourcePinsTests(unittest.TestCase):
    def test_invocation_authenticates_current_projector_and_configuration(self):
        root = Path(__file__).resolve().parents[3]
        invocation = json.loads((root / 'tools/ci/native-prefix/native-witness-invocation.json').read_text())
        for field in ('validator', 'configuration'):
            with self.subTest(field=field):
                source = root / invocation[field]
                self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),
                                 invocation[field + '_sha256'])
