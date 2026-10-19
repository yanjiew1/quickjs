#!/usr/bin/env python3
"""Resolve CLDR numeric symbol fields without declaring formatter capability."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import numbering
import parents
from resolve import BLOCK, Records

CORE_FIELDS = ('decimal', 'group', 'list', 'percentSign', 'plusSign', 'minusSign',
               'approximatelySign', 'exponential', 'superscriptingExponent',
               'perMille', 'infinity', 'nan', 'timeSeparator')
OPTIONAL_FIELDS = ('currencyDecimal', 'currencyGroup')
FIELDS = CORE_FIELDS + OPTIONAL_FIELDS
ALIAS = re.compile(r"\.\./symbols\[@numberSystem=(['\"])([^'\"]+)\1\]")


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    definitions = numbering.definitions(sources)
    systems = tuple(sorted(name for name, (kind, value) in definitions.items()
                           if kind == 'numeric'))
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    records = Records(graph, ((system, field) for system in systems for field in FIELDS))
    skipped = 0

    def baseline(nodes, label):
        nonlocal skipped
        result = []
        for node in nodes:
            fields = schema.attributes(node)
            if 'alt' in fields:
                skipped += 1
            else:
                result.append((node, fields))
        if len(result) > 1:
            raise DataError('repeated ' + label + ' container')
        return result

    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        for container, fields in baseline(root.findall('numbers'), 'numbers'):
            for alias in container.findall('alias'):
                if 'alt' in schema.attributes(alias):
                    skipped += 1
                else:
                    raise DataError('numbers alias requires reviewed numeric symbol mapping')
            seen = set()
            for group in container.findall('symbols'):
                attributes = schema.attributes(group)
                if 'alt' in attributes:
                    skipped += 1
                    continue
                if ((group.text and group.text.strip(' \t\r\n')) or
                        any(node.tail and node.tail.strip(' \t\r\n') for node in group)):
                    raise DataError('unexpected text in numeric symbol container')
                system = attributes.get('numberSystem')
                if system not in systems:
                    raise DataError('missing or nonnumeric symbol numbering system: ' + str(system))
                if system in seen:
                    raise DataError('repeated numeric symbols: ' + system)
                seen.add(system)
                selected = []
                for node in group:
                    if node.tag not in FIELDS + ('alias',):
                        raise DataError('unknown numeric symbol: ' + node.tag)
                    fields = schema.attributes(node)
                    if 'alt' in fields:
                        skipped += 1
                    else:
                        selected.append((node, fields))
                aliases = [(node, fields) for node, fields in selected if node.tag == 'alias']
                if aliases:
                    if len(selected) != 1:
                        raise DataError('mixed/repeated numeric symbol alias')
                    node, fields = aliases[0]
                    if len(node) or node.text and node.text.strip(' \t\r\n'):
                        raise DataError('unexpected numeric symbol alias children')
                    match = ALIAS.fullmatch(fields.get('path', ''))
                    if match is None or match.group(2) not in systems:
                        raise DataError('unsupported numeric symbol alias target')
                    source = fields['source']
                    source = None if source == 'locale' else parents.locale_id(source)
                    for field in FIELDS:
                        records.alias(locale, (system, field), (match.group(2), field), source)
                    continue
                for node, fields in selected:
                    if len(node):
                        raise DataError('unexpected numeric symbol children')
                    if 'numberSystem' in fields and fields['numberSystem'] != system:
                        raise DataError('numeric symbol contradicts container numbering system')
                    records.add(locale, (system, node.tag), node.text or '')
    rows = []
    for locale in graph.tags:
        for system in systems:
            values = [records.require(locale, (system, field)).value for field in CORE_FIELDS]
            for field in OPTIONAL_FIELDS:
                value = records.resolve(locale, (system, field))
                values.append(None if value is None else BLOCK if value.blocked else value.value)
            rows.append((locale, system, tuple(values)))
    return {'systems': systems, 'fields': FIELDS, 'rows': tuple(rows),
            'source_records': len(records.values), 'source_aliases': len(records.aliases),
            'skipped_alternates': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'],
                      'systems': len(data['systems']), 'fields': len(FIELDS),
                      'rows': len(data['rows']), 'source_records': data['source_records'],
                      'source_aliases': data['source_aliases'],
                      'skipped_alternates': data['skipped_alternates']},
                     sort_keys=True, indent=2))
