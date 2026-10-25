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

"""Emit owned source-candidate, masking and late syntax-error fixtures."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_available_tables as wire
import embed
from parents import ParentGraph
from resolve import BLOCK
import tables


def pack(bad=None):
    tags = ('en', 'en-GB', 'root')
    graph = ParentGraph(tags, {}, {}, {'und': 'en-Latn-US'}, (), False)
    ordinary = ('gregorian', 'availableFormats', '')
    alias = ('generic', 'availableFormats', '')
    append = ('gregorian', 'appendItems', '')
    root = [('y', '', '', 'yyyy'), ('yy', '', '', 'yy'), ('yM', '', '', 'yyyy'),
            ('M', '', '', 'LLLL'), ('d', '', '', 'dd'), ('h', '', '', 'h a'),
            ('K', '', '', 'K a'), ('Bhm', '', '', 'h BBBB:mm'), ('B', '', '', 'BBBB'),
            ('rU', '', '', 'r(U)'), ('SSS', '', '', 's.SSS'), ('E', '', '', 'EEEEEE'),
            ('Q', '', '', 'Q'), ('j', '', '', "'literal'"), ('a', '', '', 'a'),
            ('y', 'other', '', "'"), ('y', '', 'ascii', "'"),
            ('y', '', 'variant', 'yyyy'), ('M', '', 'variant', BLOCK),
            ('d', '', 'variant', ''), ('y', '', 'unsupported', 'Q'),
            ('a', '', 'unsupported', 'a'), ('j', '', 'unsupported', "'only'")]
    if bad is not None:
        root.append(('zzzz', '', '', bad))
    child = [('y', '', '', 'Q'), ('d', '', '', BLOCK), ('M', '', '', ''),
             ('K', '', '', "'literal'"), ('h', '', '', 'a')]
    data = dict(calendars=('generic', 'gregorian'), groups=tuple(sorted((ordinary, alias, append))),
                maps=(tuple(root), tuple(child), (('Year', '', '', '{0} {1}'),)),
                nodes=(('root', ordinary, 0, 0), ('en', ordinary, 1, 1),
                       ('root', append, 2, 0)),
                rows=(('root', ordinary, 1), ('en', ordinary, 2),
                      ('en-GB', alias, 1), ('en', append, 3)))
    pool = tuple(sorted(wire.strings(data) | set(tags), key=lambda value: value.encode('utf-8')))
    ids = {value: index for index, value in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: tables.encode_locales(tags, graph, ids),
                        bundle.DATE_AVAILABLE_FORMATS: wire.encode(data, tags, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack())
    for name, source in (('candidate_bad_quote', "'"), ('candidate_bad_field', 'P'),
                         ('candidate_calendar_year', 'Y/M/d'), ('candidate_week_year', 'Y-w-M-d')):
        text += '\n' + embed.render(pack(source)).replace('qjs_intl_native', name)
    atomic_write(args.output, text.encode('ascii'))
