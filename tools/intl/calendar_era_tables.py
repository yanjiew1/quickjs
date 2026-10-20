"""Canonical raw era mappings in optional QJIN section 35."""
import struct

import bundle
import calendar_eras as source


def strings(data):
    return {''} | {value for row in source.records(data) for value in row}


def encode(data, ids):
    rows = source.records(data)
    if ids.get('') != 0 or len(rows) > (bundle.MAX_SIZE // 4 - 1) // 3:
        raise ValueError('invalid calendar-era pool/extent')
    words = [1]
    words.extend(ids[value] for row in rows for value in row)
    return len(rows), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool):
    if section is None:
        return None
    count, raw = section
    if (type(count) is not int or not 0 <= count <= 0xffffffff or
            not isinstance(raw, bytes) or not 4 <= len(raw) <= bundle.MAX_SIZE or
            len(raw) != 4 + count * 12 or struct.unpack_from('<I', raw)[0] != 1 or
            not pool or pool[0] != ''):
        raise ValueError('invalid calendar-era version/extent')
    rows = []
    for row in struct.iter_unpack('<3I', raw[4:]):
        if any(index >= len(pool) for index in row):
            raise ValueError('invalid calendar-era string reference')
        rows.append(tuple(pool[index] for index in row))
    result = {'rows': tuple(rows)}
    if encode(result, {value: index for index, value in enumerate(pool)}) != section:
        raise ValueError('noncanonical calendar-era tables')
    return result
