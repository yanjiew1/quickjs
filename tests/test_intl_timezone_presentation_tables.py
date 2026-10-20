#!/usr/bin/env python3
"""Canonical global presentation tables and independent optional numeric state."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import tables
import timezone_presentation as source
import timezone_presentation_tables as wire
from test_intl_timezone_presentation import Sources
from test_intl_timezone_names import graph


def fixture():
    data = wire.runtime_records(source.collect(Sources()))
    pool = tuple(sorted(wire.strings(data) | set(graph().tags) | {'Bad ID', 'Unknown', 'Invalid', 'WEST'}))
    ids = {text: index for index, text in enumerate(pool)}
    return data, pool, ids, wire.encode(data, ids)


def unpack(section):
    return list(struct.unpack('<' + 'I' * (len(section[1]) // 4), section[1]))


def repack(words):
    return b''.join(struct.pack('<I', value) for value in words)


class TimezonePresentationTablesTests(unittest.TestCase):
    def test_projection_roundtrip_and_generic_pack(self):
        data, pool, ids, section = fixture()
        self.assertEqual(wire.decode(section, pool), data)
        self.assertTrue(all(len(row) == 2 for row in data['ids']))
        self.assertNotIn('versions', data)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(graph().tags, graph(), ids),
                              bundle.TIMEZONE_PRESENTATION: section}, bytes(32), 'native')
        self.assertEqual(tables.validate(binary)['timezone_presentation'], data)
        self.assertIsNone(wire.decode(None, pool))

    def test_signed_extremes_zero_bounds_and_independent_zero_offsets(self):
        data = {'zones': ('Area/Alpha',), 'ids': (('west', 'West'),),
                'periods': (('Area/Alpha', -(1 << 63), 0, 'West', None, 0),
                            ('Area/Alpha', 1, (1 << 63) - 1, 'West', 0, None)),
                'preferred': (('West', '001', 'Area/Alpha'),), 'primary': ()}
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, {text: i for i, text in enumerate(pool)})
        self.assertEqual(wire.decode(section, pool), data)
        words = unpack(section)
        start = wire.HEADER + wire.ID_WIDTH + wire.ZONE_WIDTH
        self.assertEqual(words[start + 5], wire.START | wire.END | wire.DAYLIGHT)
        self.assertEqual(words[start + 8 + 5], wire.START | wire.END | wire.STANDARD)
        changed = dict(data, periods=(('Area/Alpha', None, 0, 'West', None, None),
                                     ('Area/Alpha', 0, None, 'West', -86340, 86340)))
        self.assertEqual(wire.decode(wire.encode(changed, {s:i for i,s in enumerate(pool)}), pool), changed)

    def test_empty_zones_unused_ids_and_primary_without_period_rows(self):
        data = {'zones': ('Empty/Zone',), 'ids': (('west', 'West'),), 'periods': (), 'preferred': (),
                'primary': (('GB', 'Area/NoPeriods'),)}
        pool = tuple(sorted(wire.strings(data)))
        self.assertEqual(wire.decode(wire.encode(data, {s:i for i,s in enumerate(pool)}), pool), data)
        empty = {key: () for key in data}
        section = wire.encode(empty, {'':0})
        self.assertEqual(section, (0, repack([1, 0, 0, 0, 0])))
        self.assertEqual(wire.decode(section, ('',)), empty)

    def test_canonical_order_preserves_distinct_source_boundaries(self):
        data, pool, ids, section = fixture()
        changed = {key: tuple(reversed(rows)) for key, rows in data.items()}
        self.assertEqual(wire.encode(changed, ids), section)
        self.assertEqual(len(wire.decode(section, pool)['periods']), 4)
        adjacent = dict(data, periods=(('Area/Alpha', None, 0, 'West', None, None),
                                       ('Area/Alpha', 0, None, 'West', None, None)))
        # Keep this test focused on two equal adjacent records without merging;
        # other fixture mappings are not needed for this partial inventory.
        adjacent['preferred'] = ()
        encoded = wire.encode(adjacent, ids)
        self.assertEqual(unpack(encoded)[1], 2)
        self.assertEqual(wire.decode(encoded, pool), adjacent)

    def test_corrupt_dimensions_refs_flags_states_order_and_membership(self):
        data, pool, ids, section = fixture()
        words = unpack(section)
        _, period_count, preferred_count, primary_count, id_count = words[:5]
        zone_at = wire.HEADER + id_count * 2
        period_at = zone_at + section[0] * 3
        preferred_at = period_at + period_count * 8
        primary_at = preferred_at + preferred_count * 3
        last = period_at + (period_count - 1) * 8
        changes = [(0, 0), (1, 0xffffffff), (2, 0xffffffff), (3, 0xffffffff), (4, 0xffffffff),
                   (wire.HEADER, len(pool)), (wire.HEADER + 1, ids['WEST']),
                   (wire.HEADER, ids['Bad ID']), (zone_at, len(pool)), (zone_at, ids['Bad ID']),
                   (zone_at + 1, 1), (zone_at + 2, period_count + 1),
                   (period_at + 4, id_count), (period_at + 5, 16), (period_at + 6, 1),
                   (last, 1), (last + 2, 1), (last + 7, 60),
                   (preferred_at, id_count), (preferred_at + 1, len(pool)),
                   (preferred_at + 1, ids['Invalid']), (preferred_at + 2, section[0]),
                   (preferred_at + 2, 1), (primary_at, ids['Invalid']), (primary_at + 1, len(pool)),
                   (period_at + 8, 63593069)]
        for at, value in changes:
            changed = words[:]
            changed[at] = value
            with self.subTest(at=at,value=value), self.assertRaises(ValueError):
                wire.decode((section[0], repack(changed)), pool)
        for at, width in ((wire.HEADER + 2, 2), (zone_at + 3, 3), (preferred_at + 3, 3)):
            changed = words[:]
            changed[at:at + width] = changed[at - width:at]
            with self.subTest(at=at), self.assertRaises(ValueError):
                wire.decode((section[0], repack(changed)), pool)
        for broken in ((section[0] + 1, section[1]), (section[0], section[1][:-1]),
                       (section[0], section[1] + bytes(4)), (-1, section[1])):
            with self.assertRaises(ValueError):
                wire.decode(broken, pool)

    def test_invalid_semantic_records_and_value_domains(self):
        data, pool, ids, section = fixture()
        for changes in ({'zones': data['zones'] + data['zones'][:1]}, {'ids': data['ids'] + data['ids'][:1]},
                        {'primary': data['primary'] + data['primary']},
                        {'preferred': data['preferred'] + data['preferred'][:1]},
                        {'periods': data['periods'] + data['periods'][:1]},
                        {'ids': (('west','West'), ('east','West'))},
                        {'preferred': (('West','001','Etc/GMT'),)}):
            with self.subTest(changes=changes.keys()), self.assertRaises(ValueError):
                wire.encode(dict(data, **changes), ids)
        for at, value in ((1, -(1 << 63) - 1), (2, 1 << 63), (1, 0.0), (1, True),
                          (4, -86400), (5, 1), (5, 23.0)):
            row = list(data['periods'][0]); row[at] = value
            with self.subTest(at=at,value=value), self.assertRaises(ValueError):
                wire.encode(dict(data, periods=(tuple(row),) + data['periods'][1:]), ids)


if __name__ == '__main__':
    unittest.main()
