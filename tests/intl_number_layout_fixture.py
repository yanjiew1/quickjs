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

"""Emit authored number/currency layouts with sparse and exact text states."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import number_layout
import number_layout_tables
import numbering
import numbering_tables
from resolve import BLOCK, INHERIT
import tables
from test_intl_list_patterns import graph
from test_intl_number_layout import Sources


def render():
    source = Sources()
    source.set('en', '''<ldml><numbers><miscPatterns numberSystem="latn">
      <pattern type="range">\u200e{0} à {1}\u200e</pattern></miscPatterns>
      <currencyFormats numberSystem="latn"><unitPattern count="one">''' + BLOCK + '''</unitPattern>
      <unitPattern count="two"/><currencySpacing><beforeCurrency><insertBetween/></beforeCurrency>
      </currencySpacing></currencyFormats></numbers></ldml>''')
    tags = graph().tags
    data = number_layout.collect(source, graph())
    systems = numbering.collect(source, graph())
    pool = tuple(sorted(number_layout_tables.strings(data) | numbering_tables.strings(systems) |
                        set(tags) | {'', BLOCK, INHERIT}, key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                bundle.NUMBER_LAYOUTS: number_layout_tables.encode(data, tags, systems, ids)}
    sections.update(numbering_tables.encode(systems, ids))
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
