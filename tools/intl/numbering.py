#!/usr/bin/env python3
"""Collect numbering definitions and resolved locale numbering preferences.

Algorithmic rule identifiers remain references, not executed rules. This
metadata does not advertise NumberFormat locale or numbering capabilities.
"""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents
from resolve import BLOCK, INHERIT, Records

FIELDS = ('default', 'native', 'traditional', 'finance')


def system_id(value):
    if not re.fullmatch('[a-z0-9]{3,8}', value):
        raise DataError('invalid numbering system identifier: ' + value)
    return value


def definitions(sources):
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    root = read_xml(sources, 'common/supplemental/numberingSystems.xml', 'supplementalData')
    groups = root.findall('numberingSystems')
    if len(groups) != 1:
        raise DataError('missing/repeated numbering definitions')
    schema.attributes(groups[0])
    systems = {}
    for node in groups[0]:
        if node.tag != 'numberingSystem' or len(node):
            raise DataError('unknown numbering definition child')
        fields = schema.attributes(node)
        identity, kind = system_id(fields['id']), fields['type']
        if fields.get('radix', '10') != '10':
            raise DataError('unsupported non-decimal numbering radix')
        if identity in systems:
            raise DataError('repeated numbering system: ' + identity)
        if kind == 'numeric':
            digits = fields.get('digits', '')
            if (len(digits) != 10 or len(set(digits)) != 10 or 'rules' in fields or
                    any(ord(c) == 0 or 0xd800 <= ord(c) <= 0xdfff for c in digits)):
                raise DataError('invalid numeric digit repertoire')
            value = digits
        elif kind == 'algorithmic':
            value = fields.get('rules', '')
            if 'digits' in fields or not re.fullmatch('[A-Za-z0-9_-]+(?:/[A-Za-z0-9_-]+)*', value):
                raise DataError('invalid algorithmic numbering rule reference')
        else:
            raise DataError('unknown numbering system kind')
        systems[identity] = (kind, value)
    if not systems:
        raise DataError('empty numbering definitions')
    return systems


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    systems = definitions(sources)
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    records = Records(graph, ((field,) for field in FIELDS))
    skipped = 0

    def field(locale, node, name):
        nonlocal skipped
        attributes = schema.attributes(node)
        if 'alt' in attributes:
            skipped += 1
            return
        if len(node):
            raise DataError('unexpected numbering preference children')
        value = node.text or ''
        if value not in (INHERIT, BLOCK):
            system_id(value)
            if value not in systems:
                raise DataError('undefined numbering preference: ' + value)
        records.add(locale, (name,), value)

    def selected(nodes, label):
        nonlocal skipped
        result = []
        for node in nodes:
            attributes = schema.attributes(node)
            if 'alt' in attributes:
                skipped += 1
            else:
                result.append(node)
        if len(result) > 1:
            raise DataError('repeated ' + label + ' container')
        return result

    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        containers = selected(root.findall('numbers'), 'numbers')
        for container in containers:
            if container.findall('alias'):
                raise DataError('numbers alias requires reviewed preference mapping')
            for node in container.findall('defaultNumberingSystem'):
                field(locale, node, 'default')
            groups = selected(container.findall('otherNumberingSystems'), 'other numbering systems')
            for group in groups:
                for node in group:
                    if node.tag not in FIELDS[1:]:
                        raise DataError('unknown other numbering preference')
                    field(locale, node, node.tag)
    rows = []
    for locale in graph.tags:
        values = []
        for name in FIELDS:
            if name in ('default', 'native'):
                value = records.require(locale, (name,)).value
            else:
                resolved = records.resolve(locale, (name,))
                # Preserve blocked versus absent optional source records.
                value = None if resolved is None else '' if resolved.blocked else resolved.value
            values.append(value)
        rows.append((locale,) + tuple(values))
    return {'systems': tuple((identity,) + systems[identity] for identity in sorted(systems)),
            'locales': tuple(rows), 'source_records': len(records.values),
            'skipped_alternates': skipped}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(collect(acquire.Sources(args.sources)), sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
