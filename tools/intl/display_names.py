#!/usr/bin/env python3
"""Collect display-name facts without selecting API styles or fallback policy."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents
from resolve import SparseMaps

GROUPS = ('language', 'script', 'region', 'variant', 'calendar', 'pattern', 'bracket')
CONTAINERS = {'languages': ('language', 'language'),
              'scripts': ('script', 'script'),
              'territories': ('region', 'territory'),
              'variants': ('variant', 'variant'),
              'types': ('calendar', 'type'),
              'localeDisplayPattern': ('pattern', None)}
PATTERNS = ('localePattern', 'localeSeparator', 'localeKeyTypePattern')
UNOWNED = ('subdivisions', 'keys', 'typeValues', 'transformNames',
           'measurementSystemNames', 'codePatterns', 'special')
CODE = re.compile(r'[A-Za-z0-9]+(?:[_-][A-Za-z0-9]+)*')


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    records = SparseMaps(graph, GROUPS, 'display-name')
    source_records, source_aliases = 0, 0

    def children(node):
        if (node.text and node.text.strip(' \t\r\n') or
                any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
            raise DataError('unexpected display-name container text')
        return [child for child in node if child.tag != 'special']

    def plain_container(node):
        fields = schema.attributes(node)
        if any(schema.fields[node.tag][name].role != 'metadata' for name in fields):
            raise DataError('unsupported display-name container selector')
        return children(node)

    def add(locale, group, leaf, fields, code):
        nonlocal source_records
        if len(leaf):
            raise DataError('structured display-name field')
        # alt, menu and scope are distinct source identities, not synonyms for
        # ECMA-402 style. In particular, calendar scope="core" and language
        # menu="core" coexist with ordinary names in the same CLDR document.
        key = (code, fields.get('alt', ''), fields.get('menu', ''), fields.get('scope', ''))
        records.add(locale, group, key, leaf.text or '')
        source_records += 1

    for filename in sorted(sources.paths('common/main/')):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        sections = root.findall('localeDisplayNames')
        if len(sections) > 1:
            raise DataError('repeated localeDisplayNames container')
        for section in sections:
            seen = set()
            for node in plain_container(section):
                if node.tag in UNOWNED:
                    continue
                if node.tag not in CONTAINERS or node.tag in seen:
                    raise DataError('unknown/repeated display-name group')
                seen.add(node.tag)
                group, leaf_name = CONTAINERS[node.tag]
                leaves = plain_container(node)
                aliases = [leaf for leaf in leaves if leaf.tag == 'alias']
                if aliases:
                    alias = aliases[0]
                    fields = schema.attributes(alias)
                    if (len(leaves) != 1 or len(alias) or
                            alias.text and alias.text.strip(' \t\r\n') or
                            set(fields) != {'source', 'path'} or
                            fields['path'] != '../' + node.tag):
                        raise DataError('unsupported/mixed display-name group alias')
                    origin = fields['source']
                    records.alias(locale, group, group,
                                  None if origin == 'locale' else parents.locale_id(origin))
                    source_aliases += 1
                    continue
                for leaf in leaves:
                    fields = schema.attributes(leaf)
                    if group == 'pattern':
                        if leaf.tag not in PATTERNS or set(fields) - {'alt', 'draft', 'references'}:
                            raise DataError('unknown display-name pattern field')
                        code = leaf.tag
                    else:
                        if leaf.tag != leaf_name:
                            raise DataError('unknown display-name field')
                        if group == 'calendar' and fields.get('key') != 'calendar':
                            continue  # Other Unicode-key names have another owner.
                        allowed = {'type', 'alt', 'draft', 'references'}
                        if group == 'language':
                            allowed.add('menu')
                        elif group == 'calendar':
                            allowed.update(('key', 'scope'))
                        code = fields.get('type', '')
                        if set(fields) - allowed or CODE.fullmatch(code) is None:
                            raise DataError('invalid display-name code/selector')
                    add(locale, group, leaf, fields, code)

        sections = root.findall('characters')
        if len(sections) > 1:
            raise DataError('repeated display-name characters container')
        for section in sections:
            for leaf in plain_container(section):
                if leaf.tag == 'alias':
                    raise DataError('characters alias requires reviewed display-name mapping')
                if leaf.tag != 'nestedBracketReplacement':
                    continue
                fields = schema.attributes(leaf)
                if set(fields) - {'bracket', 'alt', 'draft', 'references'} or not fields.get('bracket'):
                    raise DataError('invalid nested-bracket selector')
                add(locale, 'bracket', leaf, fields, fields['bracket'])

    # Retain child-first layers instead of merging exact keys across parents.
    # Consumers may need lateral alternatives at one layer before inheritance;
    # that policy cannot be reconstructed from an already flattened name map.
    cache, frozen, maps, nodes, node_ids, rows = {}, {}, {}, [], {}, []

    def freeze(head):
        path = []
        while head is not None and id(head) not in frozen:
            path.append(head)
            head = head.parent
        tail = 0 if head is None else frozen[id(head)]
        for node in reversed(path):
            fields = tuple(key + (node.fields[key],) for key in sorted(node.fields))
            if fields not in maps:
                maps[fields] = len(maps)
            record = node.locale, node.group, maps[fields], tail
            if record not in node_ids:
                nodes.append(record)
                node_ids[record] = len(nodes)
            tail = node_ids[record]
            frozen[id(node)] = tail
        return tail

    for locale in graph.tags:
        for group in GROUPS:
            rows.append((locale, group, freeze(records.overlays(locale, group, cache))))
    ordered = tuple(sorted(maps))
    remap = {maps[fields]: index for index, fields in enumerate(ordered)}
    return {'groups': GROUPS, 'maps': ordered,
            'nodes': tuple((locale, group, remap[index], tail)
                           for locale, group, index, tail in nodes),
            'rows': tuple(rows), 'source_records': source_records,
            'source_aliases': source_aliases}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'],
                      'rows': len(data['rows']), 'maps': len(data['maps']),
                      'nodes': len(data['nodes']),
                      'source_records': data['source_records'],
                      'source_aliases': data['source_aliases']}, sort_keys=True, indent=2))
