#!/usr/bin/env python3
"""Selection auditing must detect missing and accidentally skipped coverage."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'tools/ci/audit_temporal_test262.py'
PREFIX = 'test/intl402/Temporal/'


class SelectionTest(unittest.TestCase):
    def test_real_execution_records(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sources = {
                'PlainDate/from/valid.js': '[Temporal, Intl.Era-monthcode]',
                'PlainDate/from/message.js': '[Temporal]',
                'PlainDate/prototype/toLocaleString/basic.js': '[Temporal]',
                'ZonedDateTime/enum.js': '\n  - Temporal\n  - Intl-enumeration',
            }
            rows = []
            for name, features in sources.items():
                path = root / 'test262' / PREFIX / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('/*---\nfeatures: {}\n---*/\n'
                                'assert.sameValue("Intl.DateTimeFormat", '
                                '"Intl.DateTimeFormat");\n'.format(features), encoding='utf-8')
                skipped = 'toLocaleString' in name or name.endswith('enum.js')
                rows.append(dict(file='test262/' + PREFIX + name, thread=0,
                                 index=len(rows), mode='none' if skipped else 'non-strict',
                                 status='skipped' if skipped else 'pass', duration_ms=0))
            timings, output = root / 'timings.jsonl', root / 'audit.md'

            def check(entries, expected):
                timings.write_text(''.join(json.dumps(row) + '\n' for row in entries),
                                   encoding='utf-8')
                result = subprocess.run([sys.executable, str(TOOL), str(timings),
                                         '--test262', str(root / 'test262'),
                                         '--markdown', str(output)], stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, universal_newlines=True)
                self.assertEqual(result.returncode, expected, result.stderr + result.stdout)
                return output.read_text(encoding='utf-8')

            self.assertIn('2 of 4 source files', check(rows, 0))
            self.assertIn('Missing timing record', check(rows[1:], 1))
            changed = [dict(row) for row in rows]
            changed[0].update(status='skipped', mode='none')
            self.assertIn('Applicable source skipped', check(changed, 1))
            changed = [dict(row) for row in rows]
            changed[-1].update(status='pass', mode='non-strict')
            self.assertIn('Unsupported Intl source executed', check(changed, 1))
            changed = [dict(row) for row in rows]
            changed[0]['status'] = 'fail'
            self.assertIn('Every applicable source executed', check(changed, 0))


if __name__ == '__main__':
    unittest.main()
