#!/usr/bin/env python3
"""Offline fixtures for authenticated CLDR input acquisition and consumption."""
import hashlib
import importlib.util
import io
import json
import os
import stat
import struct
from pathlib import Path
import tarfile
import tempfile
import unittest
import zipfile
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


def zip_fixture(extra=(), compression=zipfile.ZIP_STORED):
    files = {'LICENSE': b'fixture license', 'common/dtd/ldml.dtd': b'fixture DTD',
             'common/main/en.xml': b'<ldml><identity/></ldml>'}
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=compression) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
        for member, data in extra:
            archive.writestr(member, data)
    data = output.getvalue()
    manifest = {'archive_sha256': digest(data), 'release': 'fixture',
                'files': {name: digest(contents) for name, contents in files.items()}}
    pin = {'commit': 'fixture', 'release': 'fixture',
           'archive': {'format': 'zip', 'url': 'https://example.invalid/cldr.zip',
                       'sha256': digest(data)},
           'manifest_sha256': digest(acquire.json_bytes(manifest))}
    return data, pin


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
    def test_manifest_identity_must_match_its_pin(self):
        archive, pin = zip_fixture()
        for field in ('release', 'archive_sha256'):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                root = Path(directory) / 'sources'
                acquire.materialize(archive, root, pin)
                path = root / 'source-manifest.json'
                manifest = json.loads(path.read_text())
                manifest[field] = 'different'
                encoded = acquire.json_bytes(manifest)
                path.write_bytes(encoded)
                changed = dict(pin, manifest_sha256=digest(encoded))
                with self.assertRaisesRegex(ValueError, 'release/archive identity'):
                    acquire.Sources(root, changed)

    def test_distribution_cache_owners_coexist(self):
        archive, common = zip_fixture()
        git_archive, git = fixture()
        common['distribution'], git['distribution'] = 'common', 'git'
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            release_root = acquire.cache_root(cache, common)
            git_root = acquire.cache_root(cache, git)
            self.assertNotEqual(release_root, git_root)
            acquire.materialize(archive, release_root, common)
            opened = acquire.Sources(release_root, common)
            acquire.materialize(git_archive, git_root, git)
            self.assertEqual(opened.read('common/main/en.xml'), b'<ldml><identity/></ldml>')
            self.assertEqual(acquire.Sources(git_root, git).read('LICENSE'), b'fixture license')

    def test_explicit_alternate_pin(self):
        _, pin = zip_fixture()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'alternate.json'
            path.write_bytes(acquire.json_bytes({'cldr': pin}))
            self.assertEqual(acquire.load_pin(path), pin)
            self.assertEqual(acquire.load_pin(str(path)), pin)

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


class ZipSourceTests(unittest.TestCase):
    def test_deflated_release_members(self):
        archive, pin = zip_fixture(compression=zipfile.ZIP_DEFLATED)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'sources'
            acquire.materialize(archive, root, pin)
            sources = acquire.Sources(root, pin)
            self.assertEqual(sources.read('LICENSE'), b'fixture license')
            self.assertEqual(sources.read('common/dtd/ldml.dtd'), b'fixture DTD')

    def test_authenticated_sources_repair_and_unselected_files(self):
        archive, pin = zip_fixture((('common/main/', b''), ('tool.c', b'not input data')))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'sources'
            acquire.materialize(archive, root, pin)
            sources = acquire.Sources(root, pin)
            self.assertEqual(sources.paths('common/main/'), ('common/main/en.xml',))
            self.assertFalse((root / 'tool.c').exists())
            (root / 'common/main/en.xml').write_bytes(b'changed')
            with self.assertRaises(ValueError):
                sources.read('common/main/en.xml')
            acquire.materialize(archive, root, pin)
            self.assertEqual(sources.read('common/main/en.xml'), b'<ldml><identity/></ldml>')

    def test_paths_links_types_and_casefold_duplicates(self):
        for name in ('../escape', '/escape', 'common/../escape', 'common\\escape',
                     'C:escape', 'common/main/EN.xml'):
            archive, pin = zip_fixture(((name, b''),))
            with self.subTest(name=name), self.assertRaises(ValueError):
                acquire.archive_files(archive, pin)
        for kind in (stat.S_IFLNK, stat.S_IFIFO):
            member = zipfile.ZipInfo('common/main/link.xml')
            member.create_system = 3
            member.external_attr = (kind | 0o777) << 16
            archive, pin = zip_fixture(((member, b'outside'),))
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError, 'regular file'):
                acquire.archive_files(archive, pin)

    def test_original_name_retains_nul_for_path_validation(self):
        archive, pin = zip_fixture()
        archive = archive.replace(b'common/main/en.xml', b'common/main/e\0.xml')
        pin['archive']['sha256'] = digest(archive)
        with self.assertRaisesRegex(ValueError, 'unsafe source path'):
            acquire.archive_files(archive, pin)

    def test_encrypted_and_unsupported_compression_members(self):
        for field, value in ((8, 1), (10, 99)):
            archive, pin = zip_fixture()
            raw = bytearray(archive)
            end = raw.rfind(b'PK\x05\x06')
            central = struct.unpack_from('<I', raw, end + 16)[0]
            struct.pack_into('<H', raw, central + field, value)
            pin['archive']['sha256'] = digest(raw)
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'unsupported ZIP source member'):
                acquire.archive_files(bytes(raw), pin)

    def test_limits_and_directory_counts_precede_zip_objects(self):
        archive, pin = zip_fixture()
        with mock.patch.object(acquire, 'MAX_MEMBERS', 1), \
                mock.patch.object(acquire.zipfile, 'ZipFile') as parser:
            with self.assertRaisesRegex(ValueError, 'too many'):
                acquire.archive_files(archive, pin)
            parser.assert_not_called()
        raw = bytearray(archive)
        end = raw.rfind(b'PK\x05\x06')
        # A forged EOCD count must not hide the actual central-directory size.
        struct.pack_into('<2H', raw, end + 8, 1, 1)
        pin['archive']['sha256'] = digest(raw)
        with mock.patch.object(acquire.zipfile, 'ZipFile') as parser:
            with self.assertRaisesRegex(ValueError, 'member count mismatch'):
                acquire.archive_files(bytes(raw), pin)
            with mock.patch.object(acquire, 'MAX_MEMBERS', 2):
                with self.assertRaisesRegex(ValueError, 'too many'):
                    acquire.archive_files(bytes(raw), pin)
            parser.assert_not_called()

    def test_file_and_expansion_limits(self):
        archive, pin = zip_fixture()
        for bound in ('MAX_FILE', 'MAX_EXPANSION'):
            with mock.patch.object(acquire, bound, 1), self.assertRaises(ValueError):
                acquire.archive_files(archive, pin)

    def test_crc_failure_precedes_cache_modification(self):
        archive, pin = zip_fixture()
        # Keep the central/local CRC unchanged while changing a stored payload.
        archive = archive.replace(b'fixture DTD', b'Fixture DTD', 1)
        pin['archive']['sha256'] = digest(archive)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'sources'
            root.mkdir()
            preserved = root / 'preserved'
            preserved.write_bytes(b'previous')
            with self.assertRaisesRegex(ValueError, 'CRC'):
                acquire.materialize(archive, root, pin)
            # MSYS2 directory iteration can spell separators differently from
            # a joined Path. The contract is the unchanged filename inventory.
            self.assertEqual(os.listdir(root), ['preserved'])
            self.assertEqual(preserved.read_bytes(), b'previous')

    def test_truncation_prefix_and_unknown_format_rejection(self):
        archive, pin = zip_fixture()
        for changed in (archive[:-1], b'prefix' + archive):
            pin['archive']['sha256'] = digest(changed)
            with self.assertRaises(ValueError):
                acquire.archive_files(changed, pin)
        pin['archive']['sha256'] = digest(archive)
        pin['archive']['format'] = 'unknown'
        with self.assertRaisesRegex(ValueError, 'unsupported source archive format'):
            acquire.archive_files(archive, pin)


if __name__ == '__main__':
    unittest.main()
