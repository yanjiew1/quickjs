#!/usr/bin/env python3
"""Repository unit for bootstrap helpers using synthetic inputs only."""
import re
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


class HarnessTests(unittest.TestCase):
    def test_harness_keeps_original_timing_and_portable_range_fallback(self):
        patch = (ROOT / 'tests/test262.patch').read_text()
        self.assertNotIn('harness/atomicsHelper.js', patch)
        postimage = []
        selected = False
        for line in patch.splitlines():
            if line.startswith('diff --git '):
                selected = line == 'diff --git a/harness/regExpUtils.js b/harness/regExpUtils.js'
            elif selected and line[:1] in (' ', '+') and not line.startswith('+++'):
                postimage.append(line[1:])
        functions = re.findall(r'^function buildString\(args\) \{[\s\S]*?^\}',
                               '\n'.join(postimage), re.MULTILINE)
        self.assertEqual(len(functions), 1, 'patch must expose the complete optimized helper')
        script = functions[0] + (ROOT / 'tests/test_test262_harness.js').read_text()
        with tempfile.TemporaryDirectory(prefix='.test-harness-', dir=ROOT) as directory:
            root = Path(directory)
            (root / 'harness').mkdir()
            for name in ('sta.js', 'assert.js'):
                (root / 'harness' / name).write_text('')
            (root / 'cases').mkdir()
            (root / 'cases/case.js').write_text('/*---\nflags: [raw]\n---*/\n' + script)
            (root / 'config.conf').write_text('[config]\nstyle=new\nharnessdir=harness\n')
            result = subprocess.run(WRAPPER + [RUNNER, '-c', 'config.conf', '-a', '-r', 'none', '-d', 'cases'],
                                    cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    universal_newlines=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)



if __name__ == '__main__':
    unittest.main()
