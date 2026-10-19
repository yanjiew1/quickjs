#!/usr/bin/env python3
"""Sparse compact wire sharing, independent families and malformed records."""
from pathlib import Path
import struct
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import compact_patterns as source
import compact_pattern_syntax as syntax
import compact_tables as wire
import numbering
import number_pattern_syntax as tokens
from resolve import BLOCK
from test_intl_compact_patterns import Sources
from test_intl_list_patterns import graph


class CompactTableTests(unittest.TestCase):
    def fixture(self):
        inputs = Sources()
        root = ET.fromstring(inputs.files['common/main/root.xml'])
        group = root.find('./numbers/currencyFormats[@numberSystem="mathbold"]')
        group.remove(group.find('alias'))
        inputs.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        inputs.set('en', '''<ldml><numbers><decimalFormats numberSystem="latn">
          <decimalFormatLength type="short"><decimalFormat>
            <pattern type="1000" count="1">mille</pattern>
          </decimalFormat></decimalFormatLength></decimalFormats>
          <currencyFormats numberSystem="latn"><currencyFormatLength type="short"><currencyFormat>
            <pattern type="1000" count="other">0</pattern>
            <pattern type="1000" count="other" alt="alphaNextToNumber">¤ 0K</pattern>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        inputs.set('en_GB', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <currencyFormatLength type="short"><currencyFormat>
            <pattern type="1000" count="other" alt="alphaNextToNumber">''' + BLOCK + '''</pattern>
            <pattern type="1000" count="other" alt="noCurrency"/>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        data = source.collect(inputs, graph())
        definitions = numbering.collect(inputs, graph())
        prepared = wire.prepare(data, graph().tags, definitions)
        pool = tuple(sorted(wire.strings(prepared) | {''}, key=lambda text: text.encode('utf-8')))
        ids = {text: index for index, text in enumerate(pool)}
        return data, definitions, pool, ids, prepared

    def semantics(self, data, compiled=False):
        # Compile raw source independently of the prepared inventory under test.
        maps = data['maps'] if compiled else syntax.compile_inventory(data)
        return {(locale, system, kind): {(magnitude, count, variant): pattern
                                        for magnitude, count, variant, pattern in maps[index]}
                for locale, system, kind, index in data['rows']}

    def test_roundtrip_literals_families_fallbacks_and_empty_maps(self):
        data, definitions, pool, ids, prepared = self.fixture()
        section = wire.encode(prepared, ids)
        result = wire.decode(section, pool, graph().tags, definitions)
        self.assertEqual(self.semantics(result, True), self.semantics(data))
        rows = self.semantics(result, True)
        self.assertEqual(rows[('en', 'mathbold', 2)], {})
        self.assertEqual(rows[('en', 'latn', 2)][(3, 'other', 'standard')][:3],
                         (syntax.FALLBACK, 0, 1))
        self.assertEqual(rows[('en', 'latn', 2)][(3, 'other', 'alphaNextToNumber')][1], 3)
        self.assertEqual(rows[('en-GB', 'latn', 2)][(3, 'other', 'alphaNextToNumber')], BLOCK)
        self.assertEqual(rows[('en-GB', 'latn', 2)][(3, 'other', 'noCurrency')], '')
        self.assertEqual(rows[('en', 'latn', 0)][(3, '1', 'standard')][:3], (syntax.LITERAL, 3, 0))
        self.assertIsNone(wire.decode(None, pool, graph().tags, definitions))
        with self.assertRaises(ValueError):
            wire.decode(section, pool, graph().tags, None)

    def test_prepared_inventory_does_not_retain_mutable_source(self):
        data, definitions, pool, ids, prepared = self.fixture()
        expected = self.semantics(data)
        section = wire.encode(prepared, ids)
        # Replacing source collections must not change the prepared image.
        data.update(systems=(), rows=(), maps=())
        self.assertEqual(wire.strings(prepared), set(pool))
        self.assertEqual(wire.encode(prepared, ids), section)
        result = wire.decode(section, pool, graph().tags, definitions)
        self.assertEqual(self.semantics(result, True), expected)

    def test_projection_and_invalid_source_inventory(self):
        data, definitions, pool, ids, prepared = self.fixture()
        tags = ('en', 'root')
        selected = wire.select(data, tags)
        prepared = wire.prepare(selected, tags, definitions)
        result = wire.decode(wire.encode(prepared, ids), pool, tags, definitions)
        self.assertEqual(self.semantics(result, True),
                         {key: value for key, value in self.semantics(data).items() if key[0] in tags})
        for changed in (dict(data, systems=('latn',)), dict(data, kinds=()),
                        dict(data, rows=data['rows'][:-1]),
                        dict(data, rows=data['rows'] + data['rows'][:1]),
                        dict(data, maps=data['maps'] + ((),))):
            with self.assertRaises(ValueError):
                wire.prepare(changed, graph().tags, definitions)
        with self.assertRaises(ValueError):
            wire.select(dict(data, rows=(('en', 'latn', 0, len(data['maps'])),)), tags)

    def test_malformed_extents_indices_and_modes(self):
        data, definitions, pool, ids, prepared = self.fixture()
        count, raw = wire.encode(prepared, ids)
        version, systems, rows, maps, entries, patterns, affixes, token_count = struct.unpack_from('<8I', raw)
        locale_start = wire.HEADER + systems
        row_start = locale_start + count
        map_start = row_start + rows * systems * wire.KIND_COUNT
        entry_start = map_start + maps * 2
        pattern_start = entry_start + entries * wire.ENTRY_WIDTH
        affix_start = pattern_start + patterns * wire.PATTERN_WIDTH
        token_start = affix_start + affixes * 2
        changes = [(0, version + 1), (wire.HEADER, 2), (locale_start, rows), (row_start, maps),
                   (map_start, 1), (map_start + 1, entries + 1),
                   (entry_start + 1, len(source.COUNTS)), (entry_start + 2, len(source.VARIANTS)),
                   (entry_start + 3, 0xffffffff), (pattern_start, 99),
                   (pattern_start + 1, 0xffffffff), (pattern_start + 2, 0),
                   (pattern_start + 3, affixes), (affix_start, 1), (affix_start + 1, 0),
                   (token_start, len(tokens.TOKENS)), (token_start + 1, len(pool))]
        changes += [(index, 0) for index in range(1, wire.HEADER)]
        for index, value in changes:
            changed = bytearray(raw)
            struct.pack_into('<I', changed, index * 4, value)
            with self.assertRaises(ValueError, msg=str((index, value))):
                wire.decode((count, bytes(changed)), pool, graph().tags, definitions)
        for at, width in ((row_start, systems * wire.KIND_COUNT),
                          (pattern_start, wire.PATTERN_WIDTH)):
            changed = bytearray(raw)
            changed[(at + width) * 4:(at + 2 * width) * 4] = changed[at * 4:(at + width) * 4]
            with self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags, definitions)
        for section in ((0, raw), (count, raw[:-1]), (count, raw + bytes(4)),
                        (count, bytes(wire.HEADER * 4 - 4))):
            with self.assertRaises(ValueError):
                wire.decode(section, pool, graph().tags, definitions)

    def test_empty_maps_are_only_valid_for_currency(self):
        data, definitions, pool, ids, prepared = self.fixture()
        empty = data['maps'].index(())
        changed = dict(data, rows=tuple((locale, system, kind, empty if kind < 2 else index)
                                       for locale, system, kind, index in data['rows']))
        changed = wire.select(changed, graph().tags)
        with self.assertRaisesRegex(ValueError, 'empty decimal compact map'):
            wire.prepare(changed, graph().tags, definitions)

        count, raw = wire.encode(prepared, ids)
        systems, rows, maps = struct.unpack_from('<3I', raw, 4)
        row_start = wire.HEADER + systems + count
        changed = bytearray(raw)
        # Canonical map zero is empty. Replacing the first decimal field keeps
        # row order and all other references valid in this authored fixture.
        struct.pack_into('<I', changed, row_start * 4, 0)
        with self.assertRaisesRegex(ValueError, 'empty decimal compact map'):
            wire.decode((count, bytes(changed)), pool, graph().tags, definitions)


if __name__ == '__main__':
    unittest.main()
