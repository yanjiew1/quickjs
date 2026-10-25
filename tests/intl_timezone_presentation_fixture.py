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

"""Emit authored global presentation records, including an empty section."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import embed
import tables
import timezone_presentation_tables as wire


def pack(empty=False):
    data = {
        'zones': ('Area/Alpha', 'Area/Beta', 'Area/Empty', 'Area/Extremes'),
        'ids': (('east', 'East'), ('unuu', 'Unused'), ('west', 'West')),
        'periods': (('Area/Alpha', None, 0, 'West', 0, None),
                    ('Area/Alpha', 0, 10, 'West', None, 0),
                    ('Area/Alpha', 20, None, 'East', -86340, 86340),
                    ('Area/Beta', None, None, 'West', None, None),
                    ('Area/Extremes', -(1 << 63), -1, 'East', None, 0),
                    ('Area/Extremes', 1, (1 << 63) - 1, 'West', 0, None)),
        'preferred': (('East', '001', 'Area/Alpha'), ('West', '001', 'Area/Beta'),
                      ('West', 'GB', 'Area/Alpha')),
        'primary': (('GB', 'Area/NoPeriods'), ('US', 'Area/NoPeriods')),
    }
    if empty:
        data = {key: () for key in data}
    pool = tuple(sorted(wire.strings(data) | {'root', 'Bad ID', 'BAD!', 'unknown'}))
    ids = {text: i for i, text in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: (1, struct.pack('<II', ids['root'], 0xffffffff)),
                        bundle.TIMEZONE_PRESENTATION: wire.encode(data, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()) + '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'timezone_presentation_empty')
    atomic_write(args.output, text.encode('ascii'))
