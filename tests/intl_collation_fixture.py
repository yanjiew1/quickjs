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

"""Emit a small root-only pack with overlapping keys and long expansions."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import collation_contexts
import collation_root
import collation_tables


def fixture(contexts=False):
    # Deliberately exceed the pinned source's observed key/expansion maxima.
    rows = (
        ((0,), ((False, 0, 0, 0),)),
        ((0x61,), ((False, 0x1000, 0x20, 2),)),
        ((0x61, 0x62, 0x79), ((False, 0x1001, 0x20, 2), (False, 0, 0x21, 0))),
        ((0x61, 0x62, 0x79, 0x7a), ((False, 0x1002, 0x20, 2),)),
        ((0x62,), tuple((i == 0, 0x1100 + i, 0x20, 2) for i in range(19))),
        ((0xfdd0,), ((False, 0x2000, 0, 0),)),
        ((0x1f600,), ((False, 0x3000, 0x20, 2),)),
    )
    if contexts:
        rows = (
            ((0x61,), ((False, 0x1000, 0x20, 2),)),
            ((0x61, 0x62), ((False, 0x1000, 0x20, 2), (False, 0, 0x21, 2))),
            ((0x61, 0x62, 0x63), ((False, 0x1000, 0x20, 2), (False, 0, 0x21, 2),
                                (False, 0, 0x22, 2))),
        )
    root = {'version': collation_root.VERSION, 'rows': rows}
    section = collation_tables.encode(root)
    # No locale or string section: root mappings have no such dependency.
    sections = {bundle.COLLATION_ROOT: section}
    if contexts:
        sections[bundle.COLLATION_CONTEXTS] = collation_contexts.encode(((1, 1, 1), (2, 2, 2)), root)
    return bundle.pack(sections, bytes(32), 'minimal')


def emit(binary, output):
    # Authored/oracle packs omit locale/string sections. Adjacent strings keep
    # large source oracles cheap to compile; the bound excludes the terminator.
    lines = ['#include <stddef.h>', '#include <stdint.h>',
             '#if defined(__has_attribute)', '#if __has_attribute(nonstring)',
             '__attribute__((nonstring))', '#endif', '#endif',
             'const uint8_t qjs_intl_collation_fixture_bytes[%d] =' % len(binary)]
    for at in range(0, len(binary), 32):
        lines.append('    "' + ''.join('\\x%02x' % b for b in binary[at:at + 32]) + '"')
    lines.extend((';', 'const size_t qjs_intl_collation_fixture_length = %d;' % len(binary), ''))
    atomic_write(output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--contexts', action='store_true')
    mode.add_argument('--input', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    emit(args.input.read_bytes() if args.input else fixture(args.contexts), args.output)
