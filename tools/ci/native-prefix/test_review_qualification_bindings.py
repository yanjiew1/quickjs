"""Check published review identities without executing engines or Test262."""
import hashlib
import json
from pathlib import Path
import unittest


class PublishedReviewBindingsTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(__file__).resolve().parents[3]
        self.here = self.root / 'tools/ci/native-prefix'
        self.scope = self.read(self.here / 'native-normative-static-scope.json')
        self.policy = self.read(self.here / 'native-profile-diagnostic-policy.json')

    def read(self, path):
        return json.loads(Path(path).read_bytes())

    def test_every_adjudicated_review_binds_its_stable_scope_and_failure(self):
        entries = self.policy['adjudicated_diagnostic_entries']
        self.assertEqual(len(entries), 109)
        for entry in entries:
            with self.subTest(fixture=entry['path']):
                binding = entry['review_binding']
                path = self.root / binding['path']
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),
                                 binding['sha256'])
                review = self.read(path)
                for key in ('fixture_source_map_sha256', 'config_sha256',
                            'baseline_sha256', 'effective_profile_sha256'):
                    self.assertEqual(review[key], self.scope[key])
                for key in ('path', 'strict', 'line', 'signature', 'fixture_sha256'):
                    self.assertEqual(review[key], entry[key])

    def test_required_selection_names_counts_and_preserved_review_payloads(self):
        selections = self.read(self.here / 'selections.json')
        reviewed = self.policy['required_selections']
        self.assertEqual(set(reviewed), {s['name'] for s in selections})
        payload_hashes = {
            'official-calendar-required':
                '104d6b5f737f46ee474c1964e712a57a50f704cabb96efcf5eea2ba3b461d9f3',
            'official-date-required-cases':
                '2c7a56ae8267169a57098155140681c1a1694ecbff191eeec4e51e287e229e39',
            'official-general-current-required':
                '367b1632a689f349ef06cfcffc982aad81832439f1f688f72018ce74539bd789',
        }
        identity = lambda e: tuple(e[k] for k in ('path', 'strict', 'line', 'signature'))
        adjudicated = {identity(e) for e in self.policy['adjudicated_diagnostic_entries']}
        for selection in selections:
            name = selection['name']
            with self.subTest(selection=name):
                payload = reviewed[name]
                raw = (self.here / selection['path']).read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(), selection['sha256'])
                sources = set(raw.decode().splitlines())
                self.assertEqual(len(sources), selection['distinct_source_count'])
                self.assertEqual(payload['raw_counters']['executed_source_count'], len(sources))
                encoded = json.dumps(payload, sort_keys=True).encode()
                self.assertEqual(hashlib.sha256(encoded).hexdigest(), payload_hashes[name])
                for failure in payload['failure_identities']:
                    self.assertIn(failure['path'].removeprefix('test262/'), sources)
                    self.assertIn(identity(failure), adjudicated)

    def test_lunisolar_instantiation_preserves_complete_template_evidence(self):
        qualified = self.read(self.here / 'reviews/candidates/lunisolar-qualified.json')
        binding = qualified.pop('review_template_binding')
        path = self.root / binding['path']
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), binding['sha256'])
        template = self.read(path)
        for key in ('fixture_source_map_sha256', 'config_sha256', 'baseline_sha256',
                    'effective_profile_sha256', 'strict', 'line', 'signature'):
            self.assertIsNone(template[key])
            self.assertIsNotNone(qualified[key])
            qualified[key] = None
        current = qualified['stable_observation_expectation']['source_bindings']
        historical = template['stable_observation_expectation']['source_bindings']
        self.assertEqual(set(current), set(historical))
        # These are the frozen two review generations, not the current
        # checkout helper versions. Preserve the complete semantic template.
        transitions = {
            'run.py': ('bc00829adac63d344b44b77f94064a3961df9c1a288714ec858c8af1ff67e3e4',
                       '3d6deb82e6c421af0e9c506941924755e999c4a87ae2d327d1f64e0e277d9f42'),
            'native_profile_gate.py': ('9ed9acc63c6d07dedba369c9c3097fd1ba1e61b87537ac83c208193d8ed81dad',
                                       '3c4661564e3f36a049ae6d1f84aca913d4f5aeccabdcca18690f8032dd495c29'),
            'project_original_assertions.py': ('e08aa3fc5cb41508416e9c7d08f77aa407e619d1866e8e3449c019e2bf5abb51',
                                                '7779091b2f5baf8a3f65b76f6c7478b7acc673677d7dde5dbc895c299b91f5f2'),
            'launch_current_premise_witness.py': ('7f6ea3e49547854129871ae918e7de4f0f359656523dc7da0e8381484f4e632e',
                                                 'f26d23adae9a0a53e52f6f037dd6bba7518c534fb36df2836fe1e867a8b14cf5'),
        }
        for name, (old, instantiated) in transitions.items():
            self.assertEqual(historical[name], old)
            self.assertEqual(current[name], instantiated)
            historical[name] = instantiated
        self.assertEqual(qualified, template)


    def test_current_helper_derivation_authenticates_the_maintained_closure(self):
        from helper_source_derivation import (HELPERS, PREFIX, MANIFEST, MODULE,
            authenticate_helper_derivation, map_digest)
        manifest = self.read(self.root / MANIFEST)
        paths = {MANIFEST, MODULE} | {PREFIX + name for name in HELPERS}
        paths.update(row['origin_review']['path']
                     for row in manifest['historical_generations'].values())
        files = {name: (self.root / name).read_bytes() for name in paths}
        sources = {name: hashlib.sha256(raw).hexdigest() for name, raw in files.items()}
        derivation = authenticate_helper_derivation(sources, map_digest(sources), files.__getitem__)
        review = self.read(self.here / 'reviews/candidates/lunisolar-qualified.json')
        derived = derivation.derive_review(review)
        for name in HELPERS:
            self.assertEqual(derived['stable_observation_expectation']['source_bindings'][name],
                             sources[PREFIX + name])
        self.assertEqual(derived['stable_observation_expectation']['observations'],
                         review['stable_observation_expectation']['observations'])
        self.assertEqual(derived['current_capture_binding'], review['current_capture_binding'])


if __name__ == '__main__':
    unittest.main()
