"""Pure regressions for removal-only core baseline derivations."""
import copy
import json
from pathlib import Path
import unittest
from core_baseline_transition import (BASELINE_PATH, CHECKPOINT_PATH,
    CHECKPOINT_SHA256, TRANSITION_PATH, CoreBaselineTransition,
    authenticate_transition, canonical, digest, review_baseline_matches)


class CoreBaselineTests(unittest.TestCase):
    def setup_mode(self, mode):
        root = Path(__file__).resolve().parents[3]
        checkpoint_raw = (root / CHECKPOINT_PATH).read_bytes()
        checkpoint = json.loads(checkpoint_raw)
        historical = checkpoint['known_error_baseline_lines']
        removed = historical[:-1] if mode == 'rhs-eval-13' else historical
        remaining = historical[len(removed):]
        raw = ('\n'.join(remaining) + '\n').encode() if remaining else b''
        record = dict(schema='assignment-core-baseline-transition-v1', mode=mode,
            historical_checkpoint_sha256=CHECKPOINT_SHA256,
            historical_baseline_sha256=checkpoint['known_error_baseline_sha256'],
            current_baseline_sha256=digest(raw), removed_records=removed,
            remaining_source_count=len(remaining))
        self.files = {CHECKPOINT_PATH: checkpoint_raw, BASELINE_PATH: raw,
                      TRANSITION_PATH: json.dumps(record).encode()}
        self.historical = checkpoint['known_error_baseline_sha256']
        self.persist()

    def persist(self):
        self.sources = {p: digest(raw) for p, raw in self.files.items()}
        self.context = dict(production_source_map_sha256=canonical(self.sources),
                            baseline_sha256=digest(self.files[BASELINE_PATH]))

    def transition(self):
        return authenticate_transition(self.sources, self.files.__getitem__, self.context)

    def change_record(self, change):
        record = json.loads(self.files[TRANSITION_PATH]); change(record)
        self.files[TRANSITION_PATH] = json.dumps(record).encode(); self.persist()

    def test_exact_permitted_transitions(self):
        for mode, count, failures in [('rhs-eval-13', 1, 110),
                                       ('assignment-reference-14', 0, 109)]:
            with self.subTest(mode=mode):
                self.setup_mode(mode); transition = self.transition()
                self.assertEqual(transition.receipt()['remaining_source_count'], count)
                self.context['core_baseline_transition'] = transition
                self.assertTrue(review_baseline_matches(self.historical, self.context))
                policy = dict(baseline_sha256=self.historical,
                    postfix_raw_counters=dict(failed_source_count=123,
                        new=109, changed=0, fixed=0, excluded=19, skipped=856,
                        executed_source_count=52742))
                original = copy.deepcopy(policy)
                current = transition.derive_document(policy, 'diagnostic-policy', self.context)
                self.assertEqual(policy, original)
                self.assertEqual(current['postfix_raw_counters']['failed_source_count'], failures)
                self.assertEqual(current['postfix_raw_counters']['new'], 109)
                self.assertEqual(current['baseline_sha256'], self.context['baseline_sha256'])

    def test_absent_record_keeps_exact_original_binding(self):
        self.assertIsNone(authenticate_transition({}, {}.__getitem__))
        self.assertTrue(review_baseline_matches('a', {'baseline_sha256': 'a'}))
        self.assertFalse(review_baseline_matches('a', {'baseline_sha256': 'b'}))

    def test_unapproved_removals_additions_and_variants(self):
        changes = [lambda r: r['removed_records'].pop(),
                   lambda r: r['removed_records'].append('unexpected.js:1: Error'),
                   lambda r: r['removed_records'].reverse(),
                   lambda r: r['removed_records'].__setitem__(0, r['removed_records'][0] + ' strict'),
                   lambda r: r.update(mode='arbitrary-subset')]
        for change in changes:
            self.setup_mode('rhs-eval-13'); self.change_record(change)
            with self.assertRaises(ValueError): self.transition()

    def test_changed_current_baseline_bytes(self):
        for raw in [b'', b'new.js:1: Error\n', b' ', b'\n']:
            self.setup_mode('rhs-eval-13'); self.files[BASELINE_PATH] = raw
            self.persist()
            with self.assertRaises(ValueError): self.transition()

    def test_changed_record_current_count_or_hash(self):
        for change in [lambda r: r.update(current_baseline_sha256='0' * 64),
                       lambda r: r.update(remaining_source_count=0),
                       lambda r: r.update(remaining_source_count=True),
                       lambda r: r.update(failure_waiver=True),
                       lambda r: r.update(historical_checkpoint_sha256='0' * 64)]:
            self.setup_mode('rhs-eval-13'); self.change_record(change)
            with self.assertRaises(ValueError): self.transition()

    def test_changed_historical_checkpoint(self):
        self.setup_mode('rhs-eval-13'); self.files[CHECKPOINT_PATH] += b'\n'
        self.persist()
        with self.assertRaises(ValueError): self.transition()

    def test_source_map_and_context_changes(self):
        for path in [CHECKPOINT_PATH, TRANSITION_PATH, BASELINE_PATH]:
            self.setup_mode('rhs-eval-13'); self.sources[path] = '0' * 64
            with self.assertRaises(ValueError): self.transition()
        for field in ['baseline_sha256', 'production_source_map_sha256']:
            self.setup_mode('rhs-eval-13'); self.context[field] = '0' * 64
            with self.assertRaises(ValueError): self.transition()

    def test_untrusted_transition_and_stale_context(self):
        self.setup_mode('rhs-eval-13')
        with self.assertRaises(ValueError): CoreBaselineTransition({}, object())
        self.context['core_baseline_transition'] = {'historical_baseline_sha256': self.historical}
        with self.assertRaises(ValueError): review_baseline_matches(self.historical, self.context)
        self.context['core_baseline_transition'] = self.transition()
        self.context['baseline_sha256'] = '0' * 64
        with self.assertRaises(ValueError): review_baseline_matches(self.historical, self.context)

    def test_historical_counter_changes_are_not_derived(self):
        for change in [{'failed_source_count': 124}, {'new': 110}, {'changed': 1}, {'fixed': 1},
                       {'skipped': 857}, {'excluded': 20}, {'executed_source_count': 52741}]:
            self.setup_mode('rhs-eval-13'); transition = self.transition()
            counts = dict(failed_source_count=123, new=109, changed=0, fixed=0,
                          skipped=856, excluded=19, executed_source_count=52742)
            counts.update(change)
            with self.assertRaises(ValueError):
                transition.derive_document(dict(baseline_sha256=self.historical,
                    postfix_raw_counters=counts), 'diagnostic-policy', self.context)

    def test_static_scope_derives_only_authenticated_current_baseline(self):
        import test_native_normative_scope as normative
        for mode, count in [('rhs-eval-13', 110), ('assignment-reference-14', 109)]:
            self.setup_mode(mode)
            case = normative.StaticScopeTests(); case.setUp()
            case.files.update(self.files)
            case.context['baseline_sha256'] = self.context['baseline_sha256']
            case.scope['baseline_sha256'] = self.historical
            case.policy['baseline_sha256'] = self.historical
            case.policy['postfix_raw_counters'] = dict(changed=0, excluded=19,
                executed_source_count=52742, failed_source_count=123,
                fixed=0, new=109, skipped=856)
            for document in case.documents.values():
                if 'baseline_sha256' in document:
                    document['baseline_sha256'] = self.historical
            case.persist()
            original = copy.deepcopy(case.files)
            scopes = case.bind()
            policy = scopes.document_for('diagnostic-policy')
            proof = scopes.document(case.binding(case.proof_path), 'mandatory-review')
            self.assertEqual(policy['baseline_sha256'], case.context['baseline_sha256'])
            self.assertEqual(proof['baseline_sha256'], case.context['baseline_sha256'])
            self.assertEqual(policy['postfix_raw_counters']['failed_source_count'], count)
            self.assertEqual(scopes.receipt()['core_baseline_transition']['remaining_source_count'],
                             1 if mode == 'rhs-eval-13' else 0)
            self.assertEqual(case.files, original)
            case.context['baseline_sha256'] = '0' * 64
            case.rebind_current_identity()
            with self.assertRaises(ValueError): case.bind()

    def test_stable_review_retains_historical_baseline_with_current_context(self):
        import test_native_profile_gate as profile
        from native_profile_gate import reviewed_entry
        for mode in ['rhs-eval-13', 'assignment-reference-14']:
            self.setup_mode(mode)
            sample = profile.CurrentSemanticPolicyTests(); sample.setUp()
            case = sample.case
            case.update_review(baseline_sha256=self.historical)
            original = case.artifacts['review.json']
            self.files.update(case.artifacts); self.persist()
            case.context.update(self.context)
            case.context['core_baseline_transition'] = self.transition()
            sample.trace['source_tree_sha256'] = self.context['production_source_map_sha256']
            for trace in sample.trace['variant_summaries'].values():
                trace['source_tree_sha256'] = self.context['production_source_map_sha256']
            sample.derive()
            reviewed_entry(case.entry, case.context, {case.path:'b' * 64},
                           case.witness['fixture_summaries'], case.artifacts.__getitem__)
            self.assertEqual(case.artifacts['review.json'], original)
            case.context['core_baseline_transition'] = None
            with self.assertRaises(ValueError):
                reviewed_entry(case.entry, case.context, {case.path:'b' * 64},
                               case.witness['fixture_summaries'], case.artifacts.__getitem__)


if __name__ == '__main__':
    unittest.main()
