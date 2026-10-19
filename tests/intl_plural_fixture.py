#!/usr/bin/env python3
"""Emit standalone plural metadata with unused malformed condition strings."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import plural_tables
import tables


def render():
    tags = ('en', 'root')
    ranges = ((1, 1, 1), (1, 5, 5))
    data = {'rows': (('en', 0, ((1, 'i = 1 and v = 0'), (5, '')), ranges),
                     ('en', 1, ((1, 'n % 10 = 1'), (2, 'n % 10 = 2'), (5, '')), ranges),
                     ('root', 0, ((5, ''),), ()), ('root', 1, ((5, ''),), ()))}
    pool = tuple(sorted(plural_tables.strings(data) | set(tags) |
                        {'x = 0', ' n = 1', 'n  = 1', 'n = 1 @integer 1', 'n\u200e=1'},
                        key=lambda value: value.encode('utf-8')))
    ids = {value: index for index, value in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: (2, tables.LOCALE.pack(ids['en'], 1) +
                                 tables.LOCALE.pack(ids['root'], tables.ROOT)),
                bundle.PLURALS: plural_tables.encode(data, tags, ids)}
    binary = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(binary)
    lines = ['/* Authored fixture; no downloaded source records. */',
             '#include <stddef.h>', '#include <stdint.h>',
             'const uint8_t qjs_intl_native_bytes[] = {']
    for at in range(0, len(binary), 16):
        lines.append('    ' + ', '.join('0x%02x' % byte for byte in binary[at:at + 16]) + ',')
    lines += ['};', 'const size_t qjs_intl_native_length = sizeof(qjs_intl_native_bytes);', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, render().encode('ascii'))
