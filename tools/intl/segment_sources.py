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

"""Collect ordered CLDR segmentation sources, before inheritance or execution."""
import argparse
from collections import namedtuple
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents

Definition = namedtuple('Definition', 'identifier expression')
Segmentation = namedtuple('Segmentation', 'locale kind variables rules suppressions source')
KINDS = ('GraphemeClusterBreak', 'WordBreak', 'SentenceBreak')
VARIABLE = re.compile(r'\$[A-Za-z0-9_]+')
RULE = re.compile(r'[0-9]+(?:\.[0-9]+)?')


def element_only(node):
    # XML element-only content permits XML whitespace, not arbitrary Unicode
    # space. Never silently drop text that the typed records cannot represent.
    if ((node.text or '').strip(' \t\r\n') or
            any((child.tail or '').strip(' \t\r\n') for child in node)):
        raise DataError('unexpected segmentation container text: ' + node.tag)


def definitions(schema, group, child, identifier, skipped):
    element_only(group)
    result = []
    seen = set()
    for entry in group:
        if entry.tag != child or len(entry):
            raise DataError('unsupported segmentation definition: ' + entry.tag)
        fields = schema.attributes(entry)
        if 'alt' in fields:
            skipped.append(1)
            continue
        key = fields['id']
        if not identifier.fullmatch(key):
            raise DataError('invalid segmentation definition ID: ' + key)
        if child == 'rule' and key in seen:
            raise DataError('repeated segmentation rule ID: ' + key)
        seen.add(key)
        # Repeated variable IDs are valid: an expression can use its previous
        # definition. Keep exact text and order for the later semantic compiler.
        result.append(Definition(key, entry.text or ''))
    return tuple(result)


def segmentation(schema, group, locale, name, skipped):
    element_only(group)
    fields = schema.attributes(group)
    kind = fields['type']
    if kind not in KINDS:
        raise DataError('unknown segmentation kind: ' + kind)
    values = {'variables': None, 'segmentRules': None, 'suppressions': None}
    last = -1
    for node in group:
        if node.tag not in values:
            raise DataError('unsupported segmentation child: ' + node.tag)
        position = ('variables', 'segmentRules', 'suppressions').index(node.tag)
        if position <= last:
            raise DataError('repeated or unordered segmentation container')
        last = position
        attributes = schema.attributes(node)
        if node.tag == 'variables':
            value = definitions(schema, node, 'variable', VARIABLE, skipped)
        elif node.tag == 'segmentRules':
            value = definitions(schema, node, 'rule', RULE, skipped)
        else:
            element_only(node)
            if attributes.get('type', 'standard') != 'standard':
                raise DataError('unsupported segmentation suppression type')
            value = []
            for entry in node:
                if entry.tag != 'suppression' or len(entry):
                    raise DataError('unsupported segmentation suppression')
                if 'alt' in schema.attributes(entry):
                    skipped.append(1)
                    continue
                value.append(entry.text or '')
            value = tuple(value)
        values[node.tag] = value
    # None and () retain absent versus explicitly empty source containers.
    # Parent resolution and suppression policy are deliberately separate steps.
    return Segmentation(locale, kind, values['variables'], values['segmentRules'],
                        values['suppressions'], name)


def collect(sources):
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    records, excluded, skipped = [], [], []
    locales = set()
    for name in sorted(sources.paths('common/segments/')):
        if not name.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(name).stem)
        if locale in locales:
            raise DataError('repeated segmentation source locale: ' + locale)
        locales.add(locale)
        root = read_xml(sources, name, 'ldml')
        schema.attributes(root)
        element_only(root)
        if any(node.tag not in ('identity', 'segmentations') for node in root):
            raise DataError('unexpected segmentation document content: ' + name)
        containers = []
        for container in root.findall('segmentations'):
            if 'alt' in schema.attributes(container):
                skipped.append(1)
            else:
                containers.append(container)
        if len(containers) > 1:
            raise DataError('repeated segmentations container: ' + name)
        seen = set()
        for container in containers:
            element_only(container)
            for group in container:
                if group.tag != 'segmentation':
                    raise DataError('unsupported segmentations child: ' + group.tag)
                fields = schema.attributes(group)
                if 'alt' in fields:
                    skipped.append(1)
                    continue
                kind = fields['type']
                if kind in seen:
                    raise DataError('repeated segmentation kind: ' + kind)
                seen.add(kind)
                # Intl.Segmenter has no line granularity. Inventory these
                # groups explicitly without claiming to compile their contents.
                if kind == 'LineBreak':
                    excluded.append((locale, name))
                else:
                    records.append(segmentation(schema, group, locale, name, skipped))
    if 'root' not in locales:
        raise DataError('missing root segmentation source')
    return {'records': tuple(records), 'locales': tuple(sorted(locales)),
            'excluded_line_groups': tuple(excluded), 'skipped_alternates': len(skipped)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    data = collect(acquire.Sources(args.sources))
    print(json.dumps({'source_locales': len(data['locales']),
                      'segmentation_records': len(data['records']),
                      'variable_definitions': sum(len(row.variables or ()) for row in data['records']),
                      'rule_definitions': sum(len(row.rules or ()) for row in data['records']),
                      'suppressions': sum(len(row.suppressions or ()) for row in data['records']),
                      'excluded_line_groups': len(data['excluded_line_groups']),
                      'skipped_alternates': data['skipped_alternates']}, indent=2, sort_keys=True))


if __name__ == '__main__':
    main()
