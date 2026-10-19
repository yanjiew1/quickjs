#!/usr/bin/env python3
"""Collect locale aliases and Unicode extension metadata, not service coverage."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents

ALIASES = ('language', 'script', 'territory', 'variant', 'subdivision')
VALUE_TYPES = ('single', 'multiple', 'incremental', 'any')
VARIABLE_TYPES = frozenset(('CODEPOINTS', 'REORDER_CODE', 'RG_KEY_VALUE',
                            'SCRIPT_CODE', 'SUBDIVISION_CODE', 'PRIVATE_USE'))


def identifier(value):
    if not re.fullmatch(r'[A-Za-z0-9]+(?:[-_][A-Za-z0-9]+)*', value):
        raise DataError('invalid CLDR locale metadata identifier: ' + value)
    return value.replace('_', '-').lower()


def tokens(value):
    parts = value.split(' ') if value else ()
    if any(not re.fullmatch(r'[\x21-\x7e]+', part) for part in parts):
        raise DataError('invalid CLDR metadata token list')
    if len(set(parts)) != len(parts):
        raise DataError('duplicate CLDR metadata tokens')
    return tuple(parts)


def unique(table, key, value):
    if key in table:
        raise DataError('duplicate locale metadata identity: ' + repr(key))
    table[key] = value


def check_preferred(entries):
    finished = set()
    for identity in entries:
        active, target = set(), identity
        while target not in finished:
            if target in active or target not in entries:
                raise DataError('cyclic/unknown preferred BCP47 identity')
            active.add(target)
            preferred = entries[target]
            if not preferred:
                break
            target = target[:-1] + (preferred,)
        finished.update(active)


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    root = read_xml(sources, 'common/supplemental/supplementalMetadata.xml', 'supplementalData')
    aliases = {}
    containers = root.findall('./metadata/alias')
    if len(containers) != 1:
        raise DataError('missing/repeated locale alias container')
    for node in containers[0]:
        if node.tag not in tuple(kind + 'Alias' for kind in ALIASES):
            # zoneAlias is owned by the timezone data pipeline, not locale IDs.
            if node.tag == 'zoneAlias':
                continue
            raise DataError('unknown locale alias kind: ' + node.tag)
        if len(node):
            raise DataError('unexpected locale alias children')
        fields = schema.attributes(node)
        kind = node.tag[:-5]
        key = identifier(fields['type'])
        replacements = tuple(identifier(value) for value in tokens(fields['replacement']))
        if not replacements or len(set(replacements)) != len(replacements):
            raise DataError('empty/repeated locale alias replacements')
        if kind != 'territory' and kind != 'subdivision' and len(replacements) != 1:
            raise DataError('multiple nonregional alias replacements')
        unique(aliases, (kind, key), replacements)
    schema = Schema(sources.read('common/dtd/ldmlBCP47.dtd'))
    keys, types = {}, {}
    for name in sources.paths('common/bcp47/'):
        if not name.endswith('.xml'):
            continue
        root = read_xml(sources, name, 'ldmlBCP47')
        if any(child.tag not in ('version', 'generation', 'cldrVersion', 'keyword') for child in root):
            raise DataError('unsupported BCP47 root content')
        groups = root.findall('keyword')
        if len(groups) != 1:
            raise DataError('missing/repeated BCP47 keyword container')
        schema.attributes(groups[0])
        for key in groups[0]:
            if key.tag != 'key':
                raise DataError('unknown BCP47 keyword child: ' + key.tag)
            fields = schema.attributes(key)
            # These LDML semantic defaults are #IMPLIED in the DTD.
            extension, keyword = fields.get('extension', 'u'), fields['name']
            grammar = r'[0-9a-z][a-z]' if extension == 'u' else r'[a-z][0-9]'
            if extension not in ('u', 't') or not re.fullmatch(grammar, keyword):
                raise DataError('invalid BCP47 extension/key')
            value_type = fields.get('valueType', 'single')
            if value_type not in VALUE_TYPES:
                raise DataError('unknown BCP47 key value type')
            unique(keys, (extension, keyword),
                   (value_type, tokens(fields.get('alias', '')), fields.get('preferred', ''),
                    fields['deprecated'] == 'true'))
            for entry in key:
                if entry.tag != 'type' or len(entry):
                    raise DataError('unknown BCP47 type content')
                fields = schema.attributes(entry)
                spelling = fields['name']
                # Uppercase names are schema variables such as CODEPOINTS or
                # REORDER_CODE, not literal Unicode extension type values.
                variable = bool(re.fullmatch(r'[A-Z][A-Z0-9_]*', spelling))
                if variable and spelling not in VARIABLE_TYPES:
                    raise DataError('unknown indirect BCP47 type')
                if not variable and not re.fullmatch(r'[a-z0-9]{3,8}(?:-[a-z0-9]{3,8})*', spelling):
                    raise DataError('invalid literal BCP47 type')
                iana, region = fields.get('iana', ''), fields.get('region', '')
                if iana and not re.fullmatch(r'[A-Za-z]+(?:/[A-Za-z_-]+)+', iana):
                    raise DataError('invalid IANA type metadata')
                if region and not re.fullmatch('[A-Z]{2}', region):
                    raise DataError('invalid type region metadata')
                unique(types, (extension, keyword, spelling),
                       (tokens(fields.get('alias', '')), fields.get('preferred', ''),
                        fields['deprecated'] == 'true', variable, iana, region))
    # Validate preferred-name chains within their own extension/key. Aliases
    # may contain legacy non-BCP47 spellings; they do not widen runtime syntax.
    check_preferred({identity: value[2] for identity, value in keys.items()})
    check_preferred({identity: value[1] for identity, value in types.items()})
    return {'aliases': tuple((kind, key, aliases[(kind, key)]) for kind, key in sorted(aliases)),
            'keys': tuple((extension, key) + keys[(extension, key)] for extension, key in sorted(keys)),
            'types': tuple(identity + types[identity] for identity in sorted(types)),
            'likely': tuple(sorted(graph.likely.items()))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    result = collect(acquire.Sources(args.sources))
    print(json.dumps({key: len(value) for key, value in result.items()}, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
