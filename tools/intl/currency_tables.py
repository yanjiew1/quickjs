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

"""Shared currency field vectors and supplemental facts in QJIN section 19."""
import struct

import bundle
import currencies as source
from resolve import BLOCK, INHERIT

VERSION, HEADER = 2, 9
ABSENT, BLOCKED, EMPTY = 0xffffffff, 0xfffffffe, 0xfffffffd
FIELD_COUNT = len(source.COUNTS) + len(source.FIELDS) - 1
VECTOR_WIDTH = FIELD_COUNT + 1
ROOT_MASK = (1 << len(source.COUNTS)) - 2


def codes(data):
    return tuple(sorted(set(data['codes']) | {row[0] for row in data['fractions'] if row[0] != 'DEFAULT'}))


def patterns(data):
    """Legacy per-currency overrides share the typed standard-pattern owner."""
    return tuple(sorted({text for entries in data['maps'] for code, field, count, text in entries
                         if field == 'pattern' and text not in ('', BLOCK)}))


def select(data, tags):
    selected = set(tags)
    rows = tuple(row for row in data['rows'] if row[0] in selected)
    used = sorted({index for locale, index in rows})
    if any(type(index) is not int or not 0 <= index < len(data['maps']) for index in used):
        raise ValueError('invalid currency source map reference')
    indices = {old: new for new, old in enumerate(used)}
    return dict(data, maps=tuple(data['maps'][index] for index in used),
                root_names=tuple(row for row in data.get('root_names', ()) if row[0] in selected),
                rows=tuple((locale, indices[index]) for locale, index in rows))


def strings(data):
    return set(codes(data)) | {text for entries in data['maps'] for code, field, count, text in entries
                               if text not in ('', BLOCK)}


def fraction_valid(values):
    return (len(values) == 4 and all(type(value) is int and 0 <= value <= 0xffffffff for value in values)
            and values[0] <= 100 and values[2] <= 100)


def encode(data, tags, ids):
    inventory = codes(data)
    code_ids = {code: index for index, code in enumerate(inventory)}
    records = dict(data['rows'])
    if (data['fields'] != source.FIELDS or data['counts'] != source.COUNTS or not inventory or
            data['codes'] != tuple(sorted(set(data['codes']))) or
            any(source.CODE.fullmatch(code) is None for code in inventory) or
            len(records) != len(data['rows']) or set(records) != set(tags) or
            set(records.values()) != set(range(len(data['maps'])))):
        raise ValueError('invalid currency source inventory')
    fractions = {}
    for row in data['fractions']:
        code, values = row[0], row[1:]
        if (code in fractions or code != 'DEFAULT' and code not in code_ids or not fraction_valid(values)):
            raise ValueError('invalid currency fraction record')
        fractions[code] = values
    if 'DEFAULT' not in fractions:
        raise ValueError('missing currency fraction default')
    compiled = []
    for entries in data['maps']:
        fields, seen = {}, set()
        for code, field, count, text in entries:
            if (code not in code_ids or field not in source.FIELDS or
                    count not in source.COUNTS or field != 'name' and count or
                    (code, field, count) in seen or not isinstance(text, str) or text == INHERIT):
                raise ValueError('invalid/repeated currency source field')
            seen.add((code, field, count))
            vector = fields.setdefault(code, [ABSENT] * FIELD_COUNT)
            index = source.COUNTS.index(count) if field == 'name' else (
                len(source.COUNTS) + source.FIELDS.index(field) - 1)
            vector[index] = BLOCKED if text == BLOCK else EMPTY if text == '' else ids[text]
        compiled.append({code: tuple(vector) for code, vector in fields.items()})
    roots = dict(data.get('root_names', ()))
    if len(roots) != len(data.get('root_names', ())) or not set(roots) <= set(tags):
        raise ValueError('invalid currency root-name locale coverage')
    projected = {}
    for locale in tags:
        fields = compiled[records[locale]]
        keys = roots.get(locale, ())
        if tuple(keys) != tuple(sorted(set(keys))) or locale in roots and not keys:
            raise ValueError('noncanonical currency root-name keys')
        masks = {}
        for code, count in keys:
            if (code not in fields or count not in source.COUNTS[1:] or
                    fields[code][source.COUNTS.index(count)] == ABSENT):
                raise ValueError('invalid currency root-name reference')
            masks[code] = masks.get(code, 0) | (1 << source.COUNTS.index(count))
        projected[locale] = {code: vector + (masks.get(code, 0),)
                             for code, vector in fields.items()}
    vectors = tuple(sorted({vector for fields in projected.values() for vector in fields.values()}))
    vector_ids = {value: index for index, value in enumerate(vectors)}
    raw_maps = {locale: tuple(vector_ids[fields[code]] if code in fields else ABSENT
                              for code in inventory) for locale, fields in projected.items()}
    maps = tuple(sorted(set(raw_maps.values())))
    map_ids = {value: index for index, value in enumerate(maps)}
    fraction_rows = tuple((code_ids[code],) + fractions[code] for code in sorted(fractions) if code != 'DEFAULT')
    words = [VERSION, len(inventory), len(fraction_rows), len(vectors), len(maps)]
    words.extend(fractions['DEFAULT'])
    words.extend(ids[code] for code in inventory)
    words.extend(value for row in fraction_rows for value in row)
    words.extend(map_ids[raw_maps[locale]] for locale in tags)
    words.extend(value for row in maps for value in row)
    words.extend(value for row in vectors for value in row)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid currency locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, code_count, fraction_count, vector_count, map_count = words[:5]
    default = words[5:HEADER]
    expected = HEADER + code_count + fraction_count * 5 + count + map_count * code_count + vector_count * VECTOR_WIDTH
    if (version != VERSION or not code_count or fraction_count > code_count or
            not 1 <= map_count <= count or vector_count > map_count * code_count or
            expected != len(words) or not fraction_valid(default) or not pool or pool[0] != ''):
        raise ValueError('invalid currency dimensions/defaults')
    at = HEADER
    inventory = []
    for reference in words[at:at + code_count]:
        if (not 0 < reference < len(pool) or source.CODE.fullmatch(pool[reference]) is None or
                inventory and pool[reference] <= inventory[-1]):
            raise ValueError('invalid currency code reference/order')
        inventory.append(pool[reference])
    at += code_count
    fractions = [('DEFAULT',) + default]
    previous = -1
    for i in range(fraction_count):
        index, digits, rounding, cash_digits, cash_rounding = words[at:at + 5]
        at += 5
        if index <= previous or index >= code_count or not fraction_valid((digits, rounding, cash_digits, cash_rounding)):
            raise ValueError('invalid currency fraction reference/order')
        fractions.append((inventory[index], digits, rounding, cash_digits, cash_rounding))
        previous = index
    indices = words[at:at + count]
    if set(indices) != set(range(map_count)):
        raise ValueError('invalid or unused currency map')
    at += count
    maps = tuple(words[i:i + code_count] for i in range(at, at + map_count * code_count, code_count))
    at += map_count * code_count
    vectors = tuple(words[i:i + VECTOR_WIDTH] for i in range(at, len(words), VECTOR_WIDTH))
    if (any(a >= b for rows in (maps, vectors) for a, b in zip(rows, rows[1:])) or
            {index for row in maps for index in row if index != ABSENT} != set(range(vector_count))):
        raise ValueError('invalid or unused currency vector/order')
    for vector in vectors:
        if all(value == ABSENT for value in vector[:FIELD_COUNT]):
            raise ValueError('redundant absent currency vector')
        if any(value not in (ABSENT, BLOCKED, EMPTY) and
               (not 0 < value < len(pool) or not pool[value] or pool[value] in (BLOCK, INHERIT))
               for value in vector[:FIELD_COUNT]):
            raise ValueError('invalid currency text reference')
        mask = vector[FIELD_COUNT]
        if mask & ~ROOT_MASK or any(mask & (1 << field) and vector[field] == ABSENT
                                    for field in range(1, len(source.COUNTS))):
            raise ValueError('invalid currency root-name mask')
    decoded_maps = []
    for row in maps:
        fields = []
        for code, index in zip(inventory, row):
            if index == ABSENT:
                continue
            for field, value in enumerate(vectors[index][:FIELD_COUNT]):
                if value == ABSENT:
                    continue
                name = 'name' if field < len(source.COUNTS) else source.FIELDS[field - len(source.COUNTS) + 1]
                category = source.COUNTS[field] if field < len(source.COUNTS) else ''
                text = BLOCK if value == BLOCKED else '' if value == EMPTY else pool[value]
                fields.append((code, name, category, text))
        decoded_maps.append(tuple(sorted(fields)))
    root_names = []
    for locale, row_index in zip(tags, indices):
        keys = tuple(sorted((code, source.COUNTS[field]) for code, index in zip(inventory, maps[row_index])
                     if index != ABSENT for field in range(1, len(source.COUNTS))
                     if vectors[index][FIELD_COUNT] & (1 << field)))
        if keys:
            root_names.append((locale, keys))
    return {'codes': tuple(inventory), 'fields': source.FIELDS, 'counts': source.COUNTS,
            'root_names': tuple(root_names),
            'fractions': tuple(sorted(fractions)), 'maps': tuple(decoded_maps),
            'rows': tuple((locale, indices[i]) for i, locale in enumerate(tags))}
