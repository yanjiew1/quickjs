"""Prepared mocked textual fixture tests; root has not executed these yet."""
import unittest
from native_test262_diagnostic import reconcile, match_source_triage


class PureDiagnosticTests(unittest.TestCase):
    path = "test262/test/intl402/Example/a.js"
    signature = "unexpected error: Test262Error: mismatch"

    def parse(self, stdout="", stderr="Result: 0/1 error\n", rc=0,
              baseline=(), denominator=1):
        return reconcile(stdout, stderr, rc, list(baseline), {self.path}, denominator)

    def failure(self):
        return self.parse(self.path + ":9: " + self.signature + "\n",
                          "Result: 1/1 error, 1 new\n", 1)

    def test_empty_pass(self):
        p = self.parse()
        self.assertTrue(p["raw_parser_complete"])
        self.assertFalse(p["raw_collection_complete"])
        self.assertFalse(p["conformance_accepted"])

    def test_actual_new_failure_is_visible(self):
        p = self.failure()
        self.assertTrue(p["raw_parser_complete"])
        self.assertEqual(p["raw_runner_returncode"], 1)
        self.assertEqual(p["counters"]["new"], 1)
        self.assertEqual(p["observed_failure_paths"], [self.path])
        self.assertFalse(p["runtime_accepted"])

    def test_summary_grammar_and_stream(self):
        invalid = ["Result: 0/1 errors\n", "Result: 00/1 error\n",
                   "Result: 0/1 error, 0 new\n", "Result: 0/1 error extra\n",
                   "Result: 0/1 error\nResult: 0/1 error\n",
                   "Result: 0/1 error, 1 new, 1 skipped\n"]
        for raw in invalid:
            with self.subTest(raw=raw):
                self.assertFalse(self.parse(stderr=raw)["raw_parser_complete"])
        self.assertFalse(self.parse(stdout="Result: 0/1 error\n", stderr="")["raw_parser_complete"])

    def test_rc_and_missing_source_fail(self):
        self.assertFalse(self.parse(rc=1)["raw_parser_complete"])
        self.assertFalse(self.parse(rc=False)["raw_parser_complete"])
        self.assertFalse(self.parse(denominator=2)["raw_parser_complete"])

    def test_duplicate_failing_variant(self):
        line = self.path + ":9: " + self.signature + "\n"
        self.assertFalse(self.parse(line + line, "Result: 1/1 error, 1 new\n", 1)["raw_parser_complete"])

    def test_baseline_is_inference(self):
        baseline = dict(path=self.path, line=9, strict=False,
                        signature="Test262Error: mismatch", body="Test262Error: mismatch")
        p = self.parse(stderr="Result: 1/1 error\n", baseline=[baseline])
        self.assertTrue(p["raw_parser_complete"])
        self.assertEqual(p["raw_observations"], [])
        self.assertEqual(p["baseline_matched_inferred_paths"], [self.path])
        self.assertFalse(p["baseline_references_are_fresh_runtime_observations"])
        p = self.parse(stderr="Result: 1/1 error\n", baseline=[baseline, baseline])
        self.assertFalse(p["raw_parser_complete"])

    def test_sanitizer_infra_and_unknown_feature_block(self):
        for line in ["==123==ERROR: AddressSanitizer: heap-use-after-free",
                     "run-test262: cannot open harness",
                     "owner.c:12:3: runtime error: overflow"]:
            with self.subTest(line=line):
                self.assertFalse(self.parse(stdout=line + "\n")["raw_parser_complete"])
        p = self.parse(stdout=self.path + ":1: unknown feature: newtag\n")
        self.assertFalse(p["raw_parser_complete"])
        p = self.parse(stdout=self.path + ":9: unexpected error: Test262Error: AddressSanitizer data string\n",
                       stderr="Result: 1/1 error, 1 new\n", rc=1)
        self.assertTrue(p["raw_parser_complete"])
        self.assertEqual(p["blocking_tool_records"], [])

    def test_exact_triage_identity_has_no_acceptance(self):
        p = self.failure()
        record = dict(path=self.path, observed_first_failure=p["raw_observations"][0],
                      fixture_identity=dict(reviewed_overlay_sha256="a" * 64))
        match = match_source_triage(p, [record], {self.path: "a" * 64})
        self.assertEqual(match["source_triage_errors"], [])
        self.assertFalse(match["final_profile_diagnostic_adjudicated"])
        self.assertFalse(match["conformance_accepted"])
        self.assertTrue(match_source_triage(p, [record], {self.path: "b" * 64})["source_triage_errors"])
        changed = dict(record, observed_first_failure=dict(record["observed_first_failure"], line=10))
        self.assertTrue(match_source_triage(p, [changed], {self.path: "a" * 64})["source_triage_errors"])
        self.assertTrue(match_source_triage(p, [], {})["source_triage_errors"])


if __name__ == "__main__":
    unittest.main()
