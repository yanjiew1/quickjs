"""Prepared mocked policy tests. Root must execute; no engine/compiler needed."""
import copy
import hashlib
import json
import unittest
from native_profile_gate import qualify_native, MANDATORY_OBLIGATIONS
from project_original_assertions import project_variants, current_row_evidence


class ExactPolicyTests(unittest.TestCase):
    def setUp(self):
        self.path = "test262/test/intl402/Locale/prototype/getCollations/output-array-values.js"
        self.context = {key: "a" * 64 for key in
            ("production_source_map_sha256", "fixture_source_map_sha256", "config_sha256",
             "baseline_sha256", "effective_profile_sha256", "qjs_sha256")}
        self.context["mandatory_repository_gates_passed"] = True
        self.raw = {"raw_parser_complete": True, "raw_collection_complete": True,
            "sealed_inputs_unchanged": True, "blocking_tool_records": [], "unknown_feature_records": [],
            "counters": {"failed_source_count":15, "executed_source_count":20,
                         "excluded":0, "skipped":0, "new":1, "changed":0, "fixed":0},
            "raw_runner_returncode":1,
            "raw_observations":[dict(path=self.path, strict=False, line=30,
                signature="unexpected error: Test262Error: exact mocked data premise", status="observed-failure")]}
        self.artifacts = {"normative.txt": b"Pinned mocked normative excerpt", "witness.js": b"mock source"}
        identity = {key:self.raw["raw_observations"][0][key] for key in ("path","strict","line","signature")}
        review = dict(self.context, **identity, schema="native-original-assertion-review-v2",
            fixture_sha256="b"*64, complete_original_assertions_reviewed=True,
            mandatory_semantics_satisfied=True, unclassified_assertion_ids=[],
            normative_proofs=[dict(revision="c"*40, anchor="sec-mocked-data",
                excerpt_binding=self.binding("normative.txt"))], reviewed_data_mismatch_site_ids=["site-1"])
        self.artifacts["review.json"] = json.dumps(review).encode()
        self.entry = dict(identity, fixture_sha256="b"*64, raw_outcome="FAIL",
                          disposition="EXACT_PROFILE_DIAGNOSTIC_REVIEWED",
                          classification="NORMATIVE_DATA_PROFILE_DIFFERENCE",
                          review_binding=self.binding("review.json"))
        proof = {key:self.context[key] for key in self.context if key.endswith("sha256")}
        proof["mandatory_semantics_satisfied"] = True
        self.artifacts["obligation.json"] = json.dumps(proof).encode()
        self.policy = dict(proof, schema="native-exact-profile-policy-v2",
            profile_diagnostic_authorized=True, failure_waiver=False, conformance_accepted=False,
            no_new_skips_exclusions_expected_errors=True, witness_source_bindings=[self.binding("witness.js")],
            mandatory_obligations=[dict(id=name,status="SATISFIED",proof_bindings=[self.binding("obligation.json")])
                                   for name in MANDATORY_OBLIGATIONS],
            adjudicated_diagnostic_entries=[self.entry], postfix_raw_counters=copy.deepcopy(self.raw["counters"]),
            postfix_raw_returncode=1)
        trace = dict(schema="native-original-assertion-trace-v2", fixture_capture_complete=True,
            original_eof_reached=True, raw_failure_identity=identity, mandatory_failures=[], mandatory_failed_site_ids=[], fixture_sha256="b"*64,
            source_tree_sha256=self.context["production_source_map_sha256"], engine_sha256=self.context["qjs_sha256"],
            profile_sha256=self.context["effective_profile_sha256"], continued_data_site_ids=["site-1"],
            totalized_original_data_obligations=[], all_original_expressions_evaluated=True)
        trace.update(trace_sha256="d"*64, generated_sha256="e"*64, config_sha256="f"*64,
                     totalized_original_data_site_ids=[], totalized_original_data_obligations=0,
                     data_mismatches=1)
        variants = {name: dict(copy.deepcopy(trace), variant=name) for name in ("strict", "nonstrict")}
        trace = self.project(variants, identity)
        review["current_capture_binding"] = copy.deepcopy(trace["current_capture_binding"])
        self.artifacts["review.json"] = json.dumps(review).encode()
        self.entry["review_binding"] = self.binding("review.json")
        self.witness = dict(schema="native-original-assertion-capture-summary-v2", full_capture_complete=True,
            independent_revalidation_complete=True, conformance_accepted=False, fixture_summaries={self.path:trace})

    @staticmethod
    def project(variants, identity):
        trace = project_variants(variants, identity)
        trace.update(raw_failure_identity=identity, mandatory_failures=[],
            totalized_original_data_obligation_count=trace["totalized_original_data_obligations"],
            totalized_original_data_obligations=trace["totalized_original_data_site_ids"])
        return trace

    def update_review(self, **fields):
        review = json.loads(self.artifacts["review.json"])
        review.update(fields)
        self.artifacts["review.json"] = json.dumps(review).encode()
        self.entry["review_binding"] = self.binding("review.json")

    def reproject(self):
        trace = self.witness["fixture_summaries"][self.path]
        trace = self.project(trace["variant_summaries"], trace["raw_failure_identity"])
        self.witness["fixture_summaries"][self.path] = trace
        return trace

    def binding(self, path):
        return dict(path=path, sha256=hashlib.sha256(self.artifacts[path]).hexdigest())

    def gate(self, selection=None):
        return qualify_native(self.raw, self.policy, self.context, {self.path:"b"*64},
                              self.witness, self.artifacts.__getitem__, selection=selection)

    def blocked(self):
        with self.assertRaises(ValueError): self.gate()

    def test_exact_mocked_failure_preserved(self):
        result = self.gate()
        self.assertTrue(result["profile_diagnostic_qualified"])
        self.assertEqual(result["raw_runner_returncode"],1)
        self.assertEqual(result["raw_counters"]["failed_source_count"],15)
        self.assertFalse(result["conformance_accepted"])

    def test_exact_required_selection_preserves_failures(self):
        name = "official-general-current-required"
        self.policy["required_selections"] = {name: dict(
            raw_counters=copy.deepcopy(self.raw["counters"]), raw_returncode=1,
            failure_identities=[copy.deepcopy(self.raw["raw_observations"][0])])}
        result = self.gate(selection=name)
        self.assertTrue(result["raw_failures_preserved"])
        self.assertEqual(result["raw_runner_returncode"], 1)
        self.assertFalse(result["conformance_accepted"])
        with self.assertRaisesRegex(ValueError, "lacks exact review"):
            self.gate(selection="general-current-required")

    def test_required_selection_drift_still_blocks(self):
        name = "official-general-current-required"
        for mutation in ("counter", "signature", "unreviewed"):
            with self.subTest(mutation=mutation):
                self.setUp()
                self.policy["required_selections"] = {name: dict(
                    raw_counters=copy.deepcopy(self.raw["counters"]), raw_returncode=1,
                    failure_identities=[copy.deepcopy(self.raw["raw_observations"][0])])}
                if mutation == "counter":
                    self.raw["counters"]["new"] += 1
                elif mutation == "signature":
                    self.raw["raw_observations"][0]["signature"] += " changed"
                else:
                    self.policy["required_selections"][name]["failure_identities"][0]["path"] = (
                        "test262/test/intl402/new.js")
                with self.assertRaises(ValueError):
                    self.gate(selection=name)

    def test_draft_policy_blocks(self):
        self.policy["profile_diagnostic_authorized"] = False; self.blocked()

    def test_generic_rc1_does_not_authorize(self):
        self.policy["adjudicated_diagnostic_entries"] = []; self.blocked()

    def test_changed_signature_blocks(self):
        self.raw["raw_observations"][0]["signature"] += " changed"; self.blocked()

    def test_new_failure_blocks(self):
        record = dict(self.raw["raw_observations"][0],path="test262/test/intl402/new.js")
        self.raw["raw_observations"].append(record); self.blocked()

    def test_changed_counter_blocks(self):
        self.raw["counters"]["new"] += 1; self.blocked()

    def test_baseline_delta_blocks(self):
        self.raw["counters"]["changed"] = 1; self.blocked()

    def test_missing_raw_capture_blocks(self):
        self.raw["raw_collection_complete"] = False; self.blocked()

    def test_sanitizer_blocks(self):
        self.raw["blocking_tool_records"] = [dict(kind="sanitizer-report")]; self.blocked()

    def test_unknown_feature_blocks(self):
        self.raw["unknown_feature_records"] = [dict(kind="unknown")]; self.blocked()

    def test_wrong_profile_blocks(self):
        self.context["effective_profile_sha256"] = "d"*64; self.blocked()

    def test_changed_fixture_blocks(self):
        self.entry["fixture_sha256"] = "d"*64; self.blocked()

    def test_missing_original_eof_blocks(self):
        self.witness["fixture_summaries"][self.path]["original_eof_reached"] = False; self.blocked()

    def test_mandatory_failure_blocks(self):
        self.witness["fixture_summaries"][self.path]["mandatory_failed_site_ids"] = ["site-2"]; self.blocked()

    def test_unreviewed_later_data_site_blocks(self):
        self.witness["fixture_summaries"][self.path]["continued_data_site_ids"].append("site-2"); self.blocked()

    def test_unreviewed_totalization_blocks(self):
        trace = self.witness["fixture_summaries"][self.path]
        trace["all_original_expressions_evaluated"] = False
        trace["totalized_original_data_obligations"] = ["site-array"]
        self.blocked()

    def test_changed_evidence_hash_blocks(self):
        self.artifacts["review.json"] += b" "; self.blocked()

    def test_missing_mandatory_repository_gate_blocks(self):
        self.context["mandatory_repository_gates_passed"] = False; self.blocked()

    def test_opposite_variant_totalization_requires_exact_adapter(self):
        trace = self.witness["fixture_summaries"][self.path]
        strict = trace["variant_summaries"]["strict"]
        strict.update(continued_data_site_ids=["site-1", "site-array"],
                      totalized_original_data_site_ids=["site-array"],
                      totalized_original_data_obligations=2, all_original_expressions_evaluated=False,
                      trace_sha256="8"*64)
        trace = self.reproject()
        self.assertEqual(trace["variant"], "nonstrict")
        self.assertFalse(trace["all_original_expressions_evaluated"])
        self.update_review(reviewed_data_mismatch_site_ids=["site-1", "site-array"],
                           current_capture_binding=copy.deepcopy(trace["current_capture_binding"]))
        with self.assertRaisesRegex(ValueError, "incomplete without exact reviewed"):
            self.gate()
        self.artifacts["adapter.js"] = b"exact mocked array adapter"
        self.update_review(data_only_array_totalization_reviewed=True,
            approved_totalized_original_data_obligation_site_ids=["site-array"],
            approved_array_adapter_bindings=[self.binding("adapter.js")])
        result = self.gate()
        self.assertTrue(result["profile_diagnostic_qualified"])
        self.assertFalse(result["all_original_expressions_evaluated"])
        self.assertEqual(result["raw_runner_returncode"], 1)

    def test_omitted_opposite_variant_totalization_blocks(self):
        trace = self.witness["fixture_summaries"][self.path]
        trace["variant_summaries"]["strict"].update(totalized_original_data_site_ids=["site-array"],
            totalized_original_data_obligations=1, all_original_expressions_evaluated=False)
        with self.assertRaisesRegex(ValueError, "aggregate omits"):
            self.gate()

    def test_stale_review_of_changed_opposite_trace_blocks(self):
        self.witness["fixture_summaries"][self.path]["variant_summaries"]["strict"]["trace_sha256"] = "1"*64
        self.reproject()
        with self.assertRaisesRegex(ValueError, "does not bind exact current"):
            self.gate()

    def test_stale_review_of_changed_engine_blocks(self):
        self.context["qjs_sha256"] = "2"*64
        for item in self.witness["fixture_summaries"][self.path]["variant_summaries"].values():
            item["engine_sha256"] = self.context["qjs_sha256"]
        self.reproject()
        with self.assertRaisesRegex(ValueError, "does not bind exact current"):
            self.gate()

    def test_stale_review_of_changed_generated_or_config_blocks(self):
        for field in ("generated_sha256", "config_sha256"):
            with self.subTest(field=field):
                self.setUp()
                self.witness["fixture_summaries"][self.path]["variant_summaries"]["strict"][field] = "3"*64
                self.reproject()
                with self.assertRaisesRegex(ValueError, "does not bind exact current"):
                    self.gate()

    def test_exact_current_review_rebind_allows_same_raw_failure(self):
        self.witness["fixture_summaries"][self.path]["variant_summaries"]["strict"]["trace_sha256"] = "4"*64
        trace = self.reproject()
        self.update_review(current_capture_binding=copy.deepcopy(trace["current_capture_binding"]))
        self.assertTrue(self.gate()["raw_failures_preserved"])

    def test_stale_current_rtf_command_binding_blocks(self):
        trace = self.witness["fixture_summaries"][self.path]
        trace.pop("variant_summaries")
        trace.update(variant="reviewed-variant-neutral-native-input-matrix",
                     strict_variant_neutral_capture=True,
                     current_capture_binding=dict(capture_kind="rtf-variant-neutral",
                         engine_sha256=trace["engine_sha256"], trace_sha256=trace["trace_sha256"],
                         sampler_sha256=trace["generated_sha256"], command_sha256="5"*64,
                         current_bindings_sha256="6"*64))
        self.update_review(current_capture_binding=copy.deepcopy(trace["current_capture_binding"]))
        self.assertTrue(self.gate()["raw_failures_preserved"])
        trace["current_capture_binding"]["command_sha256"] = "7"*64
        with self.assertRaisesRegex(ValueError, "does not bind exact current"):
            self.gate()



class CurrentSemanticPolicyTests(unittest.TestCase):
    """Pure MOCK stable historical review + independently projected current row."""
    def setUp(self):
        self.case = ExactPolicyTests()
        self.case.setUp()
        self.trace = self.case.witness['fixture_summaries'][self.case.path]
        self.trace['stable_observation'] = dict(schema='native-complete-stable-observation-v2',
            capture_kind='script-variants', source_bindings={'mock-witness.js':'b'*64},
            observations={name:dict(events_sha256=pin*64, event_count=3,
                                   stable_configuration_sha256='d'*64)
                          for name, pin in (('strict','e'), ('nonstrict','f'))})
        self.case.update_review(stable_observation_expectation=copy.deepcopy(self.trace['stable_observation']))
        self.historical_bytes = self.case.artifacts['review.json']
        self.derive()

    def derive(self):
        self.trace['current_row_evidence'] = current_row_evidence(
            self.trace, self.case.entry['review_binding'], self.case.artifacts.__getitem__)

    def test_fresh_engine_checkout_trace_generated_config_bindings_keep_review_immutable(self):
        self.case.context['qjs_sha256'] = '1'*64
        self.case.context['production_source_map_sha256'] = '2'*64
        self.case.policy['production_source_map_sha256'] = '2'*64
        proof = json.loads(self.case.artifacts['obligation.json'])
        proof['production_source_map_sha256'] = '2'*64
        self.case.artifacts['obligation.json'] = json.dumps(proof).encode()
        for obligation in self.case.policy['mandatory_obligations']:
            obligation['proof_bindings'] = [self.case.binding('obligation.json')]
        for item in self.trace['variant_summaries'].values():
            item.update(engine_sha256='1'*64, source_tree_sha256='2'*64,
                        trace_sha256='3'*64, generated_sha256='4'*64, config_sha256='5'*64)
        stable = self.trace['stable_observation']
        self.trace = self.case.reproject()
        self.trace['stable_observation'] = stable
        self.derive()
        self.assertTrue(self.case.gate()['raw_failures_preserved'])
        self.assertEqual(self.case.artifacts['review.json'], self.historical_bytes)
        self.assertNotEqual(self.trace['current_row_evidence']['historical_capture_binding'],
                            self.trace['current_row_evidence']['current_capture_binding'])

    def test_opposite_variant_later_operand_configuration_source_and_count_drift_blocks(self):
        for mutation in ('events_sha256', 'stable_configuration_sha256', 'event_count', 'source'):
            with self.subTest(mutation=mutation):
                self.setUp()
                if mutation == 'source':
                    self.trace['stable_observation']['source_bindings']['mock-witness.js'] = '1'*64
                else:
                    self.trace['stable_observation']['observations']['strict'][mutation] = (
                        4 if mutation == 'event_count' else '1'*64)
                with self.assertRaisesRegex(ValueError, 'bounded review required'):
                    self.derive()
                with self.assertRaises(ValueError):
                    self.case.gate()

    def test_caller_equality_flag_and_substituted_current_receipt_do_not_authorize(self):
        self.trace['current_row_evidence']['equality'] = True
        with self.assertRaisesRegex(ValueError, 'Derived current row evidence'):
            self.case.gate()
        self.derive()
        self.trace['current_row_evidence']['current_capture_binding'] = {'capture_kind':'script-variants'}
        with self.assertRaisesRegex(ValueError, 'Derived current row evidence'):
            self.case.gate()

    def test_stale_or_wrong_historical_review_binding_blocks(self):
        self.trace['current_row_evidence']['stable_review_binding'] = dict(path='review.json', sha256='0'*64)
        with self.assertRaisesRegex(ValueError, 'Derived current row evidence'):
            self.case.gate()
        self.case.artifacts['review.json'] += b' '
        with self.assertRaisesRegex(ValueError, 'Changed evidence bytes'):
            self.derive()

    def test_wrong_exact_current_binding_cannot_hide_behind_equal_observations(self):
        self.trace['current_capture_binding']['variant_bindings']['strict']['trace_sha256'] = '0'*64
        self.derive()
        with self.assertRaisesRegex(ValueError, 'Current variant capture binding differs'):
            self.case.gate()

    def test_whole_rtf_drift_and_stale_current_command_receipt_block(self):
        self.trace.pop('variant_summaries')
        self.trace.update(variant='reviewed-variant-neutral-native-input-matrix',
            strict_variant_neutral_capture=True,
            current_capture_binding=dict(capture_kind='rtf-variant-neutral',
                engine_sha256=self.trace['engine_sha256'], trace_sha256=self.trace['trace_sha256'],
                sampler_sha256=self.trace['generated_sha256'], command_sha256='5'*64,
                current_bindings_sha256='6'*64))
        self.trace['stable_observation'].update(capture_kind='rtf-variant-neutral',
            observations={'whole_parsed_rtf_sha256':'7'*64})
        self.case.update_review(stable_observation_expectation=copy.deepcopy(self.trace['stable_observation']),
                                current_capture_binding=copy.deepcopy(self.trace['current_capture_binding']))
        self.derive()
        self.assertTrue(self.case.gate()['raw_failures_preserved'])
        self.trace['current_capture_binding']['command_sha256'] = '8'*64
        with self.assertRaisesRegex(ValueError, 'Derived current row evidence'):
            self.case.gate()
        self.derive()
        self.assertTrue(self.case.gate()['raw_failures_preserved'])
        self.trace['stable_observation']['observations']['whole_parsed_rtf_sha256'] = '9'*64
        with self.assertRaisesRegex(ValueError, 'bounded review required'):
            self.derive()

    def test_draft_policy_profile_fixture_raw_counter_and_totalization_still_block(self):
        for mutation in ('draft', 'profile', 'fixture', 'counter', 'totalization'):
            with self.subTest(mutation=mutation):
                self.setUp()
                if mutation == 'draft': self.case.policy['profile_diagnostic_authorized'] = False
                elif mutation == 'profile': self.case.context['effective_profile_sha256'] = '0'*64
                elif mutation == 'fixture': self.case.entry['fixture_sha256'] = '0'*64
                elif mutation == 'counter': self.case.raw['counters']['new'] += 1
                else: self.trace['totalized_original_data_obligations'] = ['unreviewed-array']
                with self.assertRaises(ValueError): self.case.gate()


if __name__ == "__main__":
    unittest.main()
