#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
# Copyright (c) 2026 Yan-Jie Wang
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

"""Compile matching plural families for gaps in the public ICU4X bake.

Keep one family per distinct rule/range vector and reference it from canonical
locale rows. Runtime prefers public data; this source is used only when a
required category set was lost by the selected library's fallback.
"""
import json
import availability
from embed import WRAPPER_LICENSE
import parents
import plural_rules


def collect(sources):
    graph, _ = availability.complete(parents.load(sources))
    return plural_rules.collect(sources, graph)


def render(data):
    families = []
    indexes = {}
    rows = {}
    origins = {(locale, kind): (source, range_source)
               for locale, kind, source, range_source in data['origins']}
    for locale, kind, rules, ranges in data['rows']:
        key = (rules, ranges)
        if key not in indexes:
            indexes[key] = len(families)
            families.append(key)
        rows.setdefault(locale, [None, None])[kind] = indexes[key]
    if len(families) > 65536:
        raise ValueError('plural supplement family index overflow')
    lines = [WRAPPER_LICENSE,
             '/* CLDR '+data['version']+'; data license: data/intl/LICENSE-CLDR. */',
             '#include "src/intl/plural/snapshot-icu4x.h"', '',
             'const char qjs_icu4x_plural_source_version[] = '+json.dumps(data['version'])+';',
             'const QJSICU4XPluralFamily qjs_icu4x_plural_families[] = {']
    for rules, ranges in families:
        values = dict(rules)
        mask = sum(1 << category for category in values)
        vector = [end for start in range(6) for end in range(6)]
        for start, end, result in ranges:
            vector[start * 6 + end] = result
        lines.append('    {{'+', '.join(json.dumps(values[i]) if i in values else 'NULL'
                                       for i in range(6))+'}, '+str(mask)+', {'+
                     ', '.join(map(str, vector))+'}},')
    lines.extend(['};', '', 'const QJSICU4XPluralSource qjs_icu4x_plural_sources[] = {'])
    for locale, kinds in sorted(rows.items()):
        if any(index is None for index in kinds):
            raise ValueError('incomplete plural source kinds')
        rule_sources = [(origins[locale, kind][0] or 'root') for kind in range(2)]
        range_source = origins[locale, 0][1] or 'root'
        if range_source != (origins[locale, 1][1] or 'root'):
            raise ValueError('plural range source depends on rule type')
        def source_tag(tag):
            return 'und' if tag == 'root' else tag
        lines.append('    {'+json.dumps(locale)+', {'+
                     ', '.join(json.dumps(source_tag(tag)) for tag in rule_sources)+'}, '+
                     json.dumps(source_tag(range_source))+', {'+', '.join(map(str, kinds))+'}},')
    lines.extend(['};', '', 'const size_t qjs_icu4x_plural_source_count =',
                  '    sizeof(qjs_icu4x_plural_sources) / sizeof(*qjs_icu4x_plural_sources);', ''])
    return '\n'.join(lines)
