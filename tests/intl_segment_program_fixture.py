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

"""Independent golden bytes plus authored mode, empty and wide-state packs."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import segment_program_tables as wire
import tables
from test_intl_segment_program_tables import golden, profile
from intl_segment_language_fixture import char, node


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    empty = profile('GraphemeClusterBreak', ())
    sentence = profile('SentenceBreak', ((char('a'), char('b'), True),))._replace(
        roles=(2,), class_flags=(0, 1, 16))
    bos = profile('GraphemeClusterBreak', ((node('concat', node('start'), char('a')), char('b'), False),))
    wide = profile()
    machine = wide.program.machines[0]
    expanded = machine._replace(dfa=machine.dfa._replace(
        rows=tuple((min(state + 1, 256), 0, 0) for state in range(257)),
        accepted=tuple(state in (7, 8, 255, 256) for state in range(257))))
    wide = wide._replace(program=wide.program._replace(machines=(expanded,) + wide.program.machines[1:]))
    sections = [('golden', (1, golden())), ('absent', None)]
    rejecting = bytearray(golden())
    rejecting[204] = rejecting[220] = 0
    lexical = bytearray(golden())
    lexical[157] = 1
    sections += [('rejecting', (1, bytes(rejecting))), ('lexical', (1, bytes(lexical)))]
    nullable = profile('GraphemeClusterBreak', ((node('epsilon'), node('epsilon'), True),))
    directional = profile('GraphemeClusterBreak', ((char('a'), node('concat', char('b'), char('c')), False),))
    priority = profile('GraphemeClusterBreak', ((char('a'), char('b'), False),
                                              (node('epsilon'), node('epsilon'), True)))
    bits = profile('GraphemeClusterBreak', tuple((char('a'), char(str(i)), False) for i in range(10)) +
                   ((char('x'), char('9'), False),))
    r = char('r')
    parity = profile('GraphemeClusterBreak', ((node('concat', node('start'), r,
                                                   node('star', node('concat', r, r))), r, False),))
    for name, source in (('empty', empty), ('sentence', sentence), ('bos', bos),
                         ('wide', wide), ('nullable', nullable), ('directional', directional),
                         ('priority', priority), ('bits', bits), ('parity', parity)):
        kind = wire.project(source).kind
        sections.append((name, wire.encode((source,), ((0, kind, 0),), bytes(32), 2)))
    sections.append(('two', wire.encode((empty, profile()), ((0, 0, 0), (0, 1, 1)), bytes(32), 2)))
    alternate = profile('WordBreak', ((char('c'), char('d'), False),))
    sections.append(('routes', wire.encode((profile(), alternate), ((0, 1, 0), (1, 1, 1)), bytes(32), 2)))
    four = profile('WordBreak', ((char('a'), char('b'), False), (char('c'), char('d'), False)))
    for name, source in (('noncanonical_machines', four), ('noncanonical_states', wide)):
        raw = bytearray(wire.encode((source,), ((0, 1, 0),), bytes(32), 2)[1])
        descriptor = wire.DESCRIPTOR.unpack_from(raw, wire.HEADER.size)
        offsets, bodies, size = wire.layout(descriptor[2], descriptor[3], descriptor[4],
                                            tuple(len(item.dfa.rows) for item in source.program.machines))
        start = descriptor[6]
        # Keep all owners/states reachable with valid directions/targets.
        # Only first-use/BFS numbering is wrong; bypass the semantic writer's
        # final decode deliberately so the C proof is tested independently.
        if name == 'noncanonical_machines':
            struct.pack_into('<HH', raw, start + offsets[3], 2, 3)
            struct.pack_into('<HH', raw, start + offsets[3] + wire.RULE.size, 0, 1)
        else:
            struct.pack_into('<3H', raw, start + bodies[0][0], 2, 0, 1)
        sections.append((name, (1, bytes(raw))))
    lines = ['/* Generated authored segmentation program fixtures. */',
             '#include <stdint.h>', '#include <stddef.h>', '']
    for name, section in sections:
        # Shared locale indexes are valid independently of section 40; en has
        # root as parent. This lets native tests corrupt only the inner tables.
        parts = {bundle.STRINGS: tables.encode_strings(('', 'en', 'root')),
                 bundle.LOCALES: (2, struct.pack('<4I', 1, 1, 2, 0xffffffff))}
        if section is not None:
            parts[bundle.SEGMENT_PROGRAMS] = section
        binary = bundle.pack(parts, bytes(32), 'native')
        symbol = 'qjs_intl_segment_program_' + name
        lines.append('const uint8_t ' + symbol + '_bytes[] = {')
        for at in range(0, len(binary), 16):
            lines.append('    ' + ', '.join('0x%02x' % value for value in binary[at:at + 16]) + ',')
        lines += ['};', 'const size_t ' + symbol + '_length = sizeof(' + symbol + '_bytes);', '']
    atomic_write(args.output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    main()
