"""Pure budget, receipt and owned-process tests; no QuickJS execution."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import signal
import tempfile
import unittest
from unittest import mock

import launch_current_premise_witness as current


class BudgetTests(unittest.TestCase):
    def setUp(self):
        workers = mock.patch.object(current, 'available_cpus', return_value=8)
        workers.start()
        self.addCleanup(workers.stop)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.args = ['make', 'CONFIG_WERROR=y', 'CONFIG_LTO=', 'CC=gcc',
            'HOST_CC=gcc', 'CONFIG_CLANG=', 'CONFIG_ASAN=y', 'CONFIG_UBSAN=',
            'CONFIG_INTL_BACKEND=native', 'CONFIG_ICU=n', 'CONFIG_INTL=y',
            'CONFIG_TEMPORAL=y', 'CONFIG_INTL_LEGACY=y']
        self.scope = {'native': {'C_targets': ['tests/unit'], 'special_C_units': []}}
        (self.root / 'activation-scope.json').write_text(json.dumps(self.scope))
        self.policy = {'schema': 'native-current-witness-execution-budget-v1',
            'backend': 'native', 'compiler': 'gcc', 'sanitizer': 'ASan',
            'source_tree_sha256': 'a' * 64, 'engine_sha256': 'b' * 64,
            'checkout_commit': 'c' * 40}
        build = {'make_args': self.args, 'unicode_backend': 'native', 'available_worker_count': 8,
                 'intl_build_config': ['y', 'n', 'y', 'y', 'native', 'y']}
        command = {'argv': self.args + ['-j8', 'all', 'tests/unit'],
            'returncode': 0, 'timed_out': False, 'interrupted': False,
            'lifecycle_closed': True, 'effective_profile_environment': {
                'PREFIX_BACKEND': 'native', 'PREFIX_COMPILER': 'gcc',
                'PREFIX_SANITIZER': 'ASan'}}
        for key, name, data in [('build_policy', 'candidate-build-policy.json', build),
                                ('build_command', 'build.command.json', command)]:
            path = self.root / name
            path.write_text(json.dumps(data))
            self.policy[key] = {'path': str(path), 'sha256': current.digest(path.read_bytes())}
        self.bindings = {'witness_execution_budget': self.policy,
            'source_tree_sha256': 'a' * 64, 'engine_sha256': 'b' * 64,
            'actual_initialized_checkout_commit': 'c' * 40}
        patch = mock.patch.object(current, 'HERE', self.root)
        patch.start()
        self.addCleanup(patch.stop)
        self.fixture = {'fixture': current.LUNISOLAR_FIXTURE,
                        'fixture_sha256': current.LUNISOLAR_SHA256}

    def test_authenticate_closed_actual_build(self):
        self.assertEqual(current.load_execution_budget(self.bindings), self.policy)

    def test_authenticate_all_supported_compiler_and_sanitizer_rows(self):
        for compiler in ['gcc', 'clang']:
            for sanitizer in ['normal', 'ASan', 'UBSan']:
                with self.subTest(compiler=compiler, sanitizer=sanitizer):
                    self.policy.update(compiler=compiler, sanitizer=sanitizer)
                    args = list(self.args)
                    args[3:8] = ['CC=' + compiler, 'HOST_CC=' + compiler,
                        'CONFIG_CLANG=' + ('y' if compiler == 'clang' else ''),
                        'CONFIG_ASAN=' + ('y' if sanitizer == 'ASan' else ''),
                        'CONFIG_UBSAN=' + ('y' if sanitizer == 'UBSan' else '')]
                    for field in ['build_policy', 'build_command']:
                        ref = self.policy[field]
                        path = Path(ref['path'])
                        data = json.loads(path.read_text())
                        if field == 'build_policy':
                            data['make_args'] = args
                        else:
                            data['argv'] = args + ['-j8', 'all', 'tests/unit']
                            data['effective_profile_environment'].update(
                                PREFIX_COMPILER=compiler, PREFIX_SANITIZER=sanitizer)
                        path.write_text(json.dumps(data))
                        ref['sha256'] = current.digest(path.read_bytes())
                    policy = current.load_execution_budget(self.bindings)
                    self.assertEqual(current.child_timeout(policy, self.fixture),
                                     600 if sanitizer == 'ASan' else 180)

    def test_exact_fixture_and_asan_only(self):
        self.assertEqual(current.child_timeout(self.policy, self.fixture), 600)
        for sanitizer in ['normal', 'UBSan']:
            self.assertEqual(current.child_timeout(dict(self.policy, sanitizer=sanitizer),
                                                   self.fixture), 180)
        for fixture in [None, {}, dict(self.fixture, fixture='other.js'),
                        dict(self.fixture, fixture_sha256='d' * 64)]:
            self.assertEqual(current.child_timeout(self.policy, fixture), 180)

    def test_missing_forged_or_unknown_row_is_rejected(self):
        for key, value in [('sanitizer', 'normal'), ('compiler', 'clang'),
                           ('backend', 'icu'), ('engine_sha256', 'd' * 64),
                           ('source_tree_sha256', 'd' * 64),
                           ('checkout_commit', 'd' * 40), ('unknown', 600)]:
            bad = copy.deepcopy(self.bindings)
            bad['witness_execution_budget'][key] = value
            with self.assertRaises(ValueError):
                current.load_execution_budget(bad)
        with self.assertRaises(ValueError):
            current.load_execution_budget({})

    def test_changed_receipt_rejected_even_with_rebound_hash(self):
        path = Path(self.policy['build_command']['path'])
        original = json.loads(path.read_text())
        for key, value in [('timed_out', True), ('lifecycle_closed', False),
                           ('returncode', 1), ('interrupted', True),
                           ('argv', original['argv'] + ['CONFIG_ASAN='])]:
            changed = dict(original, **{key: value})
            path.write_text(json.dumps(changed))
            with self.assertRaises(ValueError):
                current.load_execution_budget(self.bindings)
            self.policy['build_command']['sha256'] = current.digest(path.read_bytes())
            with self.assertRaises(ValueError):
                current.load_execution_budget(self.bindings)
        path.write_text(json.dumps(original))

    def test_receipt_binds_selected_budget_and_policy(self):
        command = {'timeout_seconds': 600,
                   'execution_budget_sha256': current.budget_digest(self.policy)}
        current.require_budget_receipt(command, self.policy, self.fixture)
        for bad in [dict(command, timeout_seconds=180),
                    dict(command, execution_budget_sha256='e' * 64)]:
            with self.assertRaises(ValueError):
                current.require_budget_receipt(bad, self.policy, self.fixture)
        with self.assertRaises(ValueError):
            current.require_budget_receipt(command, self.policy)


class OwnedExecutorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        packet = Path(current.__file__).parent / 'original-assertions'
        patch = mock.patch('sys.path', [str(packet)] + __import__('sys').path)
        patch.start()
        self.addCleanup(patch.stop)
        spec = importlib.util.spec_from_file_location('budget_test_sealed_launcher',
                                                     packet / 'launch_practical_witness.py')
        self.launcher = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.launcher)
        self.policy = {'sanitizer': 'ASan'}
        self.bindings = {'engine_sha256': 'a' * 64, 'source_tree_sha256': 'b' * 64,
                         'profile_sha256': 'c' * 64}
        self.proc = mock.Mock(pid=123)
        self.proc.poll.return_value = 0
        self.proc.wait.return_value = 0

    def run_child(self, popen=None, times=None, kill=None, fixture_config=None):
        with mock.patch.object(current.subprocess, 'Popen', side_effect=popen or
                               (lambda *a, **k: self.proc)), \
             mock.patch.object(current.os, 'killpg', side_effect=kill or ProcessLookupError), \
             mock.patch.object(current.time, 'monotonic', side_effect=times):
            return current.run_owned(self.launcher, self.policy, ['mock-child'],
                self.root, self.root / 'command', self.bindings, 'd' * 64,
                child_env={'LC_ALL': 'C'}, fixture_config=fixture_config)[0]

    def test_closed_receipt_and_empty_stderr_preserved(self):
        record = self.run_child(times=[0, 1])
        self.assertTrue(record['lifecycle_closed'])
        self.assertFalse(record['timed_out'])
        self.assertEqual(record['stderr_sha256'], hashlib.sha256(b'').hexdigest())
        current.require_budget_receipt(record, self.policy)

    def test_timeout_is_still_a_failed_capture(self):
        self.proc.poll.return_value = None
        self.proc.wait.return_value = -signal.SIGTERM
        record = self.run_child(times=[0, 181, 182])
        self.assertTrue(record['timed_out'])
        self.assertEqual(record['returncode'], -signal.SIGTERM)
        self.assertTrue(record['lifecycle_closed'])
        self.assertEqual(record['timeout_seconds'], 180)

    def test_exact_asan_fixture_still_times_out_at_600(self):
        self.proc.poll.return_value = None
        self.proc.wait.return_value = -signal.SIGTERM
        fixture = {'fixture': current.LUNISOLAR_FIXTURE,
                   'fixture_sha256': current.LUNISOLAR_SHA256}
        record = self.run_child(times=[0, 601, 602], fixture_config=fixture)
        self.assertTrue(record['timed_out'])
        self.assertEqual(record['timeout_seconds'], 600)
        self.assertEqual(record['stdout_limit_bytes'], 2081947648)
        current.require_budget_receipt(record, self.policy, fixture)

    def test_cancellation_during_creation_reaps_before_stopping(self):
        def popen(*args, **kwargs):
            self.assertTrue(self.launcher._DEFER_STOP)
            self.launcher.request_stop(signal.SIGTERM, None)
            return self.proc
        def kill(*args):
            self.assertTrue(self.launcher._DEFER_STOP)
            self.launcher.request_stop(signal.SIGTERM, None)
            raise ProcessLookupError
        record = self.run_child(popen=popen, kill=kill, times=[0, 1])
        self.assertTrue(record['interrupted'])
        self.assertTrue(record['lifecycle_closed'])
        self.assertEqual(self.proc.wait.call_count, 2)
        with mock.patch.object(current.subprocess, 'Popen') as launch:
            with self.assertRaises(KeyboardInterrupt):
                current.run_owned(self.launcher, self.policy, [], self.root,
                    self.root / 'second', self.bindings, 'd' * 64)
            launch.assert_not_called()

    def test_nonempty_stderr_is_never_discarded(self):
        def popen(*args, **kwargs):
            kwargs['stderr'].write(b'diagnostic')
            return self.proc
        record = self.run_child(popen=popen, times=[0, 1])
        self.assertEqual(record['stderr_sha256'], hashlib.sha256(b'diagnostic').hexdigest())
        self.assertEqual((self.root / 'command.stderr.log').read_bytes(), b'diagnostic')


if __name__ == '__main__':
    unittest.main()
