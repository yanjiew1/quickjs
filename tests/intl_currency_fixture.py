#!/usr/bin/env python3
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
            'rows': (('en', 0), ('root', 0 if empty else 1))}
    pool = tuple(sorted(currency_tables.strings(data) | set(tags) | {'', BLOCK, INHERIT},
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: (2, tables.LOCALE.pack(ids['en'], 1) +
                                 tables.LOCALE.pack(ids['root'], tables.ROOT)),
                bundle.CURRENCIES: currency_tables.encode(data, tags, ids)}
    binary = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


def render():
    lines = ['/* Authored records; no downloaded source data. */',
             '#include <stddef.h>', '#include <stdint.h>']
    for name, empty in (('qjs_intl_native', False), ('qjs_intl_currency_empty', True)):
        binary = pack(empty)
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
