#!/usr/bin/env python3
"""Resolve number range/approximation and currency boundary/name layouts."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import numbering
import parents
from resolve import BLOCK, INHERIT, Records

MISC = ('range', 'approximately', 'atLeast', 'atMost')
COUNTS = ('0', '1', 'zero', 'one', 'two', 'few', 'many', 'other')
SIDES = ('beforeCurrency', 'afterCurrency')
SPACING = ('currencyMatch', 'surroundingMatch', 'insertBetween')
FIELDS = (MISC + tuple('currency-unit-' + count for count in COUNTS) +
          tuple(side + '-' + field for side in SIDES for field in SPACING) +
          ('currencyPatternAppendISO',))
REQUIRED = frozenset(MISC + tuple(side + '-' + field for side in SIDES for field in SPACING))
GROUP_ALIAS = re.compile(r"\.\./(currencyFormats|miscPatterns)\[@numberSystem=(['\"])([^'\"]+)\2\]")
SPACING_ALIAS = re.compile(r"\.\./\.\./currencyFormats\[@numberSystem=(['\"])([^'\"]+)\1\]/currencySpacing")


def collect(sources, graph=None, numbering_data=None):
    graph = parents.load(sources) if graph is None else graph
    systems = tuple(sorted(name for name, (kind, value) in numbering.definitions(sources).items()
                           if kind == 'numeric'))
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    unit_fields = tuple('currency-unit-' + count for count in COUNTS)
    projected = tuple(('currency-unit-default', field) for field in unit_fields)
    records = Records(graph, [(system, field) for system in systems for field in FIELDS] +
                      list(projected))
    skipped = 0

    def selected(node):
        nonlocal skipped
        if (node.text and node.text.strip(' \t\r\n') or
                any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
            raise DataError('unexpected number layout container text')
        result = []
        for child in node:
            fields = schema.attributes(child)
            if 'alt' in fields:
                skipped += 1
            else:
                result.append((child, fields))
        return result

    def alias(children):
        values = [(node, fields) for node, fields in children if node.tag == 'alias']
        if not values:
            return None
        node, fields = values[0]
        if len(children) != 1 or len(node) or node.text and node.text.strip(' \t\r\n'):
            raise DataError('mixed/repeated number layout alias')
        return fields

    def map_alias(locale, system, fields, target_system, target):
        if target_system not in systems:
            raise DataError('unknown number layout alias system')
        source = target['source']
        source = None if source == 'locale' else parents.locale_id(source)
        for field in fields:
            records.alias(locale, (system, field), (target_system, field), source)

    def leaf(locale, system, field, node):
        if len(node):
            raise DataError('unexpected number layout field children')
        records.add(locale, (system, field), node.text or '')

    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        containers = [(node, schema.attributes(node)) for node in root.findall('numbers')]
        skipped += sum('alt' in fields for node, fields in containers)
        containers = [(node, fields) for node, fields in containers if 'alt' not in fields]
        if len(containers) > 1:
            raise DataError('repeated number layout numbers container')
        for numbers, attributes in containers:
            groups, seen = selected(numbers), set()
            if any(node.tag == 'alias' for node, fields in groups):
                raise DataError('numbers alias requires reviewed layout mapping')
            for group, attributes in groups:
                if group.tag not in ('currencyFormats', 'miscPatterns'):
                    continue
                system = attributes.get('numberSystem')
                if system not in systems or (group.tag, system) in seen:
                    raise DataError('missing/nonnumeric/repeated number layout system')
                seen.add((group.tag, system))
                children = selected(group)
                target = alias(children)
                if target is not None:
                    match = GROUP_ALIAS.fullmatch(target.get('path', ''))
                    if match is None or match.group(1) != group.tag:
                        raise DataError('unsupported number layout group alias')
                    fields = MISC if group.tag == 'miscPatterns' else FIELDS[len(MISC):]
                    map_alias(locale, system, fields, match.group(3), target)
                    continue
                spacing_seen = False
                for node, fields in children:
                    if group.tag == 'miscPatterns':
                        if node.tag != 'pattern' or fields.get('type') not in MISC:
                            raise DataError('unknown miscellaneous number pattern')
                        if 'count' in fields or 'numbers' in fields:
                            raise DataError('unsupported miscellaneous number selector')
                        leaf(locale, system, fields['type'], node)
                    elif node.tag == 'unitPattern':
                        count = fields.get('count')
                        if count not in COUNTS or 'case' in fields:
                            raise DataError('unsupported currency unit selector')
                        leaf(locale, system, 'currency-unit-' + count, node)
                    elif node.tag == 'currencyPatternAppendISO':
                        leaf(locale, system, node.tag, node)
                    elif node.tag == 'currencySpacing':
                        if spacing_seen:
                            raise DataError('repeated currency spacing container')
                        spacing_seen = True
                        sides = selected(node)
                        target = alias(sides)
                        if target is not None:
                            match = SPACING_ALIAS.fullmatch(target.get('path', ''))
                            if match is None:
                                raise DataError('unsupported currency spacing alias')
                            map_alias(locale, system,
                                      tuple(side + '-' + field for side in SIDES for field in SPACING),
                                      match.group(2), target)
                            continue
                        used = set()
                        for side, attributes in sides:
                            if side.tag not in SIDES or side.tag in used:
                                raise DataError('unknown/repeated currency spacing side')
                            used.add(side.tag)
                            for value, attributes in selected(side):
                                if value.tag not in SPACING:
                                    raise DataError('unknown currency spacing field/alias')
                                leaf(locale, system, side.tag + '-' + value.tag, value)
                    elif node.tag not in ('currencyFormatLength', 'default', 'special'):
                        raise DataError('unknown currency layout group child')
    source_records, source_aliases = len(records.values), len(records.aliases)
    # Project only local selected-default leaves into the locale-level view.
    # Parent fallback then uses that parent's own default, not the child's.
    # Aliases still target raw paths, retaining source="locale"'s requesting
    # anchor and the terminal source needed for root-excluding count fallback.
    if numbering_data is None:
        numbering_data = numbering.collect(sources, graph)
    defaults = {row[0]: row[1] for row in numbering_data['locales']}
    for locale in graph.tags:
        for key in projected:
            raw = (defaults[locale], key[1])
            state = (locale, raw)
            if state in records.values and records.values[state] != INHERIT:
                records.add(locale, key, records.values[state])
            target = records.aliases.get(state)
            if target is not None:
                records.alias(locale, key, target.target, target.source)
    currency_units = []
    for locale in graph.tags:
        values, root_mask = [], 0
        for index, key in enumerate(projected):
            result = records.resolve(locale, key)
            values.append(None if result is None else BLOCK if result.blocked else result.value)
            if result is not None and result.locale == 'root':
                root_mask |= 1 << index
        currency_units.append((locale, tuple(values), root_mask))
    rows, root_fields = [], []
    for locale in graph.tags:
        for system in systems:
            values, root_mask = [], 0
            for index, field in enumerate(FIELDS):
                result = records.resolve(locale, (system, field))
                if field in REQUIRED and (result is None or result.blocked):
                    raise DataError('missing/blocked required number layout field: ' +
                                    repr((locale, system, field)))
                value = None if result is None else BLOCK if result.blocked else result.value
                if field in REQUIRED and not value and not field.endswith('-insertBetween'):
                    raise DataError('empty required number layout field')
                values.append(value)
                if field.startswith('currency-unit-') and result is not None and result.locale == 'root':
                    root_mask |= 1 << index
            rows.append((locale, system, tuple(values)))
            if root_mask:
                root_fields.append((locale, system, root_mask))
    return {'systems': systems, 'fields': FIELDS, 'rows': tuple(rows),
            'root_fields': tuple(root_fields),
            'currency_units': tuple(currency_units),
            'source_records': source_records, 'source_aliases': source_aliases,
            'skipped_alternates': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'systems': len(data['systems']), 'fields': len(FIELDS),
                      'source_records': data['source_records'], 'source_aliases': data['source_aliases'],
                      'skipped_alternates': data['skipped_alternates']}, sort_keys=True, indent=2))
