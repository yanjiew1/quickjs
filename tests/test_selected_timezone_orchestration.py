"""Pure typed-provider input and rejection tests; no compiler/engine launch."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] /
                      'tools/ci/native-prefix/original-assertions'))
import selected_timezone_orchestration as subject
from selected_timezone_proof import EPOCH_NS


class SelectedProviderInputTests(unittest.TestCase):
    def setUp(self):
        self.scope = tempfile.TemporaryDirectory()
        self.addCleanup(self.scope.cleanup)
        self.root = Path(self.scope.name)
        self.normal = self.root / 'qjs-normal'
        self.diagnostic = self.root / 'qjs-diagnostic'
        self.probe = self.root / 'probe'
        for path, data in ((self.normal, b'ordinary'), (self.diagnostic, b'observer'),
                           (self.probe, b'probe')):
            path.write_bytes(data)
        self.bindings = dict(engine_sha256=subject.file_sha(self.normal),
            source_tree_sha256='a' * 64, profile_sha256='b' * 64,
            actual_initialized_checkout_commit='1'*40,
            premise_source_guard_review=dict(engine_sha256=subject.file_sha(self.normal),
                source_tree_sha256='a' * 64, profile_sha256='b' * 64,
                source_review_complete=True, conformance_accepted=False,
                semantic_adjudication_complete=False))
        configuration = dict(diagnostic_configuration={'CONFIG_TZ_CAPTURE_SNAPSHOT':'y',
                'CONFIG_ICU':'n', 'normal_policy':'system-first'},
            ordinary_engine_sha256=self.bindings['engine_sha256'],
            source_tree_sha256=self.bindings['source_tree_sha256'],
            diagnostic_engine_sha256=subject.file_sha(self.diagnostic), probe_sha256=subject.file_sha(self.probe),
            make_args=['make','CONFIG_TZ_CAPTURE_SNAPSHOT=y','CONFIG_ICU=n'])
        compiler = self.root / 'compiler.json'
        compiler.write_text(json.dumps(configuration))
        identity = self.root / 'identity.json'
        identity.write_text(json.dumps(dict(candidate_sha='1'*40, actual_production_source_map_sha256='a'*64,
                                           run_id='123', run_attempt='1')))
        self.bindings['selected_provider_diagnostic'] = dict(schema=subject.DIAG_SCHEMA,
            normal_engine_sha256=self.bindings['engine_sha256'],
            source_tree_sha256='a'*64, profile_sha256='b'*64,
            engine_sha256=subject.file_sha(self.diagnostic), qjs=str(self.diagnostic),
            probe=str(self.probe), probe_sha256=subject.file_sha(self.probe),
            compiler_configuration=str(compiler), compiler_configuration_sha256=subject.file_sha(compiler),
            execution_identity=str(identity), execution_identity_sha256=subject.file_sha(identity),
            hosted_identity=dict(workflow='mock',job='mock',run_id='123',run_attempt='1'),
            normal_policy='system-first', conformance_accepted=False,
            semantic_adjudication_complete=False)
        offsets = {epoch:0 for epoch in EPOCH_NS}
        row = dict(primary='UTC', origin='system', source_status=0, tzif_sha256='c'*64,
                   system_root='/usr/share/zoneinfo', offsets=offsets)
        selected = dict(schema=subject.INPUT_SCHEMA, fixture=subject.TZ_FIXTURE,
            fixture_sha256='d'*64, diagnostic_engine_sha256=subject.file_sha(self.diagnostic),
            ordinary_engine_sha256=self.bindings['engine_sha256'],
            source_tree_sha256='a'*64, profile_sha256='b'*64,
            primary_metadata_sha256=subject.canonical({'UTC':'UTC'}),
            system_root='/usr/share/zoneinfo', records={'UTC':row},
            epoch_nanoseconds=list(EPOCH_NS), conformance_accepted=False,
            semantic_adjudication_complete=False)
        self.config = dict(fixture=subject.TZ_FIXTURE, fixture_sha256='d'*64,
            engine_sha256=subject.file_sha(self.diagnostic), source_tree_sha256='a'*64,
            profile_sha256='b'*64, selected_provider_input=selected,
            premise_data={'timezoneRecords':{'UTC':dict(primary='UTC',tzif_sha256='e'*64,offsets=offsets)}})

    def test_original_role_preserves_identity(self):
        subject.validate_config(self.config)
        derived = subject.fixture_bindings(self.bindings, subject.TZ_FIXTURE)
        self.assertEqual(derived['engine_sha256'], subject.file_sha(self.diagnostic))
        self.assertEqual(derived['source_tree_sha256'], self.bindings['source_tree_sha256'])
        self.assertEqual(derived['profile_sha256'], self.bindings['profile_sha256'])
        self.assertIs(derived['premise_source_guard_review']['conformance_accepted'], False)
        self.assertEqual(self.bindings['engine_sha256'], subject.file_sha(self.normal))

    def test_other_fixture_keeps_ordinary_binding(self):
        self.assertIs(subject.fixture_bindings(self.bindings, 'other.js'), self.bindings)
        subject.validate_config({'fixture':'other.js'})

    def test_missing_dynamic_role_rejected(self):
        self.config.pop('selected_provider_input')
        with self.assertRaisesRegex(ValueError, 'lacks authenticated'):
            subject.validate_config(self.config)

    def test_other_fixture_cannot_use_dynamic_role(self):
        self.config['fixture'] = 'other.js'
        with self.assertRaises(ValueError): subject.validate_config(self.config)

    def test_ordinary_binary_cannot_claim_diagnostic(self):
        self.bindings['selected_provider_diagnostic']['engine_sha256'] = self.bindings['engine_sha256']
        self.bindings['selected_provider_diagnostic']['qjs'] = str(self.normal)
        with self.assertRaises(ValueError):
            subject.fixture_bindings(self.bindings, subject.TZ_FIXTURE)

    def test_changed_diagnostic_bytes_rejected(self):
        self.diagnostic.write_bytes(b'changed')
        with self.assertRaises(ValueError):
            subject.fixture_bindings(self.bindings, subject.TZ_FIXTURE)

    def test_wrong_primary_rejected(self):
        self.config['selected_provider_input']['records']['UTC']['primary']='Etc/UTC'
        with self.assertRaises(ValueError): subject.validate_config(self.config)

    def test_wrong_finite_epoch_rejected(self):
        self.config['selected_provider_input']['epoch_nanoseconds'].append('1')
        with self.assertRaises(ValueError): subject.validate_config(self.config)

    def test_fallback_requires_exact_embedded_data(self):
        row = self.config['selected_provider_input']['records']['UTC']
        row['origin'] = 'embedded'
        row['source_status'] = 2
        with self.assertRaisesRegex(ValueError, 'fallback'):
            subject.validate_config(self.config)
        row['tzif_sha256'] = 'e'*64
        subject.validate_config(self.config)
        row['offsets'] = dict(row['offsets'], **{EPOCH_NS[-1]:10**9})
        with self.assertRaises(ValueError): subject.validate_config(self.config)

    def test_noninteger_expected_offset_rejected(self):
        self.config['selected_provider_input']['records']['UTC']['offsets'][EPOCH_NS[0]] = False
        with self.assertRaises(ValueError): subject.validate_config(self.config)

    def test_policy_and_qualification_remain_false(self):
        self.bindings['selected_provider_diagnostic']['normal_policy']='embedded-only'
        with self.assertRaises(ValueError):
            subject.fixture_bindings(self.bindings, subject.TZ_FIXTURE)
        self.bindings['selected_provider_diagnostic']['normal_policy']='system-first'
        self.bindings['selected_provider_diagnostic']['conformance_accepted']=True
        with self.assertRaises(ValueError):
            subject.fixture_bindings(self.bindings, subject.TZ_FIXTURE)

    def test_directory_is_owned_fresh_and_bounded(self):
        subject.fresh_directory(self.root/'child',self.root)
        with self.assertRaises(ValueError): subject.fresh_directory(self.root/'child',self.root)
        with self.assertRaises(ValueError): subject.fresh_directory(self.root.parent/'outside',self.root)


if __name__ == '__main__':
    unittest.main()
