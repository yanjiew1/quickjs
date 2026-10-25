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

"""Raw duration source wire roundtrips, profile projection and corruption."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from resolve import BLOCK
import bundle
import duration_patterns as source
import duration_tables as wire
import tables
from test_intl_duration_patterns import Sources, alias, pattern
from test_intl_list_patterns import graph


def fixture():
    sources = Sources()
    sources.set('root', pattern('hm', 'h.mm:ss'))
    sources.set('en', pattern('hms', "'ga' h:mm:ss") + pattern('ms', ''))
    sources.set('en_GB', pattern('hm', BLOCK))
    sources.set('de', alias('hms', 'hms', 'en'))
    data = source.collect(sources, graph())
    data = dict(data, rows=tuple(sorted(data['rows'])))
    tags = tuple(sorted(graph().tags))
    pool = tuple(sorted(wire.strings(data) | set(tags)))
    ids = {text: index for index, text in enumerate(pool)}
    return data, tags, pool, wire.encode(data, tags, ids)


class DurationTablesTests(unittest.TestCase):
    def test_roundtrip_and_generic_pack_validation(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(decoded['rows'], data['rows'])
        self.assertEqual(wire.encode(decoded, tags, {text: i for i, text in enumerate(pool)}), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(),
                                                                  {text: i for i, text in enumerate(pool)}),
                              bundle.DURATION_PATTERNS: section}, bytes(32), 'native')
        # Locale tags are shared-pool owners, not necessarily pattern origins.
        self.assertEqual(tables.validate(binary)['duration_patterns']['rows'], data['rows'])
        sections = bundle.unpack(binary)['sections']
        sections[bundle.DURATION_PATTERNS] = (section[0], bytes(4) + section[1][4:])
        with self.assertRaises(ValueError):
            tables.validate(bundle.pack(sections, bytes(32), 'native'))

    def test_projection_retains_external_source_identity_and_states(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        rows = {(tag, name): (state, text, origin, key)
                for tag, name, state, text, origin, key in decoded['rows']}
        self.assertEqual(rows[('de', 'hms')], ('value', "'ga' h:mm:ss", 'en', 'hms'))
        self.assertEqual(rows[('root', 'ms')], ('missing', '', '', ''))
        self.assertEqual(rows[('de', 'ms')], ('missing', '', '', ''))
        self.assertIn('en', pool)
        self.assertNotIn('en', tags)
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_corruption_and_bad_source_records(self):
        data, tags, pool, section = fixture()
        count, raw = section
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        row_count, record_count = words[1:3]
        rows_at, records_at = 3 + count, 3 + count + row_count * 3
        changes = [(0, 2), (1, 0), (2, 0), (3, row_count),
                   (rows_at, record_count), (records_at, 3),
                   (records_at + 1, len(pool)), (records_at + 2, len(pool)),
                   (records_at + 3, 3)]
        missing = next(i for i in range(record_count) if words[records_at + i * 4] == 0)
        changes.append((records_at + missing * 4 + 3, 1))
        for offset, value in changes:
            changed = words[:]
            changed[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)
        for bad_section in ((count + 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(bad_section, pool, tags)
        for start, width in ((3, count), (rows_at + 3, 3), (records_at + 4, 4)):
            changed = words[:]
            changed[start:start + width] = ([0] * count if start == 3 else
                                          words[start - width:start])
            with self.subTest(start=start), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)
        for record in (('unknown', '', '', ''), ('missing', 'x', '', ''),
                       ('blocked', 'x', 'en', 'hm'), ('value', 'x', '', 'hm'),
                       ('value', BLOCK, 'en', 'hm'), ('value', '', 'en_US', 'hm'),
                       ('value', '', 'en', 'unknown')):
            with self.subTest(record=record), self.assertRaises(ValueError):
                wire.valid(*record)
        with self.assertRaises(ValueError):
            wire.encode(dict(data, rows=data['rows'][:-1]), tags, {s: i for i, s in enumerate(pool)})


if __name__ == '__main__':
    unittest.main()
