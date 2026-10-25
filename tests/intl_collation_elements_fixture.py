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

"""Authored root maps exercise matching independently of locale compilation."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from intl_collation_fixture import emit
import bundle
import collation_contexts
import collation_root
import collation_tables


def fixture():
    def ce(primary, secondary=0x20):
        return False, primary, secondary, 2

    rows = {}
    for text, primary in (('a', 0x1000), ('x', 0x1200), ('b', 0x1300), ('c', 0x1400),
                          ('l', 0x1500), ('o', 0x1600), ('·', 0x1700), (':', 0x1800),
                          ('z', 0x1900), ('v', 0x4800), ('abc', 0x2000),
                          ('bl', 0x3500), ('l·z', 0x3600), ('a\u0323\u0301', 0x3800),
                          ('a\u030a', 0x3000), ('a\u030a\u0301', 0x3100),
                          ('\u0323\u0304', 0x3200), ('ol', 0x3300), ('xl', 0x3400),
                          ('\U0001f600', 0x4500), ('\u1100', 0x4600), ('\u1161', 0x4700),
                          ('v' * 4096, 0x4900), ('\ufffe', 0x200), ('\uffff', 0xffff)):
        rows[tuple(map(ord, text))] = (ce(primary),)
    for text, secondary in (('\u0301', 0x31), ('\u0304', 0x32),
                            ('\u030a', 0x34), ('\u0323', 0x35), ('\u0331', 0x37)):
        rows[tuple(map(ord, text))] = (ce(0, secondary),)
    for digit in range(10):
        rows[(ord('0') + digit,)] = (ce(0x900 + digit),)
    rows[(0x2460,)] = ((False, 0x901, 0x20, 6),)  # Circled one is not Nd.
    rows[(0x24ea,)] = ((False, 0x900, 0x20, 6),)  # Circled zero.
    rows[(0x20c4,)] = (ce(0x8ff),)  # Immediately before the ordinary digit band.
    rows[(0,)] = ((False, 0, 0, 0),)
    rows[(ord('A'),)] = ((False, 0x1000, 0x20, 8),)
    rows[(ord('H'),)] = ((False, 0x1900, 0x20, 8),)
    rows[(ord('h'),)] = ((False, 0x1900, 0x20, 13),)
    rows[(ord('w'),)] = ((False, 0x1000, 0x20, 3),)
    rows[(ord('m'),)] = ((False, 0x1000, 0x20, 8), (False, 0x1000, 0x20, 2))
    rows[(ord('n'),)] = ((False, 0x1000, 0x20, 2), (False, 0x1000, 0x20, 8))
    rows[(ord('p'),)] = ((False, 0x1000, 0x20, 3), (False, 0x1000, 0x20, 2))
    rows[(0x30b,)] = ((False, 0, 0x31, 8),)  # Synthetic upper secondary CE.
    rows[(0x345,)] = ((False, 0, 0, 0x19),)
    rows[(0x346,)] = ((False, 0, 0, 0x1d),)  # Artificial tertiary-only CE.
    rows[(ord('u'),)] = (ce(0x1000, 0x21),)
    rows[(ord('-'),)] = ((True, 0x501, 0x20, 2),)
    rows[(ord('g'),)] = ((True, 0x500, 0x20, 2), (False, 0x1000, 0x20, 2),
                         (True, 0x501, 0x20, 2))
    rows[(0x20,)] = ((True, 0x500, 0x20, 2),)
    rows[(0x34f,)] = ((False, 0, 0, 0),)
    # allkeys combines the prefix CEs with the contextual suffix CEs. The context
    # metadata splits them again; a matcher must not consume/emit the prefix twice.
    prefixes = (('l', '·', 0x41), ('ol', '·z', 0x43), ('l', ':z', 0x44),
                ('bl', '·', 0x42), ('l', '·x', 0x46),
                ('\u030a', '·', 0x45))
    fractional = ['[UCA version = 18.0.0]']
    for before, suffix, secondary in prefixes:
        a, b = tuple(map(ord, before)), tuple(map(ord, suffix))
        rows[a + b] = rows[a] + (ce(0, secondary),)
        fractional.append('{} | {}; [, FB B6, 05]'.format(
            ' '.join('{:04X}'.format(c) for c in a), ' '.join('{:04X}'.format(c) for c in b)))
    root = {'version': collation_root.VERSION, 'rows': tuple(sorted(rows.items()))}
    contexts = collation_contexts.parse('\n'.join(fractional).encode(), root)
    return bundle.pack({bundle.COLLATION_ROOT: collation_tables.encode(root),
                        bundle.COLLATION_CONTEXTS: collation_contexts.encode(contexts, root)},
                       bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    emit(fixture(), args.output)
