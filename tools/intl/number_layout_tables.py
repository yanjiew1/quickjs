"""Shared number/currency layout vectors in QJIN section 20."""
import struct

import bundle
from number_layout import FIELDS, REQUIRED
from resolve import BLOCK, INHERIT

VERSION, HEADER, WIDTH = 2, 4, len(FIELDS)
VECTOR_WIDTH = WIDTH + 1
ROOT_MASK = sum(1 << index for index, field in enumerate(FIELDS) if field.startswith('currency-unit-'))
ABSENT, BLOCKED = 0xffffffff, 0xfffffffe


def strings(data):
    return {text for locale, system, values in data['rows'] for text in values
            if text is not None and text != BLOCK}


def value_valid(field, value):
    if value is not None and not isinstance(value, str) or value == INHERIT:
        return False
    return field not in REQUIRED or (value not in (None, BLOCK) and
                                    (value != '' or field.endswith('-insertBetween')))


def encode(data, tags, numbering, ids):
    systems = tuple((index, name) for index, (name, kind, value) in enumerate(numbering['systems'])
                    if kind == 'numeric')
    records = {(locale, system): values for locale, system, values in data['rows']}
    if (not systems or data['systems'] != tuple(name for index, name in systems) or
            data['fields'] != FIELDS or not tags or len(set(tags)) != len(tags) or
            len(records) != len(data['rows']) or
            set(records) != {(tag, name) for tag in tags for index, name in systems} or
            any(len(values) != WIDTH or any(not value_valid(field, value)
                                           for field, value in zip(FIELDS, values))
                for values in records.values())):
        raise ValueError('invalid number layout source inventory/fields')

    def reference(value):
        return ABSENT if value is None else BLOCKED if value == BLOCK else ids[value]

    roots = {(locale, system): mask for locale, system, mask in data.get('root_fields', ())}
    if (len(roots) != len(data.get('root_fields', ())) or not set(roots) <= set(records) or
            any(type(mask) is not int or not mask or mask & ~ROOT_MASK or
                any(mask & (1 << field) and records[key][field] is None for field in range(WIDTH))
                for key, mask in roots.items())):
        raise ValueError('invalid currency unit root-field mask')
    vectors = {key: tuple(reference(value) for value in values) + (roots.get(key, 0),)
               for key, values in records.items()}
    unique = tuple(sorted(set(vectors.values())))
    vector_ids = {values: index for index, values in enumerate(unique)}
    locale_rows = tuple(tuple(vector_ids[vectors[(tag, name)]] for index, name in systems)
                        for tag in tags)
    rows = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(rows)}
    words = [VERSION, len(systems), len(unique), len(rows)]
    words.extend(index for index, name in systems)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in rows for value in row)
    words.extend(value for vector in unique for value in vector)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags, numbering):
    if section is None:
        return None
    if numbering is None:
        raise ValueError('number layouts lack numbering definitions')
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid number layout locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, system_count, vector_count, row_count = words[:HEADER]
    systems = tuple(index for index, (name, kind, value) in enumerate(numbering['systems'])
                    if kind == 'numeric')
    if (version != VERSION or not systems or system_count != len(systems) or
            not 1 <= row_count <= count or not 1 <= vector_count <= row_count * system_count or
            len(words) != HEADER + system_count + count + row_count * system_count + vector_count * VECTOR_WIDTH or
            not pool or pool[0] != ''):
        raise ValueError('invalid number layout dimensions/version')
    at = HEADER
    if words[at:at + system_count] != systems:
        raise ValueError('invalid number layout numbering references')
    at += system_count
    indices = words[at:at + count]
    if set(indices) != set(range(row_count)):
        raise ValueError('invalid or unused number layout row')
    at += count
    end = at + row_count * system_count
    rows = tuple(words[i:i + system_count] for i in range(at, end, system_count))
    if (any(a >= b for a, b in zip(rows, rows[1:])) or
            {index for row in rows for index in row} != set(range(vector_count))):
        raise ValueError('invalid/unused number layout vector or row order')
    vectors = tuple(words[i:i + VECTOR_WIDTH] for i in range(end, len(words), VECTOR_WIDTH))
    if any(a >= b for a, b in zip(vectors, vectors[1:])):
        raise ValueError('noncanonical number layout vector order')
    decoded = []
    for vector in vectors:
        values = []
        mask = vector[WIDTH]
        if mask & ~ROOT_MASK or any(mask & (1 << field) and vector[field] == ABSENT
                                    for field in range(WIDTH)):
            raise ValueError('invalid currency unit root-field mask')
        for field, reference in zip(FIELDS, vector):
            if reference in (ABSENT, BLOCKED):
                value = None if reference == ABSENT else BLOCK
            elif reference >= len(pool) or pool[reference] in (BLOCK, INHERIT):
                raise ValueError('invalid number layout text reference')
            else:
                value = pool[reference]
            if not value_valid(field, value):
                raise ValueError('invalid number layout field state')
            values.append(value)
        decoded.append(tuple(values))
    names = tuple(numbering['systems'][index][0] for index in systems)
    return {'systems': names, 'fields': FIELDS,
            'root_fields': tuple((tag, name, vectors[rows[indices[i]][j]][WIDTH])
                                 for i, tag in enumerate(tags) for j, name in enumerate(names)
                                 if vectors[rows[indices[i]][j]][WIDTH]),
            'rows': tuple((tag, name, decoded[rows[indices[i]][j]])
                          for i, tag in enumerate(tags) for j, name in enumerate(names))}
