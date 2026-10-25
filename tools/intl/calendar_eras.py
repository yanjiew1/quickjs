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

"""Explicit canonical-era to raw name-key mappings from CLDR calendarData.

This is presentation metadata, not era arithmetic or localized names. Start/end
dates, aliases and inheritEras belong to their respective consumers; retain
only explicit code-bearing records, without inventing codes for cyclic epochs.
"""
import re

from cldr_xml import DataError, Schema, read_xml

IDENTITY = re.compile(r'[a-z0-9]+(?:-[a-z0-9]+)*')
CODE = re.compile(r'[a-z0-9]{2,8}(?:-[a-z0-9]{3,8})*')
RAW = re.compile(r'(?:0|[1-9][0-9]*)')


def records(data):
    if not isinstance(data, dict) or set(data) != {'rows'} or not isinstance(data['rows'], tuple):
        raise DataError('invalid calendar-era records')
    seen, keys, rows = set(), set(), []
    checked = (set(), set(), set())
    for row in data['rows']:
        if (not isinstance(row, tuple) or len(row) != 3 or
                any(not isinstance(value, str) for value in row)):
            raise DataError('invalid calendar-era record shape')
        calendar, code, raw = row
        # Decoder rows borrow pooled strings. Check each distinct value/domain
        # once rather than rescan a long shared identity for every era row.
        for value, grammar, cache in zip(row, (IDENTITY, CODE, RAW), checked):
            if value not in cache:
                if not grammar.fullmatch(value):
                    raise DataError('invalid calendar-era identity/code/key')
                cache.add(value)
        if (calendar, code) in seen or (calendar, raw) in keys:
            raise DataError('ambiguous calendar-era mapping')
        seen.add((calendar, code))
        keys.add((calendar, raw))
        rows.append(row)
    return tuple(sorted(rows))


def collect(sources):
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    root = read_xml(sources, 'common/supplemental/supplementalData.xml', 'supplementalData')

    def attributes(node, allowed=()):
        fields = schema.attributes(node)
        if (any(key not in allowed and schema.fields[node.tag][key].role != 'metadata'
                for key in fields) or node.text and node.text.strip() or
                any(child.tail and child.tail.strip() for child in node)):
            raise DataError('unhandled calendar-era structure: ' + node.tag)
        return fields

    groups = root.findall('calendarData')
    if len(groups) > 1:
        raise DataError('repeated calendarData')
    rows, calendars = [], set()
    for group in groups:
        attributes(group)
        for calendar in group:
            if calendar.tag != 'calendar':
                raise DataError('unknown calendarData child')
            name = attributes(calendar, ('type',))['type']
            if not IDENTITY.fullmatch(name) or name in calendars:
                raise DataError('invalid/repeated calendarData identity')
            calendars.add(name)
            seen = set()
            for child in calendar:
                if child.tag not in ('calendarSystem', 'inheritEras', 'eras') or child.tag in seen:
                    raise DataError('unknown/repeated calendarData field')
                seen.add(child.tag)
                attributes(child, {'calendarSystem': ('type',), 'inheritEras': ('calendar',),
                                   'eras': ()}[child.tag])
                if child.tag != 'eras':
                    if len(child):
                        raise DataError('structured calendarData scalar')
                    continue
                raw_keys = set()
                for era in child:
                    if era.tag != 'era' or len(era):
                        raise DataError('invalid calendarData era')
                    fields = attributes(era, ('type', 'code', 'start', 'end', 'aliases', 'named'))
                    raw = fields['type']
                    if not RAW.fullmatch(raw) or raw in raw_keys:
                        raise DataError('invalid/repeated calendarData era key')
                    raw_keys.add(raw)
                    if 'code' in fields:
                        rows.append((name, fields['code'], raw))
    return {'rows': records({'rows': tuple(rows)})}
