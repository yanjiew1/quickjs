#!/usr/bin/env python3
"""Compile released CLDR Windows mappings into a bounded binary table."""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import json
from pathlib import Path
import re
import struct

from acquire import atomic_write
from bundle import unpack


def pack(contents, available=None):
    groups = {}
    for item in json.loads(contents)['supplemental']['windowsZones']['mapTimezones']:
        row = item['mapZone']
        key, territory, targets = row['_other'], row['_territory'], row['_type']
        if not re.fullmatch(r'[ -~]{1,127}', key) or not re.fullmatch(r'001|[A-Z]{2}', territory):
            raise ValueError('invalid Windows key or territory')
        names = targets.split()
        if not names or any(not re.fullmatch(r'[A-Za-z0-9_+./-]{1,255}', name) for name in names):
            raise ValueError('invalid Windows mapping target')
        if available is not None and any(name not in available for name in names):
            raise ValueError('Windows mapping target absent from IANA pack')
        rows = groups.setdefault(key.lower(), {})
        if territory in rows:
            raise ValueError('duplicate Windows key/territory')
        rows[territory] = ' '.join(names)
    if not groups or any('001' not in rows for rows in groups.values()):
        raise ValueError('missing Windows world fallback')
    pool, offsets, keys, rows = bytearray(), {}, [], []

    def intern(text):
        if text not in offsets:
            offsets[text] = len(pool)
            pool.extend(text.encode('ascii') + b'\0')
        return offsets[text]

    for key, mappings in sorted(groups.items()):
        keys.append((intern(key), len(rows), len(mappings), 0))
        for territory, targets in sorted(mappings.items()):
            rows.append((territory.encode('ascii').ljust(3, b'\0'), 0,
                         intern(targets), len(targets)))
    if max(len(keys), len(rows), len(pool)) > 65535:
        raise ValueError('Windows mapping exceeds uint16 storage')
    strings = 16 + len(keys) * 8 + len(rows) * 8
    result = struct.pack('<4sIHHI', b'WTZ1', strings + len(pool), len(keys), len(rows), strings)
    result += b''.join(struct.pack('<4H', *row) for row in keys)
    result += b''.join(struct.pack('<3sBHH', *row) for row in rows)
    return result + pool


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--zones', type=Path, required=True)
    args = parser.parse_args()
    available = {row['identifier'] for row in unpack(args.zones.read_bytes())['records']}
    data = pack(args.input.read_bytes(), available)
    lines = ['/* Generated CLDR Windows mapping binary; do not edit.',
             ' * Unicode license: data/timezone/sources/cldr/LICENSE. */',
             '#include "src/timezone/windows-zone-data.h"',
             'const unsigned char qjs_windows_zone_data[] = {']
    for start in range(0, len(data), 16):
        lines.append('    ' + ', '.join('0x%02x' % b for b in data[start:start + 16]) + ',')
    lines += ['};', 'const size_t qjs_windows_zone_data_size = sizeof(qjs_windows_zone_data);', '']
    atomic_write(args.output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    main()
