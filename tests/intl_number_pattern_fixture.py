#!/usr/bin/env python3
"""Emit authored compiled numeric patterns for standalone C owner tests."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import number_patterns
import number_pattern_tables
import numbering_tables
from resolve import BLOCK
import tables


def render():
    tags = ('en', 'root')
    numbering = {'systems': (('latn', 'numeric', '0123456789'),
                              ('mathbold', 'numeric', '𝟎𝟏𝟐𝟑𝟒𝟓𝟔𝟕𝟖𝟗'),
                              ('roman', 'algorithmic', 'roman-upper')),
                 'locales': (('root', 'latn', 'latn', None, None),)}
    root = ('#,##0.###', '[#E0]', '#,##0%', '¤0.00', '¤0.00;(¤0.00)',
            None, None, None, None)
    en = ("'#{}'#,##,##0.00##", '0.000E+000', '% 0', "¤'#' 0;¤'#' 0-", '¤0;(¤0)',
          BLOCK, BLOCK, '', '')
    patterns = {'systems': ('latn', 'mathbold'), 'fields': number_patterns.FIELDS,
                'overrides': ("#,##0.###", "0.00 ¤¤", "¤'legacy'0.0;¤'legacy'0.0-"),
                'grouping': (('en', 2), ('root', 1)),
                'rows': tuple((tag, system, en if tag == 'en' else root)
                              for tag in tags for system in ('latn', 'mathbold'))}
    pool = tuple(sorted(number_pattern_tables.strings(patterns) |
                        numbering_tables.strings(numbering) | set(tags),
                        key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: (2, tables.LOCALE.pack(ids['en'], 1) +
                                 tables.LOCALE.pack(ids['root'], tables.ROOT))}
    sections.update(numbering_tables.encode(numbering, ids))
    sections[bundle.NUMBER_PATTERNS] = number_pattern_tables.encode(patterns, tags, numbering, ids)
    binary = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(binary)
    lines = ['/* Authored records; no downloaded source data. */',
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
