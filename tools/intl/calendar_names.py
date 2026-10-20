#!/usr/bin/env python3
"""Collect sparse calendar-name layers without choosing formatter fallbacks."""
import argparse
import json
from pathlib import Path
import re

import acquire
import calendar_sources
from cldr_xml import DataError, Schema
import parents
from resolve import SparseMaps, freeze_overlays

CONTEXTS = ('format', 'stand-alone')
WIDTHS = ('abbreviated', 'narrow', 'wide')
CYCLIC_SETS = ('years', 'months', 'days', 'dayParts', 'zodiacs', 'solarTerms')
DAY_PERIODS = ('am', 'pm', 'midnight', 'noon', 'morning1', 'morning2',
               'afternoon1', 'afternoon2', 'evening1', 'evening2', 'night1', 'night2')
LEAF_CODES = {
    'months': frozenset(str(i) for i in range(1, 14)),
    'days': frozenset(('sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat')),
    'quarters': frozenset(('1', '2', '3', '4')),
    'dayPeriods': frozenset(DAY_PERIODS),
    'monthPatterns': frozenset(('leap', 'standardAfterLeap', 'combined')),
    'cyclicNameSets': frozenset(str(i) for i in range(1, 61)),
}
FAMILIES = {
    'months': ('month', CONTEXTS, WIDTHS),
    'days': ('day', CONTEXTS, WIDTHS + ('short',)),
    'quarters': ('quarter', CONTEXTS, WIDTHS),
    'dayPeriods': ('dayPeriod', CONTEXTS, WIDTHS),
    'monthPatterns': ('monthPattern', CONTEXTS + ('numeric',), WIDTHS + ('all',)),
}
OWNED = frozenset(FAMILIES) | {'eras', 'cyclicNameSets'}
OTHER_CALENDAR_CHILDREN = calendar_sources.CALENDAR_CHILDREN - OWNED
SEGMENT = re.compile(r"([A-Za-z]+)(?:\[@type=(['\"])([A-Za-z0-9-]+)\2\])?")
ERA = re.compile(r'0|[1-9][0-9]*')


def group_paths():
    """Finite typed groups; their existence does not promise data or support."""
    for family, (stem, contexts, widths) in sorted(FAMILIES.items()):
        for context in contexts:
            for width in widths:
                yield ((family, '', context, width),
                       ((family, ''), (stem + 'Context', context), (stem + 'Width', width)), stem)
    for width, tag in (('abbreviated', 'eraAbbr'), ('narrow', 'eraNarrow'), ('wide', 'eraNames')):
        yield ('eras', '', '', width), (('eras', ''), (tag, '')), 'era'
    for cycle in CYCLIC_SETS:
        for context in CONTEXTS:
            for width in WIDTHS:
                yield (('cyclicNameSets', cycle, context, width),
                       (('cyclicNameSets', ''), ('cyclicNameSet', cycle),
                        ('cyclicNameContext', context), ('cyclicNameWidth', width)), 'cyclicName')


def alias_target(path, expression, calendars, prefixes):
    """Translate reviewed relative type-selector paths into typed prefixes.

    This deliberately excludes general XPath, leaf aliases and cross-family
    mappings. Ancestor aliases retain each descendant's remaining selectors.
    """
    target = list(path)
    for segment in expression.split('/'):
        if segment == '..':
            if not target:
                raise DataError('calendar-name alias escapes calendars')
            target.pop()
        else:
            match = SEGMENT.fullmatch(segment)
            if match is None:
                raise DataError('unsupported calendar-name alias path')
            target.append((match.group(1), match.group(3) or ''))
    target = tuple(target)
    if (len(target) != len(path) or not target or target[0][0] != 'calendar' or
            target[0][1] not in calendars or target[1:] not in prefixes or
            len(path) > 1 and path[1][0] != target[1][0]):
        raise DataError('unknown/incompatible calendar-name alias target')
    return target


def leaf_key(schema, leaf, family, tag):
    if leaf.tag != tag or len(leaf):
        raise DataError('unknown/structured calendar-name leaf')
    attributes = schema.attributes(leaf)
    allowed = ('type', 'alt', 'yeartype') if family == 'months' else ('type', 'alt')
    if any(name not in allowed and schema.fields[leaf.tag][name].role != 'metadata'
           for name in attributes):
        raise DataError('unsupported calendar-name leaf selector')
    code = attributes.get('type', '')
    if family == 'eras':
        # Keep raw CLDR era IDs, including historical or currently unmapped
        # names. Calendar chronology and the type-to-code bridge have owners.
        valid = ERA.fullmatch(code) is not None
    else:
        valid = code in LEAF_CODES[family]
    year = attributes.get('yeartype', '')
    if not valid or year not in ('', 'standard', 'leap'):
        raise DataError('invalid calendar-name leaf identity')
    return code, year, attributes.get('alt', '')


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    templates = tuple(group_paths())
    groups_by_path = {path: (key, tag) for key, path, tag in templates}
    prefixes = {path[:i] for path in groups_by_path for i in range(len(path) + 1)}
    next_segments = {path: set() for path in prefixes}
    for path in groups_by_path:
        for i, segment in enumerate(path):
            next_segments[path[:i]].add(segment)
    raw, aliases, calendars = [], [], set()

    def walk(locale, path, nodes):
        target = calendar_sources.alias(schema, nodes)
        if target:
            aliases.append((locale, path, target[0], target[1]))
            return
        suffix = path[1:]
        if suffix in groups_by_path:
            key, tag = groups_by_path[suffix]
            for leaf in nodes:
                raw.append((locale, (path[0][1],) + key,
                            leaf_key(schema, leaf, key[0], tag), leaf.text or ''))
            return
        seen = set()
        for node in nodes:
            if not suffix and node.tag in OTHER_CALENDAR_CHILDREN:
                continue
            attributes, children = calendar_sources.children(schema, node, ('type',))
            segment = node.tag, attributes.get('type', '')
            if segment not in next_segments[suffix] or segment in seen:
                raise DataError('unknown/repeated calendar-name container')
            seen.add(segment)
            walk(locale, path + (segment,), children)

    for locale, calendar, nodes in calendar_sources.calendars(sources, schema):
        calendars.add(calendar)
        walk(locale, (('calendar', calendar),), nodes)

    calendars = tuple(sorted(calendars))
    groups = tuple(sorted((calendar,) + key for calendar in calendars
                          for key, path, tag in templates))
    records = SparseMaps(graph, groups, 'calendar-name')
    for record in raw:
        records.add(*record)
    for locale, path, expression, source in aliases:
        target = alias_target(path, expression, calendars, prefixes)
        suffix = path[1:]
        for candidate, (key, tag) in groups_by_path.items():
            if candidate[:len(suffix)] != suffix:
                continue
            mapped = target[1:] + candidate[len(suffix):]
            if mapped not in groups_by_path:
                raise DataError('incompatible calendar-name alias descendants')
            records.alias(locale, (path[0][1],) + key,
                          (target[0][1],) + groups_by_path[mapped][0], source)

    return dict(freeze_overlays(records, groups), calendars=calendars, groups=groups,
                source_records=len(raw), source_aliases=len(aliases))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'],
                      'calendars': data['calendars'], 'groups': len(data['groups']),
                      'rows': len(data['rows']), 'maps': len(data['maps']),
                      'nodes': len(data['nodes']), 'source_records': data['source_records'],
                      'source_aliases': data['source_aliases']}, sort_keys=True, indent=2))
