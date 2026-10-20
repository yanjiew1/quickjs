#!/usr/bin/env python3
"""Available-format wire identity, canonical layering and malformed input."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import date_available_formats as source
import date_available_tables as wire
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_available_formats import Sources, alias, calendar, calendar_formats, group, layers, query
from test_intl_list_patterns import graph


def fixture():
    sources = Sources()
    sources.set('root', calendar_formats(group('''<dateFormatItem id="y">y</dateFormatItem>
        <dateFormatItem id="y" count="other">yy</dateFormatItem>
        <dateFormatItem id="j">h a</dateFormatItem><dateFormatItem id="M"/>
        <dateFormatItem id="d">''' + BLOCK + '''</dateFormatItem>''') +
        group('<dateFormatItem id="y" alt="ascii">yyyy</dateFormatItem>', alt='variant') +
        group('''<appendItem request="Year">{0} ({2}: {1})</appendItem>
        <appendItem request="Time-Day-Of-Week">{1} ob {0}</appendItem>''', 'appendItems')) +
        calendar('generic', '<dateTimeFormats>' +
                 alias("../../calendar[@type='gregorian']/dateTimeFormats") + '</dateTimeFormats>'))
    sources.set('en', calendar_formats(group('<dateFormatItem id="y" alt="variant">y年</dateFormatItem>')))
    sources.set('en_GB', calendar_formats(group('<dateFormatItem id="d" alt="variant">dd</dateFormatItem>'),
                                         code='generic'))
    sources.set('de', calendar('generic', '<dateTimeFormats>' +
                              alias("../../calendar[@type='gregorian']/dateTimeFormats", 'en') +
                              '</dateTimeFormats>'))
    data, tags = source.collect(sources, graph()), graph().tags
    pool = tuple(sorted(wire.strings(data) | set(tags) | {BLOCK, INHERIT, 'bad!', 'en_US', 'Unknown'}))
    return data, tags, pool, wire.encode(data, tags, {text: i for i, text in enumerate(pool)})


def equivalent(data):
    return tuple((locale, key, tuple(layers(data, locale, family=key[1], alt=key[2], code=key[0])))
                 for locale, key, head in data['rows'])


def repack(words):
    return struct.pack('<' + 'I' * len(words), *words)


class DateAvailableTablesTests(unittest.TestCase):
    def test_roundtrip_selectors_templates_and_generic_pack(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(data))
        self.assertEqual(query(decoded, 'en-GB', 'y', count='other')[0], 'yy')
        self.assertEqual(query(decoded, 'en-GB', 'y', leaf_alt='ascii', alt='variant')[0], 'yyyy')
        self.assertEqual(query(decoded, 'en-GB', 'y', leaf_alt='variant')[0], 'y年')
        self.assertEqual(query(decoded, 'en-GB', 'M')[0], '')
        self.assertEqual(query(decoded, 'en-GB', 'd')[0], BLOCK)
        self.assertEqual(query(decoded, 'en-GB', 'j')[0], 'h a')
        self.assertEqual(query(decoded, 'en-GB', 'Time-Day-Of-Week', family='appendItems')[0], '{1} ob {0}')
        self.assertIsNone(query(decoded, 'en-GB', 'Q'))
        ids = {text: i for i, text in enumerate(pool)}
        self.assertEqual(wire.encode(decoded, tags, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                              bundle.DATE_AVAILABLE_FORMATS: section}, bytes(32), 'native')
        self.assertEqual(equivalent(tables.validate(binary)['date_available_formats']), equivalent(data))
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_canonical_encoding_ignores_equivalent_topological_orders(self):
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

    def test_projection_keeps_external_origins_and_drops_dead_layers(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(projected))
        self.assertEqual(query(decoded, 'de', 'y', code='generic', leaf_alt='variant'),
                         ('y年', 'en', ('gregorian', 'availableFormats', '')))
        self.assertIn('en', pool)
        self.assertNotIn('en-GB', pool)
        self.assertNotIn('dd', pool)

    def test_empty_section_has_one_missing_row_and_no_layers(self):
        sources = Sources()
        sources.set('root', calendar_formats(group('')))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})
        self.assertEqual(struct.unpack_from('<7I', section[1]), (1, 1, 2, 1, 0, 0, 0))
        decoded = wire.decode(section, pool, graph().tags)
        self.assertEqual((decoded['rows'], decoded['maps'], decoded['nodes']), ((), (), ()))

    def test_container_alias_may_change_calendar_and_alternate(self):
        sources = Sources()
        sources.set('root', calendar_formats(group('<dateFormatItem id="y">yyyy</dateFormatItem>', alt='variant')) +
            calendar_formats(group(alias("../../../calendar[@type='gregorian']/dateTimeFormats/"
                                         "availableFormats[@alt='variant']")), code='generic'))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        decoded = wire.decode(wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)}), pool, graph().tags)
        self.assertEqual(query(decoded, 'en-GB', 'y', code='generic'),
                         ('yyyy', 'root', ('gregorian', 'availableFormats', 'variant')))

    def test_corrupt_dimensions_refs_states_and_canonical_order(self):
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
                   (group_at, cals), (group_at + 1, len(wire.FAMILIES)), (group_at + 2, len(pool)),
                   (locale_at, rows), (rows_at, nodes + 1), (maps_at, 1), (maps_at + 1, 0),
                   (nodes_at, len(pool)), (nodes_at, pool.index('en_US')),
                   (nodes_at + 1, groups), (nodes_at + 2, maps), (nodes_at + 3, 1),
                   (entries_at, len(pool)), (entries_at + 1, len(wire.COUNTS)),
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
        # A valid node still cannot serve a head/parent of another family.
        available = next(index for index in range(groups)
                         if words[group_at + index * wire.GROUP_WIDTH + 1] == 0)
        append = next(index + 1 for index in range(nodes)
                      if words[group_at + words[nodes_at + index * 4 + 1] * wire.GROUP_WIDTH + 1] == 1)
        changed = words[:]
        changed[rows_at + available] = append
        with self.assertRaisesRegex(ValueError, 'head has a different family'):
            wire.decode((count, repack(changed)), pool, tags)
        child = next(index for index in range(nodes) if words[nodes_at + index * 4 + 3] and index + 1 > append)
        changed = words[:]
        changed[nodes_at + child * 4 + 3] = append
        with self.assertRaisesRegex(ValueError, 'parent has a different family'):
            wire.decode((count, repack(changed)), pool, tags)
        append_map = words[nodes_at + (append - 1) * 4 + 2]
        append_entry = entries_at + words[maps_at + append_map * 2] * wire.ENTRY_WIDTH
        for offset, value in ((append_entry + 1, 1), (append_entry, pool.index('Unknown'))):
            changed = words[:]
            changed[offset] = value
            with self.assertRaises(ValueError):
                wire.decode((count, repack(changed)), pool, tags)

    def test_reject_invalid_source_and_unreachable_structures(self):
        data, tags, pool, section = fixture()
        ids = {text: i for i, text in enumerate(pool)}
        for changes in (
                {'calendars': ()}, {'calendars': ('Bad',)},
                {'groups': data['groups'] + data['groups'][:1]},
                {'groups': (('gregorian', 'availableFormats', '\0'),)},
                {'rows': data['rows'] + data['rows'][:1]},
                {'rows': (('unknown', data['groups'][0], 1),)},
                {'rows': ((tags[0], data['groups'][0], 0),)},
                {'rows': ()}, {'maps': data['maps'] + data['maps'][:1]},
                {'nodes': (('en_US', data['nodes'][0][1], 0, 0),) + data['nodes'][1:]}):
            with self.subTest(changes=changes.keys()), self.assertRaises(ValueError):
                wire.encode(dict(data, **changes), tags, ids)
        for family, code, plural, alt, text in (
                ('appendItems', 'Year', 'one', '', 'bad'), ('appendItems', 'Unknown', '', '', 'bad'),
                ('availableFormats', 'y-M', '', '', 'bad'), ('availableFormats', 'y', 'invalid', '', 'bad'),
                ('availableFormats', 'y', '', '', INHERIT), ('availableFormats', 'y', '', '', '\0'),
                ('availableFormats', 'y', '', '', '\ud800')):
            with self.subTest(family=family, code=code, text=repr(text)), self.assertRaises(ValueError):
                wire.field_valid(family, code, plural, alt, text)
        with self.assertRaises(ValueError):
            wire.select(dict(data, rows=((tags[0], data['groups'][0], 0),)), tags)


if __name__ == '__main__':
    unittest.main()
