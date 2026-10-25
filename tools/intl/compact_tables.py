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

"""Shared sparse compact-number maps and compiled forms in QJIN section 18."""
from collections import namedtuple
import struct

import bundle
import compact_patterns as source
import compact_pattern_syntax as syntax
import number_pattern_syntax as tokens
from resolve import BLOCK

VERSION = 1
HEADER = 8
BLOCKED, EMPTY = 0xfffffffe, 0xfffffffd
KIND_COUNT = len(source.KINDS)
ENTRY_WIDTH, PATTERN_WIDTH = 4, 6

# Numbering-definition indices, locale-ordered map-index rows and compiled maps.
Prepared = namedtuple('Prepared', 'systems rows maps')


def select(data, tags):
    selected = set(tags)
    rows = tuple(row for row in data['rows'] if row[0] in selected)
    used = sorted({index for locale, system, kind, index in rows})
    if any(type(index) is not int or not 0 <= index < len(data['maps']) for index in used):
        raise ValueError('invalid compact source map reference')
    indices = {old: new for new, old in enumerate(used)}
    return dict(data, maps=tuple(data['maps'][index] for index in used),
                rows=tuple((locale, system, kind, indices[index])
                           for locale, system, kind, index in rows))


def prepare(data, tags, numbering):
    """Validate and compile immutable systems, locale map rows and patterns."""
    systems = tuple((index, name) for index, (name, kind, value)
                    in enumerate(numbering['systems']) if kind == 'numeric')
    if tuple(name for index, name in systems) != data['systems'] or data['kinds'] != source.KINDS:
        raise ValueError('compact numbering/kind inventory mismatch')
    records = {(locale, system, kind): index for locale, system, kind, index in data['rows']}
    if (len(records) != len(data['rows']) or
            set(records) != {(locale, name, kind) for locale in tags for index, name in systems
                             for kind in range(KIND_COUNT)} or
            set(records.values()) != set(range(len(data['maps'])))):
        raise ValueError('compact source map coverage mismatch')
    if any(kind < 2 and not data['maps'][index]
           for (locale, system, kind), index in records.items()):
        raise ValueError('empty decimal compact map')
    rows = tuple(tuple(records[(locale, name, kind)]
                       for index, name in systems for kind in range(KIND_COUNT))
                 for locale in tags)
    return Prepared(tuple(index for index, name in systems), rows,
                    syntax.compile_inventory(data))


def strings(prepared):
    return {text for entries in prepared.maps
            for magnitude, count, variant, pattern in entries if pattern not in ('', BLOCK)
            for form in pattern[3:] for kind, text in form}


def encode(prepared, ids):
    compiled = prepared.maps
    affixes = tuple(sorted({tuple((kind, ids[text]) for kind, text in form)
                            for entries in compiled for magnitude, count, variant, pattern in entries
                            if pattern not in ('', BLOCK) for form in pattern[3:]}))
    affix_ids = {value: index for index, value in enumerate(affixes)}
    patterns = {pattern: pattern[:3] + tuple(affix_ids[tuple((kind, ids[text]) for kind, text in form)]
                                            for form in pattern[3:])
                for entries in compiled for magnitude, count, variant, pattern in entries
                if pattern not in ('', BLOCK)}
    unique_patterns = tuple(sorted(set(patterns.values())))
    pattern_ids = {value: index for index, value in enumerate(unique_patterns)}
    encoded_maps = tuple(tuple(sorted((magnitude, source.COUNTS.index(count),
                                      source.VARIANTS.index(variant),
                                      BLOCKED if pattern == BLOCK else EMPTY if pattern == '' else
                                      pattern_ids[patterns[pattern]])
                                     for magnitude, count, variant, pattern in entries))
                         for entries in compiled)
    maps = tuple(sorted(set(encoded_maps)))
    map_ids = {value: index for index, value in enumerate(maps)}
    locale_rows = tuple(tuple(map_ids[encoded_maps[index]] for index in row)
                        for row in prepared.rows)
    rows = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(rows)}
    entry_count = sum(len(entries) for entries in maps)
    token_count = sum(len(affix) for affix in affixes)
    if not entry_count or not unique_patterns or not affixes or not token_count:
        raise ValueError('compact source inventory lacks usable patterns')
    words = [VERSION, len(prepared.systems), len(rows), len(maps), entry_count,
             len(unique_patterns), len(affixes), token_count]
    words.extend(prepared.systems)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in rows for value in row)
    offset = 0
    for entries in maps:
        words.extend((offset, len(entries)))
        offset += len(entries)
    words.extend(value for entries in maps for entry in entries for value in entry)
    words.extend(value for pattern in unique_patterns for value in pattern)
    offset = 0
    for affix in affixes:
        words.extend((offset, len(affix)))
        offset += len(affix)
    words.extend(value for affix in affixes for token in affix for value in token)
    return len(prepared.rows), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags, numbering):
    if section is None:
        return None
    if numbering is None:
        raise ValueError('compact data lacks numbering definitions')
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid compact locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, system_count, row_count, map_count, entry_count, pattern_count, affix_count, token_count = words[:HEADER]
    systems = tuple(index for index, (name, kind, value) in enumerate(numbering['systems'])
                    if kind == 'numeric')
    row_width = system_count * KIND_COUNT
    expected = (HEADER + system_count + count + row_count * row_width + map_count * 2 +
                entry_count * ENTRY_WIDTH + pattern_count * PATTERN_WIDTH + affix_count * 2 + token_count * 2)
    if (version != VERSION or not systems or system_count != len(systems) or
            not 1 <= row_count <= count or not 1 <= map_count <= row_count * row_width or
            not entry_count or not 1 <= pattern_count <= entry_count or
            not 1 <= affix_count <= pattern_count * 3 or token_count < affix_count or
            expected != len(words) or not pool or pool[0] != ''):
        raise ValueError('invalid compact dimensions/version')
    if words[HEADER:HEADER + system_count] != systems:
        raise ValueError('invalid compact numbering references')
    at = HEADER + system_count
    locale_rows = words[at:at + count]
    if set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid or unused compact locale row')
    at += count

    def ordered(width, length):
        nonlocal at
        result = tuple(words[i:i + width] for i in range(at, at + width * length, width))
        at += width * length
        if any(a >= b for a, b in zip(result, result[1:])):
            raise ValueError('noncanonical compact records')
        return result

    rows = ordered(row_width, row_count)
    if {value for row in rows for value in row} != set(range(map_count)):
        raise ValueError('invalid or unused compact map')
    descriptors = tuple(words[i:i + 2] for i in range(at, at + map_count * 2, 2))
    at += map_count * 2
    maps, end, references = [], 0, set()
    for offset, length in descriptors:
        if offset != end or length > entry_count - end:
            raise ValueError('invalid compact map extent')
        entries = tuple(words[i:i + ENTRY_WIDTH] for i in range(
            at + offset * ENTRY_WIDTH, at + (offset + length) * ENTRY_WIDTH, ENTRY_WIDTH))
        if maps and entries <= maps[-1]:
            raise ValueError('noncanonical compact map order')
        previous = None
        for magnitude, category, variant, reference in entries:
            key = magnitude, category, variant
            if (category >= len(source.COUNTS) or variant >= len(source.VARIANTS) or
                    previous is not None and key <= previous):
                raise ValueError('invalid compact selector/order')
            if reference < pattern_count:
                references.add(reference)
            elif reference not in (BLOCKED, EMPTY):
                raise ValueError('invalid compact pattern reference')
            previous = key
        maps.append(entries)
        end += length
    if end != entry_count or references != set(range(pattern_count)):
        raise ValueError('unused compact entries/patterns')
    if any(column % KIND_COUNT < 2 and not maps[index]
           for row in rows for column, index in enumerate(row)):
        raise ValueError('empty decimal compact map')
    at += entry_count * ENTRY_WIDTH
    patterns = ordered(PATTERN_WIDTH, pattern_count)
    if {value for row in patterns for value in row[3:]} != set(range(affix_count)):
        raise ValueError('invalid or unused compact affix')
    descriptors = tuple(words[i:i + 2] for i in range(at, at + affix_count * 2, 2))
    at += affix_count * 2
    affixes, end = [], 0
    for offset, length in descriptors:
        if offset != end or not 1 <= length <= token_count - end:
            raise ValueError('invalid compact affix extent')
        values = tuple(words[i:i + 2] for i in range(at + offset * 2, at + (offset + length) * 2, 2))
        if affixes and values <= affixes[-1]:
            raise ValueError('noncanonical compact affix order')
        previous = None
        for kind, text in values:
            if (kind >= len(tokens.TOKENS) or kind in (tokens.PERCENT, tokens.PERMILLE) or
                    kind == tokens.LITERAL and
                    (not 0 < text < len(pool) or not pool[text] or previous == tokens.LITERAL) or
                    kind != tokens.LITERAL and text != 0):
                raise ValueError('invalid compact token/text')
            previous = kind
        affixes.append(values)
        end += length
    if end != token_count:
        raise ValueError('unused compact tokens')
    decoded_patterns = []
    fallback = (((tokens.NUMBER, 0),), ((tokens.MINUS, 0), (tokens.NUMBER, 0)),
                ((tokens.PLUS, 0), (tokens.NUMBER, 0)))
    for mode, exponent, zeros, positive, negative, plus in patterns:
        forms = tuple(affixes[index] for index in (positive, negative, plus))
        if (mode not in (syntax.SCALED, syntax.LITERAL, syntax.FALLBACK) or
                any(sum(kind == tokens.NUMBER for kind, text in form) != int(mode != syntax.LITERAL)
                    for form in forms) or
                mode == syntax.SCALED and not zeros or mode == syntax.LITERAL and zeros or
                mode == syntax.FALLBACK and (exponent or zeros != 1 or forms != fallback)):
            raise ValueError('invalid compact mode/placeholder count')
        decoded_patterns.append((mode, exponent, zeros) + tuple(
            tuple((kind, pool[text]) for kind, text in form) for form in forms))
    decoded_maps = []
    for entries in maps:
        fields = {}
        for magnitude, category, variant, reference in entries:
            pattern = (BLOCK if reference == BLOCKED else '' if reference == EMPTY else
                       decoded_patterns[reference])
            if isinstance(pattern, tuple):
                mode, exponent, zeros = pattern[:3]
                if (exponent > magnitude or mode == syntax.SCALED and exponent + zeros != magnitude + 1):
                    raise ValueError('compact divisor does not match magnitude')
            fields[(magnitude, source.COUNTS[category], source.VARIANTS[variant])] = pattern
        for magnitude, category, variant in fields:
            baseline = fields.get((magnitude, 'other', 'standard'))
            if not isinstance(baseline, tuple) or baseline[0] == syntax.LITERAL:
                raise ValueError('missing compact baseline other')
            pattern = fields[(magnitude, category, variant)]
            if isinstance(pattern, tuple) and pattern[0] == syntax.LITERAL:
                other = fields.get((magnitude, 'other', variant))
                if (not isinstance(other, tuple) or other[0] == syntax.LITERAL or pattern[1] != other[1]):
                    raise ValueError('literal compact pattern lacks its family divisor')
        decoded_maps.append(tuple((magnitude, source.COUNTS[category], source.VARIANTS[variant],
                                   fields[(magnitude, source.COUNTS[category], source.VARIANTS[variant])])
                                  for magnitude, category, variant, reference in entries))
    names = tuple(numbering['systems'][index][0] for index in systems)
    return {'systems': names, 'kinds': source.KINDS, 'maps': tuple(decoded_maps),
            'rows': tuple((locale, name, kind, rows[locale_rows[i]][j * KIND_COUNT + kind])
                          for i, locale in enumerate(tags) for j, name in enumerate(names)
                          for kind in range(KIND_COUNT))}
