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

    def overlay_semantics(self, data):
        result = {}
        for row in data['overlay_rows']:
            head, nodes = row[-1], []
            while head:
                node = data['overlay_nodes'][head - 1]
                nodes.append(node[:-2] + (data['overlay_maps'][node[-2]],))
                head = node[-1]
            result[row[:-1]] = tuple(nodes)
        return result

    def test_roundtrip_states_and_projection(self):
        data, pool, ids = self.fixture()
        result = wire.decode(wire.encode(data, graph().tags, ids), pool, graph().tags)
        keys = ('widths', 'groups', 'fields', 'counts', 'maps', 'rows')
        self.assertEqual({key: result[key] for key in keys}, {key: data[key] for key in keys})
        self.assertEqual(self.overlay_semantics(result), self.overlay_semantics(data))
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

    def test_overlay_projection_retains_hidden_maps_and_external_origins(self):
        source = Sources()
        source.set('en', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="other">HIDDEN_PARENT {0}</unitPattern></unit></unitLength>''')
        source.set('en_GB', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="other">CHILD_OTHER {0}</unitPattern></unit></unitLength>''')
        source.set('de', '''<unitLength type="narrow"><alias source="en_GB"
          path="../unitLength[@type='long']"/></unitLength>''')
        data = unit_patterns.collect(source, graph())
        tags = ('de', 'root')
        selected = wire.select(data, tags)
        self.assertNotIn('HIDDEN_PARENT {0}', {entry[-1] for entries in selected['maps'] for entry in entries})
        pool = tuple(sorted(wire.strings(selected), key=lambda text: text.encode('utf-8')))
        self.assertIn('HIDDEN_PARENT {0}', pool)
        self.assertIn('en-GB', pool)
        ids = {text: i for i, text in enumerate(pool)}
        encoded = wire.encode(selected, tags, ids)
        result = wire.decode(encoded, pool, tags)
        self.assertEqual(self.semantics(result), self.semantics(selected))
        self.assertEqual(self.overlay_semantics(result), self.overlay_semantics(selected))
        layers = self.overlay_semantics(result)[('de', 'narrow', 'unit', 'length-meter')]
        self.assertEqual([layer[0] for layer in layers], ['en-GB', 'en', 'root'])
        self.assertEqual(wire.encode(result, tags, ids), encoded)

    def test_invalid_overlay_sources_coverage_and_ownership(self):
        data, pool, ids = self.fixture()
        node = data['overlay_nodes'][0]
        changes = [dict(data, overlay_rows=data['overlay_rows'][:-1]),
                   dict(data, overlay_rows=data['overlay_rows'] + data['overlay_rows'][:1]),
                   dict(data, overlay_rows=tuple(row[:-1] + (0,) for row in data['overlay_rows'])),
                   dict(data, overlay_maps=data['overlay_maps'] + ((),))]
        for bad in (('bad_TAG',) + node[1:], node[:-1] + (1,),
                    node[:-2] + (len(data['overlay_maps']), 0)):
            changes.append(dict(data, overlay_nodes=(bad,) + data['overlay_nodes'][1:]))
        changes.append(dict(data, overlay_nodes=data['overlay_nodes'] + (node,)))
        for changed in changes:
            with self.assertRaises(ValueError):
                wire.encode(changed, graph().tags, ids)

    def test_compiled_duplicate_nodes_are_rejected(self):
        data, pool, ids = self.fixture()
        head = next(head for *key, head in data['overlay_rows']
                    if head and sum(row[-1] == head for row in data['overlay_rows']) > 1)
        node = data['overlay_nodes'][head - 1]
        maps = data['overlay_maps'] + (data['overlay_maps'][node[-2]],)
        nodes = data['overlay_nodes'] + (node[:-2] + (len(maps) - 1, node[-1]),)
        rows = list(data['overlay_rows'])
        row = next(i for i, row in enumerate(rows) if row[-1] == head)
        rows[row] = rows[row][:-1] + (len(nodes),)
        with self.assertRaisesRegex(ValueError, 'duplicate compiled'):
            wire.encode(dict(data, overlay_maps=maps, overlay_nodes=nodes, overlay_rows=tuple(rows)),
                        graph().tags, ids)

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
        data = dict(data, maps=((),), rows=tuple(row[:-1] + (0,) for row in data['rows']),
                    overlay_maps=(), overlay_nodes=(),
                    overlay_rows=tuple(row[:-1] + (0,) for row in data['overlay_rows']))
        result = wire.decode(wire.encode(data, graph().tags, ids), pool, graph().tags)
        self.assertEqual(result['maps'], ((),))
        self.assertTrue(all(index == 0 for locale, width, kind, name, index in result['rows']))

    def test_malformed_dimensions_spans_order_and_selector_words(self):
        data, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, ids)
        version, groups, cases, rows, maps, entries, overlays, nodes = struct.unpack_from('<8I', raw)
        case_start = wire.HEADER + groups * 2
        locale_start = case_start + cases
        row_start = locale_start + count
        map_start = row_start + (rows + overlays) * groups * 3 + count + nodes * 4
        entry_start = map_start + maps * 2
        changes = [(i, 0) for i in range(8)] + [(i, 0xffffffff) for i in range(8)]
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
        version, groups, cases, rows, maps, entries, overlays, nodes = struct.unpack_from('<8I', raw)
        start = (wire.HEADER + groups * 2 + cases + count * 2 +
                 (rows + overlays) * groups * 3 + nodes * 4 + maps * 2)
        last = max(index for index in range(start + 1, start + entries * 2, 2)
                   if struct.unpack_from('<I', raw, index * 4)[0] < len(pool))
        changed = bytearray(raw)
        struct.pack_into('<I', changed, last * 4, len(pool))
        for marker in (BLOCK, INHERIT):
            with self.assertRaisesRegex(ValueError, 'invalid unit text reference'):
                wire.decode((count, bytes(changed)), pool + (marker,), graph().tags)

    def test_overlay_wire_corruption(self):
        data, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, ids)
        version, groups, cases, rows, maps, entries, overlays, nodes = struct.unpack_from('<8I', raw)
        locale_start = wire.HEADER + groups * 2 + cases + count + rows * groups * 3
        row_start = locale_start + count
        node_start = row_start + overlays * groups * 3
        changes = [(locale_start, overlays), (row_start, nodes + 1),
                   (node_start, len(pool)), (node_start + 1, groups * 3),
                   (node_start + 2, maps), (node_start + 3, 1)]
        for index, value in changes:
            changed = bytearray(raw)
            struct.pack_into('<I', changed, index * 4, value)
            with self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags)
        changed = bytearray(raw)
        changed[(node_start + 4) * 4:(node_start + 8) * 4] = changed[node_start * 4:(node_start + 4) * 4]
        with self.assertRaises(ValueError):
            wire.decode((count, bytes(changed)), pool, graph().tags)
        changed = bytearray(raw)
        for i in range(overlays * groups * 3):
            struct.pack_into('<I', changed, (row_start + i) * 4, 0)
        with self.assertRaises(ValueError):
            wire.decode((count, bytes(changed)), pool, graph().tags)


if __name__ == '__main__':
    unittest.main()
