"""Optional QJIN root collation section; scalar keys and fixed-width elements."""
import argparse
import json
from pathlib import Path
import struct

import acquire
import bundle
import collation_root as source

HEADER = struct.Struct('<4I')
MAPPING = struct.Struct('<4I')
ELEMENT = struct.Struct('<4H')
UNICODE_VERSION = 18 << 16


def encode(data):
    if (not isinstance(data, dict) or data.get('version') != source.VERSION or
            not isinstance(data.get('rows'), (tuple, list)) or not data['rows']):
        raise ValueError('invalid root collation version/rows')
    rows = []
    for row in data['rows']:
        if not isinstance(row, (tuple, list)) or len(row) != 2:
            raise ValueError('invalid root collation mapping')
        key, elements = row
        if (not isinstance(key, (tuple, list)) or not key or
                any(type(c) is not int or not 0 <= c <= 0x10ffff or
                    0xd800 <= c <= 0xdfff for c in key) or
                not isinstance(elements, (tuple, list)) or not elements):
            raise ValueError('invalid root collation key/elements')
        mapped = []
        for element in elements:
            if (not isinstance(element, (tuple, list)) or len(element) != 4 or
                    type(element[0]) is not bool or
                    any(type(w) is not int or not 0 <= w <= 0xffff for w in element[1:]) or
                    element[0] and not element[1]):
                raise ValueError('invalid root collation weight/variable flag')
            mapped.append(tuple(element))
        rows.append((tuple(key), tuple(mapped)))
    rows.sort(key=lambda row: row[0])
    if any(a[0] == b[0] for a, b in zip(rows, rows[1:])):
        raise ValueError('repeated root collation key')
    key_count = sum(len(key) for key, elements in rows)
    element_count = sum(len(elements) for key, elements in rows)
    size = HEADER.size + len(rows) * MAPPING.size + key_count * 4 + element_count * ELEMENT.size
    if size > bundle.MAX_SIZE:
        raise ValueError('oversized root collation section')
    output = bytearray(HEADER.pack(1, UNICODE_VERSION, key_count, element_count))
    key_at = element_at = 0
    # Both pools are contiguous partitions. This initial format favors simple,
    # allocation-free validation over shared-vector reference bookkeeping.
    for key, elements in rows:
        output.extend(MAPPING.pack(key_at, len(key), element_at, len(elements)))
        key_at += len(key)
        element_at += len(elements)
    for key, elements in rows:
        for c in key:
            output.extend(struct.pack('<I', c))
    for key, elements in rows:
        for variable, primary, secondary, tertiary in elements:
            output.extend(ELEMENT.pack(primary, secondary, tertiary, variable))
    return len(rows), bytes(output)


def decode(section):
    if section is None:
        return None
    count, raw = section
    if (type(count) is not int or not 1 <= count <= 0xffffffff or
            not isinstance(raw, bytes) or not HEADER.size <= len(raw) <= bundle.MAX_SIZE):
        raise ValueError('invalid root collation count/extent')
    version, unicode_version, key_count, element_count = HEADER.unpack_from(raw)
    size = HEADER.size + count * MAPPING.size + key_count * 4 + element_count * ELEMENT.size
    if version != 1 or unicode_version != UNICODE_VERSION or size != len(raw):
        raise ValueError('invalid root collation version/dimensions')
    keys_at = HEADER.size + count * MAPPING.size
    elements_at = keys_at + key_count * 4
    keys = tuple(c[0] for c in struct.iter_unpack('<I', raw[keys_at:elements_at]))
    elements = []
    for primary, secondary, tertiary, variable in ELEMENT.iter_unpack(raw[elements_at:]):
        if variable > 1 or variable and not primary:
            raise ValueError('invalid root collation variable flag')
        elements.append((bool(variable), primary, secondary, tertiary))
    rows = []
    next_key = next_element = 0
    for start, length, element_start, element_length in MAPPING.iter_unpack(raw[HEADER.size:keys_at]):
        if (start != next_key or not length or length > key_count - start or
                element_start != next_element or not element_length or
                element_length > element_count - element_start):
            raise ValueError('invalid root collation pool partition')
        next_key += length
        next_element += element_length
        rows.append((keys[start:next_key], tuple(elements[element_start:next_element])))
    if next_key != key_count or next_element != element_count:
        raise ValueError('unused root collation pool data')
    result = {'version': source.VERSION, 'rows': tuple(rows)}
    if encode(result) != section:
        raise ValueError('noncanonical root collation mapping order')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    data = source.collect(acquire.Sources(args.sources))
    section = encode(data)
    if decode(section) != data:
        raise ValueError('root collation roundtrip mismatch')
    report = source.summary(data)
    report['section_bytes'] = len(section[1])
    print(json.dumps(report, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
