#!/usr/bin/env python3
"""Check pinned collector loading and bounded binary layout without CLDR IO."""
import importlib.util
from pathlib import Path
import struct
import sys
import copy
import hashlib
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "tools/intl_gen/combined_bundle.py"
# Match normal script invocation so authenticated sibling imports resolve.
sys.path.insert(0, str(PATH.parent))
SPEC = importlib.util.spec_from_file_location("qjs_intl_combined_generator", PATH)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)

class GeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.modules, cls.pins = GENERATOR.load_modules()

    def test_verified_collectors(self):
        self.assertEqual(set(self.modules), set(GENERATOR.MODULE_ORDER))
        self.assertEqual(set(self.pins), {name + ".py" for name in self.modules})

    def test_wire_layout_and_padding(self):
        blob, inventory = GENERATOR.encode_blob(
            {1: b"abc", 2: bytes(range(32))}, {1: 1, 2: 32}, {1, 2},
            self.modules["metadata"], "17.0.0")
        self.assertEqual(blob[:8], b"QJSINTL\0")
        self.assertEqual(struct.unpack_from("<HH", blob, 8), (1, 3))
        self.assertEqual(struct.unpack_from("<I", blob, 16)[0], len(blob))
        self.assertEqual(inventory[0]["offset"], 112)
        self.assertEqual(inventory[1]["offset"], 116)
        self.assertEqual(blob[112:116], b"abc\0")
        self.assertEqual(blob[116:], bytes(range(32)))

    def test_reject_missing_and_misaligned_records(self):
        for sections, widths, required in [
            ({1: b"a"}, {1: 1}, {1, 2}),
            ({1: b"a"}, {1: 2}, {1}),
            ({1: b"a", 3: b"b"}, {1: 1}, {1})
        ]:
            with self.assertRaises(ValueError):
                GENERATOR.encode_blob(sections, widths, required,
                                      self.modules["metadata"], "17.0.0")

    def test_calendar_declaration(self):
        inventory = self.modules['installed_inventory']
        xml = b'''<ldmlBCP47><keyword><key name="ca">
          <type name="gregory"/><type name="ethioaa" alias="ethiopic-amete-alem"/>
          <type name="islamicc" preferred="islamic-civil"/>
        </key></keyword></ldmlBCP47>'''
        self.assertEqual(inventory.calendars_from_release(xml, ('ethioaa', 'gregory')),
                         ('ethioaa', 'gregory'))
        for values in [('gregory', 'gregory'), ('gregory', 'ethioaa'),
                       ('ethioaa',), ('gregory', 'unknown')]:
            with self.assertRaises(ValueError):
                inventory.calendars_from_release(xml, values)
        with self.assertRaises(ValueError):
            inventory.calendars_from_release(xml.replace(b'islamic-civil', b'gregory'), ('gregory',))

    def test_timezone_regions(self):
        inventory = self.modules['installed_inventory']
        helpers = inventory.load_helpers(ROOT / 'tools/timezone', {})
        table = b'US\t+0000+00000\tAmerica/New_York\n'
        xml = b'''<ldmlBCP47><keyword><key name="tz">
          <type name="usnyc" alias="America/New_York US/Eastern"/>
          <type name="utc" alias="UTC Etc/UTC"/>
        </key></keyword></ldmlBCP47>'''
        records = [dict(identifier='America/New_York', primary='America/New_York'),
                   dict(identifier='US/Eastern', primary='America/New_York'),
                   dict(identifier='UTC', primary='UTC')]
        self.assertEqual(inventory.regions_from_primaries(helpers['primary'], table, xml, records),
                         {'US': ('America/New_York',)})
        for malformed in [records[1:], records + [records[0]],
                          [dict(identifier='America/New_York', primary='US/Eastern')]]:
            with self.assertRaises(ValueError):
                inventory.regions_from_primaries(helpers['primary'], table, xml, malformed)

    def test_inventory_shared_list_prefix(self):
        inventory, metadata = self.modules['installed_inventory'], self.modules['metadata']
        data = dict(calendars=('gregory',),
                    regions={'GB': ('Europe/London',), 'US': ('America/New_York', 'America/Chicago')})
        pool = metadata.UTF8StringPool(inventory.strings(data) + ['old'])
        prefix = pool.ref('old')
        sections = {18: prefix}
        inventory.append(data, pool, sections)
        self.assertEqual(sections[18][:8], prefix)
        self.assertEqual(sections[25], pool.ref('gregory'))
        self.assertEqual(struct.unpack_from('<II', sections[20], 8), (1, 1))
        self.assertEqual(struct.unpack_from('<II', sections[20], 24), (2, 2))
        self.assertEqual(sections[18][8:], b''.join(pool.ref(name) for name in
                         ['Europe/London', 'America/New_York', 'America/Chicago']))
        for malformed in [{18: b'x'}, {18: prefix, 20: b''}, {18: prefix, 25: b''}]:
            with self.assertRaises(ValueError):
                inventory.append(data, pool, malformed)

    def test_regional_source_parents_do_not_mutate_input(self):
        subset = self.modules['locale_subset']
        source = dict(parents={'root': None, 'zh': 'root', 'zh-Hans': 'zh',
                              'zh-Hant': 'root', 'zh-Hans-CN': 'zh-Hans', 'zh-Hant-TW': 'zh-Hant'},
                      locales={}, component_parents={('collations', 'zh'): 'root'},
                      default_content=(), evidence={})
        before = copy.deepcopy(source)
        data, policy = subset.select(source, 'zh-CN', ['zh-CN', 'zh-TW'],
                                     [('zh', 'zh-Hans-CN'), ('zh-TW', 'zh-Hant-TW')])
        self.assertEqual(source, before)
        self.assertEqual(data['parents']['zh-CN'], 'zh-Hans-CN')
        self.assertEqual(data['parents']['zh-TW'], 'zh-Hant-TW')
        self.assertEqual(data['component_parents'][('collations', 'zh-TW')], 'zh-Hant-TW')
        self.assertEqual(policy['default_locale'], 'zh-CN')
        del source['parents']['zh-Hant-TW']
        with self.assertRaises(ValueError):
            subset.select(source, 'zh-TW', ['zh-TW'], [('zh-TW', 'zh-Hant-TW')])

    def test_inventory_authentication_before_consumption(self):
        inventory = self.modules['installed_inventory']
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input'
            path.write_bytes(b'original')
            digest = hashlib.sha256(b'original').hexdigest()
            consumed = {}
            self.assertEqual(inventory.authenticated(path, digest, consumed, 'input'), b'original')
            path.write_bytes(b'changed')
            rejected = {}
            with self.assertRaises(ValueError):
                inventory.authenticated(path, digest, rejected, 'input')
            self.assertEqual(rejected, {})

    def test_embedding_preserves_every_byte_and_trailing_zero(self):
        blob = bytes(range(256)) + b'\x00\xff\x00'
        header, source = self.modules['metadata'].embedding(blob)
        self.assertEqual(header.count(b'extern const unsigned char '), 1)
        self.assertEqual(source.count(b'const unsigned char '), 1)
        constants = source.split(b'{', 1)[1].split(b'}', 1)[0].split(b',')
        decoded = bytes(int(value.strip(), 0) for value in constants if value.strip())
        self.assertEqual(decoded, blob)

    def test_currency_name_template_fallback(self):
        number = self.modules['number_patterns']
        root = b'''<ldml><numbers><defaultNumberingSystem>latn</defaultNumberingSystem>
          <currencyFormats numberSystem="latn"><unitPattern count="other">{0} {1}</unitPattern></currencyFormats>
          <currencyFormats numberSystem="arab"><currencyFormatLength><currencyFormat type="standard">
            <pattern>#,##0.00X</pattern></currencyFormat></currencyFormatLength></currencyFormats>
        </numbers></ldml>'''
        local = b'''<ldml><numbers>
          <currencyFormats numberSystem="latn"><unitPattern count="one">{1}: {0}</unitPattern></currencyFormats>
        </numbers></ldml>'''
        resolver = number.Resolver([('main/root.xml', root), ('main/xx.xml', local)],
                                   {'root': None, 'xx': 'root'})
        patterns = number.currency_name_patterns(resolver, 'xx', 'arab')
        self.assertEqual(patterns[1], '{currency}: {number}')
        self.assertEqual(patterns[-1], '{number} {currency}')
        self.assertEqual(resolver.get('xx', number.numbered('currencyFormats', 'arab') +
                         (number.segment('currencyFormatLength'),
                          number.segment('currencyFormat', type='standard'), number.segment('pattern'))),
                         '#,##0.00X')

    def test_currency_name_non_latin_default_and_requested_bank(self):
        number = self.modules['number_patterns']
        root = b'''<ldml><numbers><defaultNumberingSystem>beng</defaultNumberingSystem>
          <currencyFormats numberSystem="latn"><unitPattern count="other">LATN {0} {1}</unitPattern></currencyFormats>
          <currencyFormats numberSystem="beng"><unitPattern count="other">{1} BENG {0}</unitPattern></currencyFormats>
          <currencyFormats numberSystem="arabext"><unitPattern count="other">{0} EXPLICIT {1}</unitPattern></currencyFormats>
          <currencyFormats numberSystem="fullwide"><alias source="locale" path="../currencyFormats[@numberSystem='beng']"/></currencyFormats>
        </numbers></ldml>'''
        resolver = number.Resolver([('main/root.xml', root)], {'root': None})
        self.assertEqual(number.currency_name_patterns(resolver, 'root', 'arab')[-1],
                         '{currency} BENG {number}')
        self.assertEqual(number.currency_name_patterns(resolver, 'root', 'arabext')[-1],
                         '{number} EXPLICIT {currency}')
        self.assertEqual(number.currency_name_patterns(resolver, 'root', 'fullwide')[-1],
                         '{currency} BENG {number}')

    def test_currency_name_malformed_templates_do_not_fall_back(self):
        number = self.modules['number_patterns']
        for template in ['{0}', '{0} {1} {2}', '']:
            xml = ('''<ldml><numbers><defaultNumberingSystem>latn</defaultNumberingSystem>
              <currencyFormats numberSystem="latn"><unitPattern count="other">{0} {1}</unitPattern></currencyFormats>
              <currencyFormats numberSystem="arab"><unitPattern count="other">''' + template +
              '</unitPattern></currencyFormats></numbers></ldml>').encode()
            resolver = number.Resolver([('main/root.xml', xml)], {'root': None})
            with self.assertRaises(ValueError):
                number.currency_name_patterns(resolver, 'root', 'arab')

    def test_named_compound_encoding_and_duplicate_keys(self):
        number, metadata = self.modules['number_patterns'], self.modules['metadata']
        row = (2, 'kilometer-per-hour', 1, ('{number} km/h',) * 6)
        data = {'tables': {123: [row]}}
        pool = metadata.UTF8StringPool(number.strings(data))
        record = number.encode(data, pool)[123]
        self.assertEqual(len(record), 64)
        self.assertEqual(struct.unpack_from('<I', record)[0], 2)
        self.assertEqual(record[4:12], pool.ref('kilometer-per-hour'))
        self.assertEqual(record[12:16], b'\x01\0\0\0')
        self.assertEqual(record[16:], pool.ref('{number} km/h') * 6)
        with self.assertRaises(ValueError):
            number.encode({'tables': {123: [row, row]}}, pool)
        with self.assertRaises(ValueError):
            number.encode({'tables': {123: [row, (2, row[1], 0, row[3])]}}, pool)

    def named_resolver(self, root, child=None):
        number = self.modules['number_patterns']
        inputs = [('main/root.xml', root.encode())]
        parents = {'root': None}
        if child is not None:
            inputs.append(('main/ja.xml', child.encode()))
            parents['ja'] = 'root'
        return number.Resolver(inputs, parents)

    def test_named_compound_inheritance_width_alias_and_plural_fallback(self):
        extension = self.modules['number_extensions']
        root = '''<ldml><units>
          <unitLength type="long"><unit type="speed-kilometer-per-hour">
            <unitPattern count="one">one speed</unitPattern>
            <unitPattern count="other">{0} root speed</unitPattern></unit></unitLength>
          <unitLength type="short"><alias source="locale" path="../unitLength[@type='long']"/></unitLength>
          <unitLength type="narrow"><unit type="speed-kilometer-per-hour">
            <unitPattern count="other">{0}km/h</unitPattern></unit></unitLength>
          </units></ldml>'''
        child = '''<ldml><units><unitLength type="long"><unit type="speed-kilometer-per-hour">
          <unitPattern count="other">時速 {0}</unitPattern></unit></unitLength></units></ldml>'''
        resolver = self.named_resolver(root, child)
        gaps = []
        rows = extension.collect_named_compounds(resolver, ['ja', 'root'], gaps)
        self.assertFalse(gaps)
        self.assertEqual(len(rows), 6)
        self.assertEqual(rows[0], (0, 'kilometer-per-hour', 0,
            ('時速 {number}', 'one speed', '時速 {number}', '時速 {number}', '時速 {number}', '時速 {number}')))
        self.assertEqual(rows[1][3], rows[0][3])
        self.assertEqual(rows[2][3], ('{number}km/h',) * 6)

    def test_named_compound_vocabulary_and_conflicts(self):
        extension = self.modules['number_extensions']
        xml = '''<ldml><units><unitLength type="long">
          <unit type="speed-kilometer-per-hour"/><unit type="volume-liter"/>
          <unit type="consumption-liter-per-100-kilometer"/>
          <unit type="speed-meter-per-second-per-hour"/>
          <unit type="speed-unknown-per-second"/></unitLength></units></ldml>'''
        self.assertEqual(extension.named_compounds(self.named_resolver(xml)),
                         {'kilometer-per-hour': 'speed-kilometer-per-hour'})
        xml = xml.replace('<unit type="volume-liter"/>', '<unit type="other-kilometer-per-hour"/>')
        with self.assertRaises(ValueError):
            extension.named_compounds(self.named_resolver(xml))

    def test_named_compound_malformed_data_is_a_gap(self):
        extension = self.modules['number_extensions']
        for pattern in ['{0} {0}', '{1} speed', '{0} {bad}', '\ud800']:
            # Invalid XML scalars are rejected by the source parser itself.
            if pattern == '\ud800':
                with self.assertRaises(ValueError):
                    self.modules['number_patterns'].placeholder(pattern, number_required=False)
                continue
            xml = ('<ldml><units><unitLength type="long"><unit type="speed-meter-per-second">'
                   '<unitPattern count="other">' + pattern +
                   '</unitPattern></unit></unitLength></units></ldml>')
            gaps = []
            self.assertEqual(extension.collect_named_compounds(self.named_resolver(xml), ['root'], gaps), [])
            self.assertEqual(len(gaps), 1)
            self.assertEqual(gaps[0]['key'], ['root', 'meter-per-second', 'long', 'named-compound'])

    def test_named_compound_missing_other_is_a_gap(self):
        extension = self.modules['number_extensions']
        xml = ('<ldml><units><unitLength type="long"><unit type="speed-meter-per-second">'
               '<unitPattern count="one">{0} speed</unitPattern>'
               '</unit></unitLength></units></ldml>')
        gaps = []
        self.assertEqual(extension.collect_named_compounds(self.named_resolver(xml), ['root'], gaps), [])
        self.assertEqual(len(gaps), 1)
        self.assertIn('without other', gaps[0]['reason'])

    def test_non_english_default_has_controlled_missing_service_error(self):
        metadata = dict(parents={'root': None, 'fr': 'root'},
                        locales={'fr': {'numbering': 'latn'}}, component_parents={},
                        numbering={'latn': (10, 0, ())})
        services = dict(list=dict(rows=[]), plural=dict(locales=[]), display=dict(rows=[]),
                        relative=dict(patterns=[]), number=dict(tables={80: []}),
                        duration=dict(rows=[]), date=dict(patterns=[], range_fallbacks=[]),
                        collation=dict(capabilities=[]))
        with self.assertRaises(ValueError):
            GENERATOR.require_default_rows(metadata, {'default_locale': 'fr'}, services, self.modules)

if __name__ == "__main__":
    unittest.main()
