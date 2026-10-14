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
