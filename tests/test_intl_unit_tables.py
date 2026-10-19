#!/usr/bin/env python3
"""Unit form selectors, shared maps, profile projection and malformed data."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import unit_patterns
import unit_tables as wire
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph
from test_intl_unit_patterns import Sources


class UnitTableTests(unittest.TestCase):
    def fixture(self):
        source = Sources()
        source.set('en', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="one" case="genitive">''' + BLOCK + '''</unitPattern>
          <unitPattern count="two"/><gender>common</gender></unit></unitLength>''')
        data = unit_patterns.collect(source, graph())
        pool = tuple(sorted(wire.strings(data), key=lambda text: text.encode('utf-8')))
        return data, pool, {text: index for index, text in enumerate(pool)}

    def semantics(self, data):
        return {row[:-1]: data['maps'][row[-1]] for row in data['rows']}

    def test_roundtrip_states_and_projection(self):
        data, pool, ids = self.fixture()
        result = wire.decode(wire.encode(data, graph().tags, ids), pool, graph().tags)
        self.assertEqual(result, {key: data[key] for key in ('widths', 'groups', 'fields', 'counts', 'maps', 'rows')})
        self.assertIsNone(wire.decode(None, pool, graph().tags))
        tags = ('en', 'root')
        selected = wire.select(data, tags)
        result = wire.decode(wire.encode(selected, tags, ids), pool, tags)
        self.assertEqual(self.semantics(result),
                         {key: value for key, value in self.semantics(data).items() if key[0] in tags})
        values = self.semantics(result)[('en', 'long', 'unit', 'length-meter')]
        self.assertIn(('unitPattern', 'one', 'genitive', BLOCK), values)
        self.assertIn(('unitPattern', 'two', '', ''), values)
        self.assertNotIn(('unitPattern', 'few', '', ''), values)

    def test_invalid_source_inventory_selectors_and_family(self):
        data, pool, ids = self.fixture()
        for changed in (dict(data, widths=()), dict(data, fields=()), dict(data, counts=()),
                        dict(data, groups=data['groups'][1:]), dict(data, groups=data['groups'][::-1]),
                        dict(data, rows=data['rows'][:-1]),
                        dict(data, rows=data['rows'] + data['rows'][:1])):
            with self.assertRaises(ValueError):
                wire.encode(changed, graph().tags, ids)
        for entry in (('unknown', '', '', 'x'), ('unitPattern', '', '', 'x'),
                      ('unitPattern', 'other', 'genitive dative', 'x'),
                      ('unitPattern', 'other', '', INHERIT), ('gender', 'one', '', 'common'),
                      ('displayName', '', 'genitive', 'meters'), ('gender', '', '', 'bad gender'),
                      ('unitPattern', 'other', None, 'x'), ('displayName', '', '', None)):
            maps = list(data['maps'])
            index = data['rows'][0][-1]
            maps[index] = (entry,)
            with self.assertRaises(ValueError):
                wire.encode(dict(data, maps=tuple(maps)), graph().tags, ids)
        unit_map = next(index for locale, width, kind, name, index in data['rows']
                        if locale == 'en' and width == 'long' and kind == 'unit' and name == 'length-meter')
        rows = list(data['rows'])
        rows[0] = rows[0][:-1] + (unit_map,) # Compound group cannot use unit fields.
        with self.assertRaisesRegex(ValueError, 'wrong group kind'):
            wire.encode(dict(data, rows=tuple(rows)), graph().tags, ids)
        with self.assertRaises(ValueError):
            wire.select(dict(data, rows=(data['rows'][0][:-1] + (len(data['maps']),),)), ('de',))

    def test_empty_form_inventory(self):
        data, pool, ids = self.fixture()
        data = dict(data, maps=((),), rows=tuple(row[:-1] + (0,) for row in data['rows']))
        result = wire.decode(wire.encode(data, graph().tags, ids), pool, graph().tags)
        self.assertEqual(result['maps'], ((),))
        self.assertTrue(all(index == 0 for locale, width, kind, name, index in result['rows']))

    def test_malformed_dimensions_spans_order_and_selector_words(self):
        data, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, ids)
        version, groups, cases, rows, maps, entries = struct.unpack_from('<6I', raw)
        case_start = wire.HEADER + groups * 2
        locale_start = case_start + cases
        row_start = locale_start + count
        map_start = row_start + rows * groups * 3
        entry_start = map_start + maps * 2
        changes = [(i, 0) for i in range(6)] + [(i, 0xffffffff) for i in range(6)]
        changes += [(wire.HEADER, 2), (wire.HEADER + 1, len(pool)),
                    (case_start, ids['genitive']), (locale_start, rows),
                    (row_start, maps), (map_start, 1), (map_start + 1, entries + 1),
                    (entry_start, len(unit_patterns.FIELDS) << 24),
                    (entry_start, 9 << 16), (entry_start, cases),
                    (entry_start + 1, len(pool)), (entry_start + 1, 0xffffffff)]
        for index, value in changes:
            changed = bytearray(raw)
            struct.pack_into('<I', changed, index * 4, value)
            with self.assertRaises(ValueError, msg=str((index, value))):
                wire.decode((count, bytes(changed)), pool, graph().tags)
        changed = bytearray(raw)
        width = groups * 3
        changed[(row_start + width) * 4:(row_start + width * 2) * 4] = changed[row_start * 4:(row_start + width) * 4]
        with self.assertRaises(ValueError):
            wire.decode((count, bytes(changed)), pool, graph().tags)
        for section in ((count - 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(section, pool, graph().tags)

    def test_markers_cannot_be_ordinary_text_references(self):
        data, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, ids)
        version, groups, cases, rows, maps, entries = struct.unpack_from('<6I', raw)
        start = wire.HEADER + groups * 2 + cases + count + rows * groups * 3 + maps * 2
        last = max(index for index in range(start + 1, start + entries * 2, 2)
                   if struct.unpack_from('<I', raw, index * 4)[0] < len(pool))
        changed = bytearray(raw)
        struct.pack_into('<I', changed, last * 4, len(pool))
        for marker in (BLOCK, INHERIT):
            with self.assertRaisesRegex(ValueError, 'invalid unit text reference'):
                wire.decode((count, bytes(changed)), pool + (marker,), graph().tags)


if __name__ == '__main__':
    unittest.main()
