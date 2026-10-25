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

"""Small authored dictionary packs and structurally valid hostile DAGs."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import segment_dictionary as dictionary
import segment_dictionary_tables as wire
import segment_dictionary_trie as trie


def prepared(text, valued=False):
    return trie.prepare(dictionary.parse(text.encode('utf-8'), valued))


def hostile(data):
    # Bypass semantic encoding deliberately; the C reader must independently
    # reject these graphs after their envelope/array extents have validated.
    counts = len(data.alphabet), len(data.values), len(data.terminals), len(data.labels)
    offsets, size = wire.layout(*counts)
    start = wire.HEADER.size + wire.RECORD.size
    raw = bytearray(wire.HEADER.pack(wire.MAGIC, wire.VERSION, 1))
    raw.extend(wire.RECORD.pack(4, 0, data.root, data.words, data.max_scalars, data.max_utf16,
                                *counts, start, size))
    raw.extend(bytes(size))
    for offset, code, values in zip(offsets, ('I', 'I', 'I', 'H', 'H', 'I'), data[:6]):
        encoded = struct.pack('<{}{}'.format(len(values), code), *values)
        raw[start + offset:start + offset + len(encoded)] = encoded
    return 1, bytes(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    valid = wire.encode({'cjk': prepared('a\t0\nab\t4294967295\ncb\t4294967295\n'
                                        'abcd\t20\n𠀀𠀁𠀂\t30\n', True),
                         'thai': prepared('ก\nกา\n𠀀\n')})
    unreachable = trie.Trie((97,), (0,), (0, 0, 0, 1), (1, 1, 0), (0,), (0,), 2, False, 1, 1, 1)
    dead = trie.Trie((97, 98), (0,), (0, 0, 0, 2), (1, 0, 0), (0, 1), (0, 1), 2, False, 1, 1, 1)
    packs = [('fixture', {bundle.SEGMENT_DICTIONARIES: valid}),
             ('unreachable', {bundle.SEGMENT_DICTIONARIES: hostile(unreachable)}),
             ('dead', {bundle.SEGMENT_DICTIONARIES: hostile(dead)}),
             ('absent', {99: (0, b'')})]
    # A test-only emitter must retain hostile bytes instead of calling the
    # production embedder, which correctly requires fully valid typed tables.
    lines = ['/* Generated authored dictionary fixtures. */', '#include <stdint.h>', '#include <stddef.h>']
    for name, sections in packs:
        binary = bundle.pack(sections, bytes(32), 'native')
        symbol = 'qjs_intl_dictionary_' + name
        lines.append('const uint8_t ' + symbol + '_bytes[] = {')
        for at in range(0, len(binary), 16):
            lines.append('    ' + ', '.join('0x%02x' % value for value in binary[at:at + 16]) + ',')
        lines += ['};', 'const size_t ' + symbol + '_length = sizeof(' + symbol + '_bytes);', '']
    atomic_write(args.output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    main()
