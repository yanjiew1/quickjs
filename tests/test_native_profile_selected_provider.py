"""Pure profile engine-role checks; no native runtime/host data is trusted here."""
import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/ci/native-prefix'))
from native_profile_gate import expected_trace_engine


class DiagnosticEngineRoleTests(unittest.TestCase):
    def setUp(self):
        fixture='test262/test/intl402/Temporal/ZonedDateTime/links.js'
        diagnostic=dict(schema='qjs-original-selected-provider-diagnostic-v1',
            normal_engine_sha256='a'*64,engine_sha256='b'*64,
            source_tree_sha256='c'*64,profile_sha256='d'*64,fixture_sha256='e'*64,
            compiler_configuration_sha256='f'*64,preflight_input_proof_sha256='1'*64,
            normal_policy='system-first',conformance_accepted=False,semantic_adjudication_complete=False)
        role=dict(schema='qjs-selected-provider-trace-role-v1',fixture=fixture,
            ordinary_engine_sha256='a'*64,diagnostic_engine_sha256='b'*64,
            source_tree_sha256='c'*64,profile_sha256='d'*64,fixture_sha256='e'*64,
            compiler_configuration_sha256='f'*64,preflight_input_proof_sha256='1'*64,
            actual_comparison_canonical_sha256='2'*64,
            conformance_accepted=False,semantic_adjudication_complete=False)
        self.context=dict(qjs_sha256='a'*64,production_source_map_sha256='c'*64,
            effective_profile_sha256='d'*64,selected_provider_diagnostic=diagnostic)
        self.trace=dict(fixture=fixture,fixture_sha256='e'*64,selected_provider_diagnostic_role=role)

    def test_only_links_uses_authenticated_diagnostic_engine(self):
        self.assertEqual(expected_trace_engine(self.trace,self.context),'b'*64)
        self.assertEqual(expected_trace_engine({'fixture':'other.js'},self.context),'a'*64)

    def test_other_fixture_cannot_claim_diagnostic(self):
        self.trace['fixture']='other.js'
        with self.assertRaises(ValueError): expected_trace_engine(self.trace,self.context)

    def test_missing_provider_role_rejected(self):
        del self.trace['selected_provider_diagnostic_role']
        with self.assertRaises(ValueError): expected_trace_engine(self.trace,self.context)

    def test_each_identity_and_input_substitution_rejected(self):
        for key in ('ordinary_engine_sha256','diagnostic_engine_sha256','source_tree_sha256',
                    'profile_sha256','fixture_sha256','compiler_configuration_sha256',
                    'preflight_input_proof_sha256'):
            with self.subTest(key=key):
                trace=copy.deepcopy(self.trace)
                trace['selected_provider_diagnostic_role'][key]='9'*64
                with self.assertRaises(ValueError): expected_trace_engine(trace,self.context)

    def test_provider_role_does_not_claim_conformance(self):
        for field in ('conformance_accepted','semantic_adjudication_complete'):
            trace=copy.deepcopy(self.trace)
            trace['selected_provider_diagnostic_role'][field]=True
            with self.assertRaises(ValueError): expected_trace_engine(trace,self.context)

    def test_system_first_policy_preserved(self):
        self.context['selected_provider_diagnostic']['normal_policy']='embedded-only'
        with self.assertRaises(ValueError): expected_trace_engine(self.trace,self.context)


if __name__=='__main__':unittest.main()
