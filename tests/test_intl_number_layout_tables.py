#!/usr/bin/env python3
"""Canonical shared layout maps, projection, sparse states and corrupt data."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import number_layout
import number_layout_tables as wire
import numbering
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph
from test_intl_number_layout import Sources


class LayoutTableTests(unittest.TestCase):
    def fixture(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <unitPattern count="one">''' + BLOCK + '''</unitPattern><unitPattern count="two"/>
          <currencySpacing><beforeCurrency><insertBetween/></beforeCurrency></currencySpacing>
          </currencyFormats></numbers></ldml>''')
        data = number_layout.collect(source, graph())
        systems = numbering.collect(source, graph())
        pool = tuple(sorted(wire.strings(data) | {''}, key=lambda value: value.encode('utf-8')))
        return data, systems, pool, {value: index for index, value in enumerate(pool)}

    def test_roundtrip_and_projection(self):
        data, systems, pool, ids = self.fixture()
        section = wire.encode(data, graph().tags, systems, ids)
        self.assertEqual(wire.decode(section, pool, graph().tags, systems),
                         {key: data[key] for key in ('systems', 'fields', 'rows', 'root_fields')})
        self.assertIsNone(wire.decode(None, pool, graph().tags, systems))
        tags = ('en', 'root')
        data = dict(data, rows=tuple(row for row in data['rows'] if row[0] in tags),
                    root_fields=tuple(row for row in data['root_fields'] if row[0] in tags))
        result = wire.decode(wire.encode(data, tags, systems, ids), pool, tags, systems)
        self.assertEqual(result['rows'], data['rows'])
        english = dict(zip(result['fields'], result['rows'][0][2]))
        self.assertEqual(english['currency-unit-one'], BLOCK)
        self.assertEqual(english['currency-unit-two'], '')
        self.assertIsNone(english['currency-unit-few'])
        self.assertEqual(english['beforeCurrency-insertBetween'], '')

    def test_invalid_source_coverage_and_states(self):
        data, systems, pool, ids = self.fixture()
        for changed in (dict(data, fields=()), dict(data, systems=()),
                        dict(data, rows=data['rows'][:-1]),
                        dict(data, rows=data['rows'] + data['rows'][:1])):
            with self.assertRaises(ValueError):
                wire.encode(changed, graph().tags, systems, ids)
        for field, value in ((0, None), (0, BLOCK), (0, ''), (0, INHERIT), (0, 1),
                             (4, INHERIT), (4, True)):
            rows = list(data['rows'])
            locale, system, row = rows[0]
            row = list(row)
            row[field] = value
            rows[0] = locale, system, tuple(row)
            with self.assertRaises(ValueError):
                wire.encode(dict(data, rows=tuple(rows)), graph().tags, systems, ids)
        with self.assertRaises(ValueError):
            wire.encode(data, graph().tags + ('en',), systems, ids)

    def test_invalid_dimensions_order_and_references(self):
        data, systems, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, systems, ids)
        version, system_count, vectors, rows = struct.unpack_from('<4I', raw)
        locale_start = wire.HEADER + system_count
        map_start = locale_start + count
        vector_start = map_start + rows * system_count
        for field, value in ((0, version + 1), (1, system_count + 1), (2, 0), (3, 0),
                             (wire.HEADER, len(systems['systems'])), (locale_start, rows),
                             (map_start, vectors), (vector_start, wire.ABSENT),
                             (vector_start, wire.BLOCKED), (vector_start, 0),
                             (vector_start, len(pool))):
            changed = bytearray(raw)
            struct.pack_into('<I', changed, field * 4, value)
            with self.assertRaises(ValueError, msg=str((field, value))):
                wire.decode((count, bytes(changed)), pool, graph().tags, systems)
        for start, width, amount in ((map_start, system_count, rows),
                                      (vector_start, wire.VECTOR_WIDTH, vectors)):
            self.assertGreater(amount, 1)
            changed = bytearray(raw)
            changed[(start + width) * 4:(start + width * 2) * 4] = changed[start * 4:(start + width) * 4]
            with self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags, systems)
        for changed in ((count - 1, raw), (count, raw[:-1]), (count, raw + bytes(4)),
                        (count, bytes(wire.HEADER * 4 - 1))):
            with self.assertRaises(ValueError):
                wire.decode(changed, pool, graph().tags, systems)
        with self.assertRaises(ValueError):
            wire.decode((count, raw), pool, graph().tags, None)

    def test_unresolved_literal_text_is_not_a_field_state(self):
        data, systems, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, systems, ids)
        version, system_count, vectors, rows = struct.unpack_from('<4I', raw)
        start = wire.HEADER + system_count + count + rows * system_count
        last = max(start + row * wire.VECTOR_WIDTH + field
                   for row in range(vectors) for field in range(wire.WIDTH)
                   for index in (start + row * wire.VECTOR_WIDTH + field,)
                   if struct.unpack_from('<I', raw, index * 4)[0] < len(pool))
        changed = bytearray(raw)
        struct.pack_into('<I', changed, last * 4, len(pool))
        for marker in (BLOCK, INHERIT):
            with self.assertRaisesRegex(ValueError, 'invalid number layout text reference'):
                wire.decode((count, bytes(changed)), pool + (marker,), graph().tags, systems)

    def test_provenance_masks_reject_reserved_and_absent_fields(self):
        data, systems, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, systems, ids)
        _, system_count, vectors, rows = struct.unpack_from('<4I', raw)
        start = wire.HEADER + system_count + count + rows * system_count
        for mask in (1, 1 << 18, 1 << 4):
            changed = bytearray(raw)
            struct.pack_into('<I', changed, (start + wire.WIDTH) * 4, mask)
            with self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags, systems)
        for roots in ((('unknown', 'latn', 1 << 11),), (('en', 'unknown', 1 << 11),),
                      (('en', 'latn', True),), (('en', 'latn', 1),),
                      (('en', 'latn', 1 << 4),), (('en', 'latn', 0),)):
            with self.assertRaises(ValueError):
                wire.encode(dict(data, root_fields=roots), graph().tags, systems, ids)


if __name__ == '__main__':
    unittest.main()
