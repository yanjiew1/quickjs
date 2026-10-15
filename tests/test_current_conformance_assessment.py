"""MOCK receipts only: reject weakened conformance assessment evidence."""
import contextlib
import copy
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/ci'))
import current_test262 as collector
import current_conformance_assessment as assessment


class AssessmentTests(unittest.TestCase):
    def save(self, path, value):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value))

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.raw = self.root / 'ci-current-test262'
        self.raw.mkdir()
        self.out = self.root / 'ci-current-conformance'
        self.path = 'test262/test/MOCK-one.js'
        self.other = 'test262/test/MOCK-two.js'
        self.source = b'/*---\n---*/\nassert.sameValue(MOCK, 1);\n'
        for source in [self.path, self.other]:
            path = self.root / source
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(self.source)
        (self.root / 'test262-intl.conf').write_text('[features]\n')
        (self.root / 'test262-slow.txt').write_text('')
        self.save(self.root / 'tests/intl-date-time-oracle-policy.json', {'fixture_sha256': {}})
        self.env = {'GITHUB_ACTIONS': 'true', 'GITHUB_RUN_ID': 'MOCK-run',
                    'GITHUB_RUN_ATTEMPT': '1', 'GITHUB_JOB': 'MOCK-job',
                    'GITHUB_WORKFLOW_REF': 'MOCK-workflow', 'QJS_TEST262_COMPILER': 'MOCK-gcc',
                    'QJS_TEST262_SANITIZER': 'MOCK-asan', 'QJS_TEST262_BACKEND': 'MOCK-native'}
        self.binding = {'checkout': 'MOCK-head', 'qjs': 'MOCK-qjs', 'runner': 'MOCK-runner',
                        'sources': {'MOCK': 'MOCK-source-hash'}, 'fixture': {'MOCK': 'MOCK-fixture-hash'},
                        'execution': {key: value for key, value in self.env.items() if key != 'GITHUB_ACTIONS'},
                        'fixture_revision': 'MOCK-fixture-revision'}
        self.modes = {self.path: ['non-strict', 'strict'], self.other: ['non-strict', 'strict']}
        self.selection = {'expected_modes': self.modes, 'excluded_sources': [], 'config': '[features]\n'}
        self.rows = [{'file': self.path, 'mode': 'non-strict', 'status': 'fail'},
                     *[{'file': self.other, 'mode': mode, 'status': 'pass'} for mode in ('non-strict', 'strict')]]
        self.signature = 'unexpected error: Test262Error: MOCK unsigned snapshot'
        self.stdout = self.path + ':7: ' + self.signature + '\n'
        self.stderr = 'Result: 1/2 errors, 1 new\n'
        self.summary = collector.validate(self.stdout, self.stderr, 1, self.rows, self.modes, [])
        self.summary.update(lifecycle_closed=True, identities_unchanged=True, conformance_accepted=False)
        generated = self.source.replace(b'MOCK, 1', b'MOCK, -1')
        self.recipe = {'source': self.path, 'source_sha256': assessment.sha(self.source),
                       'generated_sha256': assessment.sha(generated),
                       'changes': [{'before': 'MOCK, 1', 'after': 'MOCK, -1', 'count': 1}],
                       'observed_conflicts': [{'line': 7, 'signature': self.signature, 'strict': False}],
                       'reason': 'MOCK unsigned expectation', 'spec': {'revision': 'MOCK-spec'}}
        self.ledger = {'schema': 1, 'backend': 'MOCK-native',
                       'fixture_revision': 'MOCK-fixture-revision', 'input_digest_sha256': 'MOCK-input',
                       'data_sha256': {}, 'recipes': [self.recipe]}
        provenance = self.root / 'src/intl/data/locale-metadata-provenance.json'
        self.save(provenance, {'input_digest_sha256': 'MOCK-input'})
        self.ledger['data_sha256'][provenance.relative_to(self.root).as_posix()] = assessment.sha(provenance.read_bytes())
        self.missing_strict = self.fail_companion = self.change_companion_identity = False
        self.change_raw_after = self.change_generated_after = False

    def write_raw(self):
        self.save(self.raw / 'summary.json', self.summary)
        self.save(self.raw / 'selection.json', self.selection)
        self.save(self.raw / 'identity-before.json', self.binding)
        (self.raw / 'stdout.log').write_text(self.stdout)
        (self.raw / 'stderr.log').write_text(self.stderr)
        (self.raw / 'timings.jsonl').write_text(''.join(json.dumps(row) + '\n' for row in self.rows))
        self.save(self.root / 'tests/test262-assertion-conflicts.json', self.ledger)

    def fake_companion_run(self, command, **unused):
        destination = Path(command[command.index('--output') + 1])
        paths = command[command.index('--sources') + 1:]
        expected = {path: ['non-strict', 'strict'] for path in paths}
        rows = [{'file': path, 'mode': mode, 'status': 'pass'}
                for path in paths for mode in ('non-strict', 'strict')]
        stderr = 'Result: 0/' + str(len(paths)) + (' error\n' if len(paths) == 1 else ' errors\n')
        summary = collector.validate('', stderr, 0, rows, expected, [])
        summary.update(lifecycle_closed=True, identities_unchanged=True, conformance_accepted=True)
        if self.missing_strict:
            rows = [row for row in rows if row['mode'] != 'strict']
        if self.fail_companion:
            rows[0]['status'] = 'fail'
        identity = copy.deepcopy(self.binding)
        if self.change_companion_identity:
            identity['execution']['GITHUB_RUN_ATTEMPT'] = 'MOCK-other'
        self.save(destination / 'identity-before.json', identity)
        self.save(destination / 'summary.json', summary)
        (destination / 'stdout.log').write_text('')
        (destination / 'stderr.log').write_text(stderr)
        (destination / 'timings.jsonl').write_text(''.join(json.dumps(row) + '\n' for row in rows))
        if self.change_raw_after:
            (self.raw / 'stderr.log').write_text('MOCK mutation')
        if self.change_generated_after:
            (self.root / paths[0]).write_text('MOCK mutation')
        return SimpleNamespace(returncode=0)

    def run_assessment(self, current_identity=None):
        self.write_raw()
        with mock.patch.object(collector, 'ROOT', self.root), \
             mock.patch.object(assessment, 'ROOT', self.root), \
             mock.patch.object(collector, 'identity', return_value=current_identity or self.binding), \
             mock.patch.object(assessment.subprocess, 'run', side_effect=self.fake_companion_run), \
             mock.patch.dict(os.environ, self.env), \
             mock.patch.object(sys, 'argv', ['MOCK-assessment']), \
             contextlib.redirect_stdout(io.StringIO()):
            result = assessment.main()
        report = json.loads((self.out / 'summary.json').read_text())
        return result, report

    def rejected(self, message=None, current_identity=None):
        result, report = self.run_assessment(current_identity)
        self.assertEqual(result, 1)
        self.assertFalse(report['assessment_accepted'])
        if message:
            self.assertIn(message, report['blocking_error'])

    def test_exact_conflict_with_full_passing_remainder(self):
        result, report = self.run_assessment()
        self.assertEqual(result, 0)
        self.assertTrue(report['assessment_accepted'])
        self.assertFalse(report['original_raw_conformance_accepted'])
        self.assertEqual(report['original_raw_failed_sources'], 1)
        self.assertEqual(report['companion_variants'], 2)

    def test_fully_passing_raw_keeps_companion_checks(self):
        self.stdout = ''
        self.stderr = 'Result: 0/2 errors\n'
        self.rows = [{'file': path, 'mode': mode, 'status': 'pass'}
                     for path in self.modes for mode in ('non-strict', 'strict')]
        self.summary = collector.validate(self.stdout, self.stderr, 0, self.rows, self.modes, [])
        self.summary.update(lifecycle_closed=True, identities_unchanged=True, conformance_accepted=True)
        result, report = self.run_assessment()
        self.assertEqual(result, 0)
        self.assertTrue(report['original_raw_conformance_accepted'])
        self.assertEqual(report['original_raw_failed_sources'], 0)
        self.assertEqual(report['companion_variants'], 2)

    def test_partial_raw_run_rejected(self):
        self.modes = {self.path: ['non-strict', 'strict']}
        self.selection['expected_modes'] = self.modes
        self.rows = self.rows[:1]
        self.stderr = 'Result: 1/1 error, 1 new\n'
        self.summary = collector.validate(self.stdout, self.stderr, 1, self.rows, self.modes, [])
        self.summary.update(lifecycle_closed=True, identities_unchanged=True, conformance_accepted=False)
        self.rejected('omitted')

    def test_replayed_workflow_attempt_rejected(self):
        current = copy.deepcopy(self.binding)
        current['execution']['GITHUB_RUN_ATTEMPT'] = '2'
        self.rejected('different source', current)

    def test_other_matrix_receipt_rejected(self):
        current = copy.deepcopy(self.binding)
        current['execution']['QJS_TEST262_COMPILER'] = 'MOCK-clang'
        self.rejected('different source', current)

    def test_hidden_sanitizer_log_rejected(self):
        self.stderr = 'file.c:1:2: runtime error: MOCK\n' + self.stderr
        self.rejected('Incomplete')

    def test_log_summary_disagreement_rejected(self):
        self.stdout = self.stdout.replace('unsigned snapshot', 'different failure')
        self.rejected('differs')

    def test_unknown_conflict_signature_remains_unresolved(self):
        self.recipe['observed_conflicts'][0]['signature'] = 'MOCK different signature'
        result, report = self.run_assessment()
        self.assertEqual(result, 1)
        self.assertEqual(len(report['unresolved_failures']), 1)
        self.assertFalse(report['individually_justified_conflicts'])

    def test_changed_original_source_rejected(self):
        self.recipe['source_sha256'] = 'MOCK wrong hash'
        self.rejected('Original source hash')

    def test_changed_recipe_output_rejected(self):
        self.recipe['generated_sha256'] = 'MOCK wrong hash'
        self.rejected('output hash')

    def test_missing_strict_companion_rejected(self):
        self.missing_strict = True
        self.rejected('variant')

    def test_failed_companion_cannot_be_accepted_by_its_summary(self):
        self.fail_companion = True
        self.rejected()

    def test_mismatched_companion_execution_rejected(self):
        self.change_companion_identity = True
        self.rejected('different source')

    def test_mutated_original_logs_rejected(self):
        self.change_raw_after = True
        self.rejected('Original evidence changed')

    def test_mutated_generated_companion_rejected(self):
        self.change_generated_after = True
        self.rejected('Companion changed')

    def test_wrong_actual_data_digest_rejected(self):
        self.ledger['input_digest_sha256'] = 'MOCK wrong digest'
        self.rejected('data input digest')

    def test_other_backend_data_rejected(self):
        self.ledger['backend'] = 'MOCK-icu'
        self.rejected('different backend')

    def test_other_fixture_revision_rejected(self):
        self.ledger['fixture_revision'] = 'MOCK-other-fixture'
        self.rejected('fixture revision')

    def test_duplicate_recipe_source_rejected(self):
        self.ledger['recipes'].append(copy.deepcopy(self.recipe))
        self.rejected('Duplicate recipe')


if __name__ == '__main__':
    unittest.main()
