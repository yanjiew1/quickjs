#!/usr/bin/env python3
"""Collect typed CLDR region preferences and script direction metadata.

These are source records, not formatter availability. Keep region/language
overrides and preference order; the consuming service applies locale selection,
calendar aliases and its actual capabilities.
"""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml

DAYS = ('mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun')
HOURS = {'K': 'h11', 'h': 'h12', 'H': 'h23', 'k': 'h24'}


def words(value):
    result = tuple(value.split(' '))
    if (not result or any(not re.fullmatch(r'[\x21-\x7e]+', item) for item in result) or
            len(set(result)) != len(result)):
        raise DataError('empty/repeated preference token')
    return result


def region(value):
    if not re.fullmatch(r'[A-Z]{2}|[0-9]{3}', value):
        raise DataError('invalid preference region: ' + value)
    return value


def target(value):
    parts = value.split('_')
    if len(parts) == 1:
        return region(value)
    if len(parts) != 2 or not re.fullmatch(r'[a-z]{2,8}', parts[0]):
        raise DataError('invalid language/region preference: ' + value)
    return parts[0] + '-' + region(parts[1])


def add(table, key, value):
    if key in table:
        raise DataError('duplicate preference record: ' + repr(key))
    table[key] = value


def container(root, name, schema):
    nodes = root.findall(name)
    if len(nodes) != 1:
        raise DataError('missing/repeated preference container: ' + name)
    schema.attributes(nodes[0])
    return nodes[0]


def collect(sources):
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    root = read_xml(sources, 'common/supplemental/supplementalData.xml', 'supplementalData')
    week = {}
    for node in container(root, 'weekData', schema):
        if node.tag not in ('minDays', 'firstDay', 'weekendStart', 'weekendEnd', 'weekOfPreference'):
            raise DataError('unknown week preference child: ' + node.tag)
        fields = schema.attributes(node)
        if len(node):
            raise DataError('unexpected week preference children')
        # weekOfPreference describes week designations, not weekday boundaries.
        if node.tag == 'weekOfPreference' or 'alt' in fields:
            continue
        if 'time' in fields and fields['time'] != '00:00':
            raise DataError('partial-day weekend requires a reviewed day policy')
        if node.tag == 'minDays':
            if not re.fullmatch('[1-7]', fields['count']):
                raise DataError('invalid minimum week days')
            value = int(fields['count'])
        else:
            if fields['day'] not in DAYS:
                raise DataError('invalid preference weekday')
            value = DAYS.index(fields['day']) + 1
        for item in words(fields['territories']):
            add(week, (region(item), node.tag), value)
    defaults = {key: week.get(('001', key))
                for key in ('firstDay', 'minDays', 'weekendStart', 'weekendEnd')}
    if None in defaults.values():
        raise DataError('missing world week defaults')
    weeks = []
    for item in sorted({key[0] for key in week}):
        values = {key: week.get((item, key), value) for key, value in defaults.items()}
        start, end = values['weekendStart'], values['weekendEnd']
        weekend = tuple(day for day in range(1, 8)
                        if (day - start) % 7 <= (end - start) % 7)
        weeks.append((item, values['firstDay'], values['minDays'], weekend))
    calendars = {}
    for node in container(root, 'calendarPreferenceData', schema):
        if node.tag != 'calendarPreference' or len(node):
            raise DataError('unknown calendar preference child')
        fields = schema.attributes(node)
        ordering = words(fields['ordering'])
        if any(not re.fullmatch('[a-z][a-z0-9]*(?:-[a-z0-9]+)*', item) for item in ordering):
            raise DataError('invalid calendar preference identifier')
        for item in words(fields['territories']):
            add(calendars, target(item), ordering)
    hours = {}
    for node in container(root, 'timeData', schema):
        if node.tag != 'hours' or len(node):
            raise DataError('unknown time preference child')
        fields = schema.attributes(node)
        preferred, allowed = fields['preferred'], words(fields['allowed'])
        if preferred not in ('H', 'h') or preferred not in allowed:
            raise DataError('invalid preferred hour pattern')
        cycles = []
        for item in (preferred,) + allowed:
            if item not in ('H', 'h', 'K', 'k', 'hB', 'hb'):
                raise DataError('invalid allowed hour pattern')
            cycle = HOURS[item[0]]
            if cycle not in cycles:
                cycles.append(cycle)
        for item in words(fields['regions']):
            add(hours, target(item), tuple(cycles))
    if '001' not in calendars or '001' not in hours:
        raise DataError('missing world calendar/time preferences')
    scripts = {}
    text = sources.read('common/properties/scriptMetadata.txt').decode('utf-8-sig')
    for line in text.splitlines():
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        fields = tuple(part.strip() for part in line.split(';'))
        if len(fields) < 11 or not re.fullmatch('[A-Z][a-z]{3}', fields[0]):
            raise DataError('invalid script metadata record')
        direction = {'YES': 'rtl', 'NO': 'ltr', 'UNKNOWN': ''}.get(fields[6])
        if direction is None:
            raise DataError('invalid script direction')
        add(scripts, fields[0], direction)
    if not scripts:
        raise DataError('missing script direction metadata')
    return {'weeks': tuple(weeks), 'calendars': tuple(sorted(calendars.items())),
            'hours': tuple(sorted(hours.items())), 'scripts': tuple(sorted(scripts.items()))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(collect(acquire.Sources(args.sources)), sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
