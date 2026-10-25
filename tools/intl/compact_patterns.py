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

"""Resolve sparse compact-number maps without expanding every plural leaf."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
from number_patterns import GROUP_ALIAS
import numbering
import parents
from resolve import SparseMaps

KINDS = (('decimal', 'short'), ('decimal', 'long'), ('currency', 'short'))
COUNTS = ('0', '1', 'zero', 'one', 'two', 'few', 'many', 'other')
VARIANTS = ('standard', 'alphaNextToNumber', 'noCurrency')
LENGTH_ALIAS = re.compile(r"\.\./([a-z]+)FormatLength\[@type=(['\"])(short|long)\2\]")


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    systems = tuple(sorted(name for name, (kind, value) in numbering.definitions(sources).items()
                           if kind == 'numeric'))
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    records = SparseMaps(graph, ((system, style, width) for system in systems
                                 for style, width in KINDS), 'compact')
    skipped = 0

    def children(node, currency_patterns=False):
        nonlocal skipped
        if (node.text and node.text.strip(' \t\r\n') or
                any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
            raise DataError('unexpected compact container text')
        result = []
        for child in node:
            fields = schema.attributes(child)
            alt = fields.get('alt')
            if alt is not None and not (currency_patterns and child.tag == 'pattern' and
                                        alt in VARIANTS[1:]):
                skipped += 1
            else:
                result.append((child, fields))
        return result

    def alias(nodes):
        found = [(node, fields) for node, fields in nodes if node.tag == 'alias']
        if not found:
            return None
        node, fields = found[0]
        if len(nodes) != 1 or len(node) or node.text and node.text.strip(' \t\r\n'):
            raise DataError('mixed/repeated compact alias')
        return fields

    def source(fields):
        return None if fields['source'] == 'locale' else parents.locale_id(fields['source'])

    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        containers = [(node, schema.attributes(node)) for node in root.findall('numbers')]
        skipped += sum('alt' in fields for node, fields in containers)
        containers = [node for node, fields in containers if 'alt' not in fields]
        if len(containers) > 1:
            raise DataError('repeated compact numbers container')
        for container in containers:
            seen = set()
            for group, fields in children(container):
                if group.tag == 'alias':
                    raise DataError('numbers alias requires reviewed compact mapping')
                if group.tag not in ('decimalFormats', 'currencyFormats'):
                    continue
                style, system = group.tag[:-7], fields.get('numberSystem')
                if system not in systems or (style, system) in seen:
                    raise DataError('missing/nonnumeric/repeated compact numbering group')
                seen.add((style, system))
                nodes = children(group)
                target = alias(nodes)
                widths = tuple(width for candidate, width in KINDS if candidate == style)
                if target is not None:
                    match = GROUP_ALIAS.fullmatch(target.get('path', ''))
                    if match is None or match.group(1) != style or match.group(3) not in systems:
                        raise DataError('unsupported compact numbering-group alias')
                    for width in widths:
                        records.alias(locale, (system, style, width),
                                      (match.group(3), style, width), source(target))
                    continue
                seen_lengths = set()
                for length, fields in nodes:
                    if length.tag != style + 'FormatLength':
                        if style == 'currency' and length.tag in (
                                'currencySpacing', 'unitPattern', 'currencyPatternAppendISO'):
                            continue
                        raise DataError('unknown compact format group child')
                    width = fields.get('type')
                    if width not in widths:
                        continue  # Standard formats have a separate owner.
                    if width in seen_lengths:
                        raise DataError('repeated compact format length')
                    seen_lengths.add(width)
                    key = system, style, width
                    nodes = children(length)
                    target = alias(nodes)
                    if target is not None:
                        match = LENGTH_ALIAS.fullmatch(target.get('path', ''))
                        if match is None or match.group(1) != style or match.group(3) not in widths:
                            raise DataError('unsupported compact length alias')
                        records.alias(locale, key, (system, style, match.group(3)), source(target))
                        continue
                    if len(nodes) != 1:
                        raise DataError('compact length requires one standard format')
                    form, fields = nodes[0]
                    if form.tag != style + 'Format' or fields.get('type', 'standard') != 'standard':
                        raise DataError('unsupported compact format scope')
                    for pattern, fields in children(form, style == 'currency'):
                        if pattern.tag != 'pattern' or len(pattern) or 'numbers' in fields:
                            raise DataError('unsupported compact pattern leaf')
                        magnitude = fields.get('type', '')
                        count = fields.get('count')
                        if re.fullmatch('10*', magnitude) is None or count not in COUNTS:
                            raise DataError('invalid compact magnitude/plural selector')
                        variant = fields.get('alt', 'standard')
                        records.add(locale, key, (len(magnitude) - 1, count, variant), pattern.text or '')
    rows, maps, indices = [], [], {}
    for locale in graph.tags:
        cache, frozen = {}, {}
        for system in systems:
            for kind, (style, width) in enumerate(KINDS):
                resolved = records.resolve(locale, (system, style, width), cache)
                if not resolved and style == 'decimal':
                    raise DataError('missing compact format map: ' + repr((locale, system, style, width)))
                identity = id(resolved)
                if identity not in frozen:
                    frozen[identity] = tuple(sorted((magnitude, count, variant, value)
                                                   for (magnitude, count, variant), value in resolved.items()))
                value = frozen[identity]
                if value not in indices:
                    indices[value] = len(maps)
                    maps.append(value)
                rows.append((locale, system, kind, indices[value]))
    ordered = tuple(sorted(maps))
    remap = {indices[value]: index for index, value in enumerate(ordered)}
    return {'systems': systems, 'kinds': KINDS, 'maps': ordered,
            'rows': tuple((locale, system, kind, remap[index]) for locale, system, kind, index in rows),
            'source_records': sum(len(value) for value in records.values.values()),
            'source_aliases': len(records.aliases), 'skipped_alternates': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'maps': len(data['maps']), 'systems': len(data['systems']),
                      'source_records': data['source_records'], 'source_aliases': data['source_aliases'],
                      'skipped_alternates': data['skipped_alternates']}, sort_keys=True, indent=2))
