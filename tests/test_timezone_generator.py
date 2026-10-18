#!/usr/bin/env python3
"""Bounded source acquisition and offline timezone generation contracts."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tools' / 'timezone'
spec = importlib.util.spec_from_file_location('timezone_acquire',
                                             str(TOOLS / 'acquire.py'))
acquire = importlib.util.module_from_spec(spec)
spec.loader.exec_module(acquire)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class AcquisitionTests(unittest.TestCase):
    def test_verified_cache_is_offline(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            path.write_bytes(b'pinned')
            source = {'url': 'https://invalid.test/data', 'sha256': sha256(b'pinned')}
            with mock.patch.object(acquire.urllib.request, 'urlopen') as network:
                self.assertEqual(acquire.fetch(path, source), b'pinned')
                network.assert_not_called()
                self.assertEqual(acquire.fetch(path, source, True), b'pinned')

    def test_bad_download_preserves_existing_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            path.write_bytes(b'existing')
            source = {'url': 'https://invalid.test/data', 'sha256': sha256(b'pinned')}
            with mock.patch.object(acquire.urllib.request, 'urlopen',
                                   return_value=io.BytesIO(b'wrong')):
                with self.assertRaises(ValueError):
                    acquire.fetch(path, source)
            self.assertEqual(path.read_bytes(), b'existing')

    def test_valid_download_is_atomic_and_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'nested' / 'source'
            source = {'url': 'https://invalid.test/data', 'sha256': sha256(b'pinned')}
            with mock.patch.object(acquire.urllib.request, 'urlopen',
                                   return_value=io.BytesIO(b'pinned')) as network:
                self.assertEqual(acquire.fetch(path, source), b'pinned')
                self.assertEqual(network.call_args[1]['timeout'], 30)
            self.assertEqual(path.read_bytes(), b'pinned')
            self.assertEqual([entry.name for entry in path.parent.iterdir()], ['source'])

    def test_offline_rejects_missing_or_modified(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source'
            source = {'url': 'https://invalid.test/data', 'sha256': sha256(b'pinned')}
            for exists in [False, True]:
                if exists:
                    path.write_bytes(b'modified')
                with mock.patch.object(acquire.urllib.request, 'urlopen') as network:
                    with self.assertRaises(ValueError):
                        acquire.fetch(path, source, True)
                    network.assert_not_called()

    @staticmethod
    def archive(rows):
        output = io.BytesIO()
        with tarfile.open(fileobj=output, mode='w:gz') as tar:
            for name, data, link in rows:
                member = tarfile.TarInfo(name)
                if link:
                    member.type = tarfile.SYMTYPE
                    member.linkname = data
                    tar.addfile(member)
                else:
                    member.size = len(data)
                    tar.addfile(member, io.BytesIO(data))
        return output.getvalue()

    def test_only_verified_regular_files_are_extracted(self):
        raw = self.archive([('tz-test/africa', b'data', False),
                            ('../escape', b'not extracted', False)])
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'source'
            acquire.extract_sources(raw, target, 'test', {'africa': sha256(b'data')})
            self.assertEqual((target / 'africa').read_bytes(), b'data')
            self.assertEqual([entry.name for entry in Path(directory).iterdir()], ['source'])

    def test_rejects_link_and_validates_all_before_writes(self):
        rows = [('tz-test/africa', b'data', False),
                ('tz-test/asia', b'wrong', False)]
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)
            with self.assertRaises(ValueError):
                acquire.extract_sources(self.archive(rows), target, 'test',
                    {'africa': sha256(b'data'), 'asia': sha256(b'expected')})
            self.assertEqual(list(target.iterdir()), [])
            raw = self.archive([('tz-test/africa', '/tmp/invalid', True)])
            with self.assertRaises(ValueError):
                acquire.extract_sources(raw, target, 'test', {'africa': sha256(b'data')})
            self.assertEqual(list(target.iterdir()), [])

    def test_pinned_metadata_is_present_and_unchanged(self):
        pin = json.loads((TOOLS / 'pin.json').read_text())
        self.assertEqual(pin['tz']['tag'], pin['tz']['release'])
        for name, source in pin['cldr']['files'].items():
            data = (ROOT / 'data' / 'timezone' / 'sources' / 'cldr' / name).read_bytes()
            self.assertEqual(sha256(data), source['sha256'], name)


if __name__ == '__main__':
    unittest.main()
