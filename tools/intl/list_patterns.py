#!/usr/bin/env python3
"""Collect typed ListFormat templates from the pinned CLDR locale graph."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents
import pair_template
from resolve import BLOCK, INHERIT, Records

PARTS = ('2', 'start', 'middle', 'end')
VARIANTS = ('standard', 'standard-short', 'standard-narrow',
            'or', 'or-short', 'or-narrow', 'unit', 'unit-short', 'unit-narrow')
ALIAS = re.compile(r"\.\./listPattern(?:\[@type=(['\"])([^'\"]+)\1\])?")


def context(locale, variant, values):
    """CLDR ListPatterns rules, only for matching pair/end default templates."""
    language = locale.split('-', 1)[0]
    kind, old, new = 0, '', ''
    if language == 'es':
        if variant.startswith('or'):
            kind, old, new = 2, '{0} o {1}', '{0} u {1}'
        else:
            kind, old, new = 1, '{0} y {1}', '{0} e {1}'
    elif language == 'he':
        kind, old, new = 3, '{0} ו{1}', '{0} ו-{1}'
    if not kind or old not in (values[0], values[3]):
        return 0, '', ''
    return kind, new if values[0] == old else values[0], new if values[3] == old else values[3]


def pattern(value):
    if value in (INHERIT, BLOCK):
        return value
    return pair_template.validate(value)


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    records = Records(graph, ((variant, part) for variant in VARIANTS for part in PARTS))
    skipped_alternates = 0
    for name in sources.paths('common/main/'):
        if not name.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(name).stem)
        root = read_xml(sources, name, 'ldml')
        groups = []
        for container in root.findall('listPatterns'):
            fields = schema.attributes(container)
            if 'alt' in fields:
                skipped_alternates += 1
            else:
                groups.append(container)
        variants_seen = set()
        if len(groups) > 1:
            raise DataError('repeated listPatterns container: ' + name)
        for container in groups:
            for group in container:
                if group.tag != 'listPattern':
                    raise DataError('unknown listPatterns child: ' + group.tag)
                fields = schema.attributes(group)
                if 'alt' in fields:
                    skipped_alternates += 1
                    continue
                variant = fields.get('type', 'standard')
                if variant not in VARIANTS:
                    raise DataError('unsupported list pattern type: ' + variant)
                if variant in variants_seen:
                    raise DataError('repeated list pattern type: ' + variant)
                variants_seen.add(variant)
                children = []
                for entry in group:
                    if entry.tag not in ('alias', 'listPatternPart'):
                        raise DataError('unknown list pattern child: ' + entry.tag)
                    fields = schema.attributes(entry)
                    if 'alt' in fields:
                        skipped_alternates += 1
                    else:
                        children.append((entry, fields))
                aliases = [(entry, fields) for entry, fields in children if entry.tag == 'alias']
                if aliases:
                    if len(children) != 1:
                        raise DataError('mixed/repeated list pattern alias')
                    entry, fields = aliases[0]
                    if len(entry):
                        raise DataError('unexpected list alias children')
                    if 'path' not in fields:
                        target = variant
                    else:
                        match = ALIAS.fullmatch(fields['path'])
                        if match is None:
                            raise DataError('unsupported list alias XPath: ' + fields['path'])
                        target = match.group(2) or 'standard'
                    source = fields['source']
                    source = None if source == 'locale' else parents.locale_id(source)
                    for part in PARTS:
                        records.alias(locale, (variant, part), (target, part), source)
                    continue
                for entry, fields in children:
                    if len(entry):
                        raise DataError('unknown list pattern part/children')
                    part = fields['type']
                    if part not in PARTS:
                        raise DataError('unknown list pattern part: ' + part)
                    records.add(locale, (variant, part), pattern(entry.text or ''))
    rows, contexts = [], []
    for locale in graph.tags:
        for variant in VARIANTS:
            values = tuple(records.require(locale, (variant, part)).value for part in PARTS)
            rows.append((locale, variant, values))
            contexts.append((locale, variant) + context(locale, variant, values))
    return {'rows': tuple(rows), 'contexts': tuple(contexts), 'source_records': len(records.values),
            'source_aliases': len(records.aliases),
            'skipped_alternates': skipped_alternates,
            'policy': 'standard variants; all published draft states; exact text; checked alias/parent resolution; CLDR ListPatterns context policy 1'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    result = collect(sources)
    print(json.dumps({'rows': len(result['rows']),
                      'unique_templates': len({text for row in result['rows'] for text in row[2]}),
                      'source_records': result['source_records'],
                      'source_aliases': result['source_aliases'],
                      'skipped_alternates': result['skipped_alternates'],
                      'policy': result['policy'],
                      'release': sources.manifest['release']}, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
