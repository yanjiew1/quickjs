#!/usr/bin/env python3
"""Collect interval source patterns/templates without range-formatting policy."""
import argparse
import json
from pathlib import Path
import re

import acquire
import calendar_sources
from cldr_xml import DataError, Schema
import date_pattern_syntax
import pair_template
import parents
from resolve import BLOCK, INHERIT, SparseMaps, freeze_overlays

FAMILIES = ('fallback', 'item', 'range')
DIFFERENCES = frozenset('BGHMadhmy')
RANGES = frozenset(('numeric', 'non-numeric', 'mixed'))
SKELETON = re.compile(r'[A-Za-z]+')
SEGMENT = re.compile(r"([A-Za-z]+)((?:\[@(?:type|id|alt)=(?:'[^']*'|\"[^\"]*\")\])*)")
PREDICATE = re.compile(r"\[@(type|id|alt)=(['\"])([^'\"]*)\2\]")
OTHER_CALENDAR_CHILDREN = calendar_sources.CALENDAR_CHILDREN - {'dateTimeFormats'}
OTHER_FORMAT_CHILDREN = frozenset(('dateTimeFormatLength', 'availableFormats', 'appendItems', 'numericSeparators'))


def group_path(group):
    calendar, alt, family, skeleton, item_alt = group
    path = (('calendar', calendar, ''), ('dateTimeFormats', '', ''), ('intervalFormats', '', alt))
    tail = {'fallback': ('intervalFormatFallback', '', ''),
            'item': ('intervalFormatItem', skeleton, item_alt),
            'range': ('intervalFormatRanges', '', '')}[family]
    return path + (tail,)


def alias_target(path, expression, prefixes):
    """Map reviewed relative container predicates, not arbitrary XPath."""
    target = list(path)
    for part in expression.split('/'):
        if part == '..':
            if not target:
                raise DataError('interval alias escapes calendars')
            target.pop()
            continue
        match = SEGMENT.fullmatch(part)
        if not match:
            raise DataError('unsupported interval alias path')
        tag, attributes = match.group(1), {}
        consumed = 0
        for field in PREDICATE.finditer(match.group(2)):
            if field.start() != consumed:
                raise DataError('unsupported interval alias predicate')
            if field.group(1) in attributes:
                raise DataError('repeated interval alias selector')
            attributes[field.group(1)] = field.group(3)
            consumed = field.end()
        if consumed != len(match.group(2)):
            raise DataError('unsupported interval alias predicate')
        if tag == 'calendar' and set(attributes) == {'type'}:
            selector, alt = attributes['type'], ''
        elif tag == 'intervalFormatItem' and 'id' in attributes and set(attributes) <= {'id', 'alt'}:
            selector, alt = attributes['id'], attributes.get('alt', '')
        elif tag == 'intervalFormats' and set(attributes) <= {'alt'}:
            selector, alt = '', attributes.get('alt', '')
        elif tag in ('dateTimeFormats', 'intervalFormatRanges') and not attributes:
            selector, alt = '', ''
        else:
            raise DataError('unsupported interval alias selector')
        target.append((tag, selector, alt))
    target = tuple(target)
    if len(target) != len(path) or target not in prefixes or target[-1][0] != path[-1][0]:
        raise DataError('unknown/incompatible interval alias target')
    return target


def leaf(schema, node, family):
    expected = {'fallback': 'intervalFormatFallback', 'item': 'greatestDifference', 'range': 'intervalFormatRange'}
    if node.tag != expected[family] or len(node):
        raise DataError('unknown/structured interval leaf')
    attributes = schema.attributes(node)
    identity = 'id' if family == 'item' else 'type' if family == 'range' else None
    if any(name not in ('alt', identity) and schema.fields[node.tag][name].role != 'metadata'
           for name in attributes):
        raise DataError('unsupported interval leaf selector')
    code = attributes.get(identity, '')
    if family == 'item' and code not in DIFFERENCES or family == 'range' and code not in RANGES:
        raise DataError('invalid interval leaf identity')
    text = node.text or ''
    if text and text not in (BLOCK, INHERIT):
        if family == 'item':
            date_pattern_syntax.compile_interval(text)
        else:
            pair_template.validate(text)
    return (code, attributes.get('alt', '')), text


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    raw, aliases, calendars, alternates, items = [], [], set(), {''}, set()

    def walk(locale, path, nodes):
        target = calendar_sources.alias(schema, nodes)
        if target:
            aliases.append((locale, path, target[0], target[1]))
            return
        if len(path) == 4:
            family = 'item' if path[-1][0] == 'intervalFormatItem' else 'range'
            group = path[0][1], path[2][2], family, path[3][1], path[3][2]
            for node in nodes:
                key, value = leaf(schema, node, family)
                raw.append((locale, group, key, value))
            return
        seen = set()
        for node in nodes:
            if len(path) == 1 and node.tag in OTHER_CALENDAR_CHILDREN:
                continue
            if len(path) == 2 and node.tag in OTHER_FORMAT_CHILDREN:
                continue
            if len(path) == 3 and node.tag == 'intervalFormatFallback':
                key, value = leaf(schema, node, 'fallback')
                raw.append((locale, (path[0][1], path[2][2], 'fallback', '', ''), key, value))
                continue
            expected = (('dateTimeFormats',) if len(path) == 1 else ('intervalFormats',)
                        if len(path) == 2 else ('intervalFormatItem', 'intervalFormatRanges'))
            if node.tag not in expected:
                raise DataError('unknown interval container')
            allowed = ('id', 'alt') if node.tag == 'intervalFormatItem' else ('alt',) if len(path) == 2 else ()
            attributes, children = calendar_sources.children(schema, node, allowed)
            skeleton, alt = attributes.get('id', ''), attributes.get('alt', '')
            if node.tag == 'intervalFormatItem':
                # Preserve raw identity; semantic skeleton order/widths belong
                # to the matcher rather than this source collector.
                if SKELETON.fullmatch(skeleton) is None:
                    raise DataError('invalid interval skeleton identity')
                items.add((skeleton, alt))
            if node.tag == 'intervalFormats':
                alternates.add(alt)
            segment = node.tag, skeleton, alt
            if segment in seen:
                raise DataError('repeated interval container')
            seen.add(segment)
            walk(locale, path + (segment,), children)

    for locale, calendar, nodes in calendar_sources.calendars(sources, schema):
        calendars.add(calendar)
        walk(locale, (('calendar', calendar, ''),), nodes)
    calendars, alternates, items = tuple(sorted(calendars)), tuple(sorted(alternates)), tuple(sorted(items))
    groups = tuple(sorted((calendar, alt, family, skeleton, item_alt)
                          for calendar in calendars for alt in alternates
                          for family in FAMILIES
                          for skeleton, item_alt in (items if family == 'item' else (('', ''),))))
    paths = {group_path(group): group for group in groups}
    prefixes = {path[:i] for path in paths for i in range(1, len(path) + 1)}
    records = SparseMaps(graph, groups, 'interval')
    for record in raw:
        records.add(*record)
    for locale, path, expression, source in aliases:
        target = alias_target(path, expression, prefixes)
        for candidate, group in paths.items():
            if candidate[:len(path)] == path:
                mapped = target + candidate[len(path):]
                if mapped not in paths:
                    raise DataError('incompatible interval alias descendants')
                records.alias(locale, group, paths[mapped], source)
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
