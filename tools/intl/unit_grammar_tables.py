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

"""Shared division component derivations in QJIN section 22."""
import struct

import bundle
from parents import locale_id
import unit_grammar as source

VERSION, HEADER, WIDTH = 1, 2, 6


def select(data, tags):
    selected = set(tags)
    return dict(data, rows=tuple(row for row in data['rows'] if row[0] in selected))


def rule(feature, origin, first, second):
    if not isinstance(origin, str) or locale_id(origin) != origin:
        raise ValueError('invalid unit grammar source locale')
    for value in (first, second):
        if not isinstance(value, str):
            raise ValueError('invalid unit grammar component token')
        source.component_value(feature, value)


def strings(data):
    return {value for locale, feature, origin, first, second in data['rows']
            for value in (origin, first, second)}


def encode(data, tags, ids):
    rows = {row[:2]: row[2:] for row in data['rows']}
    if (data['features'] != source.FEATURES or not tags or len(set(tags)) != len(tags) or
            len(rows) != len(data['rows']) or
            set(rows) != {(tag, feature) for tag in tags for feature in source.FEATURES}):
        raise ValueError('invalid unit grammar inventory/coverage')
    for (tag, feature), values in rows.items():
        rule(feature, *values)
    vectors = tuple(tuple(ids[value] for feature in source.FEATURES
                          for value in rows[tag, feature]) for tag in tags)
    shared = tuple(sorted(set(vectors)))
    indices = {vector: index for index, vector in enumerate(shared)}
    words = [VERSION, len(shared)] + [indices[vector] for vector in vectors]
    words.extend(value for vector in shared for value in vector)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid unit grammar extent/locale count')
    words = tuple(item[0] for item in struct.iter_unpack('<I', raw))
    version, size = words[:HEADER]
    if version != VERSION or not 1 <= size <= count or len(words) != HEADER + count + size * WIDTH:
        raise ValueError('invalid unit grammar version/dimensions')
    indices = words[HEADER:HEADER + count]
    if set(indices) != set(range(size)):
        raise ValueError('invalid/unused unit grammar row')
    shared, previous, checked = [], None, set()
    for at in range(HEADER + count, len(words), WIDTH):
        vector = words[at:at + WIDTH]
        if any(value >= len(pool) for value in vector) or previous is not None and vector <= previous:
            raise ValueError('invalid/noncanonical unit grammar row')
        values = tuple(pool[value] for value in vector)
        for feature, offset in zip(source.FEATURES, (0, 3)):
            record = feature, values[offset:offset + 3]
            if record not in checked:
                rule(feature, *record[1])
                checked.add(record)
        shared.append(values)
        previous = vector
    return {'features': source.FEATURES,
            'rows': tuple((tag, feature) + shared[index][offset:offset + 3]
                          for tag, index in zip(tags, indices)
                          for feature, offset in zip(source.FEATURES, (0, 3)))}
