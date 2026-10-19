#!/usr/bin/env python3
"""Resolve shared CLDR date-field labels, independently of relative patterns."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents
from resolve import Records

FIELDS = ('era', 'year', 'quarter', 'month', 'weekOfYear', 'weekday',
          'day', 'dayPeriod', 'hour', 'minute', 'second', 'timeZoneName')
WIDTHS = ('long', 'short', 'narrow')
SOURCE_NAMES = dict(zip(FIELDS, ('era', 'year', 'quarter', 'month', 'week', 'weekday',
                               'day', 'dayperiod', 'hour', 'minute', 'second', 'zone')))
GROUPS = {name + ('' if width == 'long' else '-' + width): (field, width)
          for field, name in SOURCE_NAMES.items() for width in WIDTHS}
FIELD_ALIAS = re.compile(r"\.\./field\[@type=(['\"])([a-z-]+)\1\]")


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    raw, aliases, alternates, seen = [], [], {''}, set()

    def children(node):
        if (node.text and node.text.strip(' \t\r\n') or
                any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
            raise DataError('unexpected date-field-name container text')
        return [child for child in node if child.tag != 'special']

    def container(node):
        fields = schema.attributes(node)
        if any(schema.fields[node.tag][name].role != 'metadata' for name in fields):
            raise DataError('unsupported date-field-name container selector')
        return children(node)

    for filename in sorted(sources.paths('common/main/')):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        dates = root.findall('dates')
        if len(dates) > 1:
            raise DataError('repeated date-field-name dates container')
        if not dates:
            continue
        nodes = container(dates[0])
        if any(node.tag == 'alias' for node in nodes):
            raise DataError('dates alias requires reviewed date-field-name mapping')
        groups = [node for node in nodes if node.tag == 'fields']
        if len(groups) > 1:
            raise DataError('repeated date-field-name fields container')
        for group in groups:
            for field in container(group):
                attributes = schema.attributes(field)
                if field.tag != 'field':
                    raise DataError('unsupported date-field-name fields child/alias')
                identity = GROUPS.get(attributes.get('type'))
                if identity is None:
                    continue  # Other CLDR date fields are not ECMA-402 field codes.
                if any(schema.fields[field.tag][name].role != 'metadata' and name != 'type'
                       for name in attributes):
                    raise DataError('unsupported date-field-name scope/alternate')
                state = locale, identity
                if state in seen:
                    raise DataError('repeated date-field-name group')
                seen.add(state)
                nodes = children(field)
                targets = [node for node in nodes if node.tag == 'alias']
                if targets:
                    node = targets[0]
                    fields = schema.attributes(node)
                    match = FIELD_ALIAS.fullmatch(fields.get('path', ''))
                    target = GROUPS.get(match.group(2)) if match else None
                    if (len(nodes) != 1 or len(node) or target is None or
                            node.text and node.text.strip(' \t\r\n') or
                            set(fields) != {'source', 'path'}):
                        raise DataError('unsupported/mixed date-field-name alias')
                    origin = fields['source']
                    aliases.append((locale, identity, target,
                                    None if origin == 'locale' else parents.locale_id(origin)))
                    continue
                for node in nodes:
                    if node.tag in ('relative', 'relativeTime', 'relativePeriod'):
                        continue  # RelativeTimeFormat owns these values and patterns.
                    fields = schema.attributes(node)
                    if (node.tag != 'displayName' or len(node) or
                            any(schema.fields[node.tag][name].role != 'metadata' and name != 'alt'
                                for name in fields)):
                        raise DataError('unknown/structured date-field-name value')
                    alt = fields.get('alt', '')
                    alternates.add(alt)
                    raw.append((locale, identity + (alt,), node.text or ''))

    alternates = tuple(sorted(alternates))
    records = Records(graph, ((field, width, alt) for field in FIELDS for width in WIDTHS
                              for alt in alternates))
    for row in raw:
        records.add(*row)
    for locale, identity, target, source in aliases:
        for alt in alternates:
            records.alias(locale, identity + (alt,), target + (alt,), source)
    rows = []
    for locale in graph.tags:
        for field in FIELDS:
            for width in WIDTHS:
                for alt in alternates:
                    key = field, width, alt
                    resolved = records.resolve(locale, key)
                    if resolved is None:
                        value = ('missing', '', '', '', '', '')
                    else:
                        value = ('blocked' if resolved.blocked else 'value',
                                 '' if resolved.blocked else resolved.value,
                                 resolved.locale) + resolved.key
                    rows.append((locale,) + key + value)
    return {'fields': FIELDS, 'widths': WIDTHS, 'alternates': alternates,
            'rows': tuple(rows), 'source_records': len(raw), 'source_aliases': len(aliases)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'alternates': data['alternates'], 'source_records': data['source_records'],
                      'source_aliases': data['source_aliases']}, sort_keys=True, indent=2))
