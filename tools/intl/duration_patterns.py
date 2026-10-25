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

"""Resolve CLDR digital-duration patterns without imposing display policy."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents
from resolve import Records

SKELETONS = ('hm', 'hms', 'ms')
UNIT_ALIAS = re.compile(r"\.\./durationUnit\[@type=(['\"])(hm|hms|ms)\1\]")


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    records = Records(graph, ((name,) for name in SKELETONS))
    skipped, source_records, source_aliases = 0, 0, 0

    def applicable(node):
        nonlocal skipped
        fields = schema.attributes(node)
        if 'alt' in fields:
            skipped += 1
            return False
        if 'validSubLocales' in fields:
            raise DataError('unsupported digital-duration scope')
        return True

    def selected(node):
        if (node.text and node.text.strip(' \t\r\n') or
                any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
            raise DataError('unexpected digital-duration container text')
        return [child for child in node if applicable(child)]

    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        containers = [node for node in root.findall('units') if applicable(node)]
        if len(containers) > 1:
            raise DataError('repeated digital-duration units container')
        seen = set()
        for container in containers:
            for group in selected(container):
                if group.tag in ('unitLength', 'special'):
                    continue  # Textual unit patterns have a separate owner.
                if group.tag != 'durationUnit':
                    raise DataError('unsupported whole units alias/child')
                name = schema.attributes(group).get('type')
                if name not in SKELETONS or name in seen:
                    raise DataError('invalid/repeated digital-duration skeleton')
                seen.add(name)
                children = [child for child in selected(group) if child.tag != 'special']
                aliases = [child for child in children if child.tag == 'alias']
                if aliases:
                    node = aliases[0]
                    fields = schema.attributes(node)
                    match = UNIT_ALIAS.fullmatch(fields.get('path', ''))
                    if (len(children) != 1 or len(node) or
                            node.text and node.text.strip(' \t\r\n') or match is None):
                        raise DataError('unsupported/mixed digital-duration alias')
                    origin = fields['source']
                    records.alias(locale, (name,), (match.group(2),),
                                  None if origin == 'locale' else parents.locale_id(origin))
                    source_aliases += 1
                    continue
                if len(children) > 1:
                    raise DataError('repeated digital-duration pattern')
                for leaf in children:
                    if leaf.tag != 'durationUnitPattern' or len(leaf):
                        raise DataError('unknown/structured digital-duration field')
                    records.add(locale, (name,), leaf.text or '')
                    source_records += 1

    rows = []
    for locale in graph.tags:
        for name in SKELETONS:
            resolved = records.resolve(locale, (name,))
            if resolved is None:
                rows.append((locale, name, 'missing', '', '', ''))
            else:
                rows.append((locale, name, 'blocked' if resolved.blocked else 'value',
                             '' if resolved.blocked else resolved.value,
                             resolved.locale, resolved.key[0]))
    return {'skeletons': SKELETONS, 'rows': tuple(rows),
            'source_records': source_records, 'source_aliases': source_aliases,
            'skipped_alternates': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'],
                      'rows': len(data['rows']),
                      'distinct_patterns': sorted({row[3] for row in data['rows'] if row[2] == 'value'}),
                      'source_records': data['source_records'],
                      'source_aliases': data['source_aliases'],
                      'skipped_alternates': data['skipped_alternates']},
                     sort_keys=True, indent=2, ensure_ascii=False))
