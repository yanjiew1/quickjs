#!/usr/bin/env python3
"""Numeric symbol wire sharing, exact text and malformed table regressions."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import number_symbols
import number_symbol_tables as wire
import numbering
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph
from test_intl_number_symbols import Sources


class SymbolTableTests(unittest.TestCase):
    def fixture(self):
        source = Sources()
        source.set('en', '<ldml><numbers><symbols numberSystem="latn">'
                   '<decimal>,</decimal><plusSign/><minusSign>\u061c-</minusSign>'
                   '<currencyDecimal>;</currencyDecimal><currencyGroup>' + BLOCK +
                   '</currencyGroup></symbols></numbers></ldml>')
        data = number_symbols.collect(source, graph())
        definitions = numbering.collect(source, graph())
        pool = tuple(sorted(wire.strings(data) | {'', BLOCK, INHERIT}, key=lambda s: s.encode('utf-8')))
        ids = {value: index for index, value in enumerate(pool)}
        return data, definitions, pool, ids

    def decode(self, section, pool, definitions):
        return wire.decode(section, pool, graph().tags, definitions)

    def test_roundtrip_shared_rows_and_vectors(self):
        data, definitions, pool, ids = self.fixture()
        section = wire.encode(data, graph().tags, definitions, ids)
        self.assertEqual(self.decode(section, pool, definitions),
                         {key: data[key] for key in ('systems', 'fields', 'rows')})
        # Two identical numbering maps per locale, two distinct locale maps.
        self.assertEqual(struct.unpack_from('<3I', section[1]), (2, 2, 2))
        self.assertLess(len(section[1]), len(data['rows']) * wire.WIDTH * 4)
        self.assertIsNone(self.decode(None, pool, definitions))
        with self.assertRaises(ValueError):
            self.decode(section, pool, None)

    def test_profile_selection_preserves_semantics(self):
        data, definitions, pool, ids = self.fixture()
        tags = ('en', 'root')
        selected = dict(data, rows=tuple(row for row in data['rows'] if row[0] in tags))
        result = wire.decode(wire.encode(selected, tags, definitions, ids), pool, tags, definitions)
        self.assertEqual(result['rows'], selected['rows'])
        for changed in (dict(data, systems=('latn',)),
                        dict(data, rows=data['rows'][:-1]),
                        dict(data, rows=data['rows'] + data['rows'][:1]),
                        dict(data, rows=((data['rows'][0][0], 'latn', ('.',)),) + data['rows'][1:])):
            with self.assertRaises(ValueError):
                wire.encode(changed, graph().tags, definitions, ids)

    def test_malformed_dimensions_indices_order_and_text(self):
        data, definitions, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, definitions, ids)
        systems, vectors, rows = struct.unpack_from('<3I', raw)
        maps = 3 + systems
        row_start = maps + count
        vector_start = row_start + rows * systems
        changes = ((0, 0), (0, systems + 1), (1, 0), (1, 0xffffffff),
                   (2, 0), (2, count + 1), (3, 2), (4, 0),
                   (maps, rows), (row_start, vectors),
                   (vector_start, len(pool)), (vector_start, wire.ABSENT),
                   (vector_start, wire.BLOCKED), (vector_start, ids[BLOCK]),
                   (vector_start, ids[INHERIT]),
                   (vector_start + len(number_symbols.CORE_FIELDS), ids[INHERIT]))
        for at, value in changes:
            changed = bytearray(raw)
            struct.pack_into('<I', changed, at * 4, value)
            with self.assertRaises(ValueError, msg=str((at, value))):
                self.decode((count, bytes(changed)), pool, definitions)
        for at, width in ((row_start, systems), (vector_start, wire.WIDTH)):
            changed = bytearray(raw)
            changed[(at + width) * 4:(at + 2 * width) * 4] = changed[at * 4:(at + width) * 4]
            with self.assertRaises(ValueError):
                self.decode((count, bytes(changed)), pool, definitions)
        for section in ((0, raw), (count + 1, raw), (count, raw[:-1]),
                        (count, raw + bytes(4)), (count, bytes(8))):
            with self.assertRaises(ValueError):
                self.decode(section, pool, definitions)


if __name__ == '__main__':
    unittest.main()
