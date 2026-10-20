#!/usr/bin/env python3
"""Interval calendar-block sharing, source identity and hostile wire input."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import date_interval_tables as wire
import date_intervals as source
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_intervals import Sources, alias, data_calendar, formats, item, leaf, layers, query
from test_intl_list_patterns import graph


def fixture():
    sources = Sources()
    sources.set('root', data_calendar(formats(
        leaf('intervalFormatFallback', '{0}–{1}') +
        item(leaf('greatestDifference', 'latestFirst:M d – M d', id='M') +
             leaf('greatestDifference', '', id='y') +
             leaf('greatestDifference', BLOCK, id='d')) +
        '<intervalFormatRanges>' + leaf('intervalFormatRange', '{1} to {0}', type='mixed') +
        '</intervalFormatRanges>') +
        formats(item(leaf('greatestDifference', 'dd–dd', id='d', alt='variant'), alt='ascii'), alt='variant')) +
        data_calendar(formats(alias("../../../calendar[@type='gregorian']/dateTimeFormats/intervalFormats")),
                      code='generic'))
    sources.set('en', data_calendar(formats(item(leaf('greatestDifference', 'dd–dd', id='d', alt='variant')))))
    sources.set('de', data_calendar(formats(alias("../../../calendar[@type='generic']/dateTimeFormats/"
                                                'intervalFormats', 'en')), code='japanese'))
    data, tags = source.collect(sources, graph()), graph().tags
    pool = tuple(sorted(wire.strings(data) | set(tags) | {BLOCK, INHERIT, 'bad!', 'en_US', 'Unknown'}))
    return data, tags, pool, wire.encode(data, tags, {text: i for i, text in enumerate(pool)})


def equivalent(data):
    return tuple((locale, key, tuple(layers(data, locale, key))) for locale, key, head in data['rows'])


def repack(words):
    return struct.pack('<' + 'I' * len(words), *words)


class DateIntervalTablesTests(unittest.TestCase):
    def test_roundtrip_all_families_alt_levels_and_generic_pack(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(data))
        self.assertEqual(query(decoded, 'item', 'M')[0], 'latestFirst:M d – M d')
        self.assertEqual(query(decoded, 'item', 'y')[0], '')
        self.assertEqual(query(decoded, 'item', 'd')[0], BLOCK)
        self.assertEqual(query(decoded, 'item', 'd', alt='variant')[0], 'dd–dd')
        self.assertEqual(query(decoded, 'item', 'd', alt='variant', container_alt='variant', item_alt='ascii')[0], 'dd–dd')
        self.assertEqual(query(decoded, 'range', 'mixed')[0], '{1} to {0}')
        self.assertEqual(query(decoded, 'fallback')[0], '{0}–{1}')
        self.assertIsNone(query(decoded, 'item', 'G'))
        ids = {text: i for i, text in enumerate(pool)}
        self.assertEqual(wire.strings(decoded), wire.strings(data))
        self.assertEqual(wire.encode(decoded, tags, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                              bundle.DATE_INTERVALS: section}, bytes(32), 'native')
        self.assertEqual(equivalent(tables.validate(binary)['date_intervals']), equivalent(data))
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_canonical_encoding_ignores_source_topological_orders(self):
        data, tags, pool, section = fixture()
        depths = [0]
        for locale, key, fields, parent in data['nodes']:
            depths.append(depths[parent] + 1)
        order = sorted(range(1, len(depths)), key=lambda index: (depths[index], -index))
        remap = {old: index + 1 for index, old in enumerate(order)}
        remap[0] = 0
        changed = dict(data,
            nodes=tuple((locale, key, fields, remap[parent]) for old in order
                        for locale, key, fields, parent in (data['nodes'][old - 1],)),
            rows=tuple((locale, key, remap[head]) for locale, key, head in reversed(data['rows'])),
            maps=tuple(tuple(reversed(fields)) for fields in data['maps']))
        self.assertEqual(wire.encode(changed, tags, {s: i for i, s in enumerate(pool)}), section)

    def test_projection_retains_alias_origins_outside_requester_inventory(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(projected))
        self.assertEqual(query(decoded, 'item', 'd', alt='variant', locale='de', calendar='japanese'),
                         ('dd–dd', 'en', ('gregorian', '', 'item', 'yMd', '')))
        self.assertIn('en', pool)
        self.assertNotIn('en-GB', pool)

    def test_empty_and_subset_inventories_use_shared_calendar_blocks(self):
        sources = Sources()
        sources.set('root', data_calendar(formats('')) + data_calendar(formats(''), code='generic'))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})
        self.assertEqual(struct.unpack_from('<7I', section[1]), (1, 2, 2, 1, 0, 0, 0))
        decoded = wire.decode(section, pool, graph().tags)
        self.assertEqual((decoded['rows'], decoded['maps'], decoded['nodes']), ((), (), ()))
        # The schema supports a proper suffix subset, not only the pinned data's
        # complete family/skeleton inventory. Absent combinations stay zero.
        subset = dict(decoded, calendars=('gregorian',), suffixes=(('', 'fallback', '', ''),))
        raw = wire.encode(subset, graph().tags, {s: i for i, s in enumerate(pool)})
        self.assertEqual(wire.decode(raw, pool, graph().tags), subset)
        self.assertLess(len(section[1]), 7 * 4 + 2 * 4 + 2 * 4 * 4 + len(graph().tags) * 2 * 2 * 4)

    def test_aliases_can_change_all_source_selectors_within_family(self):
        sources = Sources()
        sources.set('root', data_calendar(formats(item(alias("../../../../calendar[@type='gregorian']/"
            "dateTimeFormats/intervalFormats[@alt='variant']/intervalFormatItem[@id='yMd'][@alt='ascii']"),
            skeleton='hm')), code='generic') +
            data_calendar(formats(item(leaf('greatestDifference', 'dd–dd', id='d'), alt='ascii'), alt='variant')))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        decoded = wire.decode(wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)}), pool, graph().tags)
        self.assertEqual(query(decoded, 'item', 'd', calendar='generic', skeleton='hm'),
                         ('dd–dd', 'root', ('gregorian', 'variant', 'item', 'yMd', 'ascii')))

    def test_corrupt_dimensions_references_states_and_order(self):
        data, tags, pool, section = fixture()
        count, raw = section
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        _, cals, suffixes, rows, maps, nodes, entries = words[:wire.HEADER]
        suffix_at = wire.HEADER + cals
        locale_at = suffix_at + suffixes * wire.SUFFIX_WIDTH
        rows_at = locale_at + count * cals
        maps_at = rows_at + rows * suffixes
        nodes_at = maps_at + maps * 2
        entries_at = nodes_at + nodes * wire.NODE_WIDTH
        block = next(i for i in range(entries_at, len(words), wire.ENTRY_WIDTH) if words[i + 2] == wire.BLOCKED)
        changes = [(0, 2), (1, 0), (2, 0), (3, 0), (4, nodes + 1), (5, nodes + 1), (6, 0),
                   (wire.HEADER, len(pool)), (wire.HEADER, pool.index('bad!')),
                   (wire.HEADER + 1, words[wire.HEADER]),
                   (suffix_at, len(pool)), (suffix_at + 1, len(wire.FAMILIES)),
                   (suffix_at + 2, len(pool)), (suffix_at + 3, len(pool)),
                   (locale_at, rows), (rows_at, nodes + 1), (maps_at, 1), (maps_at + 1, 0),
                   (nodes_at, len(pool)), (nodes_at, pool.index('en_US')),
                   (nodes_at + 1, cals * suffixes), (nodes_at + 2, maps), (nodes_at + 3, 1),
                   (entries_at, len(pool)), (entries_at + 1, len(pool)), (entries_at + 2, len(pool)),
                   (entries_at + 2, pool.index(INHERIT)), (block + 2, pool.index(BLOCK))]
        for offset, value in changes:
            changed = words[:]
            changed[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                wire.decode((count, repack(changed)), pool, tags)
        # Dimensions may fit individually while the implicit group ID overflows.
        changed = words[:]
        changed[1:3] = (0x10000, 0x10000)
        with self.assertRaisesRegex(ValueError, 'uint32'):
            wire.decode((count, repack(changed)), pool, tags)
        for broken in ((count + 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(broken, pool, tags)
        item_suffix = next(i for i in range(suffixes) if words[suffix_at + i * 4 + 1] == 1)
        range_node = next(i + 1 for i in range(nodes)
                          if words[suffix_at + (words[nodes_at + i * 4 + 1] % suffixes) * 4 + 1] == 2)
        changed = words[:]
        changed[rows_at + item_suffix] = range_node
        with self.assertRaisesRegex(ValueError, 'head has a different family'):
            wire.decode((count, repack(changed)), pool, tags)
        child = next(i for i in range(nodes) if words[nodes_at + i * 4 + 3] and i + 1 > range_node)
        changed = words[:]
        changed[nodes_at + child * 4 + 3] = range_node
        with self.assertRaisesRegex(ValueError, 'parent has a different family'):
            wire.decode((count, repack(changed)), pool, tags)

    def test_reject_invalid_source_inventories_and_unreachable_layers(self):
        data, tags, pool, section = fixture()
        ids = {s: i for i, s in enumerate(pool)}
        for changes in ({'calendars': ()}, {'calendars': ('Bad',)},
                        {'groups': data['groups'] + data['groups'][:1]}, {'groups': data['groups'][1:]},
                        {'rows': data['rows'] + data['rows'][:1]}, {'rows': ()},
                        {'rows': ((tags[0], data['groups'][0], 0),)},
                        {'maps': data['maps'] + data['maps'][:1]}):
            with self.subTest(changes=changes.keys()), self.assertRaises(ValueError):
                wire.encode(dict(data, **changes), tags, ids)
        for family, code, alt, text in (('fallback', 'd', '', 'text'), ('range', 'bad', '', '{0}{1}'),
                ('item', 's', '', 's'), ('item', 'd', '\0', 'd'), ('item', 'd', '', '\ud800'),
                ('item', 'd', '', INHERIT), ('item', 'd', '', '\0')):
            with self.subTest(family=family, code=code), self.assertRaises(ValueError):
                wire.field_valid(family, code, alt, text)
        with self.assertRaises(ValueError):
            wire.select(dict(data, rows=((tags[0], data['groups'][0], 0),)), tags)


if __name__ == '__main__':
    unittest.main()
