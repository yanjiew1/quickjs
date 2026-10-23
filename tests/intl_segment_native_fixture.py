#!/usr/bin/env python3
"""Complete small native provider pack and targeted admission failures.

These authored languages isolate routing and lifetime behavior; they do not
stand in for CLDR/UAX conformance or require the production dictionaries.
"""
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
import segment_dictionary_tables
import segment_dictionary_trie
import segment_program_tables
import segment_suppression_tables
import tables


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    epsilon = node('epsilon')
    programs = (profile('GraphemeClusterBreak', ()),
                profile('WordBreak', ((epsilon, epsilon, False),))._replace(roles=(1,)),
                profile('SentenceBreak', ()),
                profile('GraphemeClusterBreak', ((char('a'), char('b'), False),)))
    routes = tuple((locale, kind, 3 if locale == 2 and kind == 0 else kind)
                   for locale in range(3) for kind in range(3))
    graphs = {}
    for family in segment_dictionary_tables.FAMILIES:
        valued = family == 'cjk'
        text = '甲\t5\n甲乙\t10\n乙丙\t1\n' if valued else 'ก\n'
        graphs[family] = segment_dictionary_trie.prepare(
            segment_dictionary.parse(text.encode('utf-8'), valued))
    identity = bytes(range(32))
    sections = {
        bundle.STRINGS: tables.encode_strings(('', 'en', 'root', 'zh')),
        bundle.LOCALES: (3, struct.pack('<6I', 1, 1, 2, 0xffffffff, 3, 1)),
        bundle.SEGMENT_DICTIONARIES: segment_dictionary_tables.encode(graphs),
        bundle.SEGMENT_PROGRAMS: segment_program_tables.encode(programs, routes, identity, 3),
        bundle.SEGMENT_SUPPRESSIONS: segment_suppression_tables.encode(
            (None,), tuple((locale, 0) for locale in range(3)), identity, 3),
    }
    cases = [sections]
    # Each variant changes only the provider-level contract named below.
    for key, value in (
            (bundle.SEGMENT_SUPPRESSIONS, segment_suppression_tables.encode(
                (None,), tuple((locale, 0) for locale in range(3)), bytes(32), 3)),
            (bundle.SEGMENT_PROGRAMS, segment_program_tables.encode(programs, routes[:-1], identity, 3)),
            (bundle.SEGMENT_DICTIONARIES, segment_dictionary_tables.encode(
                {name: graph for name, graph in graphs.items() if name != 'thai'})),
            (bundle.STRINGS, tables.encode_strings(('', 'en', 'xx', 'zh')))):
        parts = dict(sections)
        parts[key] = value
        cases.append(parts)
    cases.append(sections)  # Minimal profile has no native-provider capability.
    parts = dict(sections)
    parts[bundle.SEGMENT_SUPPRESSIONS] = segment_suppression_tables.encode(
        (None,), ((0, 0), (1, 0)), identity, 3)
    cases.append(parts)
    lines = ['/* Generated authored native provider fixtures. */',
             '#include <stdint.h>', '#include <stddef.h>', '']
    for case, parts in enumerate(cases):
        binary = bundle.pack(parts, bytes(32), 'minimal' if case == 5 else 'native')
        lines += ['#if QJS_INTL_TEST_SEGMENT_CASE == ' + str(case),
                  'const uint8_t qjs_intl_native_bytes[] = {']
        for at in range(0, len(binary), 16):
            lines.append('    ' + ', '.join('0x%02x' % value for value in binary[at:at + 16]) + ',')
        lines += ['};', '#endif', '']
    lines += ['const size_t qjs_intl_native_length = sizeof(qjs_intl_native_bytes);', '']
    atomic_write(args.output, '\n'.join(lines).encode('ascii'))


if __name__ == '__main__':
    main()
