#!/usr/bin/env python3
"""Collect RelativeTimeFormat field literals and counted patterns with origins."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents
from resolve import BLOCK, INHERIT, Records

UNITS = ('second', 'minute', 'hour', 'day', 'week', 'month', 'quarter', 'year')
WIDTHS = ('long', 'short', 'narrow')
COUNTS = ('zero', 'one', 'two', 'few', 'many', 'other')
TENSES = ('past', 'future')
FIELD_ALIAS = re.compile(r"\.\./field\[@type=(['\"])([a-z-]+)\1\]")
INTEGER = re.compile(r'-?(?:0|[1-9][0-9]*)')


def group(name):
    for unit in UNITS:
        if name == unit:
            return unit, 'long'
        for width in WIDTHS[1:]:
            if name == unit + '-' + width:
                return unit, width
    return None


def numeric_pattern(text):
    if text in (BLOCK, INHERIT):
        return text
    # Literal-only one/two forms are valid. Auto literals do not call this
    # parser: braces in a relative[@type] string are ordinary text.
    if text.count('{0}') > 1 or '{' in text.replace('{0}', '') or '}' in text.replace('{0}', ''):
        raise DataError('invalid relative-time pattern placeholder')
    return text


def literal_key(text):
    # CLDR relative offsets are canonical integer Number strings. Do not assume
    # the current inventory is only -1/0/1 (release49 also contains -2/2/3).
    # Keep the key as text; future non-integer schema extensions need explicit
    # Number-string canonicalization rather than Python float formatting.
    if len(text) > 17 or INTEGER.fullmatch(text) is None or text == '-0' or abs(int(text)) > 2 ** 53:
        raise DataError('unsupported relative literal Number key')
    return text


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    raw, aliases, literal_keys = [], [], set()
    skipped, seen_groups = 0, set()

    def applicable(node):
        nonlocal skipped
        fields = schema.attributes(node)
        if 'alt' in fields:
            skipped += 1
            return False
        if 'validSubLocales' in fields:
            raise DataError('unsupported relative-time field scope')
        return True

    def selected(node):
        if (node.text and node.text.strip(' \t\r\n') or
                any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
            raise DataError('unexpected relative-time container text')
        result = []
        for child in node:
            if applicable(child):
                result.append((child, schema.attributes(child)))
        return result

    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        dates = [node for node in root.findall('dates') if applicable(node)]
        if len(dates) > 1:
            raise DataError('repeated relative-time dates container')
        if dates and any(applicable(node) for node in dates[0].findall('alias')):
            raise DataError('unsupported whole dates alias')
        containers = [node for node in dates[0].findall('fields')
                      if applicable(node)] if dates else []
        if len(containers) > 1:
            raise DataError('repeated relative-time fields container')
        for container in containers:
            for field, attributes in selected(container):
                if field.tag == 'special':
                    continue
                if field.tag != 'field':
                    raise DataError('unsupported whole fields alias/child')
                identity = group(attributes.get('type', ''))
                if identity is None:
                    continue  # Weekdays, era/display fields and other services.
                state = locale, identity
                if state in seen_groups:
                    raise DataError('repeated relative-time field')
                seen_groups.add(state)
                children = selected(field)
                targets = [(node, fields) for node, fields in children if node.tag == 'alias']
                if targets:
                    node, fields = targets[0]
                    if len(children) != 1 or len(node) or node.text and node.text.strip(' \t\r\n'):
                        raise DataError('mixed/repeated relative-time field alias')
                    match = FIELD_ALIAS.fullmatch(fields.get('path', ''))
                    target = group(match.group(2)) if match else None
                    if target is None:
                        raise DataError('unsupported relative-time field alias')
                    source = None if fields['source'] == 'locale' else parents.locale_id(fields['source'])
                    aliases.append((locale, identity, target, source))
                    continue
                seen_tenses = set()
                for node, fields in children:
                    if node.tag in ('displayName', 'relativePeriod', 'special'):
                        continue
                    if node.tag == 'relative':
                        if len(node):
                            raise DataError('structured relative literal')
                        key = literal_key(fields['type'])
                        literal_keys.add(key)
                        raw.append((locale, identity + ('literal', key), node.text or ''))
                    elif node.tag == 'relativeTime':
                        tense = fields.get('type')
                        if tense not in TENSES or tense in seen_tenses:
                            raise DataError('invalid/repeated relative-time tense')
                        seen_tenses.add(tense)
                        for leaf, attributes in selected(node):
                            if leaf.tag == 'special':
                                continue
                            if leaf.tag != 'relativeTimePattern' or len(leaf):
                                raise DataError('unsupported relative-time tense child/alias')
                            count = attributes.get('count')
                            if count not in COUNTS:
                                raise DataError('invalid relative-time plural count')
                            raw.append((locale, identity + (tense, count), numeric_pattern(leaf.text or '')))
                    else:
                        raise DataError('unknown relative-time field child')
    selectors = tuple((tense, count) for tense in TENSES for count in COUNTS)
    selectors += tuple(('literal', key) for key in sorted(literal_keys))
    records = Records(graph, ((unit, width) + selector for unit in UNITS for width in WIDTHS
                              for selector in selectors))
    for row in raw:
        records.add(*row)
    for locale, identity, target, source in aliases:
        for selector in selectors:
            records.alias(locale, identity + selector, target + selector, source)
    rows, maps, indices = [], [], {}
    for locale in graph.tags:
        for unit in UNITS:
            for width in WIDTHS:
                values = []
                for kind, selector in selectors:
                    resolved = records.resolve(locale, (unit, width, kind, selector))
                    if resolved is not None:
                        values.append((kind, selector, resolved.blocked,
                                       '' if resolved.blocked else resolved.value,
                                       resolved.locale, resolved.key[0], resolved.key[1]))
                value = tuple(sorted(values))
                if value not in indices:
                    indices[value] = len(maps)
                    maps.append(value)
                rows.append((locale, unit, width, indices[value]))
            for tense in TENSES:
                records.require(locale, (unit, 'long', tense, 'other'))
    ordered = tuple(sorted(maps))
    remap = {indices[value]: index for index, value in enumerate(ordered)}
    return {'units': UNITS, 'widths': WIDTHS, 'counts': COUNTS,
            'literal_keys': tuple(sorted(literal_keys)), 'maps': ordered,
            'rows': tuple((locale, unit, width, remap[index]) for locale, unit, width, index in rows),
            'source_records': len(raw), 'source_aliases': len(aliases),
            'alias_fields': len(records.aliases), 'skipped_alternates': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'maps': len(data['maps']), 'literal_keys': data['literal_keys'],
                      'source_records': data['source_records'], 'source_aliases': data['source_aliases'],
                      'alias_fields': data['alias_fields'],
                      'skipped_alternates': data['skipped_alternates']}, sort_keys=True, indent=2))
