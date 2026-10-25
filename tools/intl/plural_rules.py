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

"""Resolve CLDR cardinal/ordinal conditions and explicit plural-range records."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents

CATEGORIES = ('zero', 'one', 'two', 'few', 'many', 'other')
PATHS = ('common/supplemental/plurals.xml',
         'common/supplemental/ordinals.xml',
         'common/supplemental/pluralRanges.xml')
SPACE = re.compile('[\x09-\x0d\x20\x85\u200e\u200f\u2028\u2029]+')


def relation(text):
    """Validate a condition after discarding non-executable sample metadata."""
    text = SPACE.sub(' ', text.split('@', 1)[0]).strip(' ')
    if any(ord(c) < 0x20 or ord(c) > 0x7e for c in text):
        raise DataError('non-ASCII plural condition token')
    at = 0

    def space():
        nonlocal at
        while at < len(text) and text[at] == ' ':
            at += 1

    def take(token):
        nonlocal at
        space()
        if text.startswith(token, at):
            at += len(token)
            return True
        return False

    def integer():
        nonlocal at
        space()
        start = at
        while at < len(text) and '0' <= text[at] <= '9':
            at += 1
        if at == start:
            raise DataError('missing plural integer')
        significant = text[start:at].lstrip('0') or '0'
        if len(significant) > 10 or int(significant) > 0xffffffff:
            raise DataError('plural constant exceeds uint32')
        return int(significant)

    def item():
        nonlocal at
        space()
        if at == len(text) or text[at] not in 'nivwftce':
            raise DataError('invalid plural operand')
        at += 1
        if take('%') or take('mod'):
            if integer() == 0:
                raise DataError('zero plural modulus')
        single = False
        if take('=') or take('!='):
            pass
        elif take('is'):
            take('not')
            single = True
        else:
            take('not')
            if not (take('in') or take('within')):
                raise DataError('invalid plural relation operator')
        while True:
            lower = integer()
            if not single and take('..') and lower > integer():
                raise DataError('reversed plural range')
            if single or not take(','):
                break

    if text:
        item()
        while at < len(text):
            if not (take('and') or take('or')):
                raise DataError('unexpected plural condition token')
            item()
    return text


def locales(fields):
    result = tuple(parents.locale_id(value) for value in fields['locales'].split(' '))
    if not result or len(set(result)) != len(result):
        raise DataError('empty/repeated plural locale')
    return result


def structure(node):
    if (node.text and node.text.strip(' \t\r\n') or
            any(child.tail and child.tail.strip(' \t\r\n') for child in node)):
        raise DataError('unexpected text in plural container')


def collect(sources, graph=None):
    graph = parents.load(sources) if graph is None else graph
    if 'plurals' not in graph.components:
        raise DataError('undeclared plural inheritance component')
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    definitions = []
    ranges = {}
    for identity, filename in enumerate(PATHS):
        root = read_xml(sources, filename, 'supplementalData')
        groups = root.findall('plurals')
        if len(groups) != 1:
            raise DataError('missing/repeated plural container')
        container = groups[0]
        structure(container)
        fields = schema.attributes(container)
        expected = 'ordinal' if identity == 1 else 'cardinal'
        if fields.get('type', 'cardinal') != expected:
            raise DataError('incorrect plural source type')
        mapping = {}
        for group in container:
            if group.tag != ('pluralRanges' if identity == 2 else 'pluralRules'):
                raise DataError('unknown plural source group')
            fields = schema.attributes(group)
            structure(group)
            values = {}
            for node in group:
                if len(node) or node.tag != ('pluralRange' if identity == 2 else 'pluralRule'):
                    raise DataError('unknown plural source record')
                attributes = schema.attributes(node)
                if identity == 2:
                    if (node.text and node.text.strip(' \t\r\n') or
                            'start' not in attributes or 'end' not in attributes):
                        raise DataError('plural range requires explicit category endpoints')
                    key = tuple(CATEGORIES.index(attributes[name]) for name in ('start', 'end'))
                    value = CATEGORIES.index(attributes['result'])
                else:
                    key = CATEGORIES.index(attributes['count'])
                    value = relation(node.text or '')
                    if bool(value) != (key != CATEGORIES.index('other')):
                        raise DataError('named plural condition missing or other condition nonempty')
                if key in values:
                    raise DataError('repeated plural category/range pair')
                values[key] = value
            if identity != 2 and CATEGORIES.index('other') not in values:
                raise DataError('plural rules lack other category')
            ordered = (tuple((a, b, value) for (a, b), value in sorted(values.items()))
                       if identity == 2 else tuple(sorted(values.items())))
            for locale in locales(fields):
                if locale in mapping:
                    raise DataError('repeated plural source locale')
                mapping[locale] = ordered
        if identity == 2:
            ranges = mapping
        else:
            if 'root' not in mapping:
                raise DataError('plural source lacks root rules')
            definitions.append(mapping)

    def resolve(mapping, locale, required):
        for parent in graph.chain(locale, 'plurals'):
            if parent in mapping:
                return mapping[parent], parent
        if required:
            raise DataError('unresolved required plural rules')
        return (), None

    rows, origins = [], []
    for locale in graph.tags:
        selected_ranges, range_source = resolve(ranges, locale, False)
        for kind, mapping in enumerate(definitions):
            rules, source = resolve(mapping, locale, True)
            rows.append((locale, kind, rules, selected_ranges))
            origins.append((locale, kind, source, range_source))
    return {'rows': tuple(rows), 'origins': tuple(origins),
            'source_locales': tuple(len(mapping) for mapping in definitions) + (len(ranges),),
            'conditions': tuple(sorted({rule for mapping in definitions
                                        for rules in mapping.values() for category, rule in rules}))}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    data = collect(sources)
    print(json.dumps({'release': sources.manifest['release'], 'rows': len(data['rows']),
                      'conditions': len(data['conditions']),
                      'source_locales': data['source_locales']}, sort_keys=True, indent=2))
