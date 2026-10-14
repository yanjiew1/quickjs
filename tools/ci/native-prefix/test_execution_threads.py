import unittest
from unittest import mock
import execution_threads as threads


class ExecutionThreadsTests(unittest.TestCase):
    def test_uses_affinity_instead_of_host_cpu_count(self):
        with mock.patch.object(threads.os, 'sched_getaffinity', return_value={1, 4, 7}, create=True):
            self.assertEqual(threads.available_cpus(), 3)

    def test_fallback_is_positive(self):
        with mock.patch.object(threads.os, 'sched_getaffinity', side_effect=OSError, create=True):
            for value in (None, 0, 6):
                with mock.patch.object(threads.os, 'cpu_count', return_value=value):
                    self.assertEqual(threads.available_cpus(), value or 1)

    def test_substitutes_execution_only(self):
        historical = ['./run-test262', '-T', '2', '-q']
        with mock.patch.object(threads, 'available_cpus', return_value=8):
            self.assertEqual(threads.test262_workers(historical), ['./run-test262', '-T', '8', '-q'])
        self.assertEqual(historical, ['./run-test262', '-T', '2', '-q'])
