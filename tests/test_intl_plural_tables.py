#!/usr/bin/env python3
"""Packed plural relation/range sharing and corruption regressions."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import plural_rules
import plural_tables as wire
from test_intl_plural_rules import Sources, graph


class PluralTableTests(unittest.TestCase):
    def fixture(self):
        data = plural_rules.collect(Sources(), graph())
        pool = tuple(sorted(wire.strings(data) | {'', 'x = 0', ' n = 1',
                                                'n = 1 @integer 1', 'n = 1'}))
        return data, pool, {value: index for index, value in enumerate(pool)}

    def test_roundtrip_shared_vectors_and_sparse_ranges(self):
        data, pool, ids = self.fixture()
        section = wire.encode(data, graph().tags, ids)
        self.assertEqual(wire.decode(section, pool, graph().tags), {'rows': data['rows']})
        rules, ranges = struct.unpack_from('<II', section[1])
        self.assertEqual((rules, ranges), (4, 2))
        self.assertIsNone(wire.decode(None, pool, graph().tags))
        tags = ('en', 'root')
        selected = dict(data, rows=tuple(row for row in data['rows'] if row[0] in tags))
        self.assertEqual(wire.decode(wire.encode(selected, tags, ids), pool, tags),
                         {'rows': selected['rows']})

    def test_invalid_input_inventory_and_records(self):
        data, pool, ids = self.fixture()
        first = data['rows'][0]
        for rows in (data['rows'][:-1], data['rows'] + data['rows'][:1],
                     ((first[0], first[1], ((6, ''),), first[3]),) + data['rows'][1:],
                     ((first[0], first[1], ((5, ''), (5, '')), first[3]),) + data['rows'][1:],
                     ((first[0], first[1], first[2], ((0, 0, 6),)),) + data['rows'][1:],
                     ((first[0], first[1], first[2], ((0, 0, 0), (0, 0, 0))),) + data['rows'][1:]):
            with self.assertRaises(ValueError):
                wire.encode(dict(data, rows=rows), graph().tags, ids)

    def test_dimensions_maps_order_conditions_and_range_results(self):
        data, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, ids)
        rules, ranges = struct.unpack_from('<II', raw)
        rule_start = 2 + count * 4
        range_start = (rule_start + rules * wire.WIDTH) * 4
        changes = ((0, 0), (1, 0), (0, count * 2 + 1), (1, count * 2 + 1),
                   (2, rules), (3, ranges), (rule_start + 5, wire.ABSENT),
                   (rule_start + 5, ids['n = 1']),
                   (rule_start, len(pool)), (rule_start, ids['']),
                   (rule_start, ids['x = 0']), (rule_start, ids[' n = 1']),
                   (rule_start, ids['n = 1 @integer 1']))
        for at, value in changes:
            changed = bytearray(raw)
            struct.pack_into('<I', changed, at * 4, value)
            with self.assertRaises(ValueError, msg=str((at, value))):
                wire.decode((count, bytes(changed)), pool, graph().tags)
        for start, width in ((rule_start * 4, wire.WIDTH * 4),
                             (range_start, wire.WIDTH * wire.WIDTH)):
            changed = bytearray(raw)
            changed[start + width:start + width * 2] = changed[start:start + width]
            with self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags)
        changed = bytearray(raw)
        changed[range_start] = wire.WIDTH
        with self.assertRaises(ValueError):
            wire.decode((count, bytes(changed)), pool, graph().tags)
        for section in ((0, raw), (count + 1, raw), (count, raw[:-1]),
                        (count, raw + bytes(4)), (count, bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(section, pool, graph().tags)


if __name__ == '__main__':
    unittest.main()
