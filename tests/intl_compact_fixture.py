#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Emit authored compact maps including literal, fallback and optional forms."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import compact_patterns
import compact_tables
import numbering_tables
from resolve import BLOCK
import tables


def render():
    tags = ('en', 'root')
    numbering = {'systems': (('latn', 'numeric', '0123456789'),
                              ('mathbold', 'numeric', '𝟎𝟏𝟐𝟑𝟒𝟓𝟔𝟕𝟖𝟗'),
                              ('roman', 'algorithmic', 'roman-upper')),
                 'locales': (('root', 'latn', 'latn', None, None),)}
    maps = ((),
            ((3, '1', 'standard', 'mille'),
             (3, 'other', 'standard', '0 mila')),
            ((3, 'other', 'standard', '0'),
             (3, 'other', 'alphaNextToNumber', '¤ 0K'),
             (3, 'other', 'noCurrency', '')),
            ((3, 'other', 'standard', '0K'),
             (6, 'other', 'standard', '0M'),
             (9, 'other', 'standard', ' 0 giga name '),
             (12, 'other', 'standard', '0💯'),
             (15, 'other', 'standard', '0 name-part end'),
             (18, 'other', 'standard', '0\t\n\r\u0085\u1680\u2000\u2028\u2029\u202f\u205f\u3000')),
            ((3, 'other', 'standard', '¤0K'),
             (3, 'one', 'alphaNextToNumber', BLOCK),
             (6, 'other', 'standard', '0¤K'),
             (9, 'other', 'standard', '0K ¤'),
             (12, 'other', 'standard', '¤ 0K'),
             (15, 'other', 'standard', '¤0T'),
             (15, 'other', 'alphaNextToNumber', '¤ 000T'),
             (15, 'one', 'alphaNextToNumber', '¤ 0bad'),
             (18, 'other', 'standard', '¤0Q'),
             (18, 'one', 'alphaNextToNumber', '¤ 0Q'),
             (21, 'other', 'standard', '¤0R'),
             (21, 'one', 'standard', '¤0S'),
             (21, 'one', 'alphaNextToNumber', ''),
             (24, 'other', 'standard', '¤ 0Y'),
             (24, 'other', 'alphaNextToNumber', '¤ 00Y')))
    source = {'systems': ('latn', 'mathbold'), 'kinds': compact_patterns.KINDS,
              'maps': maps,
              'rows': tuple((tag, system, kind,
                             0 if kind == 2 and system == 'mathbold' else
                             (2 if tag == 'en' else 4) if kind == 2 else
                             1 if kind == 1 and tag == 'en' else 3)
                            for tag in tags for system in ('latn', 'mathbold') for kind in range(3))}
    prepared = compact_tables.prepare(source, tags, numbering)
    pool = tuple(sorted(compact_tables.strings(prepared) | numbering_tables.strings(numbering) | set(tags),
                        key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: (2, tables.LOCALE.pack(ids['en'], 1) +
                                 tables.LOCALE.pack(ids['root'], tables.ROOT))}
    sections.update(numbering_tables.encode(numbering, ids))
    sections[bundle.COMPACT_PATTERNS] = compact_tables.encode(prepared, ids)
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
