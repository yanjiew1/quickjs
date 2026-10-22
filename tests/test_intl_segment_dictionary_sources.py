#!/usr/bin/env python3
"""Offline source integrity, release identity and dictionary-license retention."""
import copy
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import acquire
import segment_dictionary_sources as dictionaries


class DictionarySourcesTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.cache = Path(self.temporary.name)
        self.root = self.cache / 'icu-dictionaries-79.1'
        self.root.mkdir()
        self.pin = copy.deepcopy(dictionaries.load_pin())
        # Authored bytes isolate trust-boundary behavior from large downloads
        # and from the dictionary parser, which has a separate responsibility.
        for row in self.pin['files']:
            data = ('license\n' if row['name'] == 'LICENSE' else 'ภาษา\n').encode('utf-8')
            row['bytes'] = len(data)
            row['sha256'] = acquire.digest(data)
            row['git_blob'] = hashlib.sha1(b'blob ' + str(len(data)).encode('ascii') + b'\0' + data).hexdigest()
            (self.root / row['name']).write_bytes(data)

    def test_offline_preparation_and_consumed_identity(self):
        sources = dictionaries.prepare(self.cache, True, self.pin)
        self.assertEqual(sources.identity()['commit'], self.pin['commit'])
        self.assertEqual(set(sources.identity()['files']), set(dictionaries.FAMILIES))
        sources = dictionaries.Sources(self.root, self.pin)
        self.assertEqual(sources.consumed, {'LICENSE'})
        sources.read('thaidict.txt')
        self.assertEqual(sources.consumed, {'LICENSE', 'thaidict.txt'})
        self.pin['files'][0]['sha256'] = '0' * 64
        self.assertNotEqual(sources.pin['files'][0]['sha256'], '0' * 64)

    def test_consumption_rechecks_bytes_and_length(self):
        sources = dictionaries.Sources(self.root, self.pin)
        path = self.root / 'thaidict.txt'
        original = path.read_bytes()
        path.write_bytes(b'x' * len(original))
        with self.assertRaises(ValueError):
            sources.read('thaidict.txt')
        self.assertNotIn('thaidict.txt', sources.consumed)
        path.write_bytes(original[:-1])
        with self.assertRaises(ValueError):
            dictionaries.prepare(self.cache, True, self.pin)
        path.write_bytes(original)
        self.assertEqual(sources.read('thaidict.txt'), original)

    def test_missing_license_is_not_a_usable_cache(self):
        (self.root / 'LICENSE').unlink()
        with self.assertRaises(ValueError):
            dictionaries.Sources(self.root, self.pin)
        with self.assertRaises(ValueError):
            dictionaries.prepare(self.cache, True, self.pin)

    def test_release_urls_inventory_and_blob_identity(self):
        for change in ['url', 'name', 'duplicate', 'blob']:
            pin = copy.deepcopy(self.pin)
            if change == 'url':
                pin['files'][0]['url'] = pin['files'][0]['url'].replace(pin['commit'], 'main')
            elif change == 'name':
                pin['files'][0]['name'] = '../thaidict.txt'
            elif change == 'duplicate':
                pin['files'][0] = copy.deepcopy(pin['files'][1])
            else:
                pin['files'][0]['git_blob'] = '0' * 40
            with self.subTest(change=change), self.assertRaises(ValueError):
                dictionaries.prepare(self.cache, True, pin)

    def test_unlisted_files_and_cache_symlinks(self):
        sources = dictionaries.Sources(self.root, self.pin)
        for name in ['../LICENSE', 'unknown.txt']:
            with self.assertRaises(ValueError):
                sources.read(name)
        path = self.root / 'thaidict.txt'
        data = path.read_bytes()
        path.unlink()
        target = self.cache / 'outside.txt'
        target.write_bytes(data)
        path.symlink_to(target)
        with self.assertRaises(ValueError):
            sources.read('thaidict.txt')
        with self.assertRaises(ValueError):
            dictionaries.prepare(self.cache, True, self.pin)

    def test_distributed_full_license_matches_release_pin(self):
        pin = dictionaries.load_pin()
        license_row = next(row for row in pin['files'] if row['name'] == 'LICENSE')
        license_file = Path(__file__).resolve().parents[1] / 'data' / 'intl' / 'LICENSE-SEGMENT-DICTIONARIES'
        self.assertEqual(acquire.digest(license_file.read_bytes()), license_row['sha256'])


if __name__ == '__main__':
    unittest.main()
