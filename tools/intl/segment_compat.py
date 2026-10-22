"""Explicit source corrections for the authenticated segmentation profile.

Raw collection and strict compilation remain available unchanged. Corrections
produce derived records and a separate receipt, never modified source files.
"""
from collections import namedtuple
import hashlib

from cldr_xml import DataError
from segment_sources import Definition

PIN = '7cdeb9e7fac52c59f22fc7248b05f989ee5689c2'
ROOT = 'common/segments/root.xml'
JAPANESE = 'common/segments/ja.xml'
HASHES = {
    ROOT: '1f9130f21b3e72ff14d73100bb12297abebd248df6c1d292aa275c0aea60317c',
    JAPANESE: 'df79236c2d1dafe54c61880a2954fa715b736af1edd6e98ac50154f49bd2063d',
}
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
    """Restore the removed historical FE union for exactly the pinned tailoring.

    CLDR-18917 removed root's FE declaration when introducing ignore remaps,
    but Japanese's older wrappers still reference it. This is a native source
    compatibility correction, not a TR35 rule for unknown names. Hashes and
    declaration shape must both match before insertion into a derived stream.
    Records must come from unmodified segment_sources.collect(sources); the
    additional declaration guards check the inserted union's dependencies too.
    """
    if sources.pin['commit'] != PIN:
        raise DataError('unreviewed CLDR segmentation correction source pin')
    for name, expected in HASHES.items():
        if hashlib.sha256(sources.read(name)).hexdigest() != expected:
            raise DataError('changed segmentation correction source: ' + name)
    selected = [index for index, row in enumerate(records)
                if row.locale == 'ja' and row.kind == 'WordBreak']
    if len(selected) != 1:
        raise DataError('missing or repeated Japanese word tailoring')
    index = selected[0]
    row = records[index]
    if row.source != JAPANESE or row.variables != JAPANESE_VARIABLES:
        raise DataError('changed Japanese FE correction declaration shape')
    roots = [item for item in records if item.locale == 'root' and item.kind == 'WordBreak']
    if (len(roots) != 1 or roots[0].source != ROOT or
            any(item.identifier == '$FE' for item in roots[0].variables or ())):
        raise DataError('unexpected root FE binding in correction profile')
    names = frozenset(item.identifier for item in ROOT_FE_VARIABLES)
    dependencies = tuple(item for item in roots[0].variables or () if item.identifier in names)
    if dependencies != ROOT_FE_VARIABLES:
        raise DataError('changed inherited FE dependency declarations')
    # Insert before the first wrapper, not before its initial Hiragana set;
    # capture_variables will retain the same previous self-binding semantics.
    changed = row._replace(variables=row.variables[:1] + (HISTORICAL_FE,) + row.variables[1:])
    result = tuple(changed if position == index else item for position, item in enumerate(records))
    receipt = Correction('japanese-word-fe', PIN, JAPANESE, HASHES[ROOT], HASHES[JAPANESE],
                         1, HISTORICAL_FE.identifier, HISTORICAL_FE.expression,
                         '56a2de16366cd5bfe18628e941ea84849cfd4153')
    return CorrectedSources(result, (receipt,))
