"""Pure source-approval rejection tests; no engine or Test262 execution."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import native_correction_transition as correction


class CorrectionSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        actual = Path(__file__).resolve().parents[3]
        self.files = {}
        for path in (correction.MODULE, correction.POLICY,
                     correction.SCOPE,
                     correction.PREFIX + 'original-assertions/PREMISE-SOURCE-PINS.json'):
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((actual / path).read_bytes())
            self.files[path] = hashlib.sha256(target.read_bytes()).hexdigest()
        # Pure MOCK inputs do not depend on an installed or approved correction.
        subject = self.root / correction.SUBJECT
        subject.parent.mkdir(parents=True, exist_ok=True)
        subject.write_bytes(b'MOCK corrected implementation bytes; never compiled')
        review = self.root / correction.REVIEW
        review.parent.mkdir(parents=True, exist_ok=True)
        review.write_bytes(b'MOCK independent semantic review; never runtime evidence')
        for path in (correction.SUBJECT, correction.REVIEW):
            self.files[path] = correction.sha((self.root / path).read_bytes())
        pins = json.loads((self.root / correction.PREFIX /
                           'original-assertions/PREMISE-SOURCE-PINS.json').read_bytes())
        policy = json.loads((self.root / correction.POLICY).read_bytes())
        scope = json.loads((self.root / correction.SCOPE).read_bytes())
        manifest = dict(schema='native-hour-width-semantic-correction-v1',
            source_review_complete=True, semantic_review_complete=False,
            engine_algorithms_changed=True, failure_waiver=False, conformance_accepted=False,
            implementation_source=dict(path=correction.SUBJECT,
                historical_sha256=pins[correction.SUBJECT],
                current_sha256=self.files[correction.SUBJECT]),
            historical_policy_sha256=self.files[correction.POLICY],
            historical_observations_sha256=correction.canonical(scope['stable_observation_expectations']),
            repaired_sources=[{key: row[key] for key in ('path', 'fixture_sha256')}
                for row in policy['adjudicated_diagnostic_entries']
                if row['path'] in correction.REPAIRED_SOURCES],
            reviewed_observation_changes=None,
            independent_review_binding=dict(path=correction.REVIEW,
                sha256=self.files[correction.REVIEW]))
        (self.root / correction.MANIFEST).write_text(json.dumps(manifest))
        self.files[correction.MANIFEST] = correction.sha((self.root / correction.MANIFEST).read_bytes())

    def approve(self):
        return correction.source_approval(self.files, self.root)

    def authenticate(self):
        return correction.authenticate_correction(self.files,
            lambda path: (self.root / path).read_bytes())

    def synthetic_review(self):
        # MOCK expected hashes test authentication/derivation, never the engine.
        scope = json.loads((self.root / correction.SCOPE).read_bytes())
        historical = scope['stable_observation_expectations']
        def change(data):
            data['semantic_review_complete'] = True
            data['historical_observations_sha256'] = correction.canonical(historical)
            data['reviewed_observation_changes'] = {}
            for path in correction.OBSERVATION_SOURCES:
                old = historical[path]['observations']
                new = copy.deepcopy(old)
                for variant in new.values():
                    variant['events_sha256'] = 'f' * 64
                data['reviewed_observation_changes'][path] = dict(
                    historical=old, corrected=new, review_basis='MOCK reviewed event delta')
        self.modify(change)
        return historical

    def modify(self, function):
        path = self.root / correction.MANIFEST
        data = json.loads(path.read_bytes())
        function(data)
        path.write_text(json.dumps(data))
        self.files[correction.MANIFEST] = correction.sha(path.read_bytes())

    def test_source_approval_explicitly_does_not_grant_semantic_approval(self):
        receipt = self.approve()
        self.assertTrue(receipt['engine_algorithms_changed'])
        self.assertFalse(receipt['semantic_review_complete'])
        self.assertFalse(receipt['failure_waiver'])
        self.assertFalse(receipt['conformance_accepted'])
        self.assertTrue(correction.pending_capture(dict(
            premise_source_compatibility=dict(algorithm_correction=receipt))))

    def test_changed_subject_module_policy_and_manifest_bytes_are_rejected(self):
        for path in self.files:
            if path in (correction.SCOPE, correction.REVIEW):
                continue  # Source-only capture does not approve normative observations.
            original = (self.root / path).read_bytes()
            (self.root / path).write_bytes(original + b' ')
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.approve()
            (self.root / path).write_bytes(original)

    def test_historical_subject_pin_and_unrelated_correction_are_rejected(self):
        for key, value in [('path', 'src/intl/provider-native.c'),
                           ('historical_sha256', 'f' * 64),
                           ('current_sha256', 'e' * 64)]:
            original = (self.root / correction.MANIFEST).read_bytes()
            self.modify(lambda data: data['implementation_source'].update({key: value}))
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.approve()
            (self.root / correction.MANIFEST).write_bytes(original)
            self.files[correction.MANIFEST] = correction.sha(original)

    def test_missing_duplicate_and_replaced_fixture_bindings_are_rejected(self):
        mutations = [lambda data: data['repaired_sources'].pop(),
                     lambda data: data['repaired_sources'].__setitem__(0,
                         copy.deepcopy(data['repaired_sources'][1])),
                     lambda data: data['repaired_sources'][0].update(fixture_sha256='f' * 64)]
        for mutate in mutations:
            original = (self.root / correction.MANIFEST).read_bytes()
            self.modify(mutate)
            with self.assertRaises(ValueError):
                self.approve()
            (self.root / correction.MANIFEST).write_bytes(original)
            self.files[correction.MANIFEST] = correction.sha(original)

    def test_pending_observations_and_waiver_are_rejected(self):
        for key, value in [('reviewed_observation_changes', {}),
                           ('failure_waiver', True), ('conformance_accepted', True),
                           ('engine_algorithms_changed', False),
                           ('source_review_complete', False)]:
            original = (self.root / correction.MANIFEST).read_bytes()
            self.modify(lambda data: data.update({key: value}))
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.approve()
            (self.root / correction.MANIFEST).write_bytes(original)
            self.files[correction.MANIFEST] = correction.sha(original)

    def test_source_only_approval_cannot_qualify(self):
        with self.assertRaisesRegex(ValueError, 'Source-only'):
            self.authenticate()

    def test_review_derivation_preserves_every_unrelated_field_and_source(self):
        historical = self.synthetic_review()
        transition = self.authenticate()
        manifest = json.loads((self.root / correction.MANIFEST).read_bytes())
        changed = manifest['reviewed_observation_changes']
        for path, old in historical.items():
            derived = transition.derive_observation(path, old)
            self.assertEqual(derived['source_bindings'], old['source_bindings'])
            if path in changed:
                self.assertEqual(derived['observations'], changed[path]['corrected'])
            else:
                self.assertEqual(derived, old)

    def test_policy_subtracts_exactly_six_and_leaves_selections_unchanged(self):
        self.synthetic_review()
        transition = self.authenticate()
        policy = json.loads((self.root / correction.POLICY).read_bytes())
        policy['postfix_raw_counters']['failed_source_count'] = 109
        old = copy.deepcopy(policy)
        derived = transition.derive_policy(policy)
        self.assertEqual(policy, old)
        self.assertEqual(derived['required_selections'], old['required_selections'])
        self.assertEqual(len(derived['adjudicated_diagnostic_entries']), 103)
        self.assertEqual(derived['postfix_raw_counters']['new'], 103)
        self.assertEqual(derived['postfix_raw_counters']['failed_source_count'], 103)
        repaired = {row['path'] for row in transition.receipt()['repaired_sources']}
        self.assertEqual(derived['adjudicated_diagnostic_entries'],
            [row for row in old['adjudicated_diagnostic_entries'] if row['path'] not in repaired])
        for field in ('new', 'failed_source_count', 'executed_source_count'):
            changed = copy.deepcopy(old)
            changed['postfix_raw_counters'][field] += 1
            with self.subTest(field=field), self.assertRaises(ValueError):
                transition.derive_policy(changed)

    def test_altered_historical_observations_and_missing_variants_rejected(self):
        self.synthetic_review()
        original = (self.root / correction.MANIFEST).read_bytes()
        mutations = [lambda data: data.update(historical_observations_sha256='f' * 64),
            lambda data: next(iter(data['reviewed_observation_changes'].values()))['historical']
                ['strict'].update(events_sha256='e' * 64),
            lambda data: next(iter(data['reviewed_observation_changes'].values()))['corrected'].pop('strict'),
            lambda data: data['reviewed_observation_changes'].update({'test262/test/MOCK-unknown.js': {}}),
            lambda data: data['reviewed_observation_changes'].pop(next(iter(data['reviewed_observation_changes'])))]
        for mutate in mutations:
            self.modify(mutate)
            with self.assertRaises(ValueError):
                self.authenticate()
            (self.root / correction.MANIFEST).write_bytes(original)
            self.files[correction.MANIFEST] = correction.sha(original)

    def test_fixture_and_context_substitutions_rejected(self):
        self.synthetic_review()
        transition = self.authenticate()
        transition.check_context(dict(production_source_map_sha256=correction.canonical(self.files)))
        with self.assertRaises(ValueError):
            transition.check_context(dict(production_source_map_sha256='f' * 64))
        with self.assertRaisesRegex(ValueError, 'fixture'):
            correction.authenticate_correction(self.files,
                lambda path: (self.root / path).read_bytes(), {})

    def test_semantic_scope_bytes_and_complete_inventory_are_authenticated(self):
        self.synthetic_review()
        path = self.root / correction.SCOPE
        path.write_bytes(path.read_bytes() + b' ')
        with self.assertRaisesRegex(ValueError, 'scope'):
            self.authenticate()
        self.files[correction.SCOPE] = correction.sha(path.read_bytes())
        scope = json.loads(path.read_bytes())
        scope['stable_observation_expectations'].pop(next(iter(scope['stable_observation_expectations'])))
        path.write_text(json.dumps(scope))
        self.files[correction.SCOPE] = correction.sha(path.read_bytes())
        with self.assertRaisesRegex(ValueError, 'complete observation'):
            self.authenticate()

    def test_independent_review_bytes_and_binding_are_authenticated(self):
        self.synthetic_review()
        path = self.root / correction.REVIEW
        path.write_bytes(path.read_bytes() + b' ')
        with self.assertRaisesRegex(ValueError, 'independent review'):
            self.authenticate()

    def test_actual_trace_fixture_field_drives_both_review_boundaries(self):
        from native_profile_gate import semantic_row_evidence
        historical = self.synthetic_review()
        transition = self.authenticate()
        path = next(iter(correction.REPAIRED_SOURCES))
        old = historical[path]
        actual = transition.derive_observation(path, old)
        # The sealed validator emits fixture, not path, in a trace summary.
        trace = dict(fixture=path, stable_observation=actual,
                     current_capture_binding=dict(capture_kind='script-variants'))
        review = dict(path=path, stable_observation_expectation=old,
                      current_capture_binding=dict(capture_kind='script-variants'))
        result = semantic_row_evidence(trace, {}, review, correction=transition)
        self.assertEqual(result['stable_observation'], actual)
        self.assertEqual(result['semantic_correction'], transition.receipt())
        scope_review = dict(review)
        scope_review.pop('path')
        self.assertEqual(semantic_row_evidence(trace, {}, scope_review, correction=transition), result)


if __name__ == '__main__':
    unittest.main()
