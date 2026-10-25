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

"""Emit small authored packs for the division-grammar C owner."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
from parents import ParentGraph
import tables
import unit_grammar
import unit_grammar_tables
from test_intl_unit_grammar import Sources, graph


def pack(projected):
    extra = '''<grammaticalDerivations locales="de">
      <deriveComponent feature="case" structure="per" value0="compound" value1="accusative"/>
      </grammaticalDerivations><grammaticalDerivations locales="fr">
      <deriveComponent feature="plural" structure="per" value0="compound" value1="one"/>
      </grammaticalDerivations>'''
    inheritance = graph()
    tags = inheritance.tags
    if projected:
        extra += '''<grammaticalDerivations locales="en_1901_1996">
          <deriveComponent feature="plural" structure="per" value0="one" value1="other"/>
          </grammaticalDerivations>'''
        inheritance = ParentGraph(inheritance.tags + ('en-1901-1996',), {},
                                  {'grammaticalFeatures': {'de': 'en-1901-1996'}},
                                  {'und': 'en-Latn-US'}, (), False)
        tags = ('de', 'root')
    data = unit_grammar_tables.select(unit_grammar.collect(Sources(extra), inheritance), tags)
    pool = tuple(sorted(unit_grammar_tables.strings(data) | set(tags) |
                        {'', '0', 'En', 'en-us', 'en-1901-1901', 'bad_tag', '↑↑↑'}))
    ids = {value: i for i, value in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                          bundle.UNIT_GRAMMAR: unit_grammar_tables.encode(data, tags, ids)},
                         bytes(32), 'minimal')
    tables.validate(binary)
    return binary


def render():
    lines = ['/* Authored data; no downloaded source. */', '#include <stddef.h>', '#include <stdint.h>']
    for name, projected in (('qjs_intl_native', False), ('qjs_intl_grammar_projected', True)):
        binary = pack(projected)
        lines.append('const uint8_t %s_bytes[] = {' % name)
        for at in range(0, len(binary), 16):
            lines.append('    ' + ', '.join('0x%02x' % byte for byte in binary[at:at + 16]) + ',')
        lines.extend(('};', 'const size_t %s_length = sizeof(%s_bytes);' % (name, name), ''))
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, render().encode('ascii'))
