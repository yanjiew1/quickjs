"""Shared compiled standard numeric patterns in QJIN section 17."""
import struct

import bundle
from number_patterns import FIELDS
import number_pattern_syntax as syntax
from resolve import BLOCK

VERSION = 1
ABSENT, BLOCKED, EMPTY = 0xffffffff, 0xfffffffe, 0xfffffffd
HEADER = 7
WIDTH = len(FIELDS)
NUMERIC_WIDTH = 9
PATTERN_WIDTH = NUMERIC_WIDTH + 3


def strings(data):
    return {text for pattern in syntax.compile_inventory(data).values()
            for affix in pattern[1:] for kind, text in affix}


def encode(data, tags, numbering, ids):
    systems = tuple((index, name) for index, (name, kind, value)
                    in enumerate(numbering['systems']) if kind == 'numeric')
    if tuple(name for index, name in systems) != data['systems'] or data['fields'] != FIELDS:
        raise ValueError('Intl pattern numbering/field inventory mismatch')
    records = {(locale, system): row for locale, system, row in data['rows']}
    grouping = dict(data['grouping'])
    if (len(records) != len(data['rows']) or len(grouping) != len(data['grouping']) or
            set(records) != {(tag, name) for tag in tags for index, name in systems} or
            set(grouping) != set(tags) or any(len(row) != WIDTH for row in records.values())):
        raise ValueError('Intl pattern locale/system coverage mismatch')
    for row in records.values():
        if any(value in (None, BLOCK, '') for value in row[:5]):
            raise ValueError('missing required Intl number pattern')
    for value in grouping.values():
        if not bundle.uint32(value):
            raise ValueError('zero minimum grouping digits')

    compiled = syntax.compile_inventory(data)
    affixes = tuple(sorted({tuple((kind, ids[text]) for kind, text in affix)
                            for pattern in compiled.values() for affix in pattern[1:]}))
    affix_ids = {affix: index for index, affix in enumerate(affixes)}
    patterns = {raw: pattern[0] + tuple(affix_ids[tuple((kind, ids[text]) for kind, text in affix)]
                                      for affix in pattern[1:])
                for raw, pattern in compiled.items()}
    unique_patterns = tuple(sorted(set(patterns.values())))
    pattern_ids = {pattern: index for index, pattern in enumerate(unique_patterns)}

    def reference(value):
        if value is None:
            return ABSENT
        if value == BLOCK:
            return BLOCKED
        if value == '':
            return EMPTY
        return pattern_ids[patterns[value]]

    vectors = {key: tuple(reference(value) for value in row) for key, row in records.items()}
    unique = tuple(sorted(set(vectors.values())))
    vector_ids = {vector: index for index, vector in enumerate(unique)}
    locale_rows = tuple(tuple(vector_ids[vectors[(locale, name)]] for index, name in systems)
                        for locale in tags)
    rows = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(rows)}
    token_count = sum(len(affix) for affix in affixes)
    words = [VERSION, len(systems), len(unique), len(rows), len(unique_patterns),
             len(affixes), token_count]
    words.extend(index for index, name in systems)
    words.extend(value for locale, row in zip(tags, locale_rows)
                 for value in (grouping[locale], row_ids[row]))
    words.extend(value for row in rows for value in row)
    words.extend(value for vector in unique for value in vector)
    words.extend(value for pattern in unique_patterns for value in pattern)
    offset = 0
    for affix in affixes:
        words.extend((offset, len(affix)))
        offset += len(affix)
    words.extend(value for affix in affixes for token in affix for value in token)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(word)) for word in words)


def numeric_valid(row):
    primary, secondary, minimum, places, low, high, point, exponent, plus = row
    return (places > 0 and minimum <= places and low <= high and point <= 1 and plus <= 1 and
            (point or high == 0) and (exponent or plus == 0) and
            ((primary == secondary == 0) or
             (0 < primary < places and 0 < secondary < places and not exponent)))


def decode(section, pool, tags, numbering):
    if section is None:
        return None
    if numbering is None:
        raise ValueError('Intl patterns lack numbering definitions')
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid Intl pattern locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, system_count, vector_count, row_count, pattern_count, affix_count, token_count = words[:HEADER]
    systems = tuple(index for index, (name, kind, value) in enumerate(numbering['systems'])
                    if kind == 'numeric')
    size = (HEADER + system_count + 2 * count + row_count * system_count + vector_count * WIDTH +
            pattern_count * PATTERN_WIDTH + 2 * affix_count + 2 * token_count)
    if (version != VERSION or not systems or system_count != len(systems) or
            not 1 <= row_count <= count or not 1 <= vector_count <= row_count * system_count or
            not 1 <= pattern_count <= vector_count * WIDTH or
            not 1 <= affix_count <= pattern_count * 3 or not affix_count <= token_count or
            len(words) != size or not pool or pool[0] != ''):
        raise ValueError('invalid Intl pattern dimensions/version')
    if words[HEADER:HEADER + system_count] != systems:
        raise ValueError('invalid Intl pattern numbering references')
    start = HEADER + system_count
    grouping = words[start:start + 2 * count:2]
    locale_rows = words[start + 1:start + 2 * count:2]
    if not all(grouping) or set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid grouping or unused Intl pattern locale row')
    start += 2 * count

    def vectors(width, length):
        nonlocal start
        result = tuple(words[at:at + width] for at in range(start, start + width * length, width))
        start += width * length
        if any(a >= b for a, b in zip(result, result[1:])):
            raise ValueError('noncanonical Intl pattern record order')
        return result

    rows = vectors(system_count, row_count)
    if {value for row in rows for value in row} != set(range(vector_count)):
        raise ValueError('invalid or unused Intl pattern vector')
    records = vectors(WIDTH, vector_count)
    referenced = set()
    for record in records:
        for index, value in enumerate(record):
            if value < pattern_count:
                referenced.add(value)
            elif index < 5 or value not in (ABSENT, BLOCKED, EMPTY):
                raise ValueError('invalid Intl pattern reference/marker')
    if referenced != set(range(pattern_count)):
        raise ValueError('unused Intl number pattern')
    patterns = vectors(PATTERN_WIDTH, pattern_count)
    if (any(not numeric_valid(row[:NUMERIC_WIDTH]) for row in patterns) or
            {value for row in patterns for value in row[NUMERIC_WIDTH:]} != set(range(affix_count))):
        raise ValueError('invalid numeric metadata or unused Intl affix')
    descriptors = tuple(words[at:at + 2] for at in range(start, start + 2 * affix_count, 2))
    start += 2 * affix_count
    affixes, offset = [], 0
    for at, length in descriptors:
        if at != offset or not 1 <= length <= token_count - offset:
            raise ValueError('invalid Intl affix extent')
        tokens = tuple(words[i:i + 2] for i in range(start + at * 2, start + (at + length) * 2, 2))
        if (affixes and tokens <= affixes[-1] or
                sum(kind == syntax.NUMBER for kind, value in tokens) != 1):
            raise ValueError('noncanonical Intl affix order/number token')
        previous = None
        for kind, value in tokens:
            if (kind >= len(syntax.TOKENS) or
                    (kind == syntax.LITERAL and
                     (not 0 < value < len(pool) or not pool[value] or previous == syntax.LITERAL)) or
                    (kind != syntax.LITERAL and value != 0)):
                raise ValueError('invalid Intl affix token/text')
            previous = kind
        affixes.append(tokens)
        offset += length
    if offset != token_count:
        raise ValueError('unused Intl affix tokens')
    decoded_affixes = tuple(tuple((kind, pool[value]) for kind, value in affix) for affix in affixes)
    decoded_patterns = tuple((row[:NUMERIC_WIDTH],) + tuple(decoded_affixes[index]
                             for index in row[NUMERIC_WIDTH:]) for row in patterns)
    optional = {ABSENT: None, BLOCKED: BLOCK, EMPTY: ''}
    decoded_records = tuple(tuple(decoded_patterns[value] if value < pattern_count else optional[value]
                                  for value in row) for row in records)
    names = tuple(numbering['systems'][index][0] for index in systems)
    return {'systems': names, 'fields': FIELDS,
            'grouping': tuple(zip(tags, grouping)),
            'rows': tuple((locale, name, decoded_records[rows[locale_rows[i]][j]])
                          for i, locale in enumerate(tags) for j, name in enumerate(names))}
