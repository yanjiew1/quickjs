#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Resolve CLDR division-unit component plural and case derivations."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents

FEATURES = ('plural', 'case')
STRUCTURES = ('per', 'times', 'power', 'prefix')
PLURALS = ('zero', 'one', 'two', 'few', 'many', 'other')
TOKEN = re.compile('[a-z]+')


def structure(node):
    if (node.text and node.text.strip(' \t\r\n') or
            any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
        raise DataError('unexpected grammatical derivation container text')


def component_value(feature, value):
    if (value != 'compound' and
            (value not in PLURALS if feature == 'plural' else TOKEN.fullmatch(value) is None)):
        raise DataError('invalid grammatical component value')
    return value


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    if 'grammaticalFeatures' not in graph.components:
        raise DataError('undeclared grammatical inheritance component')
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    root = read_xml(sources, 'common/supplemental/grammaticalFeatures.xml', 'supplementalData')
    containers = root.findall('grammaticalData')
    if len(containers) != 1:
        raise DataError('missing/repeated grammatical data container')
    container = containers[0]
    schema.attributes(container)
    structure(container)
    records, seen = {}, set()
    skipped = 0
    for group in container:
        fields = schema.attributes(group)
        if group.tag == 'grammaticalFeatures':
            # Feature inventories describe other API contexts too; this owner
            # consumes only rules deriving division components, not inventories.
            continue
        if group.tag != 'grammaticalDerivations':
            raise DataError('unknown grammatical data child')
        structure(group)
        locales = tuple(parents.locale_id(value) for value in fields['locales'].split(' '))
        if not locales or len(set(locales)) != len(locales):
            raise DataError('empty/repeated grammatical derivation locale')
        for entry in group:
            fields = schema.attributes(entry)
            if entry.tag not in ('deriveComponent', 'deriveCompound'):
                raise DataError('unknown grammatical derivation child')
            if len(entry) or entry.text and entry.text.strip(' \t\r\n'):
                raise DataError('structured grammatical derivation')
            feature, form = fields['feature'], fields['structure']
            if form not in STRUCTURES:
                raise DataError('unsupported grammatical derivation structure')
            if entry.tag == 'deriveComponent':
                if feature not in FEATURES:
                    raise DataError('unsupported grammatical component feature')
                values = tuple(component_value(feature, fields[name]) for name in ('value0', 'value1'))
            elif entry.tag == 'deriveCompound':
                if (feature != 'gender' or fields['value'] not in ('0', '1') and
                        TOKEN.fullmatch(fields['value']) is None):
                    raise DataError('unsupported grammatical compound derivation')
            for locale in locales:
                key = locale, entry.tag, feature, form
                if key in seen:
                    raise DataError('duplicate grammatical derivation')
                seen.add(key)
                if entry.tag == 'deriveComponent' and form == 'per':
                    records[locale, feature] = values
                else:
                    skipped += 1
    if any(('root', feature) not in records for feature in FEATURES):
        raise DataError('missing root division component derivation')
    rows = []
    for locale in graph.tags:
        for feature in FEATURES:
            # Supplemental grammar is sparse per feature/structure, not per
            # language block. A plural override must not erase inherited case.
            origin = next(tag for tag in graph.chain(locale, 'grammaticalFeatures')
                          if (tag, feature) in records)
            rows.append((locale, feature, origin) + records[origin, feature])
    return {'features': FEATURES, 'rows': tuple(rows),
            'source_records': len(records), 'skipped_derivations': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps(dict(data, release=sources.manifest['release']), sort_keys=True, indent=2))
