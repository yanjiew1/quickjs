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

"""Collect supplemental timezone presentation, not timezone offset rules."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import locale_metadata
from timezone_syntax import ZONE, METAZONE

TIMESTAMP = re.compile(r'([0-9]{4})-([0-9]{2})-([0-9]{2}) ([0-9]{2}):([0-9]{2})(?::([0-9]{2}))?')
OFFSET = re.compile(r'([+-])([0-9]{2})(?::([0-9]{2}))?')
REGION = re.compile(r'[A-Z]{2}|[0-9]{3}')
MONTH_DAYS = (31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)


def timestamp(text):
    """UTC epoch seconds; actual Monrovia source requires optional seconds."""
    match = TIMESTAMP.fullmatch(text)
    if not match:
        raise DataError('invalid metazone UTC timestamp')
    year, month, day, hour, minute, second = (int(value or '0') for value in match.groups())
    leap = year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)
    if (not 1 <= month <= 12 or not 1 <= day <= MONTH_DAYS[month - 1] + (month == 2 and leap) or
            hour > 23 or minute > 59 or second > 59):
        raise DataError('invalid metazone Gregorian date/time')
    before = year - 1
    days = (365 * before + before // 4 - before // 100 + before // 400 +
            sum(MONTH_DAYS[:month - 1]) + (leap and month > 2) + day - 1 - 719162)
    return days * 86400 + hour * 3600 + minute * 60 + second


def offset(text):
    match = OFFSET.fullmatch(text)
    if not match:
        raise DataError('invalid metazone presentation offset')
    sign, hour, minute = match.groups()
    hour, minute = int(hour), int(minute or '0')
    if hour > 23 or minute > 59:
        raise DataError('out-of-range metazone presentation offset')
    seconds = hour * 3600 + minute * 60
    return -seconds if sign == '-' else seconds


def attributes(schema, node, allowed=()):
    fields = schema.attributes(node)
    if any(name not in allowed and schema.fields[node.tag][name].role != 'metadata' for name in fields):
        raise DataError('unhandled timezone presentation attribute: ' + node.tag)
    return fields


def children(schema, node, allowed=()):
    fields = attributes(schema, node, allowed)
    if (node.text and node.text.strip(' \t\r\n') or
            any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
        raise DataError('unexpected timezone presentation container text')
    return fields, tuple(node)


def empty(schema, node, allowed):
    fields, items = children(schema, node, allowed)
    if items:
        raise DataError('structured timezone presentation leaf')
    return fields


def unique(table, key, value):
    if key in table:
        raise DataError('duplicate timezone presentation key: ' + repr(key))
    table[key] = value


def collect(sources, metadata=None):
    if metadata is None:
        metadata = locale_metadata.collect(sources)
    known_zones = set()
    for extension, key, name, aliases, preferred, deprecated, variable, iana, region in metadata['types']:
        if (extension, key) == ('u', 'tz') and aliases:
            if not ZONE.fullmatch(aliases[0]) or aliases[0] in known_zones:
                raise DataError('invalid/ambiguous CLDR stable timezone identity')
            known_zones.add(aliases[0])
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    root = read_xml(sources, 'common/supplemental/metaZones.xml', 'supplementalData')
    roots = {}
    for node in children(schema, root)[1]:
        if node.tag in ('version', 'generation', 'cldrVersion'):
            continue
        if node.tag not in ('metaZones', 'primaryZones'):
            raise DataError('unknown timezone presentation root child')
        unique(roots, node.tag, node)
    sections = {}
    if 'metaZones' in roots:
        for node in children(schema, roots['metaZones'])[1]:
            if node.tag not in ('metazoneInfo', 'mapTimezones', 'metazoneIds'):
                raise DataError('unknown metazone source section')
            unique(sections, node.tag, node)
    ids, long_ids, periods, preferred, primary, versions = {}, set(), {}, {}, {}, ()
    if 'metazoneIds' in sections:
        for node in children(schema, sections['metazoneIds'])[1]:
            if node.tag != 'metazoneId':
                raise DataError('unknown metazone ID record')
            fields = empty(schema, node, ('shortId', 'longId', 'deprecated', 'preferred'))
            short, long = fields['shortId'], fields.get('longId', '')
            # Pinned active IDs have long spellings. Do not guess the semantics
            # of future deprecated/preferred forms; require a reviewed upgrade.
            if (not re.fullmatch('[a-z]{4}', short) or not METAZONE.fullmatch(long) or long in long_ids or
                    fields['deprecated'] != 'false' or fields.get('preferred')):
                raise DataError('invalid/unreviewed metazone ID form')
            unique(ids, short, (long, fields.get('since', '')))
            long_ids.add(long)
    if 'metazoneInfo' in sections:
        for node in children(schema, sections['metazoneInfo'])[1]:
            if node.tag != 'timezone':
                raise DataError('unknown metazone timezone record')
            fields, items = children(schema, node, ('type',))
            zone = fields['type']
            if zone not in known_zones:
                raise DataError('metazone source requires a CLDR stable timezone')
            values = []
            for item in items:
                if item.tag != 'usesMetazone':
                    raise DataError('unknown metazone period record')
                fields = empty(schema, item, ('from', 'to', 'mzone', 'stdOffset', 'dstOffset'))
                start = timestamp(fields['from']) if 'from' in fields else None
                end = timestamp(fields['to']) if 'to' in fields else None
                standard = offset(fields['stdOffset']) if 'stdOffset' in fields else None
                daylight = offset(fields['dstOffset']) if 'dstOffset' in fields else None
                if fields['mzone'] not in long_ids or start is not None and end is not None and start >= end:
                    raise DataError('invalid metazone period/reference')
                values.append((start, end, fields['mzone'], standard, daylight))
            values.sort(key=lambda row: (row[0] is not None, row[0] or 0))
            for previous, current in zip(values, values[1:]):
                if previous[1] is None or current[0] is None or previous[1] > current[0]:
                    raise DataError('overlapping metazone periods')
            unique(periods, zone, tuple(values))
    if 'mapTimezones' in sections:
        fields, items = children(schema, sections['mapTimezones'], ('type', 'typeVersion', 'otherVersion'))
        if fields.get('type') not in (None, 'metazones'):
            raise DataError('unexpected timezone presentation mapping type')
        versions = tuple((key, fields[key]) for key in ('typeVersion', 'otherVersion') if key in fields)
        for node in items:
            if node.tag != 'mapZone':
                raise DataError('unknown preferred metazone record')
            fields = empty(schema, node, ('other', 'territory', 'type'))
            metazone, territory, zone = fields['other'], fields.get('territory', ''), fields['type']
            if (metazone not in long_ids or not REGION.fullmatch(territory) or zone not in known_zones or
                    not any(row[2] == metazone for row in periods.get(zone, ()))):
                raise DataError('invalid preferred metazone mapping/reference')
            unique(preferred, (metazone, territory), zone)
    if 'primaryZones' in roots:
        for node in children(schema, roots['primaryZones'])[1]:
            if node.tag != 'primaryZone' or len(node):
                raise DataError('unknown/structured dominant timezone record')
            fields = attributes(schema, node, ('iso3166',))
            zone = node.text or ''
            if not REGION.fullmatch(fields['iso3166']) or zone not in known_zones:
                raise DataError('invalid dominant timezone mapping/reference')
            unique(primary, fields['iso3166'], zone)
    return {'zones': tuple(sorted(periods)),
            'periods': tuple((zone,) + row for zone in sorted(periods) for row in periods[zone]),
            'preferred': tuple((metazone, territory, zone) for (metazone, territory), zone in sorted(preferred.items())),
            'primary': tuple(sorted(primary.items())),
            'ids': tuple((short,) + value for short, value in sorted(ids.items())),
            'versions': versions}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    data = collect(acquire.Sources(args.sources))
    print(json.dumps({key: len(value) for key, value in data.items()}, sort_keys=True, indent=2))
