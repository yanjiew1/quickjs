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

"""Prepare only unit source layers and component grammar missing from ICU4X.

Public unit patterns remain preferred at runtime. These matching-CLDR layers
retain leaf provenance, explicit/grammatical selectors and eligibility, plus
missing units, cases and division templates. No native plural rules or unrelated
Intl data is included. Reuse the existing section codecs and C composer.
"""
import hashlib
from pathlib import Path
import re

import availability
import bundle
from cldr_xml import DataError
from embed import WRAPPER_LICENSE, byte_array, c_string
import parents
import tables
import unit_patterns
import unit_tables
import unit_grammar
import unit_grammar_tables


FORMAT_FIELDS = ('unitPattern', 'perUnitPattern', 'compoundUnitPattern')

def sanctioned_groups():
    definitions = Path(__file__).resolve().parents[2] / 'src/intl/unit/identifiers.def'
    singles = re.findall(r'^QJS_INTL_UNIT\("[a-z-]+", "([a-z-]+)"\)$',
                         definitions.read_text(), re.M)
    if not singles or len(singles) != len(set(singles)):
        raise DataError('invalid sanctioned unit definitions')
    compounds = re.findall(r'^QJS_INTL_UNIT_PAIR\("[a-z-]+", "[a-z-]+", "([a-z-]+)"\)$',
                           definitions.read_text(), re.M)
    if not compounds or len(compounds) != len(set(compounds)):
        raise DataError('invalid precomposed unit definitions')
    return tuple(sorted({('unit', name) for name in tuple(singles) + tuple(compounds)} |
                        {('compound', 'per')}))


def collect(sources, selected_groups=None):
    graph, _ = availability.complete(parents.load(sources))
    selected = sanctioned_groups() if selected_groups is None else selected_groups
    units = unit_patterns.collect(sources, graph, selected_groups=selected,
                                  selected_fields=FORMAT_FIELDS)
    grammar = unit_grammar.collect(sources, graph)
    return dict(graph=graph, units=units, grammar=grammar)


def encode(data):
    graph, units, grammar = data['graph'], data['units'], data['grammar']
    tags = graph.tags
    pool = tuple(sorted(set(tags) | unit_tables.strings(units) |
                        unit_grammar_tables.strings(grammar), key=lambda value: value.encode('utf-8')))
    ids = {value: index for index, value in enumerate(pool)}
    sections = {
        bundle.STRINGS: tables.encode_strings(pool),
        bundle.LOCALES: tables.encode_locales(tags, graph, ids),
        bundle.UNIT_PATTERNS: unit_tables.encode(units, tags, ids),
        bundle.UNIT_GRAMMAR: unit_grammar_tables.encode(grammar, tags, ids),
    }
    # The identity describes generated content, not an acquisition pin or a
    # runtime authentication check. Custom compatible source trees remain valid.
    identity = hashlib.sha256(data['version'].encode('ascii') +
                              b''.join(raw for count, raw in sections.values())).digest()
    binary = bundle.pack(sections, identity, 'native')
    tables.validate(binary)
    return binary


def render(data):
    lines = ['/* Generated ICU4X unit/grammar supplement; do not edit.',
             ' * Derived from matching Unicode CLDR; Copyright Unicode, Inc.',
             ' * See data/intl/LICENSE-CLDR and data/intl/pin.json.',
             ' */', '#include "src/intl/icu4x/unit-data.h"', '',
             'const char qjs_icu4x_unit_version[] = '+c_string(data['version'])+';', '']
    return WRAPPER_LICENSE + '\n'.join(lines + byte_array(encode(data), 'qjs_icu4x_unit'))
