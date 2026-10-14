"""Pure checkout identity regressions; no engine or external Git process."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parent))
import checkout_identity as identity


class CheckoutIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.sub = self.root / identity.TZ_PATH
        self.sub.mkdir(parents=True)
        self.objects = {}
        self.top_entries = []
        self.sub_entries = []
        self.head = identity.TZ_COMMIT
        self.filemodes = {str(self.root): b'true\n', str(self.sub): b'true\n'}
        digests = {}
        for name in sorted(identity.TZ_INPUTS):
            data = (name + '\n').encode()
            self.add_blob(self.sub, self.sub_entries, name, data)
            digests[name] = hashlib.sha256(data).hexdigest()
        pin = dict(commit=identity.TZ_COMMIT, release=identity.TZ_RELEASE,
                   tag=identity.TZ_RELEASE, files=digests)
        self.add_blob(self.root, self.top_entries, 'tools/timezone/pin.json',
                      json.dumps(pin).encode())
        self.add_blob(self.root, self.top_entries, 'README', b'checkout\n')
        self.top_entries.append(('160000', 'commit', identity.TZ_COMMIT,
                                 identity.TZ_PATH))

    def add_blob(self, repo, entries, name, data, mode='100644'):
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        self.objects[(str(repo), blob)] = data
        entries.append((mode, 'blob', blob, name))
        path = repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(0o755 if mode == '100755' else 0o644)

    def command(self, repo, *args, **kwargs):
        repo = Path(repo)
        if args == ('config', '--bool', '--get', 'core.filemode'):
            return self.filemodes[str(repo)]
        if args == ('ls-tree', '-rz', 'HEAD'):
            rows = self.top_entries if repo == self.root else self.sub_entries
            return b''.join(('%s %s %s\t%s\0' % row).encode() for row in rows)
        if args == ('rev-parse', 'HEAD') and repo == self.root:
            return b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n'
        if args == ('rev-parse', 'HEAD') and repo == self.sub:
            return (self.head + '\n').encode()
        if args == ('cat-file', '--batch'):
            out = b''
            for blob in kwargs['input'].decode().splitlines():
                data = self.objects[(str(repo), blob)]
                out += ('%s blob %s\n' % (blob, len(data))).encode() + data + b'\n'
            return out
        self.fail('Unexpected command: ' + repr(args))

    def inspect(self):
        return identity.inspect_checkout(self.root, self.command)

    def rejected(self):
        with self.assertRaises(ValueError):
            self.inspect()

    def test_complete_approved_release(self):
        result = self.inspect()
        self.assertEqual(len(result['sources']), len(identity.TZ_INPUTS) + 2)
        self.assertNotIn(identity.TZ_PATH, result['sources'])
        self.assertEqual(result['gitlinks'][identity.TZ_PATH]['commit'], self.head)
        self.assertEqual(len(result['gitlinks'][identity.TZ_PATH]['files']),
                         len(identity.TZ_INPUTS))

    def test_reject_wrong_gitlink_commit(self):
        self.top_entries[-1] = ('160000', 'commit', '0' * 40, identity.TZ_PATH)
        self.rejected()

    def test_reject_wrong_gitlink_path(self):
        self.top_entries[-1] = ('160000', 'commit', self.head, 'third_party/other')
        self.rejected()

    def test_reject_wrong_gitlink_mode(self):
        self.top_entries[-1] = ('100644', 'commit', self.head, identity.TZ_PATH)
        self.rejected()

    def test_reject_missing_gitlink(self):
        self.top_entries.pop()
        self.rejected()

    def test_reject_changed_submodule_head(self):
        self.head = '1' * 40
        self.rejected()

    def test_reject_changed_committed_bytes(self):
        (self.sub / 'asia').write_bytes(b'changed\n')
        self.rejected()

    def test_reject_changed_mode(self):
        (self.sub / 'asia').chmod(0o755)
        self.rejected()

    def test_posix_false_config_still_enforces_execute_bits(self):
        self.filemodes[str(self.sub)] = b'false\n'
        (self.sub / 'asia').chmod(0o755)
        with patch.object(identity.sys, 'platform', 'linux'):
            self.rejected()

    def test_windows_true_or_missing_config_enforces_execute_bits(self):
        (self.sub / 'asia').chmod(0o755)
        for value in (b'true\n', b''):
            with self.subTest(value=value):
                self.filemodes[str(self.sub)] = value
                with patch.object(identity.sys, 'platform', 'cygwin'):
                    self.rejected()

    def test_windows_false_config_records_git_modes_without_stat_claim(self):
        self.filemodes[str(self.sub)] = b'false\n'
        (self.sub / 'asia').chmod(0o755)
        for platform in ('win32', 'cygwin', 'msys'):
            with self.subTest(platform=platform):
                with patch.object(identity.sys, 'platform', platform):
                    result = self.inspect()
                self.assertEqual(result['modes'][identity.TZ_PATH + '/asia'], '100644')
                self.assertTrue(result['mode_capabilities']['root']['stat_execute_bits_enforced'])
                self.assertFalse(result['mode_capabilities'][identity.TZ_PATH]['stat_execute_bits_enforced'])

    def test_windows_capability_is_separate_for_root_and_submodule(self):
        self.filemodes[str(self.root)] = b'false\n'
        (self.root / 'README').chmod(0o755)
        with patch.object(identity.sys, 'platform', 'cygwin'):
            result = self.inspect()
            self.assertFalse(result['mode_capabilities']['root']['stat_execute_bits_enforced'])
            self.assertTrue(result['mode_capabilities'][identity.TZ_PATH]['stat_execute_bits_enforced'])
            (self.sub / 'asia').chmod(0o755)
            self.rejected()

    def test_windows_false_config_still_rejects_changed_bytes(self):
        self.filemodes[str(self.sub)] = b'false\n'
        (self.sub / 'asia').write_bytes(b'changed\n')
        with patch.object(identity.sys, 'platform', 'cygwin'):
            self.rejected()

    def test_windows_false_config_still_rejects_symlinks(self):
        self.filemodes[str(self.sub)] = b'false\n'
        path = self.sub / 'asia'
        path.unlink()
        path.symlink_to(self.sub / 'europe')
        with patch.object(identity.sys, 'platform', 'cygwin'):
            self.rejected()

    def test_invalid_windows_config_is_rejected(self):
        self.filemodes[str(self.root)] = b'unknown\n'
        with patch.object(identity.sys, 'platform', 'cygwin'):
            self.rejected()

    def test_reject_symlink(self):
        path = self.sub / 'asia'
        path.unlink()
        path.symlink_to(self.sub / 'europe')
        self.rejected()

    def test_reject_missing_input(self):
        (self.sub / 'asia').unlink()
        self.rejected()

    def test_reject_nested_gitlink(self):
        self.sub_entries.append(('160000', 'commit', self.head, 'nested'))
        self.rejected()

    def test_reject_pin_mismatch(self):
        path = self.root / 'tools/timezone/pin.json'
        value = json.loads(path.read_text())
        value['files']['asia'] = '0' * 64
        self.top_entries = [row for row in self.top_entries if row[3] != 'tools/timezone/pin.json']
        self.add_blob(self.root, self.top_entries, 'tools/timezone/pin.json',
                      json.dumps(value).encode())
        self.rejected()

    def test_all_submodule_blobs_affect_identity(self):
        before = self.inspect()
        self.add_blob(self.sub, self.sub_entries, 'extra', b'extra\n')
        after = self.inspect()
        self.assertNotEqual(before['sources'], after['sources'])
        self.assertNotEqual(before['gitlinks'], after['gitlinks'])


if __name__ == '__main__':
    unittest.main()
