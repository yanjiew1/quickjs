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

"""Policy-focused sentence program; deliberately not a UAX conformance oracle."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
from intl_segment_language_fixture import char, node
import scalar_trie
from segment_lower import LoweredProgram, LoweredRule
from segment_profile import Profile
from segment_program import assemble
import segment_program_tables
import segment_suppression_tables
import tables


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    # Every interior position is a soft candidate except positions after \n/#.
    # This isolates filtering from the separately tested base-rule automata.
    epsilon = node('epsilon')
    lowered = LoweredProgram((LoweredRule(None, char('\n#'), epsilon, True, False),
                              LoweredRule(None, epsilon, epsilon, True, False)), None)
    contexts = tuple(char(text).character.ranges for text in ('.!', ')', ' ', '\n', '\u0308'))
    program = assemble(lowered, extra_predicates=contexts)
    ids = tuple(program.alphabet.predicates.index(values) for values in contexts)
    flags = tuple(sum(1 << flag for flag, index in enumerate(ids) if mask & (1 << index))
                  for mask in program.alphabet.masks)
    profile = Profile('SentenceBreak', program, (0, 2), flags, 10)
    words = ('A.', 'A.B.', 'B.', 'Mr.', 'n. Chr.', 'x#y.', '𠀀.', 'a' * 255 + '.', 'a\nb.')
    graph = scalar_trie.prepare(tuple((text, None) for text in sorted(words)))
    binary = bundle.pack({
        bundle.STRINGS: tables.encode_strings(('', 'root')),
        bundle.LOCALES: (1, struct.pack('<II', 1, 0xffffffff)),
        bundle.SEGMENT_PROGRAMS: segment_program_tables.encode((profile,), ((0, 2, 0),), bytes(32), 1),
        bundle.SEGMENT_SUPPRESSIONS: segment_suppression_tables.encode((graph,), ((0, 0),), bytes(32), 1),
    }, bytes(32), 'native')
    lines = ['/* Generated authored sentence suppression runtime fixture. */',
             '#include <stdint.h>', '#include <stddef.h>', '',
             'const uint8_t qjs_intl_suppression_runtime_bytes[] = {']
    for at in range(0, len(binary), 16):
        lines.append('    ' + ', '.join('0x%02x' % value for value in binary[at:at + 16]) + ',')
    lines += ['};', 'const size_t qjs_intl_suppression_runtime_length =',
              '    sizeof(qjs_intl_suppression_runtime_bytes);', '']
    atomic_write(args.output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    main()
