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

"""Date/time style wire sharing, projection, provenance and corrupt input."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import date_patterns as source
import date_style_tables as wire
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_patterns import Sources, alias, calendar, style
from test_intl_list_patterns import graph


def fixture():
    sources = Sources()
    sources.set('root', calendar('gregorian', style('''<pattern numbers="y=hebr">y-MM-dd</pattern>
                <datetimeSkeleton numbers="latn">yMMdd</datetimeSkeleton>''') +
                style('<pattern>' + BLOCK + '</pattern>', family='time')) +
                calendar('generic', alias('date', 'gregorian') + alias('time', 'gregorian')) +
                calendar('japanese', '<alias source="locale" path="../calendar[@type=\'generic\']"/>'))
    sources.set('en', calendar('gregorian', style('''<pattern/>
                <pattern alt="variant" numbers="latn">d MMMM y</pattern>''')))
    sources.set('en_GB', calendar('gregorian', style('<pattern>dd/MM/y</pattern>')))
    sources.set('de', calendar('gregorian', alias('date', 'gregorian', 'en')))
    data = source.collect(sources, graph())
    tags = graph().tags
    pool = tuple(sorted(wire.strings(data) | set(tags) | {BLOCK, INHERIT, 'bad!', 'en_US'},
                        key=lambda value: value.encode('utf-8')))
    return data, tags, pool, wire.encode(data, tags, {text: i for i, text in enumerate(pool)})


class DateStyleTablesTests(unittest.TestCase):
    def test_roundtrip_and_generic_pack_validation(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(decoded['rows'], data['rows'])
        ids = {text: index for index, text in enumerate(pool)}
        self.assertEqual(wire.encode(decoded, tags, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                              bundle.DATE_STYLES: section}, bytes(32), 'native')
        self.assertEqual(tables.validate(binary)['date_styles']['rows'], data['rows'])
        parts = bundle.unpack(binary)['sections']
        parts[bundle.DATE_STYLES] = section[0], bytes(4) + section[1][4:]
        with self.assertRaises(ValueError):
            tables.validate(bundle.pack(parts, bytes(32), 'native'))

    def test_identical_inherited_rows_and_alias_records_are_shared(self):
        sources = Sources()
        sources.set('root', calendar('gregorian', style('<pattern>y</pattern>')) +
                    calendar('japanese', alias('date', 'gregorian')))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})
        version, calendars, alternates, rows, records = struct.unpack('<5I', section[1][:wire.HEADER * 4])
        self.assertEqual((version, calendars, alternates, rows, records), (1, 2, 1, 1, 2))
        self.assertEqual(wire.decode(section, pool, graph().tags)['rows'], data['rows'])

    def test_projection_preserves_external_origin_numbers_and_markers(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        rows = {row[:6]: row[6:] for row in decoded['rows']}
        self.assertEqual(rows['de', 'gregorian', 'date', 'long', 'pattern', 'variant'],
                         ('value', 'd MMMM y', 'latn', 'en', 'gregorian', 'date', 'long', 'pattern', 'variant'))
        self.assertIn('en', pool)
        self.assertNotIn('en', tags)
        self.assertEqual(rows['root', 'gregorian', 'date', 'long', 'pattern', ''][1:3], ('y-MM-dd', 'y=hebr'))
        self.assertEqual(rows['root', 'gregorian', 'date', 'long', 'skeleton', ''][1:3], ('yMMdd', 'latn'))
        self.assertEqual(rows['de', 'gregorian', 'date', 'long', 'pattern', ''][:3], ('value', '', ''))
        self.assertEqual(rows['root', 'japanese', 'time', 'long', 'pattern', ''][0], 'blocked')
        self.assertEqual(rows['root', 'gregorian', 'date', 'full', 'pattern', ''], ('missing',) + ('',) * 8)
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_corrupt_dimensions_references_and_missing_record(self):
        data, tags, pool, section = fixture()
        count, raw = section
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        _, calendars, alternates, rows, records = words[:wire.HEADER]
        row_width = calendars * 2 * 4 * 2 * alternates
        locale_at = wire.HEADER + calendars + alternates
        rows_at = locale_at + count
        records_at = rows_at + rows * row_width
        changes = [(0, 2), (1, 0), (2, 0), (3, 0), (4, 0),
                   (wire.HEADER, len(pool)), (wire.HEADER, pool.index('bad!')),
                   (wire.HEADER + 1, words[wire.HEADER]),
                   (wire.HEADER + calendars, pool.index('variant')),
                   (locale_at, rows), (rows_at, records), (records_at, 3),
                   (records_at + 1, len(pool)), (records_at + 2, len(pool)),
                   (records_at + 3, len(pool)), (records_at + 4, calendars),
                   (records_at + 5, 2), (records_at + 6, 4),
                   (records_at + 7, 2), (records_at + 8, alternates),
                   (records_at + 2, pool.index('latn'))]
        for offset, value in changes:
            changed = words[:]
            changed[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)
        for broken in ((count + 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(broken, pool, tags)
        for start, width in ((locale_at, count), (rows_at + row_width, row_width),
                             (records_at + wire.RECORD_WIDTH, wire.RECORD_WIDTH)):
            changed = words[:]
            changed[start:start + width] = ([0] * count if width == count else
                                          words[start - width:start])
            with self.subTest(start=start), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)

    def test_invalid_source_inventories_coverage_and_provenance(self):
        data, tags, pool, section = fixture()
        for calendars, alternates in (((), ('',)), (('gregorian', 'gregorian'), ('',)),
                                      (('Bad',), ('',)), (('gregorian',), ()),
                                      (('gregorian',), ('variant',)), (('gregorian',), ('', 'bad!'))):
            with self.subTest(calendars=calendars, alternates=alternates), self.assertRaises(ValueError):
                wire.valid_inventories(calendars, alternates)
        record = ('value', 'y', '', 'en', 'gregorian', 'date', 'long', 'pattern', '')
        for index, value in ((0, 'unknown'), (1, BLOCK), (1, INHERIT), (1, '\0'),
                             (1, '\ud800'), (2, '\0'), (2, '\udfff'), (3, 'en_US'),
                             (4, 'unknown'), (5, 'unknown'), (6, 'unknown'),
                             (7, 'unknown'), (8, 'unknown')):
            changed = list(record)
            changed[index] = value
            with self.subTest(index=index, value=repr(value)), self.assertRaises(ValueError):
                wire.valid(*changed, data['calendars'], data['alternates'])
        for invalid in (('missing', 'y') + ('',) * 7,
                        ('blocked', 'y', '') + record[3:],
                        ('blocked', '', 'latn') + record[3:]):
            with self.assertRaises(ValueError):
                wire.valid(*invalid, data['calendars'], data['alternates'])
        ids = {text: i for i, text in enumerate(pool)}
        for rows in (data['rows'][:-1], data['rows'] + data['rows'][:1], (data['rows'][0][:-1],)):
            with self.assertRaises(ValueError):
                wire.encode(dict(data, rows=rows), tags, ids)
        for rows in ((data['rows'][0],) + data['rows'][:-1],
                     (('unknown',) + data['rows'][0][1:],) + data['rows'][1:],
                     (data['rows'][0][:2] + ('unknown',) + data['rows'][0][3:],) + data['rows'][1:]):
            with self.assertRaises(ValueError):
                wire.encode(dict(data, rows=rows), tags, ids)
        for invalid in ((), ('en', 'en'), ('root', 'en'), ('en_US',)):
            with self.assertRaises(ValueError):
                wire.encode(data, invalid, ids)


if __name__ == '__main__':
    unittest.main()
