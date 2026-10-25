#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Calendar-name wire sharing, source layers, projection and corrupt input."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import calendar_names as source
import calendar_name_tables as wire
from resolve import BLOCK, INHERIT
import tables
from test_intl_calendar_names import Sources, alias, calendar, group, layers, query
from test_intl_list_patterns import graph


def fixture():
    sources = Sources()
    sources.set('root', calendar('gregorian', group('months',
        '<month type="1">root</month><month type="2">' + BLOCK + '</month>'
        '<month type="3"/><month type="7" yeartype="standard">ordinary</month>'
        '<month type="7" yeartype="leap" alt="variant">閏😀</month>') +
        group('days', '<day type="mon">Monday</day>') +
        group('eras', '<era type="236">Reiwa</era>')) +
        calendar('generic', '<months>' + alias("../../calendar[@type='gregorian']/months") + '</months>'))
    sources.set('en', calendar('gregorian', group('months', '<month type="1" alt="variant">child</month>')))
    sources.set('en_GB', calendar('generic', group('months', '<month type="4">own</month>')))
    sources.set('de', calendar('generic', '<months>' +
        alias("../../calendar[@type='gregorian']/months", source='en') + '</months>'))
    data, tags = source.collect(sources, graph()), graph().tags
    pool = tuple(sorted(wire.strings(data) | set(tags) | {BLOCK, INHERIT, 'bad!', 'en_US', '0', '01'}))
    return data, tags, pool, wire.encode(data, tags, {text: index for index, text in enumerate(pool)})


def equivalent(data):
    return tuple((locale, key, tuple(layers(data, locale, key))) for locale, key, head in data['rows'])


def repack(words):
    return struct.pack('<' + 'I' * len(words), *words)


class CalendarNameTablesTests(unittest.TestCase):
    def test_roundtrip_layers_and_generic_pack_validation(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(data))
        key = ('generic', 'months', '', 'format', 'wide')
        self.assertEqual([row[0] for row in layers(decoded, 'en-GB', key)], ['en-GB', 'en', 'root'])
        self.assertEqual(query(decoded, 'en-GB', key, '1', alt='variant')[1], 'child')
        self.assertEqual(query(decoded, 'en-GB', key, '2')[0], 'blocked')
        self.assertEqual(query(decoded, 'en-GB', key, '3')[1], '')
        self.assertIsNone(query(decoded, 'en-GB', key, '5'))
        ids = {text: index for index, text in enumerate(pool)}
        self.assertEqual(wire.encode(decoded, tags, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                              bundle.CALENDAR_NAMES: section}, bytes(32), 'native')
        self.assertEqual(equivalent(tables.validate(binary)['calendar_names']), equivalent(data))
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_canonical_encoding_ignores_equivalent_topological_orders(self):
        data, tags, pool, section = fixture()
        depths = [0]
        for locale, key, fields, parent in data['nodes']:
            depths.append(depths[parent] + 1)
        order = sorted(range(1, len(depths)), key=lambda index: (depths[index], -index))
        new = {old: index + 1 for index, old in enumerate(order)}
        new[0] = 0
        changed = dict(data,
            nodes=tuple((locale, key, fields, new[parent]) for old in order
                        for locale, key, fields, parent in (data['nodes'][old - 1],)),
            rows=tuple((locale, key, new[head]) for locale, key, head in reversed(data['rows'])),
            maps=tuple(tuple(reversed(fields)) for fields in data['maps']))
        self.assertEqual(wire.encode(changed, tags, {s: i for i, s in enumerate(pool)}), section)

    def test_projection_keeps_external_terminal_sources_and_drops_dead_layers(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(projected))
        key = ('generic', 'months', '', 'format', 'wide')
        self.assertEqual(query(decoded, 'de', key, '1', alt='variant')[:3], ('value', 'child', 'en'))
        self.assertIn('en', pool)
        self.assertNotIn('en-GB', pool)
        self.assertNotIn('own', pool)

    def test_empty_sources_share_one_missing_row_without_name_nodes(self):
        sources = Sources()
        sources.set('root', calendar('gregorian', '<months/>'))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})
        self.assertEqual(struct.unpack_from('<7I', section[1]), (1, 1, 77, 1, 0, 0, 0))
        decoded = wire.decode(section, pool, graph().tags)
        self.assertEqual((decoded['rows'], decoded['maps'], decoded['nodes']), ((), (), ()))

    def test_remaining_families_and_alias_selector_changes_roundtrip(self):
        sources = Sources()
        sources.set('root', calendar('chinese', '''<cyclicNameSets>
            <cyclicNameSet type="years"><cyclicNameContext type="format">
            <cyclicNameWidth type="abbreviated"><cyclicName type="1">Y</cyclicName></cyclicNameWidth>
            </cyclicNameContext><cyclicNameContext type="stand-alone">''' +
            alias("../cyclicNameContext[@type='format']") + '''</cyclicNameContext></cyclicNameSet>
            <cyclicNameSet type="months">''' + alias("../cyclicNameSet[@type='years']") + '''</cyclicNameSet>
            <cyclicNameSet type="solarTerms"><cyclicNameContext type="format">
            <cyclicNameWidth type="narrow">''' + alias("../../../cyclicNameSet[@type='years']/"
            "cyclicNameContext[@type='format']/cyclicNameWidth[@type='abbreviated']") +
            '''</cyclicNameWidth></cyclicNameContext></cyclicNameSet></cyclicNameSets>''' +
            group('dayPeriods', '<dayPeriod type="midnight">夜</dayPeriod>', width='narrow') +
            group('quarters', '<quarter type="4">Q4</quarter>', context='stand-alone') +
            group('monthPatterns', '<monthPattern type="leap">閏{0}</monthPattern>',
                  context='numeric', width='all')))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        decoded = wire.decode(wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)}), pool, graph().tags)
        self.assertEqual(equivalent(decoded), equivalent(data))
        for key, code, expected in (
                (('cyclicNameSets', 'months', 'stand-alone', 'abbreviated'), '1', 'Y'),
                (('cyclicNameSets', 'solarTerms', 'format', 'narrow'), '1', 'Y'),
                (('dayPeriods', '', 'format', 'narrow'), 'midnight', '夜'),
                (('quarters', '', 'stand-alone', 'wide'), '4', 'Q4'),
                (('monthPatterns', '', 'numeric', 'all'), 'leap', '閏{0}')):
            self.assertEqual(query(decoded, 'en-GB', ('chinese',) + key, code)[1], expected)

    def test_corrupt_dimensions_indices_states_and_canonical_order(self):
        data, tags, pool, section = fixture()
        count, raw = section
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        _, cals, groups, rows, maps, nodes, entries = words[:wire.HEADER]
        group_at = wire.HEADER + cals
        locale_at = group_at + groups * wire.GROUP_WIDTH
        rows_at = locale_at + count
        maps_at = rows_at + rows * groups
        nodes_at = maps_at + maps * 2
        entries_at = nodes_at + nodes * wire.NODE_WIDTH
        block = next(i for i in range(entries_at, len(words), wire.ENTRY_WIDTH) if words[i + 3] == wire.BLOCKED)
        changes = [(0, 2), (1, 0), (2, 0), (3, 0), (4, nodes + 1), (5, nodes + 1), (6, 0),
                   (wire.HEADER, len(pool)), (wire.HEADER, pool.index('bad!')),
                   (wire.HEADER + 1, words[wire.HEADER]),
                   (group_at, cals), (group_at + 1, len(wire.FAMILIES)),
                   (group_at + 2, len(wire.CYCLES)), (group_at + 3, len(wire.CONTEXTS)),
                   (group_at + 4, len(wire.WIDTHS)),
                   (locale_at, rows), (rows_at, nodes + 1), (maps_at, 1), (maps_at + 1, 0),
                   (nodes_at, len(pool)), (nodes_at, pool.index('en_US')),
                   (nodes_at + 1, groups), (nodes_at + 2, maps), (nodes_at + 3, 1),
                   (entries_at, len(pool)), (entries_at + 1, len(wire.YEARS)),
                   (entries_at + 2, len(pool)), (entries_at + 3, len(pool)),
                   (entries_at + 3, pool.index(INHERIT)), (block + 3, pool.index(BLOCK))]
        for offset, value in changes:
            changed = words[:]
            changed[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                wire.decode((count, repack(changed)), pool, tags)
        for broken in ((count + 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(broken, pool, tags)
        # Valid references must still belong to the requested semantic family.
        months = next(index for index in range(groups) if wire.FAMILIES[words[group_at + index * 5 + 1]] == 'months')
        day_node = next(index + 1 for index in range(nodes)
            if wire.FAMILIES[words[group_at + words[nodes_at + index * 4 + 1] * 5 + 1]] == 'days')
        changed = words[:]
        changed[rows_at + months] = day_node
        with self.assertRaises(ValueError):
            wire.decode((count, repack(changed)), pool, tags)
        child = next(index for index in range(nodes) if words[nodes_at + index * 4 + 3])
        changed = words[:]
        changed[nodes_at + child * 4 + 3] = day_node
        with self.assertRaisesRegex(ValueError, 'parent has a different family'):
            wire.decode((count, repack(changed)), pool, tags)

    def test_reject_invalid_source_inventories_fields_heads_and_reachability(self):
        data, tags, pool, section = fixture()
        ids = {text: i for i, text in enumerate(pool)}
        for changes in (
                {'calendars': ()}, {'calendars': ('Bad',)},
                {'groups': data['groups'] + data['groups'][:1]},
                {'rows': data['rows'] + data['rows'][:1]},
                {'rows': (('unknown', data['groups'][0], 1),)},
                {'rows': ((tags[0], data['groups'][0], 0),)},
                {'rows': ()}, {'maps': data['maps'] + data['maps'][:1]},
                {'nodes': (('en_US', data['nodes'][0][1], 0, 0),) + data['nodes'][1:]}):
            with self.subTest(changes=changes.keys()), self.assertRaises(ValueError):
                wire.encode(dict(data, **changes), tags, ids)
        for family, code, year, alt, text in (
                ('days', 'mon', 'leap', '', 'bad'), ('eras', '01', '', '', 'bad'),
                ('months', '14', '', '', 'bad'), ('months', '1', '', '', INHERIT),
                ('months', '1', '', '', '\0'), ('months', '1', '', '', '\ud800')):
            with self.subTest(family=family, code=code, text=repr(text)), self.assertRaises(ValueError):
                wire.field_valid(family, code, year, alt, text)
        with self.assertRaises(ValueError):
            wire.select(dict(data, rows=((tags[0], data['groups'][0], 0),)), tags)


if __name__ == '__main__':
    unittest.main()
