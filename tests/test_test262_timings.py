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

"""Check timing report results and preservation of partial CI evidence."""
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'tools/ci/report_test262_timings.py'
ROOT = TOOL.parents[2]


class TimingReportTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('sh'), 'CI reporting uses a POSIX shell')
    def test_ci_summary_preserves_partial_failure_and_missing_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first, second = root / 'first', root / 'second'
            summary = root / 'summary.md'
            environment = dict(os.environ, GITHUB_STEP_SUMMARY=str(summary))
            command = ['sh', str(TOOL.with_name('report_test262_summaries.sh')),
                       str(first), str(second)]
            self.assertEqual(subprocess.call(command, cwd=ROOT, env=environment), 0)
            self.assertFalse(summary.exists())
            row = dict(file='case.js', thread=0, index=0, mode='strict',
                       status='pass', duration_ms=1)
            Path(str(first) + '_timings.jsonl').write_text('{broken\n')
            Path(str(second) + '_timings.jsonl').write_text(json.dumps(row) + '\n')
            # A bad first report must neither hide its failure nor prevent
            # the later valid report from preserving its evidence.
            self.assertEqual(subprocess.call(command, cwd=ROOT, env=environment), 1)
            self.assertTrue(Path(str(first) + '_timings.csv').exists())
            self.assertTrue(Path(str(second) + '_timings.csv').exists())
            text = summary.read_text()
            self.assertIn('1 rejected records', text)
            self.assertIn('1 pass', text)

    @unittest.skipUnless(shutil.which('sh'), 'CI reporting uses a POSIX shell')
    def test_ci_repository_summary_accepts_partial_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            logs = root / 'build/meson-logs'
            logs.mkdir(parents=True)
            (logs / 'repo-unit.txt').write_text(''.join(str(i) + '\n' for i in range(25)))
            summary = root / 'summary.md'
            environment = dict(os.environ, GITHUB_STEP_SUMMARY=str(summary))
            self.assertEqual(subprocess.call(
                ['sh', str(TOOL.with_name('report_repo_results.sh'))],
                cwd=root, env=environment), 0)
            text = summary.read_text()
            self.assertIn('### repo-unit', text)
            self.assertNotIn('repo-integration', text)
            self.assertIn('```text\n5\n', text)
            self.assertIn('24\n', text)

    def test_complete_and_partial(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rows = [dict(file='a|`\n.js', thread=0, index=0, mode='non-strict',
                         status='pass', duration_ms=2.5),
                    dict(file='a|`\n.js', thread=1, index=0, mode='strict',
                         status='fail', duration_ms=7),
                    dict(file='skip.js', thread=0, index=1, mode='none',
                         status='skipped', duration_ms=0)]
            source = root / 'input.jsonl'
            source.write_text(''.join(json.dumps(row) + '\n' for row in rows),
                              encoding='utf-8')
            command = [sys.executable, str(TOOL), str(source),
                       '--csv', str(root / 'report.csv'),
                       '--markdown', str(root / 'report.md')]
            self.assertEqual(subprocess.call(command), 0)
            with (root / 'report.csv').open(newline='', encoding='utf-8') as stream:
                output = list(csv.DictReader(stream))
            self.assertEqual(len(output), 3)
            self.assertEqual(output[0]['file'], rows[0]['file'])
            text = (root / 'report.md').read_text(encoding='utf-8')
            self.assertIn('1 pass, 1 fail, 1 skipped', text)
            self.assertIn('0.009 seconds', text)
            self.assertIn('a&#124;&#96;&#10;.js', text)
            self.assertLess(text.index('| strict |'), text.index('| non-strict |'))
            with source.open('a', encoding='utf-8') as stream:
                stream.write(json.dumps(dict(rows[0], duration_ms=-1)) + '\n')
                stream.write('{"file":')
            self.assertEqual(subprocess.call(command), 1)
            text = (root / 'report.md').read_text(encoding='utf-8')
            self.assertIn('2 rejected records', text)
            self.assertIn('3 completed records', text)


if __name__ == '__main__':
    unittest.main()
