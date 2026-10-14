#!/usr/bin/env python3
"""Repository unit for exact runner selection; no external Test262 suite."""
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
WRAPPER = shlex.split(sys.argv[1]) if len(sys.argv) > 1 else []
RUNNER = str((ROOT / (sys.argv[2] if len(sys.argv) > 2 else 'run-test262')).resolve())
sys.argv[1:] = []


class SelectionTests(unittest.TestCase):
    def test_distinct_zero_padded_names_and_exact_duplicates(self):
        with tempfile.TemporaryDirectory(prefix='.test-selection-', dir=ROOT) as directory:
            root = Path(directory)
            cases = root / 'cases'
            cases.mkdir()
            harness = root / 'harness'
            harness.mkdir()
            for name in ('sta.js', 'assert.js'):
                (harness / name).write_text('')
            names = ['case1.js', 'case01.js', 'case001.js', 'case' + '0' * 64 + '1.js',
                     'case0.js', 'case00.js', 'case' + '9' * 64 + '.js']
            for name in names + ['helper_FIXTURE.js']:
                (cases / name).write_text('/*---\nflags: [raw]\n---*/\n0;\n')
            (root / 'config.conf').write_text('[config]\nstyle=new\nharnessdir=harness\n')
            command = WRAPPER + [RUNNER, '-c', 'config.conf', '-a', '-r', 'none',
                                '--timing-log', 'timings.jsonl']
            result = subprocess.run(command + ['-d', 'cases'], cwd=root,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    universal_newlines=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            rows = [json.loads(line) for line in (root / 'timings.jsonl').read_text().splitlines()]
            self.assertEqual({Path(row['file']).name for row in rows}, set(names))
            self.assertEqual(len(rows), len(names))

            # Exact repeated list entries are still deduplicated. Excluding
            # case1 must not exclude case01 or other zero-padded spellings.
            (root / 'empty').mkdir()
            (root / 'selected.txt').write_text(''.join('cases/' + name + '\n'
                                                       for name in names + [names[0]]))
            (root / 'excluded.txt').write_text('cases/case1.js\n')
            result = subprocess.run(command + ['-d', 'empty', '-x', 'excluded.txt', 'selected.txt'],
                                    cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    universal_newlines=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            rows = [json.loads(line) for line in (root / 'timings.jsonl').read_text().splitlines()]
            self.assertEqual({Path(row['file']).name for row in rows}, set(names[1:]))
            self.assertEqual(len(rows), len(names) - 1)
            self.assertIn('1 excluded', result.stderr)


if __name__ == '__main__':
    unittest.main()
