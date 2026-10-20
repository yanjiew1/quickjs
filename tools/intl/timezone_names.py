#!/usr/bin/env python3
"""Collect localized timezone presentation, not timezone identity or offsets."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import literal_template
import parents
from resolve import BLOCK, INHERIT, SparseMaps, freeze_overlays
import zone_pattern_syntax

FORMATS = frozenset(('hourFormat', 'gmtFormat', 'gmtZeroFormat', 'gmtUnknownFormat',
                     'dualOffsetFormat', 'regionFormat', 'fallbackFormat'))
NAMES = frozenset(('generic', 'standard', 'daylight'))
WIDTHS = ('long', 'short')
FORMAT_GROUP = ('format', '', '', '', '')
ZONE = re.compile(r'[A-Za-z0-9_+.-]+(?:/[A-Za-z0-9_+.-]+)*')
METAZONE = re.compile(r'[A-Za-z][A-Za-z0-9_]*')
SEGMENT = re.compile(r"\.\.|[A-Za-z]+(?:\[@(?:type|alt)=(?:'[^']*'|\"[^\"]*\")\])*")
ELEMENT = re.compile(r'([A-Za-z]+)(.*)')
PREDICATE = re.compile(r"\[@(type|alt)=(['\"])([^'\"]*)\2\]")


def children(schema, node, selectors=()):
    attributes = schema.attributes(node)
    if any(name not in selectors and schema.fields[node.tag][name].role != 'metadata'
           for name in attributes):
        raise DataError('unsupported timezone-name container selector')
    if (node.text and node.text.strip(' \t\r\n') or
            any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
        raise DataError('unexpected timezone-name container text')
    return attributes, [child for child in node if child.tag != 'special']


def alias_target(path, expression, paths):
    """Map only reviewed name-width aliases; slash-bearing IDs remain atomic."""
    if not expression:
        raise DataError('empty timezone-name alias path')
    target, at = list(path), 0
    while at < len(expression):
        match = SEGMENT.match(expression, at)
        if not match or match.end() < len(expression) and expression[match.end()] != '/':
            raise DataError('unsupported timezone-name alias path')
        segment = match.group()
        if segment == '..':
            if not target:
                raise DataError('timezone-name alias escapes timeZoneNames')
            target.pop()
        else:
            match = ELEMENT.fullmatch(segment)
            tag, raw = match.groups()
            attributes, consumed = {}, 0
            for field in PREDICATE.finditer(raw):
                if field.start() != consumed or field.group(1) in attributes:
                    raise DataError('unsupported/repeated timezone-name alias predicate')
                attributes[field.group(1)] = field.group(3)
                consumed = field.end()
            if consumed != len(raw):
                raise DataError('unsupported timezone-name alias predicate')
            if tag in ('zone', 'metazone') and 'type' in attributes and set(attributes) <= {'type', 'alt'}:
                identity = attributes['type']
            elif tag in WIDTHS and set(attributes) <= {'alt'}:
                identity = ''
            else:
                raise DataError('unsupported timezone-name alias selector')
            target.append((tag, identity, attributes.get('alt', '')))
        at += len(segment)
        if at < len(expression):
            at += 1
            if at == len(expression):
                raise DataError('trailing timezone-name alias separator')
    target = tuple(target)
    if len(target) != 2 or target not in paths:
        raise DataError('unknown/incompatible timezone-name alias target')
    return paths[target]


def leaf(schema, node, family):
    attributes = schema.attributes(node)
    allowed = ('type', 'alt') if node.tag == 'regionFormat' else ('alt',)
    if (len(node) or any(name not in allowed and schema.fields[node.tag][name].role != 'metadata'
                         for name in attributes)):
        raise DataError('unsupported/structured timezone-name leaf')
    if ((family == 'format' and node.tag not in FORMATS) or
            (family == 'city' and node.tag != 'exemplarCity') or
            (family in ('zone-name', 'metazone-name') and node.tag not in NAMES)):
        raise DataError('unknown timezone-name leaf')
    text = node.text or ''
    if text and text not in (BLOCK, INHERIT):
        if node.tag == 'hourFormat':
            zone_pattern_syntax.compile_hour_format(text)
        elif node.tag in ('gmtFormat', 'regionFormat'):
            literal_template.validate(text, 1)
        elif node.tag in ('fallbackFormat', 'dualOffsetFormat'):
            literal_template.validate(text, 2)
    return (node.tag, attributes.get('type', ''), attributes.get('alt', '')), text


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    raw, aliases, groups, paths = [], [], {FORMAT_GROUP}, {}
    for filename in sorted(sources.paths('common/main/')):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        dates = read_xml(sources, filename, 'ldml').findall('dates')
        if len(dates) > 1:
            raise DataError('repeated timezone-name dates container')
        if not dates:
            continue
        nodes = children(schema, dates[0])[1]
        if any(node.tag == 'alias' for node in nodes):
            raise DataError('dates alias requires reviewed timezone-name mapping')
        names = [node for node in nodes if node.tag == 'timeZoneNames']
        if len(names) > 1:
            raise DataError('repeated timeZoneNames container')
        seen = set()
        for names_node in names:
            for node in children(schema, names_node)[1]:
                if node.tag in FORMATS:
                    key, text = leaf(schema, node, 'format')
                    raw.append((locale, FORMAT_GROUP, key, text))
                    continue
                if node.tag not in ('zone', 'metazone'):
                    raise DataError('unknown timezone-name container/alias')
                attributes, nodes = children(schema, node, ('type', 'alt'))
                identity, entity_alt = attributes.get('type', ''), attributes.get('alt', '')
                if (not (ZONE if node.tag == 'zone' else METAZONE).fullmatch(identity) or
                        (node.tag, identity, entity_alt) in seen):
                    raise DataError('invalid/repeated timezone-name entity')
                seen.add((node.tag, identity, entity_alt))
                widths = set()
                for child in nodes:
                    if node.tag == 'zone' and child.tag == 'exemplarCity':
                        group = ('city', identity, entity_alt, '', '')
                        groups.add(group)
                        key, text = leaf(schema, child, 'city')
                        raw.append((locale, group, key, text))
                        continue
                    if child.tag not in WIDTHS:
                        raise DataError('unknown timezone-name entity child/alias')
                    fields, values = children(schema, child, ('alt',))
                    width_alt = fields.get('alt', '')
                    if (child.tag, width_alt) in widths:
                        raise DataError('repeated timezone-name width')
                    widths.add((child.tag, width_alt))
                    group = (node.tag + '-name', identity, entity_alt, child.tag, width_alt)
                    groups.add(group)
                    path = ((node.tag, identity, entity_alt), (child.tag, '', width_alt))
                    paths[path] = group
                    targets = [value for value in values if value.tag == 'alias']
                    if targets:
                        value = targets[0]
                        fields = schema.attributes(value)
                        if (len(values) != 1 or len(value) or set(fields) != {'source', 'path'} or
                                value.text and value.text.strip(' \t\r\n')):
                            raise DataError('mixed/unsupported timezone-name width alias')
                        origin = fields['source']
                        aliases.append((locale, group, path, fields['path'],
                                        None if origin == 'locale' else parents.locale_id(origin)))
                    else:
                        for value in values:
                            key, text = leaf(schema, value, group[0])
                            raw.append((locale, group, key, text))
    groups = tuple(sorted(groups))
    records = SparseMaps(graph, groups, 'timezone-name')
    for record in raw:
        records.add(*record)
    for locale, group, path, expression, origin in aliases:
        records.alias(locale, group, alias_target(path, expression, paths), origin)
    return dict(freeze_overlays(records, groups), groups=groups,
                source_records=len(raw), source_aliases=len(aliases))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'groups': len(data['groups']),
                      'maps': len(data['maps']), 'nodes': len(data['nodes']), 'rows': len(data['rows']),
                      'source_records': data['source_records'], 'source_aliases': data['source_aliases']},
                     sort_keys=True, indent=2))
