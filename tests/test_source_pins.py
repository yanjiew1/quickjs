#!/usr/bin/env python3
"""Offline candidate-pin contracts; no production source or network mutation."""
import copy
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from types import SimpleNamespace
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/data'))
import source_pins
import update_pin


def cldr_pin():
    return json.loads((ROOT / 'tools/intl/pin.json').read_text())


def common_archive(extra=()):
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w') as archive:
        for name, content in [('LICENSE', b'license'), ('common/dtd/ldml.dtd', b'dtd'),
                              ('common/main/root.xml', b'<ldml/>')] + list(extra):
            archive.writestr(name, content)
    return data.getvalue()


class PinTests(unittest.TestCase):
    def test_cldr_inventory_replays_strict_acquisition(self):
        template = cldr_pin()
        archive = common_archive()
        pin, manifest, report = source_pins.refresh(template, archive=archive)
        self.assertEqual(template, cldr_pin())
        self.assertIsNone(pin['reviewed'])
        self.assertFalse(report['semantic_review_performed'])
        self.assertEqual(pin['cldr']['manifest_sha256'], source_pins.digest(manifest))
        acquire = source_pins.load_tool('pin_test_acquire', 'tools/intl/acquire.py')
        _, replay = acquire.archive_files(archive, pin['cldr'])
        self.assertEqual(replay, manifest)
        bad = copy.deepcopy(pin['cldr'])
        bad['manifest_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'inventory differs'):
            acquire.archive_files(archive, bad)

    def test_cldr_unsafe_members_rejected(self):
        for name in ('../escape', 'common/../escape', 'common/C:drive'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                source_pins.refresh(cldr_pin(), archive=common_archive([(name, b'x')]))

    def test_pointer_types_and_filename_keys(self):
        value = {'release': '49', 'enabled': False, 'files': [{'sha256': 'old'}],
                 'sources': {'file.xml': {'revision': 'old'}}}
        source_pins.set_pointer(value, '/release=50')
        source_pins.set_pointer(value, '/enabled=true')
        source_pins.set_pointer(value, '/files/0/sha256=new')
        source_pins.set_pointer(value, '/sources/file.xml/revision=next')
        self.assertEqual(value['release'], '50')
        self.assertIs(value['enabled'], True)
        self.assertEqual(value['sources']['file.xml']['revision'], 'next')
        for text in ('/files/-1/sha256=x', '/files/01/sha256=x', '/missing=x', '/files/~2=x'):
            with self.subTest(text=text), self.assertRaises((ValueError, KeyError)):
                source_pins.set_pointer(value, text)

    def test_calendar_partial_update_and_review_date(self):
        template = {'schema': 1, 'review_date': '2020-01-01', 'sources': {
            'era.xml': {'sha256': 'old', 'revision': 'before', 'vendored': True},
            'other.txt': {'sha256': 'unchanged', 'vendored': False}}}
        with tempfile.TemporaryDirectory() as temp:
            raw = Path(temp) / 'era.xml'
            raw.write_bytes(b'future era')
            pin, _, _ = source_pins.refresh(template, ['/sources/era.xml/revision=after'],
                                            {'era.xml': raw})
            self.assertEqual(pin['sources']['era.xml']['sha256'], source_pins.digest(raw.read_bytes()))
            self.assertEqual(pin['sources']['other.txt'], template['sources']['other.txt'])
            self.assertIsNone(pin['review_date'])
            with self.assertRaisesRegex(ValueError, 'requires input bytes'):
                source_pins.refresh(template, ['/sources/era.xml/revision=after'])

    def test_timezone_archive_uses_the_generation_source_contract(self):
        timezone = source_pins.load_tool('pin_test_timezone', 'tools/timezone/acquire.py')
        template = {'format': 1, 'tz': {'release': '2099a', 'archive': {'sha256': 'old'}},
                    'cldr': {'release': '49', 'files': {}}}
        data = io.BytesIO()
        with tarfile.open(fileobj=data, mode='w:gz') as archive:
            for name in timezone.SOURCE_NAMES:
                raw = ('fixture ' + name).encode()
                info = tarfile.TarInfo('tz-2099a/' + name)
                info.size = len(raw)
                archive.addfile(info, io.BytesIO(raw))
        pin, _, _ = source_pins.refresh(template, archive=data.getvalue())
        self.assertNotIn('files', pin['tz'])
        self.assertEqual(pin['tz']['archive']['sha256'], source_pins.digest(data.getvalue()))
        with self.assertRaisesRegex(ValueError, 'missing required sources'):
            source_pins.refresh(template, ['/tz/release=2099b'], archive=data.getvalue())

    def test_dictionary_complete_set_preserves_git_identity(self):
        template = json.loads((ROOT / 'tools/intl/segment-dictionary-pin.json').read_text())
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for row in template['files']:
                (root / row['name']).write_bytes(('fixture ' + row['name']).encode())
            pin, _, _ = source_pins.refresh(template, ['/commit=' + 'a' * 40], files_root=root)
            for row in pin['files']:
                data = (root / row['name']).read_bytes()
                self.assertEqual(row['bytes'], len(data))
                self.assertEqual(row['sha256'], source_pins.digest(data))
                self.assertNotIn('git_blob', row)
                self.assertIn('/' + 'a' * 40 + '/', row['url'])
            with self.assertRaisesRegex(ValueError, 'complete source set'):
                source_pins.refresh(template, ['/commit=' + 'a' * 40])
            for assignment in ('/release=79.2', '/tag=release-79.2'):
                with self.subTest(assignment=assignment), self.assertRaisesRegex(
                        ValueError, 'complete source set'):
                    source_pins.refresh(template, [assignment])

    def test_cli_failure_preserves_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            raw, output = root / 'common.zip', root / 'candidate.json'
            raw.write_bytes(common_archive())
            output.write_bytes(b'preserved')
            command = [sys.executable, str(ROOT / 'tools/data/update_pin.py'),
                       '--pin', str(ROOT / 'tools/intl/pin.json'), '--archive', str(raw),
                       '--output', str(output), '--expected-sha256', '0' * 64]
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_bytes(), b'preserved')
            self.assertIn(b'independently supplied SHA-256', result.stderr)

    def test_cli_outputs_candidate_manifest_and_report(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            raw = root / 'common.zip'
            raw.write_bytes(common_archive())
            command = [sys.executable, str(ROOT / 'tools/data/update_pin.py'),
                       '--pin', str(ROOT / 'tools/intl/pin.json'), '--archive', str(raw),
                       '--output', str(root / 'pin.json'),
                       '--manifest-output', str(root / 'manifest.json'),
                       '--report', str(root / 'report.json')]
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr)
            pin = json.loads((root / 'pin.json').read_text())
            self.assertEqual(pin['cldr']['manifest_sha256'],
                             source_pins.digest((root / 'manifest.json').read_bytes()))
            report = json.loads((root / 'report.json').read_text())
            self.assertFalse(report['semantic_review_performed'])

    def test_release_shorthand_needs_no_manual_hash_or_url(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            raw = root / 'common.zip'
            raw.write_bytes(common_archive())
            command = [sys.executable, str(ROOT / 'tools/data/update_pin.py'),
                       '--pin', str(ROOT / 'tools/intl/pin.json'), '--archive', str(raw),
                       '--release', '50', '--commit', 'b' * 40,
                       '--output', str(root / 'pin.json')]
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr)
            pin = json.loads((root / 'pin.json').read_text())
            self.assertEqual(pin['cldr']['release'], '50')
            self.assertEqual(pin['cldr']['tag'], 'release-50')
            self.assertEqual(pin['cldr']['archive']['url'],
                             'https://unicode.org/Public/cldr/50/cldr-common-50.zip')
            self.assertIsNone(pin['cldr']['published'])
            self.assertEqual(pin['cldr']['archive']['sha256'], source_pins.digest(raw.read_bytes()))


    def test_tag_resolution_peels_annotated_tags(self):
        replies = [{'object': {'sha': 'a' * 40, 'type': 'tag'}},
                   {'object': {'sha': 'b' * 40, 'type': 'commit'}}]
        with mock.patch.object(update_pin, 'download', side_effect=[json.dumps(row).encode() for row in replies]) as fetch:
            self.assertEqual(update_pin.resolve_tag('unicode-org/cldr', 'release-49'), 'b' * 40)
        self.assertEqual(fetch.call_count, 2)
        self.assertIn('/git/tags/' + 'a' * 40, fetch.call_args_list[1][0][0])

    def test_verified_cldr_propagation_keeps_iana_separate(self):
        raw = common_archive([('common/bcp47/timezone.xml', b'aliases'),
                              ('common/supplemental/windowsZones.xml', b'windows')])
        selected, _, _ = source_pins.refresh(cldr_pin(), archive=raw)
        template = json.loads((ROOT / 'data/timezone/pin.json').read_text())
        acquire = source_pins.load_tool('pin_propagation_acquire', 'tools/intl/acquire.py')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            pin_path = root / 'cldr-pin.json'
            pin_path.write_text(json.dumps(selected))
            sources = root / 'sources'
            acquire.materialize(raw, sources, selected['cldr'])
            args = SimpleNamespace(cldr_pin=pin_path, cldr_sources=sources, set=[])
            assignments, files = update_pin.cldr_assignments(args, template)
            result, _, _ = source_pins.refresh(template, assignments, files)
            self.assertEqual(result['tz'], template['tz'])
            self.assertEqual(result['cldr']['files']['timezone.xml']['sha256'],
                             source_pins.digest(b'aliases'))
            for pointer in ('/cldr', '/cldr/archive', '/cldr/archive/url',
                            '/cldr/files/timezone.xml'):
                args.set = [pointer + '={}']
                with self.subTest(pointer=pointer), self.assertRaisesRegex(
                        ValueError, 'must not override provenance'):
                    update_pin.cldr_assignments(args, template)
            args.set = []
            (sources / 'common/bcp47/timezone.xml').write_bytes(b'modified')
            assignments, files = update_pin.cldr_assignments(args, template)
            result, _, _ = source_pins.refresh(template, assignments, files)
            self.assertEqual(result['cldr']['files']['timezone.xml']['sha256'],
                             source_pins.digest(b'modified'))


if __name__ == '__main__':
    unittest.main()
