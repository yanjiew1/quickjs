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

"""Emit authored sparse currency fields and a supplemental-only pack."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import currencies
import currency_tables
from resolve import BLOCK, INHERIT
import tables


def pack(empty):
    tags = ('en', 'root')
    entries = (('JPY', 'symbol', '', '¥'),
               ('USD', 'name', '', 'US dollar'),
               ('USD', 'name', 'other', 'US dollars'),
               ('USD', 'symbol', '', '$'),
               ('USD', 'narrow', '', '$'))
    english = entries + (('USD', 'name', 'one', 'dollar'),
                         ('USD', 'formal', '', 'US$'),
                         ('USD', 'variant', '', 'USD'),
                         ('USD', 'decimal', '', ';'),
                         ('USD', 'group', '', ' '),
                         ('USD', 'pattern', '', '¤0.00'))
    english = tuple((code, field, count, BLOCK if count == 'other' else '' if field == 'symbol' and
                     code == 'USD' else text) for code, field, count, text in english)
    data = {'fields': currencies.FIELDS, 'counts': currencies.COUNTS,
            'codes': ('JPY', 'USD'),
            'fractions': (('CHF', 2, 0, 2, 5), ('DEFAULT', 2, 0, 2, 0), ('JPY', 0, 0, 0, 0)),
            'maps': ((),) if empty else (english, entries),
            'root_names': () if empty else (('root', (('USD', 'other'),)),),
            'rows': (('en', 0), ('root', 0 if empty else 1))}
    return encode(data, tags, {'en': 'root', 'root': None})


def encode(data, tags, parents):
    pool = tuple(sorted(currency_tables.strings(data) | set(tags) | {'', BLOCK, INHERIT},
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: (len(tags), b''.join(tables.LOCALE.pack(ids[tag], tables.ROOT if
                                 parents[tag] is None else tags.index(parents[tag])) for tag in tags)),
                bundle.CURRENCIES: currency_tables.encode(data, tags, ids)}
    binary = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


def display_pack():
    from test_intl_currencies import Sources
    from test_intl_list_patterns import graph
    source = Sources()
    source.set('root', '''<currency type="USD"><displayName>root dollar</displayName>
      <displayName count="one">root one dollar</displayName>
      <displayName count="other">root dollars</displayName><symbol>$</symbol>
      <symbol alt="narrow">$</symbol></currency>
      <currency type="EUR"><displayName>root euro</displayName>
      <displayName count="0"/><displayName count="1">root exact euro</displayName>
      <displayName count="one">root one euro</displayName>
      <displayName count="other">root euros</displayName><symbol>€</symbol></currency>
      <currency type="GBP"><displayName>root pound</displayName><symbol>£</symbol></currency>
      <currency type="JPY"><symbol>¥</symbol></currency>''')
    source.set('en', '''<currency type="USD"><displayName count="one">dollar</displayName>
      <displayName count="other">''' + BLOCK + '''</displayName><symbol/></currency>
      <currency type="EUR"><displayName count="0"/><displayName count="1">exact euro</displayName>
      <displayName count="one">euro</displayName><displayName count="other">euros</displayName>
      <symbol alt="narrow">''' + BLOCK + '''</symbol></currency>
      <currency type="GBP"><displayName count="one"/><symbol>''' + BLOCK + '''</symbol></currency>''')
    source.set('en_GB', '''<currency type="USD"><displayName count="other">child dollars</displayName>
      </currency><currency type="EUR"><displayName count="1">''' + BLOCK + '''</displayName></currency>''')
    parents = graph()
    return encode(currencies.collect(source, parents), parents.tags,
                  {tag: parents.parent(tag) for tag in parents.tags})


def render():
    lines = ['/* Authored records; no downloaded source data. */',
             '#include <stddef.h>', '#include <stdint.h>']
    for name, binary in (('qjs_intl_native', pack(False)),
                         ('qjs_intl_currency_empty', pack(True)),
                         ('qjs_intl_currency_display', display_pack())):
        lines.append('const uint8_t %s_bytes[] = {' % name)
        for at in range(0, len(binary), 16):
            lines.append('    ' + ', '.join('0x%02x' % byte for byte in binary[at:at + 16]) + ',')
        lines += ['};', 'const size_t %s_length = sizeof(%s_bytes);' % (name, name), '']
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, render().encode('ascii'))
