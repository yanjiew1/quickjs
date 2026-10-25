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

"""Layered names: normalized indexes, provenance, projection and corruptions."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import display_names as source
import display_tables as wire
from resolve import BLOCK, INHERIT
import tables
from test_intl_display_names import Sources, layers
from test_intl_list_patterns import graph


def equivalent(data):
    return {(tag, group): layers(data, tag, group) for tag, group, head in data['rows']}


def fixture():
    sources = Sources()
    sources.set('root', '''<languages><language type="en">English</language>
        <language type="ca_VALENCIA">Valencian</language></languages>
        <scripts><script type="Latn">Latin</script></scripts>
        <territories><territory type="US">United States</territory></territories>
        <variants><variant type="VALENCIA">Valencian variant</variant></variants>
        <types><type key="calendar" type="gregorian">Gregorian Calendar</type></types>
        <localeDisplayPattern><localePattern>{0} ({1})</localePattern>
        <localeSeparator>{0}, {1}</localeSeparator></localeDisplayPattern>''',
        '<nestedBracketReplacement bracket="(">[</nestedBracketReplacement>')
    sources.set('en', '''<languages><language type="ZH_hant" alt="short">short</language>
        <language type="zh_Hant">default</language><language type="fr"/>
        <language type="de">''' + BLOCK + '''</language>
        <language type="en" menu="core">English core</language></languages>
        <types><type key="calendar" type="gregorian" scope="core">Gregorian</type></types>''')
    sources.set('en_GB', '<languages><language type="en">British English</language></languages>')
    sources.set('de', '<languages><alias source="en" path="../languages"/></languages>')
    data = source.collect(sources, graph())
    tags = graph().tags
    pool = tuple(sorted(wire.strings(data) | set(tags) | {BLOCK, INHERIT, 'bad_tag', 'WRONG'}))
    ids = {text: index for index, text in enumerate(pool)}
    return data, tags, pool, wire.encode(data, tags, ids)


class DisplayTablesTests(unittest.TestCase):
    def test_roundtrip_lookup_order_and_generic_pack_validation(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(data))
        ids = {text: index for index, text in enumerate(pool)}
        self.assertEqual(wire.encode(decoded, tags, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                              bundle.DISPLAY_NAMES: section}, bytes(32), 'native')
        self.assertEqual(equivalent(tables.validate(binary)['display_names']), equivalent(data))
        self.assertIn('zh-hant', pool)
        self.assertIn('ZH_hant', pool)
        self.assertIn('zh_Hant', pool)
        self.assertIn(('fr', '', '', '', ''), layers(decoded, 'en', 'language')[0][2])
        self.assertIn(('de', '', '', '', BLOCK), layers(decoded, 'en', 'language')[0][2])

    def test_projection_retains_external_alias_origin(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        self.assertEqual(equivalent(decoded), equivalent(projected))
        self.assertEqual(layers(decoded, 'de', 'language')[0][0], 'en')
        self.assertIn('en', pool)
        self.assertNotIn('en', tags)
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_normalized_collisions_are_local_not_cross_layer(self):
        sources = Sources()
        sources.set('root', '<languages><language type="zh_Hant">root</language></languages>')
        sources.set('en', '<languages><language type="ZH-hant">child</language></languages>')
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})
        self.assertEqual(equivalent(wire.decode(section, pool, graph().tags)), equivalent(data))
        sources.set('en', '''<languages><language type="zh_Hant">one</language>
            <language type="ZH-hant">two</language></languages>''')
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        with self.assertRaisesRegex(ValueError, 'collision'):
            wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})

    def test_shared_raw_map_compiles_separately_for_exact_and_identifier_groups(self):
        sources = Sources()
        sources.set('root', '<scripts><script type="Latn">Latin</script></scripts>',
                    '<nestedBracketReplacement bracket="Latn">Latin</nestedBracketReplacement>')
        data = source.collect(sources, graph())
        self.assertEqual(len(data['maps']), 1)
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})
        header = struct.unpack('<6I', section[1][:wire.HEADER * 4])
        self.assertEqual(header[2], 2)
        decoded = wire.decode(section, pool, graph().tags)
        self.assertEqual(equivalent(decoded), equivalent(data))
        self.assertEqual(wire.encode(decoded, graph().tags, {s: i for i, s in enumerate(pool)}), section)
        self.assertEqual(wire.normalized('script', 'Latn'), 'latn')
        self.assertEqual(wire.normalized('bracket', 'Latn'), 'Latn')
        self.assertEqual(wire.normalized('pattern', 'localePattern'), 'localePattern')

    def test_equivalent_source_topological_orders_have_identical_encoding(self):
        data, tags, pool, section = fixture()
        # Choose another valid topological order without changing any layers.
        pending = dict(enumerate(data['nodes'], 1))
        remap, nodes = {0: 0}, []
        while pending:
            old = max(index for index, node in pending.items() if node[3] in remap)
            locale, group, index, parent = pending.pop(old)
            nodes.append((locale, group, index, remap[parent]))
            remap[old] = len(nodes)
        changed = dict(data, nodes=tuple(nodes),
                       rows=tuple((tag, group, remap[head]) for tag, group, head in data['rows']))
        self.assertNotEqual(changed['nodes'], data['nodes'])
        self.assertEqual(wire.encode(changed, tags, {s: i for i, s in enumerate(pool)}), section)

    def test_empty_profile_has_only_one_zero_row(self):
        data = source.collect(Sources(), graph())
        section = wire.encode(data, graph().tags, {'': 0})
        self.assertEqual(struct.unpack('<6I', section[1][:wire.HEADER * 4]), (1, 1, 0, 0, 0, 0))
        self.assertEqual(equivalent(wire.decode(section, ('',), graph().tags)), equivalent(data))

    def test_row_head_must_match_its_group(self):
        sources = Sources()
        sources.set('root', '<languages><language type="en">English</language></languages>')
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        count, raw = wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        at = wire.HEADER + count
        self.assertEqual(words[at:at + 7], [1, 0, 0, 0, 0, 0, 0])
        words[at:at + 2] = [0, 1]
        with self.assertRaisesRegex(ValueError, 'wrong group'):
            wire.decode((count, struct.pack('<' + 'I' * len(words), *words)), pool, graph().tags)

    def test_corruption_extents_references_order_and_provenance(self):
        data, tags, pool, section = fixture()
        count, raw = section
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        row_count, map_count, node_count, entry_count, key_count = words[1:6]
        rows_at = wire.HEADER + count
        maps_at = rows_at + row_count * 7
        nodes_at = maps_at + map_count * 2
        entries_at = nodes_at + node_count * wire.NODE_WIDTH
        keys_at = entries_at + entry_count * wire.ENTRY_WIDTH
        changes = [(0, 2), (1, 0), (wire.HEADER, row_count), (rows_at, node_count + 1),
                   (maps_at, 1), (maps_at + 1, 0), (nodes_at, len(pool)),
                   (nodes_at, pool.index('bad_tag')), (nodes_at + 1, 7),
                   (nodes_at + 2, map_count), (nodes_at + 3, 1),
                   (entries_at, len(pool)), (entries_at + 4, len(pool)),
                   (entries_at + 5, len(pool)), (entries_at + 5, pool.index(BLOCK)),
                   (entries_at + 5, pool.index(INHERIT)), (keys_at, 7)]
        # A valid pool reference with the wrong normalization is still invalid.
        changes.append((entries_at, pool.index('WRONG')))
        for offset, value in changes:
            changed = words[:]
            changed[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)
        for bad in ((count + 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(bad, pool, tags)
        # Duplicate canonical rows, nodes and inventory keys must be rejected.
        for start, width in ((rows_at + 7, 7), (nodes_at + wire.NODE_WIDTH, wire.NODE_WIDTH),
                             (keys_at + 2, 2)):
            changed = words[:]
            changed[start:start + width] = words[start - width:start]
            with self.subTest(start=start), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)


if __name__ == '__main__':
    unittest.main()
