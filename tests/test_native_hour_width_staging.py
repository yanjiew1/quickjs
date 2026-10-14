"""Negative checks for staged raw deltas; MOCK streams never qualify engines."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/ci/native-prefix'))
import stage_hour_width as stage


class StagingTests(unittest.TestCase):
    def test_focused_argv_uses_an_exact_source_list_and_suppresses_config_enumeration(self):
        paths = ['test262/test/MOCK-one.js', 'test262/test/MOCK-two.js']
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(stage, 'OUT', Path(directory)), \
                mock.patch.object(stage, 'ROOT', Path(directory)):
            args = stage.selection_args(paths)
            self.assertEqual(args[:2], ['-d', str(Path(directory) / 'empty-selection')])
            self.assertTrue(Path(args[1]).is_dir())
            self.assertNotIn('/', args[2])
            self.assertEqual((Path(directory) / args[2]).read_text().splitlines(), paths)
            self.assertEqual(len(args), 3)

    def setUp(self):
        self.policy = json.loads(stage.POLICY.read_bytes())
        self.manifest = json.loads(stage.MANIFEST.read_bytes())
        fixed = {x['path'] for x in self.manifest['repaired_sources']}
        self.rows = []
        self.records = []
        for entry in self.policy['adjudicated_diagnostic_entries']:
            path = entry['path']
            if path in fixed:
                self.rows.extend(dict(file=path, mode=mode, status='pass')
                                 for mode in ('non-strict', 'strict'))
            else:
                self.rows.append(dict(file=path,
                    mode='strict' if entry['strict'] else 'non-strict', status='fail'))
                self.records.append(f"{path}:{entry['line']}: {entry['signature']}")
        self.rows.append(dict(file='test262/test/MOCK-untouched-pass.js',
                              mode='non-strict', status='pass'))
        executed = sorted({row['file'] for row in self.rows})
        self.manifest['executed_source_count'] = len(executed)
        self.manifest['executed_sources_sha256'] = stage.canonical(executed)
        self.manifest['excluded'], self.manifest['skipped'] = 2, 3
        skipped = ['test262/test/MOCK-skipped-' + str(i) + '.js' for i in range(3)]
        self.manifest['skipped_sources_sha256'] = stage.canonical(skipped)
        self.rows.extend(dict(file=path, mode='none', status='skipped') for path in skipped)

    def stderr(self, excluded=2):
        count = len(self.records)
        return ('\n'.join(self.records) +
                f'\nResult: {count}/110 errors, {excluded} excluded, 3 skipped, {count} new\n')

    def validate(self, stderr=None, rows=None, status=1):
        return stage.validate_full('', self.stderr() if stderr is None else stderr,
            status, self.rows if rows is None else rows, self.policy, self.manifest)

    def test_exact_six_repairs_only(self):
        self.assertEqual(self.validate()['failed_source_count'], 103)

    def test_changed_remaining_signature_rejected(self):
        with self.assertRaises(ValueError):
            self.validate(stderr=self.stderr().replace(self.records[0], self.records[0] + ' altered'))

    def test_substituted_failure_rejected(self):
        changed = self.stderr().replace(self.records[0].split(':')[0],
                                       'test262/test/MOCK-untouched-pass.js')
        with self.assertRaises(ValueError):
            self.validate(stderr=changed)

    def test_repaired_source_must_pass_both_modes(self):
        rows = [r for r in self.rows if not (r['status'] == 'pass' and r['mode'] == 'strict')]
        with self.assertRaises(ValueError):
            self.validate(rows=rows)

    def test_failed_repaired_variant_rejected(self):
        rows = copy.deepcopy(self.rows)
        next(r for r in rows if r['status'] == 'pass')['status'] = 'fail'
        with self.assertRaises(ValueError):
            self.validate(rows=rows)

    def test_duplicate_timing_record_rejected(self):
        with self.assertRaises(ValueError):
            self.validate(rows=self.rows + [self.rows[0]])

    def test_equal_count_different_selection_rejected(self):
        rows = copy.deepcopy(self.rows)
        next(r for r in rows if r['file'] == 'test262/test/MOCK-untouched-pass.js')['file'] = \
            'test262/test/MOCK-substituted-pass.js'
        with self.assertRaises(ValueError):
            self.validate(rows=rows)

    def test_more_exclusions_rejected(self):
        with self.assertRaises(ValueError):
            self.validate(stderr=self.stderr(excluded=3))

    def test_incomplete_or_abnormal_runner_rejected(self):
        with self.assertRaises(ValueError):
            self.validate(stderr='\n'.join(self.records))
        with self.assertRaises(ValueError):
            self.validate(status=-11)

    def test_failure_timing_mislabeled_as_pass_rejected(self):
        rows = copy.deepcopy(self.rows)
        next(r for r in rows if r['status'] == 'fail')['status'] = 'pass'
        with self.assertRaises(ValueError):
            self.validate(rows=rows)

    def test_invalid_timing_status_or_mode_rejected(self):
        for change in ({'status': 'ignored'}, {'mode': 'invalid'}):
            rows = copy.deepcopy(self.rows)
            rows[0].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.validate(rows=rows)

    def test_skipped_source_substitution_rejected(self):
        rows = copy.deepcopy(self.rows)
        rows[-1]['file'] = 'test262/test/MOCK-other-skipped.js'
        with self.assertRaises(ValueError):
            self.validate(rows=rows)

    def test_focused_exact_source_variants_required(self):
        rows = [dict(file=entry['path'], mode=mode, status='pass')
                for entry in self.manifest['repaired_sources']
                for mode in ('non-strict', 'strict')]
        result = 'Result: 0/6 errors\n'
        stage.validate_focused('', result, 0, rows, self.manifest)
        for change in ('substitute', 'duplicate', 'malformed-result'):
            altered = copy.deepcopy(rows)
            if change == 'substitute':
                altered[0]['file'] = 'test262/test/MOCK-unrelated.js'
            elif change == 'duplicate':
                altered[0] = altered[1]
            with self.subTest(change=change), self.assertRaises(ValueError):
                stage.validate_focused('', '' if change == 'malformed-result' else result,
                                       0, altered, self.manifest)


if __name__ == '__main__':
    unittest.main()
