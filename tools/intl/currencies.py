#!/usr/bin/env python3
"""Resolve sparse currency names/symbols and supplemental fraction metadata."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents
from resolve import INHERIT

COUNTS = ('', '0', '1', 'zero', 'one', 'two', 'few', 'many', 'other')
FIELDS = ('name', 'symbol', 'narrow', 'formal', 'variant', 'decimal', 'group', 'pattern')
CODE = re.compile('[A-Z]{3}')


def container(schema, node):
    if (node.text and node.text.strip(' \t\r\n') or
            any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
        raise DataError('unexpected currency container text')
    result = []
    for child in node:
        attributes = schema.attributes(child)
        if 'alt' not in attributes:
            result.append((child, attributes))
    return result


def fraction_data(sources):
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    root = read_xml(sources, 'common/supplemental/supplementalData.xml', 'supplementalData')
    rows = {}
    groups = root.findall('currencyData')
    if len(groups) != 1:
        raise DataError('missing/repeated currency fraction container')
    schema.attributes(groups[0])
    for fraction in groups[0].findall('fractions'):
        schema.attributes(fraction)
        for node, attributes in container(schema, fraction):
            if node.tag != 'info' or len(node) or node.text and node.text.strip(' \t\r\n'):
                raise DataError('invalid currency fraction record')
            code = attributes.get('iso4217')
            if (code != 'DEFAULT' and (not code or CODE.fullmatch(code) is None) or code in rows):
                raise DataError('invalid/repeated currency fraction code')
            values = []
            for name in ('digits', 'rounding', 'cashDigits', 'cashRounding'):
                raw = attributes.get(name)
                if raw is None:
                    values.append(values[0 if name == 'cashDigits' else 1] if name.startswith('cash')
                                  else 2 if name == 'digits' else 0)
                elif raw is not None and re.fullmatch('[0-9]+', raw):
                    value = int(raw)
                    if value > (100 if name in ('digits', 'cashDigits') else 0xffffffff):
                        raise DataError('currency fraction value outside bounds')
                    values.append(value)
                else:
                    raise DataError('missing/invalid currency fraction value')
            rows[code] = tuple(values)
    # LDML specifies defaults even without an explicit DEFAULT source record.
    rows.setdefault('DEFAULT', (2, 0, 2, 0))
    return tuple((code,) + rows[code] for code in sorted(rows))


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    values, codes, records, skipped = {}, set(), 0, 0
    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        groups = [(node, schema.attributes(node)) for node in root.findall('numbers')]
        groups = [(node, attributes) for node, attributes in groups if 'alt' not in attributes]
        if len(groups) > 1:
            raise DataError('repeated currency numbers container')
        fields = {}
        for numbers, attributes in groups:
            children = container(schema, numbers)
            if any(node.tag == 'alias' for node, attributes in children):
                raise DataError('numbers alias requires reviewed currency mapping')
            containers = [node for node, attributes in children if node.tag == 'currencies']
            if len(containers) > 1:
                raise DataError('repeated currencies container')
            seen = set()
            for currencies in containers:
                for currency, attributes in container(schema, currencies):
                    code = attributes.get('type')
                    if currency.tag != 'currency' or not code or CODE.fullmatch(code) is None or code in seen:
                        raise DataError('invalid/repeated currency record')
                    if 'numberSystem' in attributes:
                        raise DataError('unsupported currency numbering selector')
                    seen.add(code)
                    codes.add(code)
                    if (currency.text and currency.text.strip(' \t\r\n') or
                            any(child.tail and child.tail.strip(' \t\r\n') for child in currency)):
                        raise DataError('unexpected currency record text')
                    for node in currency:
                        attributes = schema.attributes(node)
                        if node.tag == 'symbol':
                            alternate = attributes.get('alt')
                            if alternate is not None and alternate not in FIELDS[2:5]:
                                skipped += 1
                                continue
                            field = alternate or 'symbol'
                            count = ''
                        elif 'alt' in attributes:
                            skipped += 1
                            continue
                        elif node.tag == 'displayName':
                            field, count = 'name', attributes.get('count', '')
                            if count not in COUNTS:
                                raise DataError('invalid currency name count')
                        elif node.tag in FIELDS[5:]:
                            field, count = node.tag, ''
                            if ('count' in attributes or 'numberSystem' in attributes or 'numbers' in attributes or
                                    attributes.get('type', 'standard') != 'standard'):
                                raise DataError('unsupported currency field selector')
                        else:
                            raise DataError('unknown currency field/alias: ' + node.tag)
                        if len(node):
                            raise DataError('unexpected currency field children')
                        key = code, field, count
                        if key in fields:
                            raise DataError('repeated currency field')
                        fields[key] = node.text or ''
                        records += 1
        values[locale] = fields

    resolved = {}
    for locale in graph.tags:
        path, active, current = [], set(), locale
        while current is not None and current not in resolved:
            if current in active:
                raise DataError('currency parent cycle')
            active.add(current)
            path.append(current)
            current = graph.parent(current)
        base = {} if current is None else resolved[current]
        for current in reversed(path):
            # Preserve origin even when a child explicitly repeats root text.
            # Counted display names have a consumer fallback that excludes root;
            # uncounted names and symbols retain ordinary inheritance.
            candidates = {key: (value, current == 'root' and key[1] == 'name' and bool(key[2]))
                          for key, value in values.get(current, {}).items() if value != INHERIT}
            overrides = {key: value for key, value in candidates.items() if base.get(key) != value}
            if overrides:
                base = dict(base)
                base.update(overrides)
            resolved[current] = base
    # Sort each shared map only once, then deduplicate by semantic contents.
    converted = {id(value): tuple(key + (value[key][0],) for key in sorted(value))
                 for value in {id(value): value for value in resolved.values()}.values()}
    root_names = {id(value): tuple((key[0], key[2]) for key in sorted(value) if value[key][1])
                  for value in {id(value): value for value in resolved.values()}.values()}
    maps = tuple(sorted(set(converted.values())))
    indices = {value: index for index, value in enumerate(maps)}
    return {'codes': tuple(sorted(codes)), 'fields': FIELDS, 'counts': COUNTS,
            'fractions': fraction_data(sources), 'maps': maps,
            'rows': tuple((locale, indices[converted[id(resolved[locale])]]) for locale in graph.tags),
            'root_names': tuple((locale, root_names[id(resolved[locale])]) for locale in graph.tags
                                if root_names[id(resolved[locale])]),
            'source_records': records, 'skipped_alternates': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'codes': len(data['codes']),
                      'fraction_rows': len(data['fractions']), 'maps': len(data['maps']),
                      'rows': len(data['rows']), 'source_records': data['source_records'],
                      'skipped_alternates': data['skipped_alternates']}, sort_keys=True, indent=2))
