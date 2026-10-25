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

"""Emit explicit day-period origins, shared/empty lists and a selection-only list."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import dayperiod_rules as source
import dayperiod_tables as wire
import embed
import tables
from test_intl_dayperiod_rules import Sources


def pack(empty=False, mixed=False):
    rows = list(source.collect(Sources())['rows'])
    by_key = {(kind, tag): rules for kind, tag, rules in rows}
    points = (('midnight', 'at', 0, 0),)
    midnight_range = (('morning1', 'range', 0, 43200), ('night1', 'range', 43200, 0))
    long_range = (('morning1', 'range', 22500, 65700), ('night1', 'range', 65700, 22500))
    rows.extend((('format', 'de', by_key['format', 'root']),
                 ('format', 'en-1901-1996', by_key['format', 'en']),
                 ('format', 'fr', ()), ('format', 'gsw', points),
                 ('format', 'xx', midnight_range), ('selection', 'fr', ()),
                 ('selection', 'gsw', points), ('selection', 'zz', long_range)))
    if mixed:
        rows.append(('format', 'yy', by_key['format', 'en'] + by_key['format', 'root']))
    data = {'rows': () if empty else tuple(sorted(rows))}
    pool = tuple(sorted(wire.strings(data) | {'root', 'bad_tag', 'en-us', 'en-1901-1901'}))
    ids = {text: index for index, text in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: (1, struct.pack('<II', ids['root'], 0xffffffff)),
                        bundle.DAYPERIOD_RULES: wire.encode(data, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()) + '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'dayperiod_empty')
    text += '\n' + embed.render(pack(mixed=True)).replace('qjs_intl_native', 'dayperiod_mixed')
    atomic_write(args.output, text.encode('ascii'))
