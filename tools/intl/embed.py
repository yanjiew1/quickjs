#!/usr/bin/env python3
"""Validate a prepared native Intl pack and emit its single immutable C owner."""
import argparse
from pathlib import Path

from acquire import atomic_write
from tables import validate


def render(binary):
    validate(binary)
    lines = ['/* Generated native Intl data; do not edit.',
             ' * Derived from Unicode CLDR; Copyright Unicode, Inc.',
             ' * See data/intl/LICENSE-CLDR and the matching data manifest.',
             ' */', '#include "src/intl/native/data.h"', '',
             'const uint8_t qjs_intl_native_bytes[] = {']
    for at in range(0, len(binary), 16):
        lines.append('    ' + ', '.join('0x%02x' % byte for byte in binary[at:at + 16]) + ',')
    lines += ['};', '', 'const size_t qjs_intl_native_length = sizeof(qjs_intl_native_bytes);', '']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, render(args.input.read_bytes()).encode('ascii'))


if __name__ == '__main__':
    main()
