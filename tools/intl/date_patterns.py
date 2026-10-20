#!/usr/bin/env python3
"""Resolve CLDR date/time styles, independently of skeleton/interval selection."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema
import calendar_sources
from calendar_sources import CALENDAR_ALIAS, CALENDAR_CHILDREN, WIDTHS
import date_pattern_syntax
import parents
from resolve import BLOCK, INHERIT, Records

FAMILIES = ('date', 'time')
FIELDS = ('pattern', 'skeleton')
GROUP_ALIAS = re.compile(
    r"\.\./\.\./calendar\[@type=(['\"])([a-z0-9-]+)\1\]/(date|time)Formats")
# These calendar children have separate semantic owners. They must not become
# style patterns merely because they contain text or a child named pattern.
OTHER_CALENDAR_CHILDREN = CALENDAR_CHILDREN - {'dateFormats', 'timeFormats'}


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    raw, aliases, calendars, alternates = [], [], set(), {''}
    source_aliases = 0

    def children(node, selectors=()):
        return calendar_sources.children(schema, node, selectors)

    def alias(nodes):
        return calendar_sources.alias(schema, nodes)

    for locale, calendar, nodes in calendar_sources.calendars(sources, schema):
        calendars.add(calendar)
        target = alias(nodes)
        if target:
            match = CALENDAR_ALIAS.fullmatch(target[0])
            if not match:
                raise DataError('unsupported date-style calendar alias')
            for family in FAMILIES:
                aliases.append((locale, calendar, family, match.group(2), target[1]))
            source_aliases += 1
            continue
        seen_families = set()
        for family_node in nodes:
            if family_node.tag in OTHER_CALENDAR_CHILDREN:
                continue
            family = family_node.tag[:-7] if family_node.tag.endswith('Formats') else ''
            if family not in FAMILIES or family in seen_families:
                raise DataError('unknown/repeated date-style family')
            seen_families.add(family)
            lengths = children(family_node)[1]
            target = alias(lengths)
            if target:
                match = GROUP_ALIAS.fullmatch(target[0])
                if target[0] == '../' + family + 'Formats':
                    target_calendar = calendar
                elif match and match.group(3) == family:
                    target_calendar = match.group(2)
                else:
                    raise DataError('unsupported date-style family alias')
                aliases.append((locale, calendar, family, target_calendar, target[1]))
                source_aliases += 1
                continue
            seen_widths = set()
            for length in lengths:
                if length.tag != family + 'FormatLength':
                    raise DataError('unknown date-style length child/alias')
                attributes, formats = children(length, ('type',))
                width = attributes.get('type')
                if width not in WIDTHS or width in seen_widths:
                    raise DataError('unknown/repeated date-style length')
                seen_widths.add(width)
                seen_format = False
                for form in formats:
                    if form.tag != family + 'Format' or seen_format:
                        raise DataError('unknown/repeated date-style format/alias')
                    seen_format = True
                    attributes, leaves = children(form, ('type',))
                    if attributes.get('type', 'standard') != 'standard':
                        raise DataError('unsupported date-style format type')
                    for leaf in leaves:
                        if leaf.tag not in ('pattern', 'datetimeSkeleton') or len(leaf):
                            raise DataError('unknown/structured date-style leaf')
                        attributes = schema.attributes(leaf)
                        allowed = ('alt', 'numbers', 'type') if leaf.tag == 'pattern' else ('alt', 'numbers')
                        if (any(name not in allowed and schema.fields[leaf.tag][name].role != 'metadata'
                                for name in attributes) or
                                attributes.get('type', 'standard') != 'standard'):
                            raise DataError('unsupported date-style leaf selector')
                        alt = attributes.get('alt', '')
                        alternates.add(alt)
                        text = leaf.text or ''
                        if leaf.tag == 'pattern' and text and text not in (BLOCK, INHERIT):
                            date_pattern_syntax.compile_pattern(text)
                        field = 'pattern' if leaf.tag == 'pattern' else 'skeleton'
                        value = text if text in (BLOCK, INHERIT) else (text, attributes.get('numbers', ''))
                        raw.append((locale, (calendar, family, width, field, alt), value))

    calendars, alternates = tuple(sorted(calendars)), tuple(sorted(alternates))
    keys = tuple((calendar, family, width, field, alt) for calendar in calendars
                 for family in FAMILIES for width in WIDTHS for field in FIELDS for alt in alternates)
    records = Records(graph, keys)
    for record in raw:
        records.add(*record)
    for locale, calendar, family, target, source in aliases:
        for width in WIDTHS:
            for field in FIELDS:
                for alt in alternates:
                    records.alias(locale, (calendar, family, width, field, alt),
                                  (target, family, width, field, alt), source)
    rows = []
    for locale in graph.tags:
        for key in keys:
            result = records.resolve(locale, key)
            if result is None:
                value = ('missing', '', '', '', '', '', '', '', '')
            else:
                value = (('blocked', '', '') if result.blocked else ('value',) + result.value)
                value += (result.locale,) + result.key
            rows.append((locale,) + key + value)
    return {'calendars': calendars, 'widths': WIDTHS, 'fields': FIELDS,
            'alternates': alternates, 'rows': tuple(rows),
            'source_records': len(raw), 'source_aliases': source_aliases}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'calendars': data['calendars'], 'alternates': data['alternates'],
                      'source_records': data['source_records'],
                      'source_aliases': data['source_aliases']}, sort_keys=True, indent=2))
