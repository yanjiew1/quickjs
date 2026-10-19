"""Shared numeric symbol vectors and per-locale numbering maps in QJIN."""
import struct

import bundle
from number_symbols import CORE_FIELDS, FIELDS
from resolve import BLOCK, INHERIT

ABSENT = 0xffffffff
BLOCKED = 0xfffffffe
WIDTH = len(FIELDS)


def strings(data):
    return {value for locale, system, values in data['rows'] for value in values
            if value is not None and value != BLOCK}


def encode(data, tags, numbering, ids):
    systems = tuple((index, name) for index, (name, kind, value)
                    in enumerate(numbering['systems']) if kind == 'numeric')
    if tuple(name for index, name in systems) != data['systems']:
        raise ValueError('Intl symbol numbering inventory mismatch')
    records = {(locale, system): values for locale, system, values in data['rows']}
    if (len(records) != len(data['rows']) or
            set(records) != {(locale, name) for locale in tags for index, name in systems}):
        raise ValueError('Intl symbol locale/system coverage mismatch')

    def reference(value):
        return ABSENT if value is None else BLOCKED if value == BLOCK else ids[value]

    vectors = {key: tuple(reference(value) for value in values)
               for key, values in records.items()}
    if any(len(vector) != WIDTH for vector in vectors.values()):
        raise ValueError('invalid Intl symbol vector width')
    unique = tuple(sorted(set(vectors.values())))
    vector_ids = {vector: index for index, vector in enumerate(unique)}
    locale_rows = tuple(tuple(vector_ids[vectors[(locale, name)]] for index, name in systems)
                        for locale in tags)
    rows = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(rows)}
    words = [len(systems), len(unique), len(rows)]
    words.extend(index for index, name in systems)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(index for row in rows for index in row)
    words.extend(value for vector in unique for value in vector)
    return len(tags), b''.join(struct.pack('<I', word) for word in words)


def decode(section, pool, tags, numbering):
    if section is None:
        return None
    if numbering is None:
        raise ValueError('Intl symbols lack numbering definitions')
    count, raw = section
    if count != len(tags) or not count or len(raw) < 12 or len(raw) % 4:
        raise ValueError('invalid Intl symbol locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    system_count, vector_count, row_count = words[:3]
    systems = tuple(index for index, (name, kind, value) in enumerate(numbering['systems'])
                    if kind == 'numeric')
    if (not systems or system_count != len(systems) or not 1 <= row_count <= count or
            not 1 <= vector_count <= row_count * system_count or
            len(words) != 3 + system_count + count + row_count * system_count + vector_count * WIDTH):
        raise ValueError('invalid Intl symbol dimensions')
    if words[3:3 + system_count] != systems:
        raise ValueError('invalid Intl symbol numbering references')
    start = 3 + system_count
    locale_rows = words[start:start + count]
    if set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid or unused Intl symbol locale row')
    start += count
    row_end = start + row_count * system_count
    rows = tuple(words[at:at + system_count] for at in range(start, row_end, system_count))
    if any(a >= b for a, b in zip(rows, rows[1:])):
        raise ValueError('noncanonical Intl symbol row order')
    if {index for row in rows for index in row} != set(range(vector_count)):
        raise ValueError('invalid or unused Intl symbol vector')
    vectors = tuple(words[at:at + WIDTH] for at in range(row_end, len(words), WIDTH))
    if any(a >= b for a, b in zip(vectors, vectors[1:])):
        raise ValueError('noncanonical Intl symbol vector order')
    decoded = []
    for vector in vectors:
        values = []
        for field, value in enumerate(vector):
            if value in (ABSENT, BLOCKED) and field >= len(CORE_FIELDS):
                values.append(None if value == ABSENT else BLOCK)
            elif value >= len(pool) or pool[value] in (INHERIT, BLOCK):
                raise ValueError('invalid or unresolved Intl symbol text')
            else:
                values.append(pool[value])
        decoded.append(tuple(values))
    names = tuple(numbering['systems'][index][0] for index in systems)
    return {'systems': names, 'fields': FIELDS,
            'rows': tuple((locale, name, decoded[rows[locale_rows[i]][j]])
                          for i, locale in enumerate(tags) for j, name in enumerate(names))}
