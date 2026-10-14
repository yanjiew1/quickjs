#!/usr/bin/env python3
"""Check timing report results and preservation of partial CI evidence."""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'tools/ci/report_test262_timings.py'


class TimingReportTest(unittest.TestCase):
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
