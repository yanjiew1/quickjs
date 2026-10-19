#!/usr/bin/env python3
"""Typed Intl pack sections and deterministic authored generation fixtures."""
from pathlib import Path
import json
import re
import struct
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import acquire
import bundle
import generate
import locale_metadata
import locale_tables
import preferences
import preference_tables
import tables
from test_intl_list_patterns import Sources, graph
from test_intl_parents import DTD as PARENT_DTD
from test_intl_locale_metadata import Sources as MetadataSources
from test_intl_preferences import Sources as PreferenceSources
from test_intl_numbering import Sources as NumberingSources
from test_intl_number_symbols import Sources as SymbolSources
from number_symbols import FIELDS as SYMBOL_FIELDS
from test_intl_plural_rules import Sources as PluralSources
from test_intl_number_patterns import Sources as PatternSources
import number_patterns
import number_pattern_syntax


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
        metadata = MetadataSources()
        for name, value in metadata.files.items():
            if name.startswith('common/bcp47/') or name in (
                    'common/dtd/ldmlBCP47.dtd', 'common/dtd/ldmlSupplemental.dtd',
                    'common/supplemental/supplementalMetadata.xml'):
                self.files[name] = value
        preference_source = PreferenceSources()
        self.files['common/dtd/ldmlSupplemental.dtd'] += preference_source.files['common/dtd/ldmlSupplemental.dtd']
        for name in ('common/supplemental/supplementalData.xml', 'common/properties/scriptMetadata.txt'):
            self.files[name] = preference_source.files[name]
        numbering_source = NumberingSources()
        for name in ('common/dtd/ldmlSupplemental.dtd', 'common/dtd/ldml.dtd'):
            # The list fixture already declares the common alias attributes.
            self.files[name] += numbering_source.files[name].split(b'<!ELEMENT alias')[0]
        name = 'common/supplemental/numberingSystems.xml'
        self.files[name] = numbering_source.files[name]
        for name in numbering_source.paths('common/main/'):
            numbers = ET.fromstring(numbering_source.files[name]).find('numbers')
            if numbers is not None:
                root = ET.fromstring(self.files[name])
                root.append(numbers)
                self.files[name] = ET.tostring(root, encoding='utf-8')
        self.files['common/dtd/ldml.dtd'] += b'''<!ELEMENT symbols ANY>
          <!ATTLIST symbols numberSystem CDATA #IMPLIED>'''
        for field in SYMBOL_FIELDS:
            self.files['common/dtd/ldml.dtd'] += ('<!ELEMENT ' + field + ' (#PCDATA)>').encode('ascii')
        symbol_root = ET.fromstring(SymbolSources().files['common/main/root.xml'])
        root = ET.fromstring(self.files['common/main/root.xml'])
        root.find('numbers').extend(symbol_root.findall('./numbers/symbols'))
        self.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        pattern_source = PatternSources()
        self.files['common/dtd/ldml.dtd'] += b'<!ELEMENT minimumGroupingDigits (#PCDATA)>'
        self.files['common/dtd/ldml.dtd'] += b'<!ELEMENT pattern (#PCDATA)>'
        self.files['common/dtd/ldml.dtd'] += b'<!ATTLIST pattern type NMTOKEN "standard">'
        for style in number_patterns.STYLES:
            for suffix, attributes in (('Formats', ('numberSystem',)),
                                       ('FormatLength', ('type',)), ('Format', ())):
                tag = style + suffix
                self.files['common/dtd/ldml.dtd'] += ('<!ELEMENT ' + tag + ' ANY>').encode('ascii')
                for attribute in attributes:
                    self.files['common/dtd/ldml.dtd'] += (
                        '<!ATTLIST ' + tag + ' ' + attribute + ' NMTOKEN #IMPLIED>').encode('ascii')
            self.files['common/dtd/ldml.dtd'] += (
                '<!ATTLIST ' + style + 'Format type NMTOKEN "standard">').encode('ascii')
        names = {style + 'Formats' for style in number_patterns.STYLES} | {'minimumGroupingDigits'}
        root = ET.fromstring(self.files['common/main/root.xml'])
        root.find('numbers').extend(node for node in ET.fromstring(
            pattern_source.files['common/main/root.xml']).find('numbers') if node.tag in names)
        self.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        plural_source = PluralSources()
        self.files['common/dtd/ldmlSupplemental.dtd'] += (
            b'<!ELEMENT plurals' +
            plural_source.files['common/dtd/ldmlSupplemental.dtd'].split(b'<!ELEMENT plurals', 1)[1])
        for name in plural_source.files:
            if name != 'common/dtd/ldmlSupplemental.dtd':
                self.files[name] = plural_source.files[name]
        name = 'common/supplemental/supplementalData.xml'
        supplemental = ET.fromstring(self.files[name])
        ET.SubElement(supplemental, 'parentLocales', component='plurals')
        self.files[name] = ET.tostring(supplemental, encoding='utf-8')
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
            self.assertIn(('latn', 'numeric', '0123456789'), view['numbering']['systems'])
            self.assertIn('root', {row[0] for row in view['numbering']['locales']})
            symbols = view['number_symbols']
            self.assertEqual(len(symbols['systems']), 78)
            self.assertEqual(len(symbols['rows']), len(view['locales']) * 78)
            fields = {(locale, system): dict(zip(symbols['fields'], values))
                      for locale, system, values in symbols['rows']}
            self.assertEqual(fields[('en', 'latn')]['decimal'], '.')
            self.assertEqual(fields[('en', 'latn')]['group'], ',')
            plurals = view['plurals']['rows']
            self.assertEqual(len(plurals), len(view['locales']) * 2)
            cardinal = next(rules for locale, kind, rules, ranges in plurals
                            if locale == 'en' and kind == 0)
            self.assertIn((1, 'i = 1 and v = 0'), cardinal)
            self.assertEqual(next(rules for locale, kind, rules, ranges in plurals
                                  if locale == 'root' and kind == 0), ((5, ''),))
            patterns = view['number_patterns']
            self.assertEqual(len(patterns['rows']), len(view['locales']) * 78)
            standard = next(values for locale, system, values in patterns['rows']
                            if locale == 'en' and system == 'latn')
            self.assertEqual(standard[0], number_pattern_syntax.compile_pattern('#,##0.###'))
            if profile == 'native':
                self.assertEqual(fields[('ar', 'arab')]['decimal'], '\u066b')
                self.assertEqual(fields[('fr', 'latn')]['group'], '\u202f')
                self.assertEqual(fields[('en', 'mathbold')], fields[('en', 'latn')])
            tags = set(view['locales'])
            self.assertTrue({'en', 'en-US', 'root'} <= tags)
            for tag in tags:
                parts = tag.split('-')
                if (len(parts) >= 3 and re.fullmatch('[A-Z][a-z]{3}', parts[1]) and
                        re.fullmatch(r'(?:[A-Z]{2}|[0-9]{3})', parts[2])):
                    self.assertIn(parts[0] + '-' + parts[2], tags)
            aliases = manifest['inputs']['formatter_fallbacks']
            self.assertEqual(bool(aliases), profile == 'native')
            for target, source in aliases:
                at = view['locales'].index(target)
                self.assertEqual(view['locales'][view['parents'][at]], source)
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
        self.assertEqual(full_view['number_symbols']['systems'], ('latn', 'mathbold'))
        self.assertEqual(minimal_view['number_symbols']['rows'],
                         tuple(row for row in full_view['number_symbols']['rows']
                               if row[0] in minimal_view['locales']))
        self.assertEqual(minimal_view['plurals']['rows'],
                         tuple(row for row in full_view['plurals']['rows']
                               if row[0] in minimal_view['locales']))
        for field in ('rows', 'grouping'):
            self.assertEqual(minimal_view['number_patterns'][field],
                             tuple(row for row in full_view['number_patterns'][field]
                                   if row[0] in minimal_view['locales']))
        self.assertLess(len(minimal), len(full))
        self.assertNotEqual(manifest['generation'], minimal_manifest['generation'])
        self.assertEqual(full_view['locale_metadata'], minimal_view['locale_metadata'])
        self.assertEqual(full_view['preferences'], minimal_view['preferences'])
        self.assertEqual(full_view['numbering'], minimal_view['numbering'])
        for row in minimal_view['lists']:
            self.assertEqual(row, (' {0}\u00a0& {1} ',) * 36)
        changed = VerifiedFixture()
        changed.pin['commit'] = '1' * 40
        binary, changed_manifest = generate.generate(changed, 'native')
        self.assertNotEqual(manifest['generation'], changed_manifest['generation'])
        original = acquire.json_bytes(manifest)
        source.pin['archive']['sha256'] = '2' * 64
        self.assertEqual(acquire.json_bytes(manifest), original)
        for name in ('supplementalData', 'supplementalMetadata', 'likelySubtags', 'numberingSystems'):
            self.assertIn('common/supplemental/' + name + '.xml', manifest['inputs']['sources'])

    def test_preference_wire_roundtrip_and_invalid_fields(self):
        value = preferences.collect(PreferenceSources())
        pool = tuple(sorted(preference_tables.strings(value) |
                            {'bad', 'h12 h12', 'gregorian gregorian', 'h99', 'abcd-Latn-US'}))
        ids = {text: index for index, text in enumerate(pool)}
        sections = preference_tables.encode(value, ids)
        self.assertEqual(preference_tables.decode(sections, pool), value)
        self.assertIsNone(preference_tables.decode({}, pool))
        for identity in preference_tables.SECTIONS:
            changed = dict(sections)
            del changed[identity]
            with self.assertRaises(ValueError):
                preference_tables.decode(changed, pool)
        changes = ((bundle.WEEKS, 0, ids['bad']), (bundle.WEEKS, 1, 0),
                   (bundle.WEEKS, 2, 8), (bundle.WEEKS, 3, 0), (bundle.WEEKS, 3, 128),
                   (bundle.CALENDAR_PREFERENCES, 0, ids['abcd-Latn-US']),
                   (bundle.CALENDAR_PREFERENCES, 1, ids['gregorian gregorian']),
                   (bundle.HOUR_PREFERENCES, 1, ids['h12 h12']),
                   (bundle.HOUR_PREFERENCES, 1, ids['h99']),
                   (bundle.HOUR_PREFERENCES, 2, 0),
                   (bundle.SCRIPT_DIRECTIONS, 0, ids['bad']),
                   (bundle.SCRIPT_DIRECTIONS, 1, 3))
        for identity, word, replacement in changes:
            changed = dict(sections)
            count, raw = sections[identity]
            values = list(tables.read_words(raw))
            values[word] = replacement
            changed[identity] = count, tables.words(values)
            with self.assertRaises(ValueError, msg=repr((identity, word, replacement))):
                preference_tables.decode(changed, pool)
        for identity, width in ((bundle.WEEKS, 4), (bundle.CALENDAR_PREFERENCES, 3),
                                 (bundle.HOUR_PREFERENCES, 3), (bundle.SCRIPT_DIRECTIONS, 2)):
            count, raw = sections[identity]
            for replacement in ((count, raw[:-1]), (count - 1, raw),
                                (count, raw[:width * 4] * count)):
                changed = dict(sections)
                changed[identity] = replacement
                with self.assertRaises(ValueError):
                    preference_tables.decode(changed, pool)

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

    def test_list_context_table_fields_and_canonical_sets(self):
        strings = ('', '{0} e {1}', '{0} y {1}', 'invalid')
        vector = [1, 1, 2] + [0] * 24
        valid = (2, tables.words([1, 0, 0] + vector))
        expected = ((1, strings[1], strings[2]),) + ((0, '', ''),) * 8
        self.assertEqual(tables.decode_contexts(valid, strings, 2), (expected,) * 2)
        invalid = ((1, valid[1]), (2, valid[1][:-1]),
                   (2, tables.words([1, 0, 1] + vector)),
                   (2, tables.words([1, 0, 0, 4, 1, 2] + [0] * 24)),
                   (2, tables.words([1, 0, 0, 0, 1, 2] + [0] * 24)),
                   (2, tables.words([1, 0, 0, 1, 0, 2] + [0] * 24)),
                   (2, tables.words([1, 0, 0, 1, 3, 2] + [0] * 24)),
                   (2, tables.words([1, 0, 0, 1, 4, 2] + [0] * 24)),
                   (2, tables.words([2, 0, 1] + vector * 2)),
                   (2, tables.words([2, 0, 0] + vector + [0] * 27)))
        for section in invalid:
            with self.assertRaises(ValueError):
                tables.decode_contexts(section, strings, 2)

    def test_locale_metadata_roundtrip_and_invalid_wire_rows(self):
        metadata = locale_metadata.collect(VerifiedFixture())
        pool = tuple(sorted(locale_tables.strings(metadata) | {'unknown', 'bad\talias', 'abcd-Latn-US'},
                            key=lambda s: s.encode('utf-8')))
        ids = {text: index for index, text in enumerate(pool)}
        sections = locale_tables.encode(metadata, ids)
        self.assertEqual(locale_tables.decode(sections, pool), metadata)
        self.assertIsNone(locale_tables.decode({}, pool))
        missing = dict(sections)
        del missing[bundle.KEYS]
        with self.assertRaises(ValueError):
            locale_tables.decode(missing, pool)
        changes = ((bundle.ALIASES, 0, 5), (bundle.ALIASES, 1, len(pool)),
                   (bundle.ALIASES, 3, 0), (bundle.LIKELY, 0, 0),
                   (bundle.LIKELY, 1, ids['abcd-Latn-US']),
                   (bundle.KEYS, 2, 4), (bundle.KEYS, 4, ids['unknown']),
                   (bundle.KEYS, 3, ids['bad\talias']),
                   (bundle.TYPES, 1, ids['unknown']), (bundle.TYPES, 5, 4),
                   (bundle.TYPES, 3, ids['bad\talias']),
                   (bundle.TYPES, 5, 0), (bundle.TYPES, 4, ids['unknown']))
        for identity, word, value in changes:
            changed = dict(sections)
            count, data = sections[identity]
            values = list(tables.read_words(data))
            values[word] = value
            changed[identity] = count, tables.words(values)
            with self.assertRaises(ValueError, msg=repr((identity, word, value))):
                locale_tables.decode(changed, pool)
        changed = dict(sections)
        count, data = changed[bundle.TYPES]
        changed[bundle.TYPES] = count, data[:-1]
        with self.assertRaises(ValueError):
            locale_tables.decode(changed, pool)
        for identity, width, target in ((bundle.ALIASES, 4, None),
                                         (bundle.KEYS, 6, 1), (bundle.TYPES, 8, 2)):
            changed = dict(sections)
            count, data = sections[identity]
            values = list(tables.read_words(data))
            if target is None:
                values[width:width * 2] = values[:width]
            else:
                values[4] = values[target]  # preferred name is its own identity
            changed[identity] = count, tables.words(values)
            with self.assertRaises(ValueError):
                locale_tables.decode(changed, pool)
        changed = dict(sections)
        count, data = sections[bundle.LIKELY]
        rows = [row for row in struct.iter_unpack('<II', data) if pool[row[0]] != 'und']
        changed[bundle.LIKELY] = len(rows), tables.words([word for row in rows for word in row])
        with self.assertRaises(ValueError):
            locale_tables.decode(changed, pool)


if __name__ == '__main__':
    unittest.main()
