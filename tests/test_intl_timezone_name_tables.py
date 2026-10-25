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

"""Timezone family rows, source provenance and malformed binary tables."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
from resolve import BLOCK, INHERIT
import tables
import timezone_names as source
import timezone_name_tables as wire
from test_intl_timezone_names import Sources, alias, entity, graph, key, leaf, query, width


def fixture():
    sources = Sources()
    sources.set('root', leaf('gmtFormat', 'GMT{0}') + leaf('gmtFormat', 'UTC{0}', alt='utc') +
        leaf('hourFormat', '+HH:mm;−HH:mm') + leaf('regionFormat', '{0} Time') +
        leaf('regionFormat', '{0} Standard', type='standard') +
        leaf('regionFormat', '{0} Summer', type='daylight') +
        entity(leaf('exemplarCity', 'London') + width(leaf('standard', 'London Standard') + leaf('generic', '')) +
               width(leaf('generic', 'LS'), code='short', alt='ascii')) +
        entity(width(leaf('daylight', 'Summer', alt='variant'), alt='ascii'), alt='variant') +
        entity(width(leaf('generic', 'GMT')), 'GMT', 'metazone') +
        entity(width(alias("../../zone[@type='Europe/London']/short[@alt='ascii']")), 'Irish', 'metazone'))
    sources.set('en', leaf('gmtFormat', '') + entity(leaf('exemplarCity', INHERIT) +
        leaf('exemplarCity', BLOCK, alt='secondary') + width(leaf('standard', 'English London'))) +
        entity(width(leaf('generic', 'Greenwich')), 'GMT', 'metazone') +
        entity(width(alias("../../metazone[@type='GMT']/long")), 'Etc/Unknown'))
    sources.set('de', entity(width(alias("../../zone[@type='Europe/London']/long", 'en')), 'British', 'metazone'))
    data, tags = source.collect(sources, graph()), graph().tags
    pool = tuple(sorted(wire.strings(data) | set(tags) | {BLOCK, INHERIT, 'Unknown', 'bad!', 'Bad ID', 'en_US'}))
    return data, tags, pool, wire.encode(data, tags, {text: i for i, text in enumerate(pool)})


def equivalent(data):
    nodes = ['']
    for locale, group, fields, parent in data['nodes']:
        nodes.append((locale, group, tuple(sorted(data['maps'][fields])), nodes[parent]))
    return tuple((locale, group, nodes[head]) for locale, group, head in data['rows'])


def repack(words):
    return struct.pack('<' + 'I' * len(words), *words)


def offsets(words, count):
    group_count, map_count, node_count = words[1:4]
    groups = tuple(tuple(words[i:i + 5]) for i in range(wire.HEADER, wire.HEADER + group_count * 5, 5))
    at, families = wire.HEADER + group_count * 5, []
    for family, rows in enumerate(words[5:9]):
        span = tuple(index for index, key in enumerate(groups) if key[0] == family)
        if not span:
            families.append(None)
            continue
        families.append((at, at + count, span, rows))
        at += count + rows * len(span)
    return groups, families, at, at + map_count * 2, at + map_count * 2 + node_count * 4


class TimezoneNameTablesTests(unittest.TestCase):
    def test_all_selectors_states_alias_classes_and_generic_pack(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(data))
        self.assertEqual(query(decoded, source.FORMAT_GROUP, 'gmtFormat')[:2], ('', 'en'))
        self.assertEqual(query(decoded, source.FORMAT_GROUP, 'gmtFormat', alt='utc')[0], 'UTC{0}')
        self.assertEqual(query(decoded, source.FORMAT_GROUP, 'regionFormat', subtype='standard')[0], '{0} Standard')
        self.assertEqual(query(decoded, ('city', 'Europe/London', '', '', ''), 'exemplarCity', alt='secondary')[0], BLOCK)
        self.assertEqual(query(decoded, key('Etc/Unknown'), 'generic')[:2], ('Greenwich', 'en'))
        self.assertEqual(query(decoded, key('Irish', 'metazone-name'), 'generic')[2], key(width='short', width_alt='ascii'))
        self.assertEqual(query(decoded, key(entity_alt='variant', width_alt='ascii'), 'daylight', alt='variant')[0], 'Summer')
        self.assertEqual(query(decoded, key(), 'generic')[0], '')
        self.assertIsNone(query(decoded, key(), 'daylight'))
        ids = {text: i for i, text in enumerate(pool)}
        self.assertEqual(wire.encode(decoded, tags, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                              bundle.TIMEZONE_NAMES: section}, bytes(32), 'native')
        self.assertEqual(equivalent(tables.validate(binary)['timezone_names']), equivalent(data))
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_projection_retains_external_origin_and_source_group(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(projected))
        self.assertEqual(query(decoded, key('British', 'metazone-name'), 'standard', locale='de'),
                         ('English London', 'en', key()))
        self.assertIn('en', pool)
        self.assertNotIn('en-GB', pool)

    def test_canonical_encoding_ignores_source_order(self):
        data, tags, pool, section = fixture()
        depths = [0]
        for locale, group, fields, parent in data['nodes']:
            depths.append(depths[parent] + 1)
        order = sorted(range(1, len(depths)), key=lambda index: (depths[index], -index))
        remap = {old: index + 1 for index, old in enumerate(order)}
        remap[0] = 0
        changed = dict(data, nodes=tuple((locale, group, fields, remap[parent]) for old in order
                            for locale, group, fields, parent in (data['nodes'][old - 1],)),
                       rows=tuple((locale, group, remap[head]) for locale, group, head in reversed(data['rows'])),
                       maps=tuple(tuple(reversed(fields)) for fields in data['maps']))
        self.assertEqual(wire.encode(changed, tags, {s: i for i, s in enumerate(pool)}), section)

    def test_empty_one_family_subset_and_independent_family_sharing(self):
        tags = graph().tags
        data = {'groups': (('city', 'Europe/London', '', '', ''),), 'maps': (), 'nodes': (), 'rows': ()}
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, tags, {s: i for i, s in enumerate(pool)})
        self.assertEqual(struct.unpack_from('<9I', section[1]), (1, 1, 0, 0, 0, 0, 1, 0, 0))
        self.assertEqual(wire.decode(section, pool, tags), data)
        sources = Sources()
        sources.set('root', leaf('gmtFormat', 'GMT{0}') + entity(leaf('exemplarCity', 'London')))
        sources.set('en', leaf('gmtFormat', 'UTC{0}'))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, tags, {s: i for i, s in enumerate(pool)})
        self.assertEqual(struct.unpack_from('<9I', section[1])[5:], (2, 1, 0, 0))
        self.assertEqual(equivalent(wire.decode(section, pool, tags)), equivalent(data))

    def test_corrupt_dimensions_refs_states_and_canonical_order(self):
        data, tags, pool, section = fixture()
        count, raw = section
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        groups, families, maps_at, nodes_at, entries_at = offsets(words, count)
        _, group_count, map_count, node_count, entry_count = words[:5]
        changes = [(0, 2), (1, 0), (1, 0xffffffff), (2, node_count + 1), (3, 0xffffffff), (4, 0),
                   (wire.HEADER, len(wire.FAMILIES)), (wire.HEADER + 1, len(pool)),
                   (wire.HEADER + 1, pool.index('Bad ID')), (wire.HEADER + 3, len(wire.WIDTHS)),
                   (wire.HEADER + 4, len(pool)), (maps_at, 1), (maps_at + 1, 0),
                   (nodes_at, len(pool)), (nodes_at, pool.index('en_US')), (nodes_at + 1, group_count),
                   (nodes_at + 2, map_count), (nodes_at + 3, 1), (entries_at, len(pool)),
                   (entries_at + 1, len(wire.TYPES)), (entries_at + 2, len(pool)),
                   (entries_at + 3, len(pool)), (entries_at + 3, pool.index(INHERIT)),
                   (entries_at + 3, pool.index(BLOCK))]
        for family, parts in enumerate(families):
            if parts:
                refs, rows, span, row_count = parts
                changes.extend(((5 + family, 0), (5 + family, count + 1), (refs, row_count), (rows, node_count + 1)))
        for offset, value in changes:
            changed = words[:]
            changed[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                wire.decode((count, repack(changed)), pool, tags)
        for broken in ((count + 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(broken, pool, tags)
        for at, width in ((wire.HEADER + 5, 5), (nodes_at + 4, 4)):
            changed = words[:]
            changed[at:at + width] = changed[at - width:at]
            with self.assertRaises(ValueError):
                wire.decode((count, repack(changed)), pool, tags)
        name_node = next(i + 1 for i in range(node_count) if groups[words[nodes_at + i * 4 + 1]][0] >= 2)
        child = next(i for i in range(node_count) if groups[words[nodes_at + i * 4 + 1]][0] == 0
                     and words[nodes_at + i * 4 + 3] and i + 1 > name_node)
        changed = words[:]
        changed[nodes_at + child * 4 + 3] = name_node
        with self.assertRaisesRegex(ValueError, 'parent has a different semantic class'):
            wire.decode((count, repack(changed)), pool, tags)

    def test_reject_invalid_source_classes_and_unreachable_layers(self):
        data, tags, pool, section = fixture()
        ids = {s: i for i, s in enumerate(pool)}
        name_head = next(head for tag, group, head in data['rows'] if group[0] == 'zone-name')
        for changes in ({'groups': ()}, {'groups': data['groups'] + data['groups'][:1]},
                        {'rows': data['rows'] + data['rows'][:1]}, {'rows': ()},
                        {'rows': ((tags[0], source.FORMAT_GROUP, name_head),)},
                        {'maps': data['maps'] + data['maps'][:1]}):
            with self.subTest(changes=changes.keys()), self.assertRaises(ValueError):
                wire.encode(dict(data, **changes), tags, ids)
        for family, code, subtype, alt, text in (
                ('format', 'gmtFormat', 'standard', '', '{0}'), ('city', 'standard', '', '', 'S'),
                ('zone-name', 'generic', 'daylight', '', 'G'), ('metazone-name', 'exemplarCity', '', '', 'C'),
                ('format', 'regionFormat', 'unknown', '', '{0}'), ('city', 'exemplarCity', '', '', INHERIT),
                ('zone-name', 'generic', '', '\0', 'G'), ('zone-name', 'generic', '', '', '\ud800')):
            with self.subTest(family=family, code=code), self.assertRaises(ValueError):
                wire.field_valid(family, code, subtype, alt, text)


if __name__ == '__main__':
    unittest.main()
