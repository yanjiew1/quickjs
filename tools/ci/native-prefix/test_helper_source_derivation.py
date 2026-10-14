"""Bound helper substitutions retain historical reviews and semantic checks."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from unittest import mock

import helper_source_derivation as helpers
from native_profile_gate import semantic_row_evidence


class HelperDerivationTests(unittest.TestCase):
    def setUp(self):
        self.files = {helpers.MODULE: b'mock derivation source'}
        for name in helpers.HELPERS:
            self.files[helpers.PREFIX + name] = ('current:' + name).encode()
        self.old = {name: helpers.digest(('historical:' + name).encode())
                    for name in helpers.HELPERS}
        self.old['witness-runtime.js'] = 'a' * 64
        self.observation = dict(schema='native-complete-stable-observation-v2',
            capture_kind='rtf-variant-neutral', source_bindings=self.old,
            observations={'whole_parsed_rtf_sha256': 'b' * 64})
        self.review = dict(stable_observation_expectation=self.observation,
                          current_capture_binding={'capture_kind': 'rtf-variant-neutral'})
        origin = helpers.PREFIX + 'reviews/mock-historical.json'
        self.files[origin] = json.dumps(self.review).encode()
        self.manifest = dict(schema='quickjs-reconstructed-observation-helper-bindings-v1',
            identity_only=True, observations_changed=False, conformance_accepted=False,
            current_helper_sources={name: helpers.digest(self.files[helpers.PREFIX + name])
                                    for name in helpers.HELPERS}, historical_generations={
                helpers.map_digest(self.old): dict(source_bindings=self.old,
                    origin_review={'path': origin, 'sha256': helpers.digest(self.files[origin])})})
        self.refresh()

    def refresh(self):
        self.files[helpers.MANIFEST] = json.dumps(self.manifest).encode()
        self.sources = {name: helpers.digest(raw) for name, raw in self.files.items()}

    def authenticate(self):
        return helpers.authenticate_helper_derivation(self.sources,
            helpers.map_digest(self.sources), self.files.__getitem__)

    def test_exact_identity_change_does_not_mutate_review_or_observations(self):
        before = copy.deepcopy(self.review)
        derivation = self.authenticate()
        result = derivation.derive_review(self.review)
        self.assertEqual(self.review, before)
        self.assertEqual(result['stable_observation_expectation']['observations'],
                         before['stable_observation_expectation']['observations'])
        self.assertEqual(result['current_capture_binding'], before['current_capture_binding'])
        self.assertEqual(result['stable_observation_expectation']['source_bindings']['witness-runtime.js'],
                         'a' * 64)
        self.assertEqual({name: result['stable_observation_expectation']['source_bindings'][name]
                          for name in helpers.HELPERS}, self.manifest['current_helper_sources'])

    def test_events_and_configuration_remain_strict_semantic_inputs(self):
        derivation = self.authenticate()
        current = derivation.derive_observation(self.observation)
        trace = dict(stable_observation=current,
                     current_capture_binding={'capture_kind': 'rtf-variant-neutral'})
        receipt = semantic_row_evidence(trace, {'path': 'review', 'sha256': 'c' * 64},
                                        self.review, derivation)
        self.assertIn('helper_source_derivation', receipt)
        trace['stable_observation']['observations']['whole_parsed_rtf_sha256'] = 'd' * 64
        with self.assertRaises(ValueError):
            semantic_row_evidence(trace, {}, self.review, derivation)

    def test_projector_and_scope_use_the_same_authenticated_row_receipt(self):
        from project_original_assertions import current_row_evidence
        from native_normative_scope import BoundNormativeScopes
        derivation = self.authenticate()
        origin = helpers.PREFIX + 'reviews/mock-historical.json'
        binding = dict(path=origin, sha256=self.sources[origin])
        trace = dict(stable_observation=derivation.derive_observation(self.observation),
                     current_capture_binding={'capture_kind': 'rtf-variant-neutral'})
        projected = current_row_evidence(trace, binding, self.files.__getitem__, derivation)
        expected = semantic_row_evidence(trace, binding, self.review, derivation)
        self.assertEqual(projected, expected)
        scope = BoundNormativeScopes({origin: self.review},
            {origin: ('mandatory-review', binding['sha256'])}, self.files.__getitem__,
            helpers.map_digest(self.sources), {}, helper_derivation=derivation)
        self.assertIs(scope.helper_derivation(), derivation)
        self.assertEqual(scope.document(binding, 'mandatory-review')['stable_observation_expectation'],
                         trace['stable_observation'])

    def test_isolated_projector_loads_sibling_helpers_outside_repository(self):
        projector = Path(__file__).with_name('project_original_assertions.py').resolve()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # Caller modules are untrusted; -I and the validator's own helper
            # directory must win before it rejects our missing capture packet.
            (root / 'helper_source_derivation.py').write_text(
                'raise RuntimeError("UNTRUSTED_CALLER_MODULE")\n')
            config = root / 'config.json'
            config.write_text(json.dumps({'packet_root': str(root / 'missing-packet')}))
            bindings = root / 'bindings.json'
            bindings.write_text(json.dumps({'current_source_files': {},
                                           'source_tree_sha256': 'a' * 64}))
            raw = root / 'raw.json'
            raw.write_text('{}')
            result = subprocess.run([sys.executable, '-I', '-B', str(projector),
                '--config', str(config), '--bindings', str(bindings),
                '--raw-full-run', str(raw), '--capture', str(root),
                '--summary', str(root / 'summary.json')], cwd=root,
                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('FileNotFoundError', result.stderr)
            self.assertIn('SEAL.json', result.stderr)
            self.assertNotIn('ModuleNotFoundError', result.stderr)
            self.assertNotIn('UNTRUSTED_CALLER_MODULE', result.stderr)

    def test_projector_startup_has_a_safe_loader_before_authentication(self):
        import project_original_assertions as projector
        class StartupCheckpoint(Exception):
            pass
        helper_path = Path(__file__).with_name('helper_source_derivation.py')
        # A path relative to the caller keeps the production loader's safety
        # checks exercised without reading an engine or Test262 capture.
        from os.path import relpath
        relative_helper = relpath(helper_path)
        def authenticate(sources, source_map, loader):
            self.assertEqual(loader(relative_helper), helper_path.read_bytes())
            raise StartupCheckpoint()
        argv = ['projector']
        for option in ('config', 'bindings', 'raw-full-run', 'capture', 'summary'):
            argv.extend(['--' + option, option])
        with mock.patch.object(sys, 'argv', argv), mock.patch.object(
                projector, 'read_json', side_effect=[{}, {'source_tree_sha256': 'a' * 64}]), mock.patch.object(
                helpers, 'authenticate_helper_derivation', side_effect=authenticate):
            with self.assertRaises(StartupCheckpoint):
                projector.main()

    def test_changed_non_helper_or_mixed_generation_is_rejected(self):
        derivation = self.authenticate()
        for name in ('witness-runtime.js', 'run.py'):
            bad = copy.deepcopy(self.observation)
            bad['source_bindings'][name] = 'f' * 64
            with self.assertRaises(ValueError):
                derivation.derive_observation(bad)

    def test_unknown_helper_is_rejected(self):
        self.manifest['current_helper_sources']['unreviewed.py'] = 'a' * 64
        self.refresh()
        with self.assertRaises(ValueError):
            self.authenticate()

    def test_wrong_current_helper_digest_is_rejected(self):
        self.manifest['current_helper_sources']['run.py'] = 'f' * 64
        self.refresh()
        with self.assertRaises(ValueError):
            self.authenticate()

    def test_wrong_historical_generation_cannot_be_reapproved_by_its_manifest(self):
        generation = next(iter(self.manifest['historical_generations'].values()))
        generation['source_bindings']['run.py'] = 'f' * 64
        self.manifest['historical_generations'] = {helpers.map_digest(generation['source_bindings']): generation}
        self.refresh()
        with self.assertRaises(ValueError):
            self.authenticate()

    def test_changed_manifest_or_origin_review_bytes_are_rejected(self):
        for path in (helpers.MANIFEST, helpers.PREFIX + 'reviews/mock-historical.json'):
            before = self.files[path]
            self.files[path] = b'changed'
            with self.assertRaises(ValueError):
                self.authenticate()
            self.files[path] = before

    def test_source_map_mismatch_is_rejected(self):
        with self.assertRaises(ValueError):
            helpers.authenticate_helper_derivation(self.sources, 'f' * 64, self.files.__getitem__)

    def test_legacy_exact_scope_has_no_implicit_derivation(self):
        self.assertIsNone(helpers.authenticate_helper_derivation({}, 'f' * 64,
                         lambda name: self.fail('Unrequested source read')))


if __name__ == '__main__':
    unittest.main()
