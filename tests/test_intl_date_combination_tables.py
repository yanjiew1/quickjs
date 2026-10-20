#!/usr/bin/env python3
"""Date/time connector wire sharing, projection, provenance and corrupt input."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import date_combinations as source
import date_combination_tables as wire
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_combinations import Sources, calendar, connector, group_alias
from test_intl_list_patterns import graph


def fixture():
    sources = Sources()
    sources.set('root', calendar('gregorian', '''<dateTimeFormats>
        <dateTimeFormatLength type="long"><dateTimeFormat>
        <pattern numbers="y=hebr">{1}, {0}</pattern></dateTimeFormat>
        <dateTimeFormat type="atTime"><pattern>''' + BLOCK + '''</pattern></dateTimeFormat>
        </dateTimeFormatLength></dateTimeFormats>''') +
        calendar('generic', group_alias('gregorian')) +
        calendar('japanese', '<alias source="locale" path="../calendar[@type=\'generic\']"/>'))
    sources.set('en', calendar('gregorian', connector('''<pattern/>
        <pattern alt="variant" numbers="latn">{1} 'at' {0}</pattern>''')))
    sources.set('en_GB', calendar('gregorian', connector('<pattern>{0} – {1}</pattern>')))
    sources.set('de', calendar('gregorian', group_alias('gregorian', source='en')))
    data = source.collect(sources, graph())
    tags = graph().tags
    pool = tuple(sorted(wire.strings(data) | set(tags) | {BLOCK, INHERIT, 'bad!', 'en_US'},
                        key=lambda value: value.encode('utf-8')))
    return data, tags, pool, wire.encode(data, tags, {text: i for i, text in enumerate(pool)})


class DateCombinationTablesTests(unittest.TestCase):
    def test_roundtrip_determinism_and_generic_pack_validation(self):
        data, tags, pool, section = fixture()
        decoded = wire.decode(section, pool, tags)
        self.assertEqual(decoded['rows'], data['rows'])
        ids = {text: index for index, text in enumerate(pool)}
        self.assertEqual(wire.encode(decoded, tags, ids), section)
        self.assertEqual(wire.encode(dict(data, rows=tuple(reversed(data['rows']))), tags, ids), section)
        binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                              bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                              bundle.DATE_COMBINATIONS: section}, bytes(32), 'native')
        self.assertEqual(tables.validate(binary)['date_combinations']['rows'], data['rows'])
        parts = bundle.unpack(binary)['sections']
        parts[bundle.DATE_COMBINATIONS] = section[0], bytes(4) + section[1][4:]
        with self.assertRaises(ValueError):
            tables.validate(bundle.pack(parts, bytes(32), 'native'))

    def test_identical_rows_and_aliased_records_are_shared(self):
        sources = Sources()
        sources.set('root', calendar('gregorian', connector('<pattern>{1} {0}</pattern>')) +
                    calendar('japanese', group_alias('gregorian')))
        data = source.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data)))
        section = wire.encode(data, graph().tags, {s: i for i, s in enumerate(pool)})
        self.assertEqual(struct.unpack('<5I', section[1][:wire.HEADER * 4]), (1, 2, 1, 1, 2))
        self.assertEqual(wire.decode(section, pool, graph().tags)['rows'], data['rows'])

    def test_projection_preserves_external_source_overrides_and_states(self):
        data, tags, pool, section = fixture()
        tags = ('de', 'root')
        projected = wire.select(data, tags)
        pool = tuple(sorted(wire.strings(projected)))
        decoded = wire.decode(wire.encode(projected, tags, {s: i for i, s in enumerate(pool)}), pool, tags)
        rows = {row[:5]: row[5:] for row in decoded['rows']}
        self.assertEqual(rows['de', 'gregorian', 'long', 'standard', 'variant'],
                         ('value', "{1} 'at' {0}", 'latn', 'en', 'gregorian', 'long', 'standard', 'variant'))
        self.assertIn('en', pool)
        self.assertNotIn('en', tags)
        self.assertEqual(rows['root', 'gregorian', 'long', 'standard', ''][1:3], ('{1}, {0}', 'y=hebr'))
        self.assertEqual(rows['de', 'gregorian', 'long', 'standard', ''][:3], ('value', '', ''))
        self.assertEqual(rows['root', 'japanese', 'long', 'atTime', ''][0], 'blocked')
        self.assertEqual(rows['root', 'gregorian', 'full', 'relative', ''], ('missing',) + ('',) * 7)
        self.assertIsNone(wire.decode(None, pool, tags))

    def test_corrupt_dimensions_indices_states_and_canonical_sharing(self):
        data, tags, pool, section = fixture()
        count, raw = section
        words = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
        _, calendars, alternates, rows, records = words[:wire.HEADER]
        row_width = calendars * 4 * 3 * alternates
        locale_at = wire.HEADER + calendars + alternates
        rows_at = locale_at + count
        records_at = rows_at + rows * row_width
        blocked_at = next(i for i in range(records_at, len(words), wire.RECORD_WIDTH) if words[i] == 2)
        changes = [(0, 2), (1, 0), (2, 0), (3, 0), (4, 0),
                   (wire.HEADER, len(pool)), (wire.HEADER, pool.index('bad!')),
                   (wire.HEADER + 1, words[wire.HEADER]),
                   (wire.HEADER + calendars, pool.index('variant')),
                   (locale_at, rows), (rows_at, records), (records_at, 3),
                   (records_at + 1, len(pool)), (records_at + 2, len(pool)),
                   (records_at + 3, len(pool)), (records_at + 4, calendars),
                   (records_at + 5, 4), (records_at + 6, 3), (records_at + 7, alternates),
                   (records_at + 2, pool.index('latn')), (blocked_at + 2, pool.index('latn'))]
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
            changed[start:start + width] = ([0] * count if width == count else words[start - width:start])
            with self.subTest(start=start), self.assertRaises(ValueError):
                wire.decode((count, struct.pack('<' + 'I' * len(changed), *changed)), pool, tags)

    def test_invalid_source_inventories_coverage_and_provenance(self):
        data, tags, pool, section = fixture()
        for calendars, alternates in (((), ('',)), (('gregorian', 'gregorian'), ('',)),
                                      (('Bad',), ('',)), (('gregorian',), ()),
                                      (('gregorian',), ('variant',)), (('gregorian',), ('', 'bad!'))):
            with self.subTest(calendars=calendars, alternates=alternates), self.assertRaises(ValueError):
                wire.valid_inventories(calendars, alternates)
        record = ('value', '{1} {0}', '', 'en', 'gregorian', 'long', 'standard', '')
        for index, value in ((0, 'unknown'), (1, BLOCK), (1, INHERIT), (1, '\0'),
                             (1, '\ud800'), (2, '\0'), (2, '\udfff'), (3, 'en_US'),
                             (4, 'unknown'), (5, 'unknown'), (6, 'unknown'), (7, 'unknown')):
            changed = list(record)
            changed[index] = value
            with self.subTest(index=index, value=repr(value)), self.assertRaises(ValueError):
                wire.valid(*changed, data['calendars'], data['alternates'])
        for invalid in (('missing', '{1}{0}') + ('',) * 6,
                        ('blocked', '{1}{0}', '') + record[3:],
                        ('blocked', '', 'latn') + record[3:]):
            with self.assertRaises(ValueError):
                wire.valid(*invalid, data['calendars'], data['alternates'])
        ids = {text: i for i, text in enumerate(pool)}
        for rows in (data['rows'][:-1], data['rows'] + data['rows'][:1], (data['rows'][0][:-1],),
                     (data['rows'][0],) + data['rows'][:-1],
                     (('unknown',) + data['rows'][0][1:],) + data['rows'][1:]):
            with self.assertRaises(ValueError):
                wire.encode(dict(data, rows=rows), tags, ids)
        for invalid in ((), ('en', 'en'), ('root', 'en'), ('en_US',)):
            with self.assertRaises(ValueError):
                wire.encode(data, invalid, ids)


if __name__ == '__main__':
    unittest.main()
