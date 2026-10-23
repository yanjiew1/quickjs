#!/usr/bin/env python3
"""Small authored cost languages and protected-boundary programs."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
from intl_segment_language_fixture import char, node
from test_intl_segment_program_tables import profile
import segment_dictionary
import segment_dictionary_trie
import segment_dictionary_tables
import segment_program_tables
import tables


def dictionary(text, valued):
    return segment_dictionary_trie.prepare(segment_dictionary.parse(text.encode('utf-8'), valued))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    epsilon = node('epsilon')
    lexical = profile('WordBreak', ((epsilon, epsilon, False),))._replace(roles=(1,))
    blocked = profile('WordBreak', ((char('甲'), epsilon, False),
                                    (epsilon, epsilon, False)))._replace(roles=(0, 1))
    hard = profile('WordBreak', ((char('甲'), epsilon, True),))
    marks = '\u0301\u034f\u093e\u1037\u1cd0'
    grapheme = profile('GraphemeClusterBreak', ((char('甲ก'), char(marks), False),
                                              (char(marks), char(marks), False)))
    programs = (grapheme, profile('WordBreak', ()), lexical, blocked, hard)
    routes = ((0, 0, 0), (0, 1, 1), (1, 1, 2), (2, 1, 3), (3, 1, 4))
    sections = [('programs', {
        bundle.STRINGS: tables.encode_strings(('', 'en', 'fr', 'ja', 'root')),
        bundle.LOCALES: (4, struct.pack('<8I', 1, 3, 2, 3, 3, 3, 4, 0xffffffff)),
        bundle.SEGMENT_PROGRAMS: segment_program_tables.encode(programs, routes, bytes(32), 4),
    })]
    costs = {
        'weighted': '甲\t5\n甲乙\t10\n乙丙\t1\n',
        'zero': '甲乙丙\t0\n',
        'huge': '甲\t4294967295\n',
        'tie': '甲\t255\n甲乙\t510\n',
        'prefix': '甲\t0\n',
        'suffix': '丙\t0\n',
        'pass': '甲\t0\n甲乙\t0\n',
        'mixed': '甲カ\t0\nカーカ\t0\nカ・カ\t0\n𠀀乙\t0\n',
    }
    for name, text in costs.items():
        sections.append((name, {bundle.SEGMENT_DICTIONARIES:
                                segment_dictionary_tables.encode({'cjk': dictionary(text, True)})}))
    for name, text in (('sea', 'ก\nกข\nกขค\nก\u034fข\nก\u0301ข\nก\u093eข\nก\u1037ข\nก\u1cd0ข\n'),
                       ('sea_prefix', 'ก\nกขฆ\n')):
        sections.append((name, {bundle.SEGMENT_DICTIONARIES:
                                segment_dictionary_tables.encode({'thai': dictionary(text, False)})}))
    lines = ['/* Generated authored dictionary-refinement fixtures. */',
             '#include <stdint.h>', '#include <stddef.h>', '']
    for name, parts in sections:
        binary = bundle.pack(parts, bytes(32), 'native')
        symbol = 'qjs_intl_dictionary_refine_' + name
        lines.append('const uint8_t ' + symbol + '_bytes[] = {')
        for at in range(0, len(binary), 16):
            lines.append('    ' + ', '.join('0x%02x' % value for value in binary[at:at + 16]) + ',')
        lines += ['};', 'const size_t ' + symbol + '_length = sizeof(' + symbol + '_bytes);', '']
    atomic_write(args.output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    main()
