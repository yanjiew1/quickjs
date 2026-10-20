#!/usr/bin/env python3
"""Canonical global day-period lists and per-kind semantic admission."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import dayperiod_rules as source
import dayperiod_tables as wire
import tables
from test_intl_dayperiod_rules import Sources


def fixture(data=None):
    if data is None:
        data = source.collect(Sources())
    pool = tuple(sorted(wire.strings(data) | {'root', 'bad_tag', 'en-1901-1901', 'en-us'}))
    ids = {text: index for index, text in enumerate(pool)}
    return data, pool, ids, wire.encode(data, ids)


def unpack(section):
    return list(struct.unpack('<' + 'I' * (len(section[1]) // 4), section[1]))


def repack(words):
    return b''.join(struct.pack('<I', value) for value in words)


class DayPeriodTablesTests(unittest.TestCase):
    def test_roundtrip_generic_pack_and_canonical_reordering(self):
        data, pool, ids, section = fixture()
        self.assertEqual(wire.decode(section, pool), data)
        reordered = {'rows': tuple((kind, tag, tuple(reversed(rules)))
                                   for kind, tag, rules in reversed(data['rows']))}
        self.assertEqual(wire.encode(reordered, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: (1, struct.pack('<II', ids['root'], 0xffffffff)),
                              bundle.DAYPERIOD_RULES: section}, bytes(32), 'minimal')
        self.assertEqual(tables.validate(binary)['dayperiod_rules'], data)
        # en/sr/hi-Latn share a list without becoming profile locale rows.
        self.assertEqual(unpack(section)[1], 3)
        self.assertIsNone(wire.decode(None, pool))

    def test_cross_kind_sharing_empty_point_only_and_empty_section(self):
        points = (('midnight','at',0,0),)
        data = {'rows': (('format','en',()), ('format','gsw',points),
                         ('selection','en',()), ('selection','gsw',points))}
        _, pool, ids, section = fixture(data)
        self.assertEqual(wire.decode(section, pool), data)
        self.assertEqual(unpack(section)[1:3], [2, 1])
        empty = wire.encode({'rows': ()}, {'': 0})
        self.assertEqual(empty, (0, repack([1, 0, 0])))
        self.assertEqual(wire.decode(empty, ('',)), {'rows': ()})

    def test_selection_admission_does_not_admit_format_alias(self):
        rules = (('morning1','range',22500,65700), ('night1','range',65700,22500))
        data = {'rows': (('selection','en',rules),)}
        _, pool, ids, section = fixture(data)
        self.assertEqual(wire.decode(section, pool), data)
        words = unpack(section)
        words[wire.HEADER] = 0
        with self.assertRaises(ValueError):
            wire.decode((section[0], repack(words)), pool)
        with self.assertRaises(ValueError):
            wire.encode({'rows': (('selection','en',rules), ('format','sr',rules))}, ids)

    def test_corrupt_counts_spans_refs_order_and_rule_semantics(self):
        data, pool, ids, section = fixture()
        words = unpack(section)
        maps = wire.HEADER + section[0] * wire.ROW_WIDTH
        rules = maps + words[1] * wire.MAP_WIDTH
        changes = [(0, 0), (1, 0xffffffff), (2, 0xffffffff),
                   (wire.HEADER, 2), (wire.HEADER + 1, len(pool)),
                   (wire.HEADER + 1, ids['bad_tag']), (wire.HEADER + 1, ids['en-us']),
                   (wire.HEADER + 1, ids['en-1901-1901']), (wire.HEADER + 2, words[1]),
                   (wire.HEADER + 3 + 1, words[wire.HEADER + 1]),
                   (maps, 1), (maps + 1, 13), (maps + 2, 0),
                   (rules, len(wire.CODES)), (rules + 1, 2), (rules + 2, 1), (rules + 3, 86401)]
        for at, value in changes:
            modified = words[:]
            modified[at] = value
            with self.subTest(at=at,value=value), self.assertRaises(ValueError):
                wire.decode((section[0], repack(modified)), pool)
        for raw in (section[1][:-4], section[1] + bytes(4)):
            with self.assertRaises(ValueError):
                wire.decode((section[0], raw), pool)
        # Make a complete physical list unused while preserving dimensions.
        modified = words[:]
        for i in range(section[0]):
            if modified[wire.HEADER + i * 3 + 2] == 2:
                modified[wire.HEADER + i * 3 + 2] = 0
        with self.assertRaises(ValueError):
            wire.decode((section[0], repack(modified)), pool)

    def test_invalid_source_shapes_domains_and_duplicate_bindings(self):
        data, pool, ids, section = fixture()
        kind, tag, rules = data['rows'][0]
        cases = ({'rows': data['rows'] + (data['rows'][0],)},
                 {'rows': ((kind, 'en_US', rules),)}, {'rows': ((kind, tag, []),)},
                 {'rows': ((kind, tag, (('am','range',False,43200),)),)},
                 {'rows': ((kind, tag, (('midnight','at',86400,86400),)),)},
                 {'rows': ((kind, tag, rules + (rules[0],)),)},
                 {'rows': ((kind, tag, (('morning1','range',0,3600),)),)},
                 {'rows': []}, {'rows': (('format',),)}, {'rows': (), 'extra': ()})
        for case in cases:
            with self.subTest(case=case), self.assertRaises(ValueError):
                wire.encode(case, ids)


if __name__ == '__main__':
    unittest.main()
