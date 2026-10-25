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

"""Shared cardinal/ordinal conditions and sparse range vectors in QJIN."""
import struct

import plural_rules

ABSENT = 0xffffffff
RANGE_ABSENT = 0xff
WIDTH = len(plural_rules.CATEGORIES)


def strings(data):
    return {condition for locale, kind, rules, ranges in data['rows']
            for category, condition in rules}


def encode(data, tags, ids):
    records = {(locale, kind): (rules, ranges) for locale, kind, rules, ranges in data['rows']}
    if (len(records) != len(data['rows']) or
            set(records) != {(locale, kind) for locale in tags for kind in range(2)}):
        raise ValueError('Intl plural locale/type coverage mismatch')

    def rules(values):
        vector = [ABSENT] * WIDTH
        for category, condition in values:
            if not 0 <= category < WIDTH or vector[category] != ABSENT:
                raise ValueError('invalid/repeated Intl plural category')
            vector[category] = ids[condition]
        return tuple(vector)

    def ranges(values):
        vector = [RANGE_ABSENT] * (WIDTH * WIDTH)
        for start, end, result in values:
            if any(not 0 <= value < WIDTH for value in (start, end, result)):
                raise ValueError('invalid Intl plural range category')
            key = start * WIDTH + end
            if vector[key] != RANGE_ABSENT:
                raise ValueError('repeated Intl plural range pair')
            vector[key] = result
        return bytes(vector)

    mapped = tuple((rules(records[(locale, kind)][0]), ranges(records[(locale, kind)][1]))
                   for locale in tags for kind in range(2))
    rule_vectors = tuple(sorted({row[0] for row in mapped}))
    range_vectors = tuple(sorted({row[1] for row in mapped}))
    rule_ids = {vector: index for index, vector in enumerate(rule_vectors)}
    range_ids = {vector: index for index, vector in enumerate(range_vectors)}
    words = [len(rule_vectors), len(range_vectors)]
    words.extend(index for a, b in mapped for index in (rule_ids[a], range_ids[b]))
    words.extend(index for vector in rule_vectors for index in vector)
    return len(tags), (b''.join(struct.pack('<I', word) for word in words) +
                       b''.join(range_vectors))


def decode(section, pool, tags):
    if section is None:
        return None
    count, raw = section
    if count != len(tags) or not count or len(raw) < 8:
        raise ValueError('invalid Intl plural locale count/extent')
    rule_count, range_count = struct.unpack_from('<II', raw)
    if (not 1 <= rule_count <= count * 2 or not 1 <= range_count <= count * 2 or
            len(raw) != 8 + count * 16 + rule_count * WIDTH * 4 + range_count * WIDTH * WIDTH):
        raise ValueError('invalid Intl plural dimensions')
    word_end = 8 + count * 16 + rule_count * WIDTH * 4
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw[8:word_end]))
    mapped = tuple(zip(words[:count * 4:2], words[1:count * 4:2]))
    if ({a for a, b in mapped} != set(range(rule_count)) or
            {b for a, b in mapped} != set(range(range_count))):
        raise ValueError('invalid or unused Intl plural map')
    rules = tuple(words[at:at + WIDTH] for at in range(count * 4, len(words), WIDTH))
    if any(a >= b for a, b in zip(rules, rules[1:])):
        raise ValueError('noncanonical Intl plural condition order')
    decoded, checked = [], set()
    for vector in rules:
        result = []
        for category, index in enumerate(vector):
            if index == ABSENT and category < WIDTH - 1:
                continue
            if index >= len(pool) or bool(pool[index]) != (category != WIDTH - 1):
                raise ValueError('invalid Intl plural condition reference/category')
            if index not in checked:
                if plural_rules.relation(pool[index]) != pool[index]:
                    raise ValueError('noncanonical Intl plural condition text')
                checked.add(index)
            result.append((category, pool[index]))
        decoded.append(tuple(result))
    ranges = tuple(raw[at:at + WIDTH * WIDTH]
                   for at in range(word_end, len(raw), WIDTH * WIDTH))
    if any(a >= b for a, b in zip(ranges, ranges[1:])):
        raise ValueError('noncanonical Intl plural range order')
    decoded_ranges = []
    for vector in ranges:
        if any(value >= WIDTH and value != RANGE_ABSENT for value in vector):
            raise ValueError('invalid Intl plural range result')
        decoded_ranges.append(tuple((key // WIDTH, key % WIDTH, value)
                                    for key, value in enumerate(vector) if value != RANGE_ABSENT))
    return {'rows': tuple((locale, kind, decoded[mapped[i * 2 + kind][0]],
                          decoded_ranges[mapped[i * 2 + kind][1]])
                         for i, locale in enumerate(tags) for kind in range(2))}
