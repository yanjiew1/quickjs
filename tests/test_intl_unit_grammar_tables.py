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

"""Division-grammar wire round trips, hidden origins and malformed records."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import unit_grammar
import unit_grammar_tables as wire
from test_intl_unit_grammar import Sources, graph


def fixture():
    return unit_grammar.collect(Sources('''<grammaticalDerivations locales="de">
      <deriveComponent feature="case" structure="per" value0="compound" value1="accusative"/>
      </grammaticalDerivations><grammaticalDerivations locales="fr">
      <deriveComponent feature="plural" structure="per" value0="compound" value1="one"/>
      </grammaticalDerivations>'''), graph())


class UnitGrammarTableTests(unittest.TestCase):
    def setUp(self):
        self.data = fixture()
        self.tags = graph().tags
        self.pool = tuple(sorted(wire.strings(self.data) |
                                 {'', '0', 'En', 'en-1901-1901', 'bad_tag', '↑↑↑'}))
        self.ids = {text: i for i, text in enumerate(self.pool)}
        self.section = wire.encode(self.data, self.tags, self.ids)

    def test_roundtrip_shared_rows_and_external_origin_projection(self):
        value = wire.decode(self.section, self.pool, self.tags)
        self.assertEqual(value['rows'], self.data['rows'])
        self.assertEqual(wire.encode(value, self.tags, self.ids), self.section)
        self.assertIsNone(wire.decode(None, self.pool, self.tags))
        tags = ('de-AT', 'root')
        selected = wire.select(self.data, tags)
        self.assertNotIn('de', tags)
        self.assertIn('de', wire.strings(selected))
        decoded = wire.decode(wire.encode(selected, tags, self.ids), self.pool, tags)
        self.assertEqual(decoded['rows'], selected['rows'])
        words = struct.unpack('<{}I'.format(len(self.section[1]) // 4), self.section[1])
        self.assertLess(words[1], len(self.tags))

    def test_invalid_source_coverage_and_tokens(self):
        invalid = (
            dict(self.data, features=('case', 'plural')),
            dict(self.data, rows=self.data['rows'][1:]),
            dict(self.data, rows=self.data['rows'] + self.data['rows'][:1]),
        )
        for data in invalid:
            with self.subTest(data=data), self.assertRaises(ValueError):
                wire.encode(data, self.tags, self.ids)
        for column, value in ((2, 'En'), (2, 'en-1901-1901'), (3, '0'), (4, '↑↑↑')):
            row = list(self.data['rows'][0])
            row[column] = value
            data = dict(self.data, rows=(tuple(row),) + self.data['rows'][1:])
            with self.subTest(value=value), self.assertRaises(ValueError):
                wire.encode(data, self.tags, self.ids)

    def test_bad_wire_extent_dimensions_references_and_order(self):
        count, raw = self.section
        original = list(struct.unpack('<{}I'.format(len(raw) // 4), raw))
        start = wire.HEADER + count
        changes = [(0, 0), (0, 2), (1, 0), (1, count + 1), (wire.HEADER, original[1]),
                   (start, len(self.pool)), (start, self.ids['En']),
                   (start, self.ids['en-1901-1901']), (start + 1, self.ids['0']),
                   (start + 2, self.ids['↑↑↑']), (start + 4, self.ids['']),
                   (start + 5, self.ids['bad_tag'])]
        for position, value in changes:
            words = original[:]
            words[position] = value
            section = count, struct.pack('<{}I'.format(len(words)), *words)
            with self.subTest(position=position, value=value), self.assertRaises(ValueError):
                wire.decode(section, self.pool, self.tags)
        for section in ((count - 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.subTest(section=section), self.assertRaises(ValueError):
                wire.decode(section, self.pool, self.tags)
        for duplicate in (False, True):
            words = original[:]
            first, second = words[start:start + 6], words[start + 6:start + 12]
            words[start:start + 12] = first + first if duplicate else second + first
            with self.subTest(duplicate=duplicate), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<{}I'.format(len(words)), *words)), self.pool, self.tags)
        words = original[:]
        words[wire.HEADER:wire.HEADER + count] = [0] * count
        with self.assertRaises(ValueError):
            wire.decode((count, struct.pack('<{}I'.format(len(words)), *words)), self.pool, self.tags)


if __name__ == '__main__':
    unittest.main()
