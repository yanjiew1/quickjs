#!/usr/bin/env python3
"""Resolve sparse unit names, plural/case forms, gender and division layouts."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents
from resolve import BLOCK, INHERIT, SparseMaps

WIDTHS = ('long', 'short', 'narrow')
COUNTS = ('0', '1', 'zero', 'one', 'two', 'few', 'many', 'other')
FIELDS = ('displayName', 'unitPattern', 'perUnitPattern', 'gender', 'compoundUnitPattern')
IDENTIFIER = re.compile('[a-z][a-z0-9]*(?:-[a-z0-9]+)*')
CASE = re.compile('[a-z]+')
LENGTH_ALIAS = re.compile(r"\.\./unitLength\[@type=(['\"])(long|short|narrow)\1\]")
UNIT_ALIAS = re.compile(r"\.\./unit\[@type=(['\"])([a-z][a-z0-9-]*)\1\]")
PER_ALIAS = re.compile(r"\.\./compoundUnit\[@type=(['\"])per\1\]")


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    raw, aliases, length_aliases, identifiers, seen_states = [], [], [], set(), set()
    skipped = 0

    def selected(node):
        nonlocal skipped
        if (node.text and node.text.strip(' \t\r\n') or
                any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
            raise DataError('unexpected unit container text')
        result = []
        for child in node:
            fields = schema.attributes(child)
            if 'alt' in fields:
                skipped += 1
            else:
                result.append((child, fields))
        return result

    def alias(children):
        nodes = [(node, fields) for node, fields in children if node.tag == 'alias']
        if not nodes:
            return None
        node, fields = nodes[0]
        if len(children) != 1 or len(node) or node.text and node.text.strip(' \t\r\n'):
            raise DataError('mixed/repeated unit alias')
        return fields

    def source(fields):
        return None if fields['source'] == 'locale' else parents.locale_id(fields['source'])

    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        containers = [(node, schema.attributes(node)) for node in root.findall('units')]
        skipped += sum('alt' in fields for node, fields in containers)
        containers = [(node, fields) for node, fields in containers if 'alt' not in fields]
        if len(containers) > 1:
            raise DataError('repeated units container')
        for units, attributes in containers:
            seen_lengths = set()
            for length, fields in selected(units):
                if length.tag in ('durationUnit', 'special'):
                    continue  # Digital duration layouts have a separate consumer.
                if length.tag != 'unitLength' or fields.get('type') not in WIDTHS:
                    raise DataError('unsupported unit length/container alias')
                width = fields['type']
                if width in seen_lengths:
                    raise DataError('repeated unit width')
                seen_lengths.add(width)
                nodes = selected(length)
                target = alias(nodes)
                if target is not None:
                    match = LENGTH_ALIAS.fullmatch(target.get('path', ''))
                    if match is None:
                        raise DataError('unsupported unit length alias')
                    length_aliases.append((locale, width, match.group(2), source(target)))
                    continue
                for group, fields in nodes:
                    if group.tag in ('coordinateUnit', 'special'):
                        continue  # Coordinate formatting is not an Intl unit style.
                    if group.tag not in ('unit', 'compoundUnit'):
                        raise DataError('unknown unit group')
                    name = fields.get('type', '')
                    if not name:
                        raise DataError('missing unit data identifier')
                    if group.tag == 'compoundUnit' and name != 'per':
                        continue  # NumberFormat sanctions division, not arbitrary products/powers.
                    if IDENTIFIER.fullmatch(name) is None:
                        raise DataError('invalid unit data identifier')
                    kind = 'unit' if group.tag == 'unit' else 'compound'
                    if kind == 'unit':
                        identifiers.add(name)
                    state = locale, width, kind, name
                    if state in seen_states:
                        raise DataError('repeated unit group')
                    seen_states.add(state)
                    leaves = selected(group)
                    target = alias(leaves)
                    if target is not None:
                        match = (UNIT_ALIAS if kind == 'unit' else PER_ALIAS).fullmatch(target.get('path', ''))
                        if match is None:
                            raise DataError('unsupported unit group alias')
                        other = match.group(2) if kind == 'unit' else 'per'
                        aliases.append((locale, (width, kind, name), (width, kind, other), source(target)))
                        continue
                    allowed = FIELDS[:-1] if kind == 'unit' else FIELDS[-1:]
                    for leaf, attributes in leaves:
                        if leaf.tag == 'special':
                            continue
                        if leaf.tag not in allowed or len(leaf):
                            raise DataError('unknown/structured unit field')
                        identity = {name for name in attributes
                                    if schema.fields[leaf.tag][name].role == 'identity'}
                        if identity - {'count', 'case'}:
                            raise DataError('unsupported unit field identity selector')
                        count, case = attributes.get('count', ''), attributes.get('case', '')
                        if (leaf.tag == 'unitPattern' and count not in COUNTS or
                                leaf.tag != 'unitPattern' and count or
                                leaf.tag not in ('unitPattern', 'compoundUnitPattern') and case or
                                case and CASE.fullmatch(case) is None):
                            raise DataError('unsupported unit field selector')
                        value = leaf.text or ''
                        if (leaf.tag == 'gender' and value not in ('', BLOCK, INHERIT) and
                                IDENTIFIER.fullmatch(value) is None):
                            raise DataError('invalid unit gender token')
                        raw.append((locale, (width, kind, name), (leaf.tag, count, case), value))
    if not identifiers:
        raise DataError('missing unit data inventory')
    groups = tuple(sorted([('unit', name) for name in identifiers] + [('compound', 'per')]))
    records = SparseMaps(graph, ((width, kind, name) for width in WIDTHS for kind, name in groups), 'unit')
    for row in raw:
        records.add(*row)
    for row in aliases:
        records.alias(*row)
    for locale, width, other, origin in length_aliases:
        for kind, name in groups:
            records.alias(locale, (width, kind, name), (other, kind, name), origin)
    rows, maps, indices = [], [], {}
    for locale in graph.tags:
        cache, frozen = {}, {}
        for width in WIDTHS:
            for kind, name in groups:
                resolved = records.resolve(locale, (width, kind, name), cache)
                identity = id(resolved)
                if identity not in frozen:
                    frozen[identity] = tuple(key + (resolved[key],) for key in sorted(resolved))
                value = frozen[identity]
                if value not in indices:
                    indices[value] = len(maps)
                    maps.append(value)
                rows.append((locale, width, kind, name, indices[value]))
    ordered = tuple(sorted(maps))
    remap = {indices[value]: index for index, value in enumerate(ordered)}
    return {'widths': WIDTHS, 'groups': groups, 'fields': FIELDS, 'counts': COUNTS, 'maps': ordered,
            'rows': tuple((locale, width, kind, name, remap[index])
                          for locale, width, kind, name, index in rows),
            'source_records': len(raw), 'source_aliases': len(aliases) + len(length_aliases),
            'alias_fields': len(records.aliases), 'skipped_alternates': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'groups': len(data['groups']), 'maps': len(data['maps']),
                      'source_records': data['source_records'], 'source_aliases': data['source_aliases'],
                      'alias_fields': data['alias_fields'],
                      'skipped_alternates': data['skipped_alternates']}, sort_keys=True, indent=2))
