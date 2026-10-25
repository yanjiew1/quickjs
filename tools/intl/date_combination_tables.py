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

"""Canonical shared date/time source connectors in QJIN section 28."""
import re
import struct

import bundle
from calendar_sources import CALENDAR
import date_combinations as source
from parents import locale_id
from resolve import BLOCK, INHERIT

VERSION, HEADER, RECORD_WIDTH = 1, 5, 8
STATES = ('missing', 'value', 'blocked')
ALTERNATE = re.compile(r'[A-Za-z0-9]+(?:[-_ ][A-Za-z0-9]+)*')


def select(data, tags):
    selected = set(tags)
    return dict(data, rows=tuple(row for row in data['rows'] if row[0] in selected))


def strings(data):
    return ({''} | set(data['calendars']) | set(data['alternates']) |
            {value for row in data['rows'] for value in row[6:9]})


def valid_inventories(calendars, alternates):
    if (not isinstance(calendars, tuple) or not calendars or
            any(not isinstance(value, str) or not CALENDAR.fullmatch(value)
                for value in calendars) or calendars != tuple(sorted(set(calendars)))):
        raise ValueError('invalid date-combination calendar inventory')
    if (not isinstance(alternates, tuple) or not alternates or alternates[0] != '' or
            any(not isinstance(value, str) or value and not ALTERNATE.fullmatch(value)
                for value in alternates) or alternates != tuple(sorted(set(alternates)))):
        raise ValueError('invalid date-combination alternate inventory')


def valid_tags(tags):
    if (not isinstance(tags, tuple) or not tags or
            any(not isinstance(tag, str) or locale_id(tag) != tag for tag in tags) or
            tags != tuple(sorted(set(tags)))):
        raise ValueError('invalid date-combination locale inventory')


def valid(state, text, numbers, origin, calendar, width, form, alt,
          calendars, alternates):
    if (state not in STATES or
            not all(isinstance(value, str) for value in
                    (text, numbers, origin, calendar, width, form, alt))):
        raise ValueError('invalid date-combination source record')
    if state == 'missing':
        if any((text, numbers, origin, calendar, width, form, alt)):
            raise ValueError('noncanonical missing date-combination record')
    elif (not origin or locale_id(origin) != origin or calendar not in calendars or
          width not in source.WIDTHS or form not in source.FORMS or alt not in alternates or
          text in (BLOCK, INHERIT) or
          any(not ord(char) or 0xd800 <= ord(char) <= 0xdfff for char in text + numbers) or
          state == 'blocked' and (text or numbers)):
        raise ValueError('invalid date-combination source state/provenance')


def selectors(calendars, alternates):
    return tuple((calendar, width, form, alt) for calendar in calendars
                 for width in source.WIDTHS for form in source.FORMS for alt in alternates)


def encode(data, tags, ids):
    calendars, alternates = data['calendars'], data['alternates']
    valid_inventories(calendars, alternates)
    valid_tags(tags)
    columns = selectors(calendars, alternates)
    if (data['forms'] != source.FORMS or data['widths'] != source.WIDTHS or
            ids.get('') != 0 or len(data['rows']) != len(tags) * len(columns)):
        raise ValueError('invalid date-combination source coverage')
    domains = tuple(set(values) for values in
                    (tags, calendars, source.WIDTHS, source.FORMS, alternates))
    compiled, cache = {}, {}
    for row in data['rows']:
        if not isinstance(row, tuple) or len(row) != 13:
            raise ValueError('invalid date-combination source row')
        identity, record = row[:5], row[5:]
        if (any(not isinstance(value, str) or value not in domain
                for value, domain in zip(identity, domains)) or identity in compiled):
            raise ValueError('invalid/duplicate date-combination source selector')
        if not all(isinstance(value, str) for value in record):
            raise ValueError('invalid date-combination source record')
        if record not in cache:
            state, text, numbers, origin, calendar, width, form, alt = record
            valid(*record, calendars, alternates)
            cache[record] = ((0,) * RECORD_WIDTH if state == 'missing' else
                             (STATES.index(state), ids[text], ids[numbers], ids[origin],
                              calendars.index(calendar), source.WIDTHS.index(width),
                              source.FORMS.index(form), alternates.index(alt)))
        compiled[identity] = cache[record]
    # Unique selectors in the finite domains, at their product cardinality,
    # establish exact coverage without allocating another full selector set.
    # Intern records during compilation rather than copying inherited values
    # once for every cell in the resolved source matrix.
    records = tuple(sorted(set(cache.values())))
    record_ids = {record: index for index, record in enumerate(records)}
    locale_rows = tuple(tuple(record_ids[compiled[(tag,) + key]] for key in columns) for tag in tags)
    rows = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(rows)}
    words = [VERSION, len(calendars), len(alternates), len(rows), len(records)]
    words.extend(ids[calendar] for calendar in calendars)
    words.extend(ids[alt] for alt in alternates)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in rows for value in row)
    words.extend(value for record in records for value in record)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    valid_tags(tags)
    count, raw = section
    if count != len(tags) or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid date-combination locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, calendar_count, alternate_count, row_count, record_count = words[:HEADER]
    row_width = calendar_count * len(source.WIDTHS) * len(source.FORMS) * alternate_count
    expected = HEADER + calendar_count + alternate_count + count + row_count * row_width + record_count * RECORD_WIDTH
    if (version != VERSION or not calendar_count or not alternate_count or
            not 1 <= row_count <= count or not 1 <= record_count <= row_count * row_width or
            len(words) != expected or not pool or pool[0] != ''):
        raise ValueError('invalid date-combination version/dimensions')
    at = HEADER
    refs = words[at:at + calendar_count + alternate_count]
    if any(index >= len(pool) for index in refs):
        raise ValueError('invalid date-combination inventory reference')
    calendars = tuple(pool[index] for index in refs[:calendar_count])
    alternates = tuple(pool[index] for index in refs[calendar_count:])
    valid_inventories(calendars, alternates)
    at += calendar_count + alternate_count
    locale_rows = words[at:at + count]
    if set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid/unused date-combination locale row')
    at += count
    rows = tuple(words[start:start + row_width]
                 for start in range(at, at + row_count * row_width, row_width))
    if (rows != tuple(sorted(set(rows))) or
            {index for row in rows for index in row} != set(range(record_count))):
        raise ValueError('invalid/noncanonical date-combination rows')
    at += row_count * row_width
    records, previous = [], None
    for start in range(at, len(words), RECORD_WIDTH):
        record = words[start:start + RECORD_WIDTH]
        state, text, numbers, origin, calendar, width, form, alt = record
        if (state >= len(STATES) or any(index >= len(pool) for index in (text, numbers, origin)) or
                calendar >= calendar_count or width >= len(source.WIDTHS) or
                form >= len(source.FORMS) or
                alt >= alternate_count or previous is not None and record <= previous):
            raise ValueError('invalid/noncanonical date-combination record')
        if state == 0 and record != (0,) * RECORD_WIDTH:
            raise ValueError('noncanonical missing date-combination record')
        decoded = ((STATES[0],) + ('',) * 7 if state == 0 else
                   (STATES[state], pool[text], pool[numbers], pool[origin], calendars[calendar],
                    source.WIDTHS[width], source.FORMS[form], alternates[alt]))
        valid(*decoded, calendars, alternates)
        records.append(decoded)
        previous = record
    columns = selectors(calendars, alternates)
    return {'calendars': calendars, 'widths': source.WIDTHS, 'forms': source.FORMS,
            'alternates': alternates,
            'rows': tuple((tag,) + key + records[rows[index][i]]
                          for tag, index in zip(tags, locale_rows) for i, key in enumerate(columns))}
