#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Bounded property-tool failures, schema checks and real C-owner integration."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from segment_expression import parse_expression
from segment_property import DEFAULT_LIMITS, PropertyResolver
from segment_sets import SetEvaluator
import segment_property

tool = workdir = None


class PropertyResolverTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='segment-property-', dir=workdir)
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def command(self, body):
        script = self.root / 'property_fixture.py'
        script.write_text('import sys, os, time, json\n' + body + '\n')
        return (sys.executable, str(script))

    def reply(self, data):
        return self.command('sys.stdout.write(' + repr(json.dumps(data)) + ')')

    def test_successful_exact_queries_cache_immutable_results(self):
        log = self.root / 'queries.txt'
        command = self.command('''with open({!r}, 'a') as output: output.write(sys.argv[1] + '\\n')
print('{{"unicode":"18.0.0","ranges":[[0,1114112]]}}')'''.format(str(log)))
        resolver = PropertyResolver(command)
        first = resolver('Any')
        self.assertEqual(first, ((0, 0x110000),))
        self.assertIs(first, resolver('Any'))
        resolver('ASCII')
        self.assertEqual(log.read_text().splitlines(), ['Any', 'ASCII'])

    def test_strict_schema_version_and_duplicate_keys(self):
        outputs = ['not json', '{}', '[]', '{"unicode":"18.0.0","ranges":[],"extra":1}',
                   '{"unicode":"17.0.0","ranges":[]}',
                   '{"unicode":"18.0.0","unicode":"18.0.0","ranges":[]}',
                   '{"unicode":"18.0.0","ranges":[[NaN,2]]}',
                   '{"unicode":"18.0.0","ranges":[[true,2]]}',
                   '{"unicode":"18.0.0","ranges":[[1.0,2]]}',
                   '{"unicode":"18.0.0","ranges":[[0,2],[2,3]]}']
        for output in outputs:
            with self.subTest(output=output):
                resolver = PropertyResolver(self.command('sys.stdout.write(' + repr(output) + ')'))
                with self.assertRaises(DataError):
                    resolver('Any')
                self.assertEqual(resolver.cache, {})

    def test_failures_and_output_bounds_release_and_reap_child(self):
        cases = ["sys.stderr.write('failed'); sys.exit(2)",
                 "sys.stderr.write('unexpected diagnostic')",
                 "sys.stdout.write('x' * 1000)", "sys.stderr.write('x' * 1000)"]
        original = subprocess.Popen
        for body in cases:
            children = []
            def spawn(*args, **kwargs):
                child = original(*args, **kwargs)
                children.append(child)
                return child
            with self.subTest(body=body), mock.patch.object(segment_property.subprocess, 'Popen', spawn):
                resolver = PropertyResolver(self.command(body), DEFAULT_LIMITS._replace(stdout=128, stderr=64))
                with self.assertRaises(DataError):
                    resolver('Any')
                self.assertEqual(resolver.cache, {})
                self.assertEqual(len(children), 1)
                self.assertIsNotNone(children[0].poll())
                self.assertTrue(children[0].stdout.closed)
                self.assertTrue(children[0].stderr.closed)

    def test_timeout_missing_executable_and_reader_start_failure(self):
        resolver = PropertyResolver(self.command('time.sleep(5)'),
                                    DEFAULT_LIMITS._replace(seconds=0.05))
        with self.assertRaisesRegex(DataError, 'timed out'):
            resolver('Any')
        self.assertEqual(resolver.cache, {})
        with self.assertRaisesRegex(DataError, 'cannot start'):
            PropertyResolver(self.root / 'missing-executable')('Any')
        original_start, original_spawn = segment_property.threading.Thread.start, subprocess.Popen
        for failing_start in (1, 2):
            children, threads = [], []
            def start(thread):
                threads.append(thread)
                if len(threads) == failing_start:
                    raise RuntimeError('reader unavailable')
                original_start(thread)
            def spawn(*args, **kwargs):
                child = original_spawn(*args, **kwargs)
                children.append(child)
                return child
            resolver = PropertyResolver(self.reply_command())
            with self.subTest(failing_start=failing_start), \
                    mock.patch.object(segment_property.threading.Thread, 'start', start), \
                    mock.patch.object(segment_property.subprocess, 'Popen', spawn):
                with self.assertRaisesRegex(DataError, 'cannot start property pipe reader'):
                    resolver('Any')
            self.assertIsNotNone(children[0].poll())
            self.assertTrue(children[0].stdout.closed)
            self.assertTrue(children[0].stderr.closed)
            self.assertTrue(all(not thread.is_alive() for thread in threads))
        with mock.patch.object(segment_property.threading, 'Event', side_effect=MemoryError), \
                mock.patch.object(segment_property.subprocess, 'Popen') as launch:
            with self.assertRaises(MemoryError):
                PropertyResolver(self.reply_command())('Any')
            launch.assert_not_called()

    @unittest.skipUnless(os.name == 'posix', 'process-group signaling is POSIX-only')
    def test_denied_group_signal_still_kills_and_reaps_owned_child(self):
        children = []
        original_spawn = subprocess.Popen

        def spawn(*args, **kwargs):
            child = original_spawn(*args, **kwargs)
            children.append(child)
            return child

        # Stay alive after exceeding the diagnostic cap so successful cleanup
        # proves the direct-child fallback, rather than ordinary process exit.
        command = self.command("sys.stderr.write('x' * 1000); sys.stderr.flush(); time.sleep(5)")
        resolver = PropertyResolver(command, DEFAULT_LIMITS._replace(stderr=64))
        with mock.patch.object(segment_property.subprocess, 'Popen', spawn), \
                mock.patch.object(segment_property.os, 'killpg', side_effect=PermissionError) as group:
            with self.assertRaisesRegex(DataError, 'output budget'):
                resolver('Any')
        group.assert_called_once()
        self.assertEqual(len(children), 1)
        self.assertIsNotNone(children[0].poll())
        self.assertTrue(children[0].stdout.closed)
        self.assertTrue(children[0].stderr.closed)
        self.assertEqual(resolver.cache, {})

    def reply_command(self):
        return self.reply({'unicode': '18.0.0', 'ranges': [[0, 1], [2, 3]]})

    def test_budgets_and_failed_reply_can_be_retried(self):
        command = self.reply_command()
        resolver = PropertyResolver(command, DEFAULT_LIMITS._replace(queries=1))
        resolver('A')
        with self.assertRaisesRegex(DataError, 'distinct property budget'):
            resolver('B')
        for limits, error in [(DEFAULT_LIMITS._replace(ranges=1), 'range budget'),
                              (DEFAULT_LIMITS._replace(retained_pairs=1), 'cache range budget')]:
            resolver = PropertyResolver(command, limits)
            with self.assertRaisesRegex(DataError, error):
                resolver('A')
            self.assertEqual(resolver.cache, {})
            self.assertEqual(resolver.pairs, 0)
        resolver = PropertyResolver(command)
        resolver.elapsed = resolver.limits.total_seconds
        with self.assertRaisesRegex(DataError, 'cumulative property time budget'):
            resolver('A')
        resolver = PropertyResolver(self.command('sys.exit(1)'))
        with self.assertRaises(DataError):
            resolver('A')
        self.reply_command()  # Replace the same test executable with a valid producer.
        self.assertEqual(resolver('A'), ((0, 1), (2, 3)))

    def test_invalid_queries_limits_and_commands_fail_before_spawn(self):
        resolver = PropertyResolver(self.root / 'missing-executable')
        for query in ('', 'a b', 'a-b', 'a=b=c', '=a', 'é', 'A' * 96, 'A=' + 'B' * 96):
            with self.subTest(query=query), self.assertRaisesRegex(DataError, 'spelling'):
                resolver(query)
        for limits in (DEFAULT_LIMITS._replace(stdout=True), DEFAULT_LIMITS._replace(seconds=float('nan')),
                       DEFAULT_LIMITS._replace(stdout=10 ** 1000),
                       DEFAULT_LIMITS._replace(ranges=1.5), DEFAULT_LIMITS._replace(queries=0)):
            with self.assertRaises(DataError):
                PropertyResolver(self.root / 'unused', limits)
        for command in ((), ('',), ('a\0b',), [True], ('intl-segment-property',)):
            with self.assertRaises(DataError):
                PropertyResolver(command)

    def test_real_c_tool_and_set_evaluator_share_unicode_owner(self):
        if tool is None:
            self.skipTest('explicit build-machine C tool required')
        resolver = PropertyResolver(tool)
        self.assertEqual(resolver('gc=Cs'), ((0xd800, 0xe000),))
        self.assertEqual(resolver('ASCII=False'), ((0x80, 0x110000),))
        self.assertEqual(resolver('Any'), ((0, 0x110000),))
        self.assertEqual(resolver('GCB=CR'), ((13, 14),))
        self.assertTrue(any(start <= 0x3042 < end for start, end in resolver('Hiragana')))
        node = parse_expression(r'[^\p{gc=Cs}]')
        self.assertEqual(SetEvaluator((), resolver).evaluate(node),
                         ((0, 0xd800), (0xe000, 0x110000)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool')
    parser.add_argument('--workdir')
    args, remaining = parser.parse_known_args()
    tool, workdir = args.tool, args.workdir
    unittest.main(argv=[__file__] + remaining)
