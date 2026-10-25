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

"""Emit authored available-format layers, external origins and an empty pack."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_available_formats
import date_available_tables as wire
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_available_formats import Sources, alias, calendar, calendar_formats, group
from test_intl_list_patterns import graph


def pack(empty=False):
    source = Sources()
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'en', 'en-GB', 'root')
    if empty:
        source.set('root', calendar_formats(group('')))
    else:
        source.set('root', calendar_formats(group('''<dateFormatItem id="y">y</dateFormatItem>
            <dateFormatItem id="y" count="zero">y 'zero'</dateFormatItem>
            <dateFormatItem id="y" count="two">y 'two'</dateFormatItem>
            <dateFormatItem id="y" count="few">y 'few'</dateFormatItem>
            <dateFormatItem id="y" count="many">y 'many'</dateFormatItem>
            <dateFormatItem id="y" count="other">yy</dateFormatItem>
            <dateFormatItem id="y" count="one" alt="variant">y年😀</dateFormatItem>
            <dateFormatItem id="j">h a</dateFormatItem><dateFormatItem id="M"/>
            <dateFormatItem id="d">''' + BLOCK + '''</dateFormatItem>''') +
            group('<dateFormatItem id="y" alt="ascii">yyyy</dateFormatItem>', alt='variant') +
            group('<dateFormatItem id="M">MMM</dateFormatItem>', alt='narrow variant') +
            group('''<appendItem request="Year">{0} ({2}: {1})</appendItem>
            <appendItem request="Time-Day-Of-Week">{1} ob {0}</appendItem>''', 'appendItems')) +
            calendar('generic', '<dateTimeFormats>' +
                alias("../../calendar[@type='gregorian']/dateTimeFormats") + '</dateTimeFormats>') +
            calendar_formats(group('<dateFormatItem id="' + 'y' * 4096 + '">y</dateFormatItem>') +
                group('<dateFormatItem id="' + 'y' * 4096 + '">yy</dateFormatItem>', alt='variant'),
                code='chinese'))
        source.set('en', calendar_formats(group('<dateFormatItem id="y" alt="variant">&apos;child&apos;</dateFormatItem>')))
        source.set('en_GB', calendar_formats(group('<dateFormatItem id="d" alt="variant">&apos;own&apos;</dateFormatItem>'),
                                            code='generic'))
        source.set('en_1901_1996', calendar_formats(group('<dateFormatItem id="y">&apos;external&apos;</dateFormatItem>')))
        source.set('de', calendar('generic', '<dateTimeFormats>' +
            alias("../../calendar[@type='gregorian']/dateTimeFormats", 'en_1901_1996') + '</dateTimeFormats>'))
    data = wire.select(date_available_formats.collect(source, inheritance), tags)
    if empty:
        data['groups'] = (('gregorian', 'availableFormats', ''),)
    pool = tuple(sorted(wire.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'Generic', 'En', 'en-us', 'bad_tag', 'en-1901-1901', 'bad!', 'Unknown'},
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                        bundle.DATE_AVAILABLE_FORMATS: wire.encode(data, tags, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()) + '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'date_available_empty')
    atomic_write(args.output, text.encode('ascii'))
