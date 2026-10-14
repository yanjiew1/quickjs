#!/usr/bin/env python3
"""Pure mocked lifecycle tests; never execute a child command or import CI main."""
import ast
import contextlib
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import types
import unittest
from unittest import mock

SOURCE = Path(__file__).with_name('run.py')
FUNCTIONS = {'sha', 'canonical', 'save', 'require', 'process_cpu_seconds',
             'wait_with_progress', 'run'}


class Clock:
    def __init__(self):
        self.now = 0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class Process:
    pid = 123456789
    args = ['mock-child']

    def __init__(self, clock, completion, result=0):
        self.clock = clock
        self.completion = completion
        self.result = result
        self.waits = []

    def wait(self, timeout=None):
        self.waits.append(timeout)
        if timeout is None:
            return self.result
        if self.completion <= self.clock.now + timeout:
            self.clock.now = self.completion
            return self.result
        self.clock.now += timeout
        raise subprocess.TimeoutExpired(self.args, timeout)


def actual_functions(namespace):
    parsed = ast.parse(SOURCE.read_text())
    nodes = [node for node in parsed.body
             if isinstance(node, ast.FunctionDef) and node.name in FUNCTIONS]
    assert {node.name for node in nodes} == FUNCTIONS
    exec(compile(ast.Module(body=nodes, type_ignores=[]), str(SOURCE), 'exec'), namespace)
    return namespace


class ProgressTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.clock = Clock()
        self.os = mock.Mock(wraps=os)
        self.os.killpg.side_effect = ProcessLookupError
        self.namespace = actual_functions(dict(
            hashlib=hashlib, json=json, os=self.os, Path=Path,
            subprocess=subprocess, time=self.clock, datetime=datetime,
            timezone=timezone, ENV={}, OUT=self.root,
            classify_tool_records=lambda *args: []))
        self.namespace['process_cpu_seconds'] = lambda pid: None
        self.stdout = self.root / 'stdout'
        self.stderr = self.root / 'stderr'
        self.stdout.write_bytes(b'abc')
        self.stderr.write_bytes(b'de')

    def wait(self, completion, timeout=200, raw=True):
        process = Process(self.clock, completion)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            result = self.namespace['wait_with_progress'](
                'command', process, timeout, 0, self.stdout, self.stderr, raw)
        return result, process, output.getvalue()

    def test_short_command_has_no_heartbeat(self):
        result, process, output = self.wait(1)
        self.assertEqual(result, 0)
        self.assertEqual(process.waits, [60])
        self.assertEqual(output, '')

    def test_raw_counts_and_60_second_heartbeats(self):
        result, process, output = self.wait(125)
        self.assertEqual(result, 0)
        self.assertEqual(process.waits, [60, 60, 60])
        self.assertEqual(output.count('[progress]'), 2)
        self.assertIn('elapsed_seconds=60.0', output)
        self.assertIn('elapsed_seconds=120.0', output)
        self.assertIn('process_cpu_seconds=unavailable stdout_bytes=3 stderr_bytes=2', output)
        self.assertEqual(self.stdout.read_bytes(), b'abc')
        self.assertEqual(self.stderr.read_bytes(), b'de')

    def test_merged_mode_labels_combined_count(self):
        _, _, output = self.wait(61, raw=False)
        self.assertIn('merged_output_bytes=3', output)
        self.assertNotIn('stderr_bytes=', output)

    def test_total_deadline_is_not_renewed(self):
        process = Process(self.clock, 1000)
        with contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(subprocess.TimeoutExpired) as raised:
                self.namespace['wait_with_progress'](
                    'command', process, 125, 0, self.stdout, self.stderr, True)
        self.assertEqual(process.waits, [60, 60, 5])
        self.assertEqual(self.clock.now, 125)
        self.assertEqual(raised.exception.timeout, 125)
        self.assertEqual(raised.exception.cmd, process.args)

    def test_zero_timeout_keeps_immediate_poll(self):
        process = Process(self.clock, 1)
        with self.assertRaises(subprocess.TimeoutExpired):
            self.namespace['wait_with_progress'](
                'command', process, 0, 0, self.stdout, self.stderr, True)
        self.assertEqual(process.waits, [0])

    def test_other_wait_error_is_not_swallowed(self):
        process = mock.Mock()
        process.wait.side_effect = RuntimeError('interrupted')
        with self.assertRaisesRegex(RuntimeError, 'interrupted'):
            self.namespace['wait_with_progress'](
                'command', process, 125, 0, self.stdout, self.stderr, True)

    def execute_run(self, completion, timeout=200, raw=True, result=0):
        process = Process(self.clock, completion, result=result)

        def popen(argv, **kwargs):
            kwargs['stdout'].write(b'alpha')
            kwargs['stdout'].flush()
            if kwargs['stderr'] == subprocess.STDOUT:
                kwargs['stdout'].write(b'beta')
                kwargs['stdout'].flush()
            else:
                kwargs['stderr'].write(b'beta')
                kwargs['stderr'].flush()
            return process

        fake_subprocess = types.SimpleNamespace(
            Popen=popen, STDOUT=subprocess.STDOUT,
            TimeoutExpired=subprocess.TimeoutExpired)
        self.namespace['subprocess'] = fake_subprocess
        self.namespace['signal'] = __import__('signal')
        return process, lambda: self.namespace['run'](
            'actual-run', ['mock-child'], timeout=timeout, raw_streams=raw)

    def test_actual_run_preserves_raw_bytes_receipt_and_group_cleanup(self):
        process, execute = self.execute_run(125)
        with contextlib.redirect_stdout(io.StringIO()) as output:
            result = execute()
        self.assertEqual(result, 'alpha\nbeta')
        self.assertEqual((self.root / 'actual-run.stdout.log').read_bytes(), b'alpha')
        self.assertEqual((self.root / 'actual-run.stderr.log').read_bytes(), b'beta')
        self.assertEqual(output.getvalue().count('[progress]'), 2)
        receipt = json.loads((self.root / 'actual-run.command.json').read_text())
        self.assertEqual(receipt['stdout_sha256'], hashlib.sha256(b'alpha').hexdigest())
        self.assertEqual(receipt['stderr_sha256'], hashlib.sha256(b'beta').hexdigest())
        self.assertTrue(receipt['lifecycle_closed'])
        self.assertFalse(receipt['timed_out'])
        self.assertFalse(receipt['interrupted'])
        self.assertEqual(process.waits, [60, 60, 60, None])
        self.assertEqual(self.os.killpg.call_count, 2)

    def test_failed_command_reports_tail_without_changing_retained_raw_logs(self):
        _, execute = self.execute_run(0, result=9)
        with contextlib.redirect_stdout(io.StringIO()) as output:
            with self.assertRaises(RuntimeError):
                execute()
        self.assertIn('[failure] actual-run:', output.getvalue())
        self.assertIn('alpha\nbeta', output.getvalue())
        self.assertEqual((self.root / 'actual-run.stdout.log').read_bytes(), b'alpha')
        self.assertEqual((self.root / 'actual-run.stderr.log').read_bytes(), b'beta')
        receipt = json.loads((self.root / 'actual-run.command.json').read_text())
        self.assertEqual(receipt['returncode'], 9)
        self.assertTrue(receipt['lifecycle_closed'])

    def test_diagnostic_failure_also_reports_unchanged_output(self):
        _, execute = self.execute_run(0)
        self.namespace['classify_tool_records'] = lambda *args: ['mock-diagnostic']
        with contextlib.redirect_stdout(io.StringIO()) as output:
            with self.assertRaises(RuntimeError):
                execute()
        self.assertIn('[failure] actual-run:', output.getvalue())
        self.assertEqual((self.root / 'actual-run.log').read_bytes(), b'alpha\nbeta')

    def test_actual_run_timeout_still_closes_and_records_raw_streams(self):
        process, execute = self.execute_run(1000, timeout=125)
        with contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(subprocess.TimeoutExpired):
                execute()
        receipt = json.loads((self.root / 'actual-run.command.json').read_text())
        self.assertTrue(receipt['timed_out'])
        self.assertFalse(receipt['interrupted'])
        self.assertTrue(receipt['lifecycle_closed'])
        self.assertEqual(process.waits, [60, 60, 5, None])
        self.assertEqual((self.root / 'actual-run.stdout.log').read_bytes(), b'alpha')
        self.assertEqual((self.root / 'actual-run.stderr.log').read_bytes(), b'beta')

    def test_actual_run_merged_capture_is_unchanged(self):
        _, execute = self.execute_run(61, raw=False)
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(execute(), 'alphabeta')
        self.assertEqual((self.root / 'actual-run.log').read_bytes(), b'alphabeta')
        self.assertEqual((self.root / 'actual-run.stderr.log').read_bytes(), b'')
        self.assertIn('merged_output_bytes=9', output.getvalue())


class CpuCounterTests(unittest.TestCase):
    def setUp(self):
        self.namespace = actual_functions(dict(Path=mock.Mock(), os=mock.Mock()))
        self.namespace['os'].sysconf.return_value = 100

    def test_linux_comm_with_spaces_and_parentheses(self):
        fields = ['S'] + ['0'] * 10 + ['125', '75', '0', '0']
        self.namespace['Path'].return_value.read_text.return_value = (
            '123 (name with (parentheses)) ' + ' '.join(fields))
        self.assertEqual(self.namespace['process_cpu_seconds'](123), 2)
        self.namespace['os'].sysconf.assert_called_once_with('SC_CLK_TCK')

    def test_absent_proc_is_unavailable(self):
        self.namespace['Path'].return_value.read_text.side_effect = FileNotFoundError
        self.assertIsNone(self.namespace['process_cpu_seconds'](123))

    def test_malformed_stat_is_unavailable(self):
        self.namespace['Path'].return_value.read_text.return_value = 'bad stat'
        self.assertIsNone(self.namespace['process_cpu_seconds'](123))

    def test_missing_sysconf_is_unavailable(self):
        self.namespace['Path'].return_value.read_text.return_value = (
            '123 (process) ' + ' '.join(['S'] + ['0'] * 14))
        self.namespace['os'].sysconf.side_effect = AttributeError
        self.assertIsNone(self.namespace['process_cpu_seconds'](123))


class CurrentQualificationPinsTests(unittest.TestCase):
    def test_reviewed_static_sources_and_required_manifest_match_checkout(self):
        root = Path(__file__).resolve().parents[3]
        scope = json.loads((root / 'tools/ci/native-prefix/native-normative-static-scope.json').read_text())
        bindings = {item['path']: item['sha256'] for item in scope['source_bindings']}
        required = json.loads((root / 'tools/ci/stop39b-qualification/current-required-source-sha256.json').read_text())
        for kind, sources in (('static scope', bindings), ('required source', required)):
            for name, expected in sources.items():
                with self.subTest(kind=kind, source=name):
                    self.assertEqual(hashlib.sha256((root / name).read_bytes()).hexdigest(),
                                     expected)


if __name__ == '__main__':
    unittest.main()
