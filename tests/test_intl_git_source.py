#!/usr/bin/env python3
"""Git commit/blob identity and authenticated archive bytes must agree."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/intl'))
import check_git


@unittest.skipUnless(shutil.which('git'), 'Git is needed for source comparison')
class GitSourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / 'repo'
        self.root.mkdir()
        hooks = Path(self.temporary.name) / 'empty-hooks'
        hooks.mkdir()
        command = ['git', '-C', str(self.root), '-c', 'core.hooksPath=' + str(hooks),
                   '-c', 'commit.gpgsign=false', '-c', 'user.name=Source fixture',
                   '-c', 'user.email=fixture@example.invalid']
        subprocess.check_call(command + ['init', '-q'])
        files = {'LICENSE': b'authored license', 'common/main/en.xml': b'<ldml/>',
                 'common/dtd/ldml.dtd': b'authored DTD'}
        for name, data in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (self.root / 'tool.c').write_bytes(b'unselected program')
        subprocess.check_call(command + ['add', '.'])
        subprocess.check_call(command + ['commit', '-qm', 'authored fixture'])
        commit = subprocess.check_output(command + ['rev-parse', 'HEAD']).decode().strip()

        class Sources:
            pin = {'distribution': 'git', 'commit': commit, 'manifest_sha256': 'fixture'}
            manifest = {'files': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}

        self.sources = Sources()

    def test_complete_snapshot_ignores_unselected_programs(self):
        result = check_git.verify(self.root, self.sources)
        self.assertEqual(result['logical_files'], 3)
        self.assertEqual(result['commit'], self.sources.pin['commit'])
        (self.root / 'tool.c').write_bytes(b'changed but not source data')
        self.assertEqual(check_git.verify(self.root, self.sources), result)

    def test_commit_distribution_and_inventory_mismatches(self):
        self.sources.pin = dict(self.sources.pin, commit='0' * 40)
        with self.assertRaisesRegex(ValueError, 'release commit'):
            check_git.verify(self.root, self.sources)
        self.sources.pin['distribution'] = 'common'
        with self.assertRaisesRegex(ValueError, 'full-Git'):
            check_git.verify(self.root, self.sources)

    def test_modified_working_tree_or_manifest(self):
        path = self.root / 'common/main/en.xml'
        path.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'modified Git source'):
            check_git.verify(self.root, self.sources)
        path.write_bytes(b'<ldml/>')
        del self.sources.manifest['files']['LICENSE']
        with self.assertRaisesRegex(ValueError, 'absent from pinned inventory'):
            check_git.verify(self.root, self.sources)


if __name__ == '__main__':
    unittest.main()
