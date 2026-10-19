#!/usr/bin/env python3
"""Typed Intl pack sections and deterministic authored generation fixtures."""
from pathlib import Path
import json
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import acquire
import bundle
import generate
import tables
from test_intl_list_patterns import Sources, graph
from test_intl_parents import DTD as PARENT_DTD


class VerifiedFixture(Sources):
    def __init__(self):
        super().__init__()
        self.files.update({
            'LICENSE': b'Authored test fixture; no external data.',
            'common/dtd/ldmlSupplemental.dtd': PARENT_DTD,
            'common/supplemental/supplementalData.xml': b'<supplementalData/>',
            'common/supplemental/supplementalMetadata.xml': b'<supplementalData/>',
            'common/supplemental/likelySubtags.xml': b'''<supplementalData><likelySubtags>
              <likelySubtag from="und" to="en_Latn_US"/>
            </likelySubtags></supplementalData>''',
        })
        self.consumed = set()
        self.pin = {'release': 'authored-fixture', 'commit': '0' * 40,
                    'archive': {'sha256': '0' * 64}}
        self.manifest = {'files': {name: acquire.digest(value) for name, value in self.files.items()}}

    def read(self, name):
        self.consumed.add(name)
        return super().read(name)


class TableTests(unittest.TestCase):
    def test_prepared_profiles_and_manifest_identity(self):
        root = Path(__file__).resolve().parents[1] / 'data' / 'intl'
        for profile in ('minimal', 'native'):
            binary = (root / (profile + '.bin')).read_bytes()
            manifest = json.loads((root / (profile + '-manifest.json')).read_text())
            view = tables.validate(binary)
            self.assertEqual(manifest['sha256'], acquire.digest(binary))
            self.assertEqual(manifest['bytes'], len(binary))
            self.assertEqual(manifest['generation'], acquire.digest(acquire.json_bytes(manifest['inputs'])))
            self.assertEqual(view['envelope']['generation'].hex(), manifest['generation'])
            self.assertEqual(view['envelope']['profile'], bundle.PROFILES[profile])
            self.assertEqual(manifest['inputs']['cldr'], acquire.load_pin())
            self.assertEqual(manifest['locales'], len(view['locales']))
            self.assertEqual(manifest['strings'], len(view['strings']))
            self.assertEqual(manifest['complete_services'], [])
            for identity, (count, data) in view['envelope']['sections'].items():
                self.assertEqual(manifest['inputs']['sections'][str(identity)],
                                 {'count': count, 'sha256': acquire.digest(data)})

    def test_string_pool_roundtrip_and_invalid_offsets_text(self):
        strings = ('', 'a', 'é', '日本')
        section = tables.encode_strings(strings)
        self.assertEqual(tables.decode_strings(section), strings)
        invalid = ((1, bytes(4)), (1, tables.words([1, 1])),
                   (1, tables.words([0, 1]) + b'a'),
                   (2, tables.words([0, 0, 1]) + b'\xff'),
                   (2, tables.words([0, 0, 1]) + b'\0'),
                   (3, tables.words([0, 0, 0, 0])),
                   (2, tables.words([0, 1, 0]) + b'a'))
        for value in invalid:
            with self.assertRaises(ValueError):
                tables.decode_strings(value)
        for strings in ((), ('a',), ('', 'a', 'a'), ('', 'b', 'a'), ('', '\0')):
            with self.assertRaises(ValueError):
                tables.encode_strings(strings)

    def test_locales_validate_references_and_cycles(self):
        strings = ('', 'de', 'en', 'en-GB', 'root')
        ids = {text: index for index, text in enumerate(strings)}
        tags = graph().tags
        section = tables.encode_locales(tags, graph(), ids)
        self.assertEqual(tables.decode_locales(section, strings)[0], tags)
        rows = list(struct.iter_unpack('<II', section[1]))
        invalid = ((0, (1, 0)), (0, (100, 3)), (0, (1, 100)),
                   (3, (4, 0)), (1, (1, 3)), (2, (3, tables.ROOT)))
        for index, row in invalid:
            changed = rows[:]
            changed[index] = row
            with self.assertRaises(ValueError):
                tables.decode_locales((4, b''.join(tables.LOCALE.pack(*r) for r in changed)), strings)

    def test_full_and_minimal_deterministic_generation(self):
        source = VerifiedFixture()
        source.consumed.add('previous-unrelated-read')
        full, manifest = generate.generate(source, 'native')
        second, duplicate = generate.generate(source, 'native')
        self.assertEqual((full, manifest), (second, duplicate))
        self.assertEqual((full, manifest), generate.generate(VerifiedFixture(), 'native'))
        self.assertEqual(manifest['sha256'], acquire.digest(full))
        self.assertEqual(manifest['complete_services'], [])
        full_view = tables.validate(full)
        self.assertEqual(full_view['locales'], graph().tags)
        minimal, minimal_manifest = generate.generate(VerifiedFixture(), 'minimal')
        minimal_view = tables.validate(minimal)
        self.assertEqual(minimal_view['locales'], ('en', 'root'))
        self.assertLess(len(minimal), len(full))
        self.assertNotEqual(manifest['generation'], minimal_manifest['generation'])
        for row in minimal_view['lists']:
            self.assertEqual(row, (' {0}\u00a0& {1} ',) * 36)
        changed = VerifiedFixture()
        changed.pin['commit'] = '1' * 40
        binary, changed_manifest = generate.generate(changed, 'native')
        self.assertNotEqual(manifest['generation'], changed_manifest['generation'])
        original = acquire.json_bytes(manifest)
        source.pin['archive']['sha256'] = '2' * 64
        self.assertEqual(acquire.json_bytes(manifest), original)
        for name in ('supplementalData', 'supplementalMetadata', 'likelySubtags'):
            self.assertIn('common/supplemental/' + name + '.xml', manifest['inputs']['sources'])

    def test_list_table_extents_ids_patterns_and_canonical_sets(self):
        strings = ('', '{0} & {1}', '{0} + {1}', '{0} invalid {2}')
        valid = (2, tables.words([1, 0, 0] + [1] * 36))
        self.assertEqual(tables.decode_lists(valid, strings, 2), ((strings[1],) * 36,) * 2)
        invalid = ((1, valid[1]), (2, valid[1][:-1]),
                   (2, tables.words([1, 0, 1] + [1] * 36)),
                   (2, tables.words([1, 0, 0] + [100] * 36)),
                   (2, tables.words([1, 0, 0] + [3] * 36)),
                   (2, tables.words([2, 0, 1] + [1] * 72)),
                   (2, tables.words([2, 0, 0] + [1] * 36 + [2] * 36)))
        for section in invalid:
            with self.assertRaises(ValueError):
                tables.decode_lists(section, strings, 2)


if __name__ == '__main__':
    unittest.main()
