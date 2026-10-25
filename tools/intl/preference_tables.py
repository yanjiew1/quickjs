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

"""Typed native wire records for regional preferences and script directions."""
import re
import struct

import bundle
import preferences

SECTIONS = (bundle.WEEKS, bundle.CALENDAR_PREFERENCES,
            bundle.HOUR_PREFERENCES, bundle.SCRIPT_DIRECTIONS)
DIRECTIONS = ('', 'ltr', 'rtl')


def strings(data):
    result = {''}
    result.update(row[0] for row in data['weeks'])
    result.update(key for key, value in data['scripts'])
    for kind in ('calendars', 'hours'):
        for key, value in data[kind]:
            result.update((key, ' '.join(value)))
    return result


def encode(data, ids):
    def table(rows, width):
        return len(rows), b''.join(struct.pack('<' + 'I' * width, *row) for row in rows)

    weeks = [(ids[key], first, minimum, sum(1 << (day - 1) for day in weekend))
             for key, first, minimum, weekend in data['weeks']]
    calendars = [(ids[key], ids[' '.join(value)], len(value)) for key, value in data['calendars']]
    hours = [(ids[key], ids[' '.join(value)], len(value)) for key, value in data['hours']]
    scripts = [(ids[key], DIRECTIONS.index(value)) for key, value in data['scripts']]
    return {bundle.WEEKS: table(weeks, 4), bundle.CALENDAR_PREFERENCES: table(calendars, 3),
            bundle.HOUR_PREFERENCES: table(hours, 3), bundle.SCRIPT_DIRECTIONS: table(scripts, 2)}


def decode(sections, pool):
    if not any(identity in sections for identity in SECTIONS):
        return None
    if not all(identity in sections for identity in SECTIONS):
        raise ValueError('incomplete Intl preference sections')

    def rows(identity, width):
        count, data = sections[identity]
        if not count or count * width * 4 != len(data):
            raise ValueError('invalid Intl preference extent')
        result = tuple(struct.iter_unpack('<' + 'I' * width, data))
        if any(a[0] >= b[0] for a, b in zip(result, result[1:])):
            raise ValueError('noncanonical Intl preference order')
        return result

    def text(index):
        if index >= len(pool):
            raise ValueError('invalid Intl preference string reference')
        return pool[index]

    weeks = []
    for key, first, minimum, mask in rows(bundle.WEEKS, 4):
        key = preferences.region(text(key))
        if not 1 <= first <= 7 or not 1 <= minimum <= 7 or not 1 <= mask <= 127:
            raise ValueError('invalid Intl week fields')
        weeks.append((key, first, minimum, tuple(day for day in range(1, 8) if mask & 1 << (day - 1))))
    lists = {}
    for identity, kind in ((bundle.CALENDAR_PREFERENCES, 'calendars'), (bundle.HOUR_PREFERENCES, 'hours')):
        values = []
        for key, value, count in rows(identity, 3):
            key = text(key)
            if preferences.target(key.replace('-', '_')) != key:
                raise ValueError('noncanonical Intl preference key')
            tokens = preferences.words(text(value))
            if len(tokens) != count:
                raise ValueError('invalid Intl preference token count')
            if kind == 'hours':
                valid = all(token in preferences.HOURS.values() for token in tokens)
            else:
                valid = all(re.fullmatch('[a-z][a-z0-9]*(?:-[a-z0-9]+)*', token) for token in tokens)
            if not valid:
                raise ValueError('invalid Intl preference value')
            values.append((key, tokens))
        lists[kind] = tuple(values)
    scripts = []
    for key, direction in rows(bundle.SCRIPT_DIRECTIONS, 2):
        key = text(key)
        if not re.fullmatch('[A-Z][a-z]{3}', key) or direction >= len(DIRECTIONS):
            raise ValueError('invalid Intl script direction')
        scripts.append((key, DIRECTIONS[direction]))
    if ('001' not in {row[0] for row in weeks} or
            any('001' not in dict(lists[kind]) for kind in lists)):
        raise ValueError('Intl preferences lack world defaults')
    return dict(lists, weeks=tuple(weeks), scripts=tuple(scripts))
