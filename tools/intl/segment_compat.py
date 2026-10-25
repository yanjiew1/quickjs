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

"""Narrow structural corrections for compatible segmentation source data.

Raw collection and strict compilation remain available unchanged. Corrections
produce derived records and a separate receipt, never modified source files.
"""
from collections import namedtuple
import hashlib

from segment_sources import Definition

ROOT = 'common/segments/root.xml'
JAPANESE = 'common/segments/ja.xml'
JAPANESE_VARIABLES = (
    Definition('$Hiragana', r'\p{Hiragana}'),
    Definition('$Hiragana', '($Hiragana $FE*)'),
    Definition('$Ideographic', r'[[\p{Ideographic}] [\u3005 \u3007 \u303B]]'),
    Definition('$Ideographic', '($Ideographic $FE*)'),
)
HISTORICAL_FE = Definition('$FE', '[$Format $Extend $ZWJ]')
ROOT_FE_VARIABLES = (
    Definition('$Extend', r'\p{Word_Break=Extend}'),
    Definition('$Format', r'[\p{Word_Break=Format}]'),
    Definition('$ZWJ', r'\p{Word_Break=ZWJ}'),
)
Correction = namedtuple('Correction', 'name commit source root_sha256 source_sha256 '
                        'insert_before identifier expression historical_source')
CorrectedSources = namedtuple('CorrectedSources', 'records corrections')


def japanese_word_fe(sources, records):
    """Restore the historical FE union only for its known declaration shape.

    CLDR-18917 removed root's FE declaration when introducing ignore remaps,
    while the Japanese wrappers still reference it. This is a compatibility
    correction, not a TR35 fallback for unknown names. Already-correct or other
    source data passes through unchanged for ordinary strict compilation.
    """
    records = tuple(records)
    unchanged = CorrectedSources(records, ())
    selected = [index for index, row in enumerate(records)
                if row.locale == 'ja' and row.kind == 'WordBreak']
    if len(selected) != 1:
        return unchanged
    index = selected[0]
    row = records[index]
    if row.source != JAPANESE or row.variables != JAPANESE_VARIABLES:
        return unchanged
    roots = [item for item in records if item.locale == 'root' and item.kind == 'WordBreak']
    if (len(roots) != 1 or roots[0].source != ROOT or
            any(item.identifier == '$FE' for item in roots[0].variables or ())):
        return unchanged
    names = frozenset(item.identifier for item in ROOT_FE_VARIABLES)
    dependencies = tuple(item for item in roots[0].variables or () if item.identifier in names)
    if dependencies != ROOT_FE_VARIABLES:
        return unchanged
    hashes = {name: hashlib.sha256(sources.read(name)).hexdigest()
              for name in (ROOT, JAPANESE)}
    # Insert before the first wrapper, not before its initial Hiragana set;
    # capture_variables will retain the same previous self-binding semantics.
    changed = row._replace(variables=row.variables[:1] + (HISTORICAL_FE,) + row.variables[1:])
    result = tuple(changed if position == index else item for position, item in enumerate(records))
    receipt = Correction('japanese-word-fe', sources.pin.get('commit'), JAPANESE,
                         hashes[ROOT], hashes[JAPANESE],
                         1, HISTORICAL_FE.identifier, HISTORICAL_FE.expression,
                         '56a2de16366cd5bfe18628e941ea84849cfd4153')
    return CorrectedSources(result, (receipt,))
