#!/usr/bin/env python3
"""Shared date-field wire roundtrips, projection, provenance and corruption."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import date_field_names as source
import date_field_tables as wire
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_field_names import Sources, alias, field
from test_intl_list_patterns import graph


def fixture():
    sources = Sources()
    sources.set('root', field('year', 'year') + alias('year-short', 'year'))
    sources.set('en', field('era', '') + field('month', BLOCK) + field('zone', 'time zone') +
                '<field type="dayperiod"><displayName alt="variant">am/pm</displayName></field>')
    sources.set('en_GB', field('year', 'British year'))
    sources.set('de', alias('zone-short', 'zone', 'en'))
    data = source.collect(sources, graph())
    tags = graph().tags
    pool = tuple(sorted(wire.strings(data) | set(tags)))
    ids = {text: index for index, text in enumerate(pool)}
    return data, tags, pool, wire.encode(data, tags, ids)


class DateFieldTablesTests(unittest.TestCase):
    def test_identical_inherited_locale_rows_share_one_encoded_row(self):
        sources = Sources()
        sources.set('root', field('year', 'year'))
        data = source.collect(sources, graph())
        tags = graph().tags
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, tags, {text: i for i, text in enumerate(pool)})
        header = struct.unpack('<4I', section[1][:wire.HEADER * 4])
        self.assertEqual(header[2], 1)
        self.assertEqual(wire.decode(section, pool, tags)['rows'], data['rows'])

    def test_roundtrip_and_generic_pack_validation(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(decoded['rows'], data['rows'])
        ids = {text: index for index, text in enumerate(pool)}
        self.assertEqual(wire.encode(decoded, tags, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                              bundle.DATE_FIELD_NAMES: section}, bytes(32), 'native')
        self.assertEqual(tables.validate(binary)['date_field_names']['rows'], data['rows'])
        sections = bundle.unpack(binary)['sections']
        sections[bundle.DATE_FIELD_NAMES] = section[0], bytes(4) + section[1][4:]
        with self.assertRaises(ValueError):
            tables.validate(bundle.pack(sections, bytes(32), 'native'))

    def test_projection_retains_external_origin_and_marker_states(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        rows = {row[:4]: row[4:] for row in decoded['rows']}
        self.assertEqual(rows['de', 'timeZoneName', 'short', ''],
                         ('value', 'time zone', 'en', 'timeZoneName', 'long', ''))
        self.assertIn('en', pool)
        self.assertNotIn('en', tags)
        self.assertEqual(rows['root', 'quarter', 'long', ''], ('missing', '', '', '', '', ''))
        original = {row[:4]: row[4:] for row in data['rows']}
        self.assertEqual(original['en', 'month', 'long', ''], ('blocked', '', 'en', 'month', 'long', ''))
        self.assertEqual(original['en', 'era', 'long', ''], ('value', '', 'en', 'era', 'long', ''))
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_corruption_dimensions_references_order_and_canonical_missing(self):
        data, tags, pool, section = fixture()
        count, raw = section
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        alternate_count, row_count, record_count = words[1:4]
        row_width = 12 * 3 * alternate_count
        rows_at = wire.HEADER + alternate_count + count
        records_at = rows_at + row_count * row_width
        changes = [(0, 2), (1, 0), (2, 0), (3, 0), (wire.HEADER, len(pool)),
                   (wire.HEADER, pool.index('variant')), (wire.HEADER + alternate_count, row_count),
                   (rows_at, record_count), (records_at, 3), (records_at + 1, len(pool)),
                   (records_at + 2, len(pool)), (records_at + 3, 12),
                   (records_at + 4, 3), (records_at + 5, alternate_count),
                   (records_at + 3, 1)]
        for offset, value in changes:
            changed = words[:]
            changed[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)
        for bad in ((count + 1, raw), (count, raw[:-1]), (count, raw + bytes(4))):
            with self.assertRaises(ValueError):
                wire.decode(bad, pool, tags)
        for start, width in ((wire.HEADER + alternate_count, count),
                             (rows_at + row_width, row_width),
                             (records_at + wire.RECORD_WIDTH, wire.RECORD_WIDTH)):
            changed = words[:]
            changed[start:start + width] = ([0] * count if width == count else
                                          words[start - width:start])
            with self.subTest(start=start), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)

    def test_invalid_source_inventory_and_records(self):
        data, tags, pool, section = fixture()
        for alternates in ((), ('variant',), ('', ''), ('', 'bad!'), ('', 1)):
            with self.subTest(alternates=alternates), self.assertRaises(ValueError):
                wire.valid_alternates(alternates)
        for record in (('unknown', '', '', '', '', ''), ('missing', 'x', '', '', '', ''),
                       ('blocked', 'x', 'en', 'year', 'long', ''),
                       ('value', BLOCK, 'en', 'year', 'long', ''),
                       ('value', INHERIT, 'en', 'year', 'long', ''),
                       ('value', '\0', 'en', 'year', 'long', ''),
                       ('value', 'x', 'en_US', 'year', 'long', ''),
                       ('value', 'x', 'en', 'bad', 'long', ''),
                       ('value', 'x', 'en', 'year', 'bad', ''),
                       ('value', 'x', 'en', 'year', 'long', 'bad')):
            with self.subTest(record=record), self.assertRaises(ValueError):
                wire.valid(*record, data['alternates'])
        ids = {text: index for index, text in enumerate(pool)}
        for rows in (data['rows'][:-1], data['rows'] + data['rows'][:1]):
            with self.assertRaises(ValueError):
                wire.encode(dict(data, rows=rows), tags, ids)


if __name__ == '__main__':
    unittest.main()
