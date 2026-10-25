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

"""Resolve standard number patterns; compact/currency-name data has other owners."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import numbering
import parents
from resolve import BLOCK, Records

STYLES = ('decimal', 'scientific', 'percent', 'currency')
FIELDS = ('decimal', 'scientific', 'percent', 'currency-standard', 'currency-accounting',
          'currency-standard-alpha', 'currency-accounting-alpha',
          'currency-standard-none', 'currency-accounting-none')
GROUP_ALIAS = re.compile(r"\.\./([a-z]+)Formats\[@numberSystem=(['\"])([^'\"]+)\2\]")
FORMAT_ALIAS = re.compile(r"\.\./currencyFormat\[@type=(['\"])(standard|accounting)\1\]")
GROUPING = ('minimumGroupingDigits',)


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    systems = tuple(sorted(name for name, (kind, value) in numbering.definitions(sources).items()
                           if kind == 'numeric'))
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    keys = [(system, field) for system in systems for field in FIELDS] + [GROUPING]
    records = Records(graph, keys)
    skipped = 0

    def selected(node, alts=()):
        nonlocal skipped
        result = []
        if (node.text and node.text.strip(' \t\r\n') or
                any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
            raise DataError('unexpected text in number pattern container')
        for child in node:
            fields = schema.attributes(child)
            if 'alt' in fields and (child.tag != 'pattern' or fields['alt'] not in alts):
                skipped += 1
            else:
                result.append((child, fields))
        return result

    def source(fields):
        value = fields['source']
        return None if value == 'locale' else parents.locale_id(value)

    def alias(children):
        aliases = [(node, fields) for node, fields in children if node.tag == 'alias']
        if not aliases:
            return None
        if (len(children) != 1 or len(aliases[0][0]) or
                aliases[0][0].text and aliases[0][0].text.strip(' \t\r\n')):
            raise DataError('mixed/repeated number pattern alias')
        return aliases[0][1]

    def style_fields(style):
        return tuple(field for field in FIELDS
                     if field == style or field.startswith(style + '-'))

    for filename in sources.paths('common/main/'):
        if not filename.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(filename).stem)
        root = read_xml(sources, filename, 'ldml')
        containers = [(node, schema.attributes(node)) for node in root.findall('numbers')]
        skipped += sum('alt' in fields for node, fields in containers)
        containers = [(node, fields) for node, fields in containers if 'alt' not in fields]
        if len(containers) > 1:
            raise DataError('repeated number pattern numbers container')
        for container, attributes in containers:
            groups = selected(container)
            if any(node.tag == 'alias' for node, fields in groups):
                raise DataError('numbers alias requires reviewed number pattern mapping')
            seen = set()
            for group, attributes in groups:
                if group.tag == GROUPING[0]:
                    if len(group):
                        raise DataError('unexpected grouping children')
                    records.add(locale, GROUPING, group.text or '')
                    continue
                if group.tag not in tuple(style + 'Formats' for style in STYLES):
                    continue  # Symbols, currencies and misc patterns have separate collectors.
                style = group.tag[:-7]
                system = attributes.get('numberSystem')
                if system not in systems or (style, system) in seen:
                    raise DataError('missing/nonnumeric/repeated number pattern system')
                seen.add((style, system))
                children = selected(group)
                target = alias(children)
                if target is not None:
                    match = GROUP_ALIAS.fullmatch(target.get('path', ''))
                    if match is None or match.group(1) != style or match.group(3) not in systems:
                        raise DataError('unsupported number pattern group alias')
                    for field in style_fields(style):
                        records.alias(locale, (system, field), (match.group(3), field), source(target))
                    continue
                lengths = []
                for node, fields in children:
                    if node.tag == style + 'FormatLength':
                        if 'type' not in fields:
                            lengths.append(node)
                    elif style != 'currency' or node.tag not in (
                            'currencySpacing', 'unitPattern', 'currencyPatternAppendISO'):
                        raise DataError('unknown standard number pattern group child')
                if len(lengths) > 1:
                    raise DataError('repeated standard number format length')
                for length in lengths:
                    seen_formats = set()
                    children = selected(length)
                    if alias(children) is not None:
                        raise DataError('standard length alias requires reviewed mapping')
                    for form, fields in children:
                        if form.tag != style + 'Format':
                            raise DataError('unknown standard number format child')
                        kind = fields.get('type', 'standard')
                        allowed = ('standard', 'accounting') if style == 'currency' else ('standard',)
                        if kind not in allowed or kind in seen_formats:
                            raise DataError('unknown/repeated standard number format type')
                        seen_formats.add(kind)
                        children = selected(form, ('alphaNextToNumber', 'noCurrency')
                                            if style == 'currency' else ())
                        target = alias(children)
                        prefix = 'currency-' + kind if style == 'currency' else style
                        fields_for_form = tuple(field for field in style_fields(style)
                                                if field == prefix or field.startswith(prefix + '-'))
                        if target is not None:
                            match = FORMAT_ALIAS.fullmatch(target.get('path', ''))
                            if style != 'currency' or match is None:
                                raise DataError('unsupported standard number format alias')
                            for field in fields_for_form:
                                other = field.replace(prefix, 'currency-' + match.group(2), 1)
                                records.alias(locale, (system, field), (system, other), source(target))
                            continue
                        for pattern, fields in children:
                            if (pattern.tag != 'pattern' or len(pattern) or
                                    fields.get('type', 'standard') != 'standard' or
                                    'count' in fields or 'numbers' in fields):
                                raise DataError('unsupported standard number pattern record')
                            suffix = {'alphaNextToNumber': '-alpha', 'noCurrency': '-none'}.get(
                                fields.get('alt'), '')
                            records.add(locale, (system, prefix + suffix), pattern.text or '')
    rows, grouping = [], []
    for locale in graph.tags:
        value = records.require(locale, GROUPING).value
        if not re.fullmatch('[0-9]+', value):
            raise DataError('invalid minimum grouping digits')
        significant = value.lstrip('0') or '0'
        if len(significant) > 10 or not 1 <= int(significant) <= 0xffffffff:
            raise DataError('minimum grouping digits outside uint32')
        grouping.append((locale, int(significant)))
        for system in systems:
            values = [records.require(locale, (system, field)).value for field in FIELDS[:5]]
            if any(not value for value in values):
                raise DataError('empty required number pattern')
            for field in FIELDS[5:]:
                value = records.resolve(locale, (system, field))
                values.append(None if value is None else BLOCK if value.blocked else value.value)
            rows.append((locale, system, tuple(values)))
    return {'systems': systems, 'fields': FIELDS, 'rows': tuple(rows),
            'grouping': tuple(grouping), 'source_records': len(records.values),
            'source_aliases': len(records.aliases), 'skipped_alternates': skipped}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'systems': len(data['systems']), 'fields': len(FIELDS),
                      'source_records': data['source_records'], 'source_aliases': data['source_aliases'],
                      'skipped_alternates': data['skipped_alternates']}, sort_keys=True, indent=2))
