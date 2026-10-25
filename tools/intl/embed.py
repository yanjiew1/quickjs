#!/usr/bin/env python3
"""Validate a prepared native Intl pack and emit its single immutable C owner."""
import argparse
from pathlib import Path

from acquire import atomic_write
from tables import validate
import bundle


def render(binary, profile=None):
    decoded = validate(binary)
    if profile is not None and (profile not in bundle.PROFILES or
                                decoded['envelope']['profile'] != bundle.PROFILES[profile]):
        raise ValueError('prepared Intl pack does not match the selected build profile')
    lines = ['/* Generated native Intl data; do not edit.',
             ' * Derived from Unicode CLDR; Copyright Unicode, Inc.',
             ' * See data/intl/LICENSE-CLDR and the matching data manifest.',
             ' */', '#include "src/intl/native/data.h"', '',
             '/* Counted binary bytes, not a NUL-terminated C string. */',
             '#if defined(__has_attribute)', '#if __has_attribute(nonstring)',
             '__attribute__((nonstring))', '#endif', '#endif',
             'const uint8_t qjs_intl_native_bytes[%d] =' % len(binary)]
    if bundle.SEGMENT_DICTIONARIES in decoded['envelope']['sections']:
        lines[3:3] = [
            ' * Dictionary data derived from ICU; see data/intl/LICENSE-SEGMENT-DICTIONARIES',
            ' * and data/intl/NOTICE-SEGMENT-DICTIONARIES for complete source attributions.',
        ]
    # A string initializer avoids a compiler AST node per byte of a large pack.
    # C's explicit array bound omits only the implicit literal terminator; real
    # embedded/trailing zero bytes remain part of the exact binary object.
    for at in range(0, len(binary), 64):
        lines.append('    "' + ''.join('\\x%02x' % byte for byte in binary[at:at + 64]) + '"')
    lines += [';', '', 'const size_t qjs_intl_native_length = sizeof(qjs_intl_native_bytes);', '']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(bundle.PROFILES))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, render(args.input.read_bytes(), args.profile).encode('ascii'))


if __name__ == '__main__':
    main()
