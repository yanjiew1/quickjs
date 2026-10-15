"""Reject incomplete raw Test262 reports; MOCK data is never engine evidence."""
import copy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/ci'))
import current_test262 as collector


class RawCollectionTests(unittest.TestCase):
    def setUp(self):
        self.path = 'test262/test/MOCK-one.js'
        self.rows = [{'file': self.path, 'mode': mode, 'status': 'pass'}
                     for mode in ('non-strict', 'strict')]

    def validate(self, rows=None, stderr='Result: 0/1 error\n', status=0, expected=None):
        if expected is None:
            expected = {self.path: ['non-strict', 'strict']}
        return collector.validate('', stderr, status, self.rows if rows is None else rows,
                                  expected, [])

    def test_complete_passing_source(self):
        self.assertEqual(self.validate()['counters']['failed_source_count'], 0)

    def test_raw_failure_is_preserved(self):
        row = dict(self.rows[0], status='fail')
        stderr = self.path + ':9: unexpected error: Test262Error: MOCK\nResult: 1/1 error, 1 new\n'
        result = self.validate([row], stderr, 1)
        self.assertEqual(result['raw_runner_returncode'], 1)
        self.assertFalse(result['conformance_accepted'])
        self.assertEqual(result['raw_observations'][0]['line'], 9)

    def test_duplicate_variant_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            self.validate(self.rows + [self.rows[0]])

    def test_disagreeing_timing_failure_rejected(self):
        rows = copy.deepcopy(self.rows)
        rows[0]['status'] = 'fail'
        with self.assertRaises(ValueError):
            self.validate(rows)

    def test_unknown_mode_rejected(self):
        rows = copy.deepcopy(self.rows)
        rows[0]['mode'] = 'unknown'
        with self.assertRaisesRegex(ValueError, 'Invalid'):
            self.validate(rows)

    def test_missing_result_rejected(self):
        with self.assertRaises(ValueError):
            self.validate(stderr='')

    def test_interrupted_runner_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            self.validate(status=-9)

    def test_hidden_baseline_failure_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            self.validate(stderr='Result: 1/1 error\n')

    def test_sanitizer_report_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            self.validate(stderr='file.c:1:2: runtime error: MOCK\nResult: 0/1 error\n')

    def test_skipped_source_counts_required(self):
        with self.assertRaisesRegex(ValueError, 'Timing source'):
            self.validate(stderr='Result: 0/1 error, 1 skipped\n')

    def test_missing_strict_pass_rejected(self):
        with self.assertRaisesRegex(ValueError, 'variant'):
            self.validate(self.rows[:1])

    def test_omitted_source_rejected(self):
        with self.assertRaisesRegex(ValueError, 'independent selection'):
            self.validate([], 'Result: 0/0 errors\n')

    def test_executed_and_skipped_same_source_rejected(self):
        with self.assertRaisesRegex(ValueError, 'variant'):
            self.validate(self.rows + [dict(self.rows[0], mode='none', status='skipped')],
                          'Result: 0/1 error, 1 skipped\n')

    def test_legitimate_feature_skip(self):
        self.validate([dict(self.rows[0], mode='none', status='skipped')],
                      'Result: 0/0 errors, 1 skipped\n', expected={self.path: ['none']})

    def test_default_configuration_restores_only_reviewed_oracles(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(collector, 'ROOT', Path(directory)):
            root = Path(directory)
            (root / 'tests').mkdir()
            (root / 'tests/intl-date-time-oracle-policy.json').write_text(
                '{"fixture_sha256": {"test262/test/oracle.js": "MOCK"}}')
            (root / 'test262-intl.conf').write_text(
                '[exclude]\ntest262/test/oracle.js\ntest262/test/keep.js\n')
            self.assertEqual(collector.configuration(), '[exclude]\ntest262/test/keep.js\n')

    def test_explicit_feature_configuration_is_preserved_exactly(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'features.conf'
            source = '[features]\nIntl=skip\nTemporal\n[exclude]\nMOCK-optional.js\n'
            path.write_text(source)
            self.assertEqual(collector.configuration(path), source)

    def test_multiline_frontmatter_lists(self):
        source = '/*---\nflags:\n  - noStrict\n  - async\nfeatures: ["Temporal", Intl]\n---*/'
        self.assertEqual(collector.metadata_list(source, 'flags'), {'noStrict', 'async'})
        self.assertEqual(collector.metadata_list(source, 'features'), {'Temporal', 'Intl'})

    def test_census_filters_fixtures_and_preserves_exclusions(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(collector, 'ROOT', Path(directory)):
            root = Path(directory)
            tests = root / 'test262/test'
            tests.mkdir(parents=True)
            (tests / 'ordinary.js').write_text('/*---\nfeatures: [Temporal]\n---*/')
            (tests / 'raw.js').write_text('/*---\nflags: [raw]\n---*/')
            (tests / 'excluded.js').write_text('')
            (tests / 'slow.js').write_text('')
            (tests / 'skip.js').write_text('/*---\nfeatures: [tail]\n---*/')
            (tests / 'helper_FIXTURE.js').write_text('')
            (root / 'test262-slow.txt').write_text('test262/test/slow.js\n')
            config = '[features]\nTemporal\ntail=skip\n[exclude]\ntest262/test/excluded.js\n'
            modes, excluded = collector.census(config)
            self.assertEqual(modes, {'test262/test/ordinary.js': ['non-strict', 'strict'],
                                     'test262/test/raw.js': ['non-strict'], 'test262/test/skip.js': ['none']})
            self.assertEqual(excluded, ['test262/test/excluded.js', 'test262/test/slow.js'])


if __name__ == '__main__':
    unittest.main()
