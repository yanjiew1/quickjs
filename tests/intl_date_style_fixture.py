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

"""Emit authored style facts with all states and an external source locale."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_patterns
import date_style_tables
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_patterns import Sources, alias, calendar, style
from test_intl_list_patterns import graph


def pack():
    source = Sources()
    source.set('root', calendar('gregorian', style('''<pattern numbers="y=hebr">y-MM-dd</pattern>
               <datetimeSkeleton numbers="latn">yMMdd</datetimeSkeleton>''') +
               style('<pattern>' + BLOCK + '</pattern>', family='time')) +
               calendar('generic', alias('date', 'gregorian') + alias('time', 'gregorian')) +
               calendar('japanese', '<alias source="locale" path="../calendar[@type=\'generic\']"/>'))
    source.set('en', calendar('gregorian', style('''<pattern/>
               <pattern alt="variant" numbers="latn">d MMMM y</pattern>''')))
    source.set('en_GB', calendar('gregorian', style('<pattern>dd/MM/y</pattern>')))
    source.set('en_1901_1996', calendar('gregorian', style("<pattern numbers=\"latn\">y 'external' 😀</pattern>")))
    source.set('de', calendar('gregorian', alias('date', 'gregorian', 'en_1901_1996')))
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'en', 'en-GB', 'root')
    data = date_style_tables.select(date_patterns.collect(source, inheritance), tags)
    pool = tuple(sorted(date_style_tables.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'Generic', 'japanese-', 'En', 'en-us',
                         'bad_tag', 'en-1901-1901', 'bad!'}, key=lambda s: s.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                          bundle.DATE_STYLES: date_style_tables.encode(data, tags, ids)},
                         bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack()).encode('ascii'))
