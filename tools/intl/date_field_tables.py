"""Canonical shared date-field label records in QJIN section 25."""
import re
import struct

import bundle
import date_field_names as source
from parents import locale_id
from resolve import BLOCK, INHERIT

VERSION, HEADER, RECORD_WIDTH = 1, 4, 6
STATES = ('missing', 'value', 'blocked')
ALTERNATE = re.compile(r'[A-Za-z0-9]+(?:[-_ ][A-Za-z0-9]+)*')


def select(data, tags):
    selected = set(tags)
    return dict(data, rows=tuple(row for row in data['rows'] if row[0] in selected))


def strings(data):
    return {''} | set(data['alternates']) | {value for row in data['rows'] for value in row[5:7]}


def valid_alternates(alternates):
    if (not isinstance(alternates, tuple) or not alternates or alternates[0] != '' or
            any(not isinstance(alt, str) or alt and ALTERNATE.fullmatch(alt) is None
                for alt in alternates) or alternates != tuple(sorted(set(alternates)))):
        raise ValueError('invalid date-field alternate inventory')


def valid(state, text, origin, field, width, alt, alternates):
    if (state not in STATES or
            not all(isinstance(value, str) for value in (text, origin, field, width, alt))):
        raise ValueError('invalid date-field source record')
    if state == 'missing':
        if text or origin or field or width or alt:
            raise ValueError('noncanonical missing date-field record')
    elif (not origin or locale_id(origin) != origin or field not in source.FIELDS or
          width not in source.WIDTHS or alt not in alternates or
          text in (BLOCK, INHERIT) or '\0' in text or state == 'blocked' and text):
        raise ValueError('invalid date-field source state/provenance')


def encode(data, tags, ids):
    alternates = data['alternates']
    valid_alternates(alternates)
    values = {row[:4]: row[4:] for row in data['rows']}
    if (data['fields'] != source.FIELDS or data['widths'] != source.WIDTHS or
            not tags or len(set(tags)) != len(tags) or ids.get('') != 0 or
            len(values) != len(data['rows']) or
            set(values) != {(tag, field, width, alt) for tag in tags for field in source.FIELDS
                            for width in source.WIDTHS for alt in alternates}):
        raise ValueError('invalid date-field source coverage')
    compiled = {}
    for identity, record in values.items():
        state, text, origin, field, width, alt = record
        valid(*record, alternates)
        compiled[identity] = ((0,) * RECORD_WIDTH if state == 'missing' else
                              (STATES.index(state), ids[text], ids[origin],
                               source.FIELDS.index(field), source.WIDTHS.index(width),
                               alternates.index(alt)))
    records = tuple(sorted(set(compiled.values())))
    record_ids = {record: index for index, record in enumerate(records)}
    locale_rows = tuple(tuple(record_ids[compiled[tag, field, width, alt]]
                              for field in source.FIELDS for width in source.WIDTHS
                              for alt in alternates) for tag in tags)
    rows = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(rows)}
    words = [VERSION, len(alternates), len(rows), len(records)]
    words.extend(ids[alt] for alt in alternates)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in rows for value in row)
    words.extend(value for record in records for value in record)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid date-field locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, alternate_count, row_count, record_count = words[:HEADER]
    row_width = len(source.FIELDS) * len(source.WIDTHS) * alternate_count
    expected = HEADER + alternate_count + count + row_count * row_width + record_count * RECORD_WIDTH
    if (version != VERSION or not alternate_count or not 1 <= row_count <= count or
            not 1 <= record_count <= row_count * row_width or len(words) != expected or
            not pool or pool[0] != ''):
        raise ValueError('invalid date-field version/dimensions')
    at = HEADER
    refs = words[at:at + alternate_count]
    if any(index >= len(pool) for index in refs):
        raise ValueError('invalid date-field alternate reference')
    alternates = tuple(pool[index] for index in refs)
    valid_alternates(alternates)
    at += alternate_count
    locale_rows = words[at:at + count]
    if set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid/unused date-field locale row')
    at += count
    rows = tuple(words[start:start + row_width]
                 for start in range(at, at + row_count * row_width, row_width))
    if (rows != tuple(sorted(set(rows))) or
            {index for row in rows for index in row} != set(range(record_count))):
        raise ValueError('invalid/noncanonical date-field rows')
    at += row_count * row_width
    records, previous = [], None
    for start in range(at, len(words), RECORD_WIDTH):
        record = words[start:start + RECORD_WIDTH]
        state, text, origin, field, width, alt = record
        if (state >= len(STATES) or text >= len(pool) or origin >= len(pool) or
                field >= len(source.FIELDS) or width >= len(source.WIDTHS) or
                alt >= alternate_count or previous is not None and record <= previous):
            raise ValueError('invalid/noncanonical date-field record')
        if state == 0 and record != (0,) * RECORD_WIDTH:
            raise ValueError('noncanonical missing date-field record')
        decoded = ((STATES[0],) + ('',) * 5 if state == 0 else
                   (STATES[state], pool[text], pool[origin], source.FIELDS[field],
                    source.WIDTHS[width], alternates[alt]))
        valid(*decoded, alternates)
        records.append(decoded)
        previous = record
    selectors = tuple((field, width, alt) for field in source.FIELDS for width in source.WIDTHS
                      for alt in alternates)
    return {'fields': source.FIELDS, 'widths': source.WIDTHS, 'alternates': alternates,
            'rows': tuple((tag,) + key + records[rows[index][i]]
                          for tag, index in zip(tags, locale_rows) for i, key in enumerate(selectors))}
