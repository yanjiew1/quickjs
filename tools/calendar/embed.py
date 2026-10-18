#!/usr/bin/env python3
"""Validate a QJCA pack and generate its sole immutable C data owner."""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
from pathlib import Path

from acquire import atomic_write
from bundle import unpack


def render(binary):
    datasets = unpack(binary)
    lines = ['/* Generated calendar data; do not edit.',
             ' * Includes derived ICU4X data, Copyright 2020-2024 Unicode, Inc.',
             ' * Unicode License V3: data/calendar/sources/LICENSE-ICU4X.',
             ' * Persian factual transcription is CC0; see tools/calendar/README.md.',
             ' * PMO/KASI factual records and explicit boundary approximations are',
             ' * documented in the same source manifest; source code is not embedded.',
             ' * Japanese era data derives from CLDR; see data/calendar/sources/LICENSE-CLDR.',
             ' */', '#include "src/calendar/data.h"', '',
             'static const uint8_t calendar_bytes[] = {']
    for n in range(0, len(binary), 16):
        lines.append('    ' + ', '.join('0x%02x' % b for b in binary[n:n + 16]) + ',')
    lines += ['};', '', 'const QJSCalendarData qjs_native_calendar_data = {',
              '    calendar_bytes, sizeof(calendar_bytes), %d,' % len(datasets), '};', '']
    return '\n'.join(lines)


def generate(input_path, output_path):
    source = render(input_path.read_bytes())
    atomic_write(output_path, source.encode('ascii'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generate(args.input, args.output)


if __name__ == '__main__':
    main()
