"""Keep the explicit slow-test exclusion narrow and historical reviews intact."""
import copy
import json
from pathlib import Path
import unittest
from slow_test_selection import (CHECKPOINT_PATH, DST_PATHS, EXCLUSION_PATH,
                                 authenticate_slow_selection, canonical, digest)
from execution_threads import test262_workers


class SlowSelectionTest(unittest.TestCase):
    def setUp(self):
        self.root = Path(__file__).resolve().parents[3]
        self.files = {CHECKPOINT_PATH: (self.root / CHECKPOINT_PATH).read_bytes(),
                      EXCLUSION_PATH: ('\n'.join(DST_PATHS) + '\n').encode()}
        manifest = 'tools/ci/native-prefix/selections.json'
        self.files[manifest] = (self.root / manifest).read_bytes()
        for item in json.loads(self.files[manifest]):
            name = 'tools/ci/native-prefix/' + item['path']
            self.files[name] = (self.root / name).read_bytes()
        self.context = dict(fixture_source_map_sha256='f' * 64,
                            config_sha256='c' * 64, baseline_sha256='b' * 64,
                            qjs_sha256='e' * 64)
        self.command = dict(
            argv=test262_workers(json.loads(self.files[CHECKPOINT_PATH])['argv']) + [
                '--progress-log', '/reports/test262-execution.jsonl',
                '--timing-log', '/reports/test262-timings.jsonl', '-x', EXCLUSION_PATH],
            returncode=1, lifecycle_closed=True, timed_out=False, interrupted=False)
        self.policy = dict(postfix_raw_counters=dict(
            changed=0, excluded=19, executed_source_count=52742,
            failed_source_count=123, fixed=0, new=109, skipped=856),
            postfix_raw_returncode=1, no_new_skips_exclusions_expected_errors=True,
            failure_waiver=False, conformance_accepted=False,
            failure_identities=['must remain unchanged'])

    def authorize(self, protected=()):
        sources = {name: digest(raw) for name, raw in self.files.items()}
        self.context['production_source_map_sha256'] = canonical(sources)
        command = json.dumps(self.command).encode()
        parsed = dict(input_identity=copy.deepcopy(self.context),
                      command_sha256=digest(command), raw_parser_complete=True,
                      raw_collection_complete=True, sealed_inputs_unchanged=True,
                      raw_runner_returncode=1)
        return authenticate_slow_selection(sources, self.files.__getitem__,
            self.context, command, parsed, protected, '/reports')

    def test_exact_transition_and_immutable_policy(self):
        transition = self.authorize()
        original = copy.deepcopy(self.policy)
        derived = transition.derive_policy(self.policy, self.context)
        self.assertEqual(self.policy, original)
        counts = copy.deepcopy(original['postfix_raw_counters'])
        counts['executed_source_count'] -= 8
        counts['excluded'] += 8
        self.assertEqual(derived['postfix_raw_counters'], counts)
        self.assertFalse(derived['no_new_skips_exclusions_expected_errors'])
        self.assertEqual(derived['failure_identities'], original['failure_identities'])
        transition.check_policy(derived, self.context)
        for field in counts:
            changed = copy.deepcopy(derived)
            changed['postfix_raw_counters'][field] += 1
            with self.assertRaises(ValueError):
                transition.check_policy(changed, self.context)
        changed = copy.deepcopy(derived)
        changed['failure_identities'] = []
        with self.assertRaises(ValueError):
            transition.check_policy(changed, self.context)

    def test_baseline_subtraction_and_gate_authorization(self):
        from core_baseline_transition import BASELINE_PATH, TRANSITION_PATH
        from test_native_profile_gate import ExactPolicyTests
        fixture = ExactPolicyTests()
        fixture.setUp()
        for name in (BASELINE_PATH, TRANSITION_PATH):
            self.files[name] = (self.root / name).read_bytes()
        self.context['baseline_sha256'] = digest(self.files[BASELINE_PATH])
        transition = self.authorize([fixture.path])
        fixture.context.update(self.context)
        # Bind the small mocked witness to the same current execution context.
        bindings = dict(fixture.context,
                        engine_sha256=fixture.context['qjs_sha256'],
                        source_tree_sha256=fixture.context['production_source_map_sha256'],
                        profile_sha256=fixture.context['effective_profile_sha256'])
        def rebind(value):
            if isinstance(value, dict):
                return {key: bindings[key] if key in bindings else rebind(item)
                        for key, item in value.items()}
            if isinstance(value, list):
                return [rebind(item) for item in value]
            return value
        fixture.policy = rebind(fixture.policy)
        fixture.witness = rebind(fixture.witness)
        for name in ('review.json', 'obligation.json'):
            fixture.artifacts[name] = json.dumps(rebind(json.loads(fixture.artifacts[name]))).encode()
        fixture.entry = fixture.policy['adjudicated_diagnostic_entries'][0]
        fixture.entry['review_binding'] = fixture.binding('review.json')
        for obligation in fixture.policy['mandatory_obligations']:
            obligation['proof_bindings'] = [fixture.binding('obligation.json')]
        fixture.policy['postfix_raw_counters'] = dict(self.policy['postfix_raw_counters'],
                                                    failed_source_count=109)
        fixture.context['slow_test_selection'] = transition
        fixture.policy = transition.derive_policy(fixture.policy, fixture.context)
        fixture.raw['counters'] = copy.deepcopy(fixture.policy['postfix_raw_counters'])
        result = fixture.gate()
        self.assertEqual(result['raw_counters']['failed_source_count'], 109)
        self.assertEqual(result['raw_counters']['executed_source_count'], 52734)
        self.assertEqual(result['raw_counters']['excluded'], 27)
        self.assertTrue(result['raw_failures_preserved'])
        self.assertFalse(result['conformance_accepted'])
        fixture.context.pop('slow_test_selection')
        with self.assertRaisesRegex(ValueError, 'Changed skip policy'):
            fixture.gate()
        fixture.context['slow_test_selection'] = object()
        with self.assertRaisesRegex(ValueError, 'Untrusted slow selection'):
            fixture.gate()
        fixture.context['slow_test_selection'] = transition
        fixture.policy['postfix_raw_counters']['excluded'] += 1
        with self.assertRaisesRegex(ValueError, 'exact authorized slow selection'):
            fixture.gate()

    def test_modified_missing_extra_or_duplicate_exclusions(self):
        original = self.files[EXCLUSION_PATH]
        for body in [original[:-1], b'\n'.join(original.splitlines()[:-1]) + b'\n',
                     original + b'test262/test/other.js\n',
                     original + original.splitlines()[0] + b'\n',
                     original.replace(b'-1-of-8', b'-9-of-8')]:
            with self.subTest(body=body):
                self.files[EXCLUSION_PATH] = body
                with self.assertRaises(ValueError):
                    self.authorize()
        self.files[EXCLUSION_PATH] = original

    def test_overlap_and_modified_command(self):
        with self.assertRaises(ValueError):
            self.authorize([DST_PATHS[0]])
        original = copy.deepcopy(self.command)
        for argv in [original['argv'][:-2], original['argv'] + ['-x', EXCLUSION_PATH],
                     original['argv'] + ['-f', 'other.js']]:
            self.command = dict(original, argv=argv)
            with self.assertRaises(ValueError):
                self.authorize()
        for field in ('timed_out', 'interrupted'):
            self.command = dict(original, **{field: True})
            with self.assertRaises(ValueError):
                self.authorize()
        self.command = dict(original, lifecycle_closed=False)
        with self.assertRaises(ValueError):
            self.authorize()


if __name__ == '__main__':
    unittest.main()
