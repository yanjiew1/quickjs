#!/usr/bin/env python3
"""Offline fixtures for authenticated CLDR input acquisition and consumption."""
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
spec = importlib.util.spec_from_file_location(
    'intl_acquire', str(ROOT / 'tools' / 'intl' / 'acquire.py'))
acquire = importlib.util.module_from_spec(spec)
spec.loader.exec_module(acquire)


def digest(data):
    return hashlib.sha256(data).hexdigest()


class Response(io.BytesIO):
    def geturl(self):
        return 'https://example.invalid/cldr.tar.gz'


def fixture(extra=()):
    files = {'LICENSE': b'fixture license', 'common/dtd/ldml.dtd': b'fixture DTD',
             'common/main/en.xml': b'<ldml><identity/></ldml>'}
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode='w:gz') as tar:
        for name, data in sorted(files.items()):
            member = tarfile.TarInfo('cldr-fixture/' + name)
            member.size = len(data)
            tar.addfile(member, io.BytesIO(data))
        for member in extra:
            tar.addfile(member)
    archive = output.getvalue()
    manifest = {'archive_sha256': digest(archive), 'release': 'fixture',
                'files': {name: digest(data) for name, data in files.items()}}
    encoded = (json.dumps(manifest, sort_keys=True, indent=2) + '\n').encode('utf-8')
    pin = {'commit': 'fixture', 'release': 'fixture',
           'archive': {'url': 'https://example.invalid/cldr.tar.gz',
                       'sha256': digest(archive)}, 'manifest_sha256': digest(encoded)}
    return archive, pin


class DownloadTests(unittest.TestCase):
    def test_verified_cache_and_offline_rejection(self):
        archive, pin = fixture()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'archive'
            path.write_bytes(archive)
            with mock.patch.object(acquire.urllib.request, 'urlopen') as network:
                self.assertEqual(acquire.fetch(path, pin['archive']), archive)
                self.assertEqual(acquire.fetch(path, pin['archive'], True), archive)
                network.assert_not_called()
            path.write_bytes(b'corrupt')
            with self.assertRaises(ValueError):
                acquire.fetch(path, pin['archive'], True)

    def test_download_verifies_before_replacing(self):
        archive, pin = fixture()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'archive'
            path.write_bytes(b'previous')
            with mock.patch.object(acquire.urllib.request, 'urlopen',
                                   return_value=Response(b'wrong')):
                with self.assertRaises(ValueError):
                    acquire.fetch(path, pin['archive'])
            self.assertEqual(path.read_bytes(), b'previous')
            with mock.patch.object(acquire.urllib.request, 'urlopen',
                                   return_value=Response(archive)) as network:
                self.assertEqual(acquire.fetch(path, pin['archive']), archive)
                self.assertEqual(network.call_args[1]['timeout'], 30)
            self.assertEqual(path.read_bytes(), archive)
            self.assertEqual([p.name for p in path.parent.iterdir()], ['archive'])

    def test_https_and_download_bound(self):
        archive, pin = fixture()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'archive'
            source = dict(pin['archive'], url='http://example.invalid/archive')
            with self.assertRaises(ValueError):
                acquire.fetch(path, source)
            with mock.patch.object(acquire, 'MAX_DOWNLOAD', 8), \
                    mock.patch.object(acquire.urllib.request, 'urlopen',
                                      return_value=Response(archive)):
                with self.assertRaises(ValueError):
                    acquire.fetch(path, pin['archive'])
            self.assertFalse(path.exists())

    def test_progress_cannot_bypass_deadline(self):
        archive, pin = fixture()

        class SlowResponse(Response):
            calls = 0

            def read(self, size=-1):
                raise AssertionError('download must use bounded read1')

            def read1(self, size=-1):
                self.calls += 1
                return b'x'

        response = SlowResponse(archive)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'archive'
            path.write_bytes(b'previous')
            with mock.patch.object(acquire.urllib.request, 'urlopen',
                                   return_value=response), \
                    mock.patch.object(acquire.time, 'monotonic',
                                      side_effect=[0, 0, 121, 121]):
                with self.assertRaises(TimeoutError):
                    acquire.fetch(path, pin['archive'])
            self.assertEqual(response.calls, 1)
            self.assertEqual(path.read_bytes(), b'previous')


class SourceTests(unittest.TestCase):
    def test_authenticated_sources_and_repair(self):
        archive, pin = fixture()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'sources'
            acquire.materialize(archive, root, pin)
            before = (root / 'source-manifest.json').read_bytes()
            sources = acquire.Sources(root, pin)
            pin['archive']['url'] = 'https://example.invalid/changed'
            self.assertEqual(sources.pin['archive']['url'], 'https://example.invalid/cldr.tar.gz')
            self.assertEqual(sources.paths('common/main/'), ('common/main/en.xml',))
            self.assertEqual(sources.read('common/main/en.xml'), b'<ldml><identity/></ldml>')
            self.assertEqual(sources.consumed, {'common/main/en.xml'})
            with self.assertRaises(ValueError):
                sources.read('unlisted.xml')
            (root / 'common/main/en.xml').write_bytes(b'changed')
            with self.assertRaises(ValueError):
                sources.read('common/main/en.xml')
            acquire.materialize(archive, root, pin)
            self.assertEqual((root / 'source-manifest.json').read_bytes(), before)
            self.assertEqual(sources.read('common/main/en.xml'), b'<ldml><identity/></ldml>')
            (root / 'source-manifest.json').write_bytes(b'{}')
            with self.assertRaises(ValueError):
                acquire.Sources(root, pin)

    def test_invalid_inventory_does_not_create_output(self):
        archive, pin = fixture()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'sources'
            pin['manifest_sha256'] = '0' * 64
            with self.assertRaises(ValueError):
                acquire.materialize(archive, root, pin)
            self.assertFalse(root.exists())

    def test_archive_paths_links_and_duplicates(self):
        invalid = ('../escape', '/escape', 'cldr-fixture/../escape',
                   'cldr-fixture/common\\escape', 'cldr-fixture/C:escape',
                   'cldr-fixture/common/main/EN.xml')
        for name in invalid:
            archive, pin = fixture((tarfile.TarInfo(name),))
            with self.assertRaises(ValueError):
                acquire.archive_files(archive, pin)
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
            member = tarfile.TarInfo('cldr-fixture/common/main/link.xml')
            member.type = kind
            member.linkname = '/escape'
            archive, pin = fixture((member,))
            with self.assertRaises(ValueError):
                acquire.archive_files(archive, pin)

    def test_archive_resource_bounds(self):
        archive, pin = fixture()
        for bound, limit in (('MAX_FILE', 1), ('MAX_EXPANSION', 1), ('MAX_MEMBERS', 1)):
            with mock.patch.object(acquire, bound, limit):
                with self.assertRaises(ValueError):
                    acquire.archive_files(archive, pin)

    @unittest.skipUnless(hasattr(Path, 'symlink_to'), 'symlinks unavailable')
    def test_source_symlink_does_not_modify_target(self):
        archive, pin = fixture()
        with tempfile.TemporaryDirectory() as directory:
            outside = Path(directory) / 'outside'
            outside.mkdir()
            root = Path(directory) / 'sources'
            root.mkdir()
            try:
                (root / 'common').symlink_to(outside, target_is_directory=True)
            except (OSError, NotImplementedError):
                self.skipTest('symlink creation unavailable')
            with self.assertRaises(ValueError):
                acquire.materialize(archive, root, pin)
            self.assertEqual(list(outside.iterdir()), [])


if __name__ == '__main__':
    unittest.main()
