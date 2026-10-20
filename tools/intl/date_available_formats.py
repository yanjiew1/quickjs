#!/usr/bin/env python3
"""Collect available date patterns and append templates as sparse source layers.

Skeleton matching, field-width adjustment and alternate/plural fallback belong
to consumers. Append items are substitution templates, not date patterns.
"""
import argparse
import json
from pathlib import Path
import re

import acquire
import calendar_sources
from cldr_xml import DataError, Schema
import date_pattern_syntax
import parents
from resolve import BLOCK, INHERIT, SparseMaps, freeze_overlays

FAMILIES = ('availableFormats', 'appendItems')
COUNTS = ('', 'zero', 'one', 'two', 'few', 'many', 'other')
REQUESTS = frozenset(('Date-Timezone', 'Day', 'Day-Of-Week', 'Era', 'Hour',
                      'Minute', 'Month', 'Quarter', 'Second', 'Time-Day-Of-Week',
                      'Timezone', 'Week', 'Year'))
OTHER_CALENDAR_CHILDREN = calendar_sources.CALENDAR_CHILDREN - {'dateTimeFormats'}
OTHER_FORMAT_CHILDREN = frozenset(('dateTimeFormatLength', 'intervalFormats', 'numericSeparators'))
SEGMENT = re.compile(r"([A-Za-z]+)(?:\[@(type|alt)=(['\"])([A-Za-z0-9-]+)\3\])?")
IDENTIFIER = re.compile(r'[A-Za-z]+')


def alias_target(path, expression, calendars, groups):
    """Map reviewed calendar/container aliases without general XPath evaluation."""
    target = list(path)
    for part in expression.split('/'):
        if part == '..':
            if not target:
                raise DataError('available-format alias escapes calendars')
            target.pop()
        else:
            match = SEGMENT.fullmatch(part)
            if not match:
                raise DataError('unsupported available-format alias path')
            tag, selector, value = match.group(1), match.group(2), match.group(4) or ''
            if ((tag == 'calendar' and selector != 'type') or
                    (tag == 'dateTimeFormats' and selector is not None) or
                    (tag in FAMILIES and selector not in (None, 'alt')) or
                    tag not in ('calendar', 'dateTimeFormats') + FAMILIES):
                raise DataError('unsupported available-format alias selector')
            target.append((tag, value))
    target = tuple(target)
    if (len(target) != len(path) or not target or target[0][0] != 'calendar' or
            target[0][1] not in calendars or
            len(path) > 1 and target[1] != ('dateTimeFormats', '') or
            len(path) == 3 and (target[2][0] != path[2][0] or target[2] not in groups)):
        raise DataError('unknown/incompatible available-format alias target')
    return target


def leaf_key(schema, leaf, family):
    expected = 'dateFormatItem' if family == 'availableFormats' else 'appendItem'
    if leaf.tag != expected or len(leaf):
        raise DataError('unknown/structured available-format leaf')
    attributes = schema.attributes(leaf)
    identity = 'id' if family == 'availableFormats' else 'request'
    allowed = (identity, 'count', 'alt') if identity == 'id' else (identity, 'alt')
    if any(name not in allowed and schema.fields[leaf.tag][name].role != 'metadata'
           for name in attributes):
        raise DataError('unsupported available-format leaf selector')
    code, count = attributes.get(identity, ''), attributes.get('count', '')
    # This checks source identity, not semantic skeleton ordering/field widths.
    # Preserve the original spelling for the later pattern matcher.
    if (count not in COUNTS or
            (IDENTIFIER.fullmatch(code) is None if identity == 'id' else code not in REQUESTS)):
        raise DataError('invalid available-format leaf identity')
    text = leaf.text or ''
    if identity == 'id' and text and text not in (BLOCK, INHERIT):
        date_pattern_syntax.compile_pattern(text)
    return (code, count, attributes.get('alt', '')), text


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    raw, aliases, calendars = [], [], set()
    selectors = {(family, '') for family in FAMILIES}

    def walk(locale, path, nodes):
        target = calendar_sources.alias(schema, nodes)
        if target:
            aliases.append((locale, path, target[0], target[1]))
            return
        if len(path) == 3:
            family, alt = path[2]
            for leaf in nodes:
                key, value = leaf_key(schema, leaf, family)
                raw.append((locale, (path[0][1], family, alt), key, value))
            return
        seen = set()
        for node in nodes:
            if len(path) == 1 and node.tag in OTHER_CALENDAR_CHILDREN:
                continue
            if len(path) == 2 and node.tag in OTHER_FORMAT_CHILDREN:
                continue
            expected = ('dateTimeFormats',) if len(path) == 1 else FAMILIES
            if node.tag not in expected:
                raise DataError('unknown available-format container')
            attributes, children = calendar_sources.children(schema, node, ('alt',) if len(path) == 2 else ())
            segment = node.tag, attributes.get('alt', '')
            if segment in seen:
                raise DataError('repeated available-format container')
            seen.add(segment)
            if len(path) == 2:
                selectors.add(segment)
            walk(locale, path + (segment,), children)

    for locale, calendar, nodes in calendar_sources.calendars(sources, schema):
        calendars.add(calendar)
        walk(locale, (('calendar', calendar),), nodes)
    calendars, selectors = tuple(sorted(calendars)), tuple(sorted(selectors))
    groups = tuple((calendar,) + selector for calendar in calendars for selector in selectors)
    records = SparseMaps(graph, groups, 'available-format')
    for record in raw:
        records.add(*record)
    for locale, path, expression, source in aliases:
        target = alias_target(path, expression, calendars, selectors)
        for family, alt in selectors if len(path) < 3 else (path[2],):
            mapped = target[2] if len(path) == 3 else (family, alt)
            records.alias(locale, (path[0][1], family, alt), (target[0][1],) + mapped, source)

    return dict(freeze_overlays(records, groups), calendars=calendars, groups=groups,
                source_records=len(raw), source_aliases=len(aliases))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'calendars': data['calendars'],
                      'groups': len(data['groups']), 'maps': len(data['maps']),
                      'nodes': len(data['nodes']), 'rows': len(data['rows']),
                      'source_records': data['source_records'],
                      'source_aliases': data['source_aliases']}, sort_keys=True, indent=2))
