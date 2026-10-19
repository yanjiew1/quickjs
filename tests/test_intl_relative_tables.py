#!/usr/bin/env python3
"""Relative-time shared maps, source provenance and malformed wire records."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import relative_patterns as source
import relative_tables as wire
from resolve import BLOCK, INHERIT
from test_intl_relative_patterns import Sources, field, pattern
from test_intl_list_patterns import graph


class RelativeTableTests(unittest.TestCase):
    def fixture(self):
        inputs = Sources()
        inputs.set('en', field('day', '<relative type="0">today {literal}</relative>' +
                   '<relative type="1">' + BLOCK + '</relative>' +
                   '<relative type="2"/>' + pattern('past', 'one', 'yesterday')))
        inputs.set('en_GB', field('day', pattern('future', 'one', 'GB {0}')))
        data = source.collect(inputs, graph())
        pool = tuple(sorted(wire.strings(data) | {BLOCK, INHERIT, 'bad_tag', 'en-us', 'invalid {1}'},
                            key=lambda text: text.encode('utf-8')))
        return data, pool, {text: i for i, text in enumerate(pool)}

    def semantics(self, data):
        return {row[:-1]: data['maps'][row[-1]] for row in data['rows']}

    def test_roundtrip_and_projection(self):
        data, pool, ids = self.fixture()
        encoded = wire.encode(data, graph().tags, ids)
        result = wire.decode(encoded, pool, graph().tags)
        keys = ('units', 'widths', 'counts', 'literal_keys', 'maps', 'rows')
        self.assertEqual({key: data[key] for key in keys}, result)
        self.assertEqual(wire.encode(result, graph().tags, ids), encoded)
        self.assertIsNone(wire.decode(None, pool, graph().tags))
        tags = ('en-GB', 'root')
        selected = wire.select(data, tags)
        result = wire.decode(wire.encode(selected, tags, ids), pool, tags)
        self.assertEqual(self.semantics(result), self.semantics(selected))
        value = self.semantics(result)['en-GB', 'day', 'narrow']
        self.assertIn(('literal', '0', False, 'today {literal}', 'en', 'day', 'long'), value)
        self.assertIn(('literal', '1', True, '', 'en', 'day', 'long'), value)
        self.assertIn(('literal', '2', False, '', 'en', 'day', 'long'), value)
        self.assertIn(('future', 'one', False, 'GB {0}', 'en-GB', 'day', 'long'), value)
        selected = wire.select(data, ('de', 'root'))
        self.assertEqual(selected['literal_keys'], ())
        wire.decode(wire.encode(selected, ('de', 'root'), ids), pool, ('de', 'root'))

    def test_invalid_source_inventory(self):
        data, pool, ids = self.fixture()
        changes = (dict(data, units=()), dict(data, widths=()), dict(data, counts=()),
                   dict(data, rows=data['rows'][:-1]),
                   dict(data, rows=data['rows'] + data['rows'][:1]),
                   dict(data, literal_keys=('0', '0')),
                   dict(data, literal_keys=('0', '1', '2', '3')),
                   dict(data, maps=data['maps'] + ((),)))
        for changed in changes:
            with self.assertRaises(ValueError):
                wire.encode(changed, graph().tags, ids)
        with self.assertRaises(ValueError):
            wire.select(dict(data, rows=(data['rows'][0][:-1] + (len(data['maps']),),)), ('de',))
        original = next(entry for entries in data['maps'] for entry in entries if entry[:2] == ('past', 'one'))
        bad_entries = (
            ('wrong',) + original[1:], original[:1] + ('wrong',) + original[2:],
            original[:2] + (1,) + original[3:], original[:3] + (INHERIT,) + original[4:],
            original[:3] + ('bad {1}',) + original[4:],
            original[:4] + ('bad_tag',) + original[5:],
            original[:5] + ('wrong',) + original[6:], original[:-1] + ('wide',),
            ('literal', '01') + original[2:],
        )
        index = next(i for i, entries in enumerate(data['maps']) if original in entries)
        for entry in bad_entries + (original,):
            maps = list(data['maps'])
            maps[index] += (entry,)
            with self.subTest(entry=entry), self.assertRaises(ValueError):
                wire.encode(dict(data, maps=tuple(maps)), graph().tags, ids)
        maps = list(data['maps'])
        index = next(index for tag, unit, width, index in data['rows'] if width == 'long')
        maps[index] = tuple(entry for entry in maps[index] if entry[:2] != ('past', 'other'))
        with self.assertRaisesRegex(ValueError, 'required'):
            wire.encode(dict(data, maps=tuple(maps)), graph().tags, ids)

    def test_malformed_wire(self):
        data, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, ids)
        version, keys, rows, maps, entries = struct.unpack_from('<5I', raw)
        locale_at = wire.HEADER + keys
        row_at = locale_at + count
        map_at = row_at + rows * wire.ROW_WIDTH
        entry_at = map_at + maps * 2
        changes = [(i, 0xffffffff) for i in range(wire.HEADER)]
        changes += [(0, 0), (2, 0), (3, 0), (4, 0),
                    (wire.HEADER, len(pool)), (wire.HEADER, ids['bad_tag']),
                    (locale_at, rows), (row_at, maps),
                    (map_at, 1), (map_at + 1, entries + 1),
                    (entry_at, 12 + keys), (entry_at + 1, len(pool)),
                    (entry_at + 1, ids[INHERIT]), (entry_at + 1, ids[BLOCK]),
                    (entry_at + 2, len(pool)), (entry_at + 2, ids['en-us']),
                    (entry_at + 3, wire.ROW_WIDTH)]
        # Find a counted entry before injecting a malformed numeric template.
        numeric = next(i for i in range(entries) if struct.unpack_from('<I', raw, (entry_at + i * 4) * 4)[0] < 12)
        changes.append((entry_at + numeric * 4 + 1, ids['invalid {1}']))
        for index, value in changes:
            changed = bytearray(raw)
            struct.pack_into('<I', changed, index * 4, value)
            with self.subTest(index=index, value=value), self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags)
        # Repeated selectors, keys and complete row/map encodings are rejected.
        for start, width in ((wire.HEADER, 1), (row_at, wire.ROW_WIDTH), (entry_at, 4)):
            changed = bytearray(raw)
            changed[(start + width) * 4:(start + width * 2) * 4] = changed[start * 4:(start + width) * 4]
            with self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags)
        for section in ((count - 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(section, pool, graph().tags)


if __name__ == '__main__':
    unittest.main()
