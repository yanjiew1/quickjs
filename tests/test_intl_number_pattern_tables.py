#!/usr/bin/env python3
"""Compiled number pattern sharing, typed tokens and corrupted wire records."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import number_patterns
import number_pattern_syntax as syntax
import number_pattern_tables as wire
import numbering
from resolve import BLOCK
from test_intl_list_patterns import graph
from test_intl_number_patterns import Sources


class NumberPatternTableTests(unittest.TestCase):
    def fixture(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><minimumGroupingDigits>2</minimumGroupingDigits>
          <currencyFormats numberSystem="latn"><currencyFormatLength><currencyFormat>
            <pattern>¤'#{}' 0;¤'#{}' 0-</pattern>
            <pattern alt="alphaNextToNumber">''' + BLOCK + '''</pattern>
            <pattern alt="noCurrency"/>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        data = number_patterns.collect(source, graph())
        definitions = numbering.collect(source, graph())
        pool = tuple(sorted(wire.strings(data) | {''}, key=lambda s: s.encode('utf-8')))
        ids = {text: index for index, text in enumerate(pool)}
        return data, definitions, pool, ids

    def test_roundtrip_tokens_grouping_and_distinct_markers(self):
        data, definitions, pool, ids = self.fixture()
        section = wire.encode(data, graph().tags, definitions, ids)
        result = wire.decode(section, pool, graph().tags, definitions)
        expected = dict(data, rows=tuple((locale, system, tuple(
            value if value in (None, BLOCK, '') else syntax.compile_pattern(value)
            for value in values)) for locale, system, values in data['rows']))
        self.assertEqual(result, {key: expected[key] for key in ('systems', 'fields', 'rows', 'grouping')})
        fields = {(locale, system): values for locale, system, values in result['rows']}
        self.assertEqual(fields[('en', 'latn')][5:], (BLOCK, BLOCK, '', ''))
        self.assertEqual(fields[('root', 'latn')][5:], (None,) * 4)
        self.assertEqual(fields[('en', 'latn')], fields[('en', 'mathbold')])
        header = struct.unpack_from('<7I', section[1])
        self.assertEqual(header[:4], (wire.VERSION, 2, 2, 2))
        self.assertLess(header[4], len(data['rows']) * wire.WIDTH)
        self.assertIsNone(wire.decode(None, pool, graph().tags, definitions))
        with self.assertRaises(ValueError):
            wire.decode(section, pool, graph().tags, None)

    def test_profile_and_invalid_encode_inputs(self):
        data, definitions, pool, ids = self.fixture()
        tags = ('en', 'root')
        selected = dict(data, rows=tuple(row for row in data['rows'] if row[0] in tags),
                        grouping=tuple(row for row in data['grouping'] if row[0] in tags))
        self.assertEqual(wire.decode(wire.encode(selected, tags, definitions, ids), pool,
                                     tags, definitions)['grouping'], selected['grouping'])
        for changed in (dict(data, fields=()), dict(data, systems=('latn',)),
                        dict(data, rows=data['rows'][:-1]),
                        dict(data, rows=data['rows'] + data['rows'][:1]),
                        dict(data, grouping=data['grouping'][:-1]),
                        dict(data, grouping=data['grouping'] + data['grouping'][:1]),
                        dict(data, grouping=tuple((tag, 0) for tag, value in data['grouping'])),
                        dict(data, rows=tuple((tag, system, ('',) + values[1:])
                                             for tag, system, values in data['rows']))):
            with self.assertRaises(ValueError):
                wire.encode(changed, graph().tags, definitions, ids)

    def test_corrupt_dimensions_references_and_tokens(self):
        data, definitions, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, definitions, ids)
        version, systems, vectors, rows, patterns, affixes, tokens = struct.unpack_from('<7I', raw)
        maps = wire.HEADER + systems
        row_start = maps + 2 * count
        vector_start = row_start + rows * systems
        pattern_start = vector_start + vectors * wire.WIDTH
        affix_start = pattern_start + patterns * wire.PATTERN_WIDTH
        token_start = affix_start + 2 * affixes
        changes = ((0, version + 1), (1, 0), (2, 0), (3, count + 1), (4, 0), (5, 0), (6, 0),
                   (wire.HEADER, 2), (maps, 0), (maps + 1, rows), (row_start, vectors),
                   (vector_start, patterns), (vector_start, wire.EMPTY),
                   (vector_start + 5, wire.EMPTY - 1),
                   (pattern_start + 3, 0), (pattern_start + 2, 0xffffffff),
                   (pattern_start + 4, 0xffffffff), (pattern_start + 6, 2),
                   (pattern_start + 8, 2), (pattern_start + wire.NUMERIC_WIDTH, affixes),
                   (affix_start, 1), (affix_start + 1, 0),
                   (token_start, len(syntax.TOKENS)), (token_start + 1, len(pool)))
        for at, value in changes:
            changed = bytearray(raw)
            struct.pack_into('<I', changed, at * 4, value)
            with self.assertRaises(ValueError, msg=str((at, value))):
                wire.decode((count, bytes(changed)), pool, graph().tags, definitions)
        for at, width in ((row_start, systems), (vector_start, wire.WIDTH),
                          (pattern_start, wire.PATTERN_WIDTH)):
            changed = bytearray(raw)
            changed[(at + width) * 4:(at + 2 * width) * 4] = changed[at * 4:(at + width) * 4]
            with self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags, definitions)
        for section in ((0, raw), (count, raw[:-1]), (count, raw + bytes(4)),
                        (count, bytes(wire.HEADER * 4 - 4))):
            with self.assertRaises(ValueError):
                wire.decode(section, pool, graph().tags, definitions)


if __name__ == '__main__':
    unittest.main()
