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

"""Emit nearest-key masking and repeated physical source-map fixtures."""
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
from resolve import BLOCK
import tables
from test_intl_date_available_formats import Sources, alias, calendar, calendar_formats, group


def pack(empty=False):
    source = Sources()
    tags = ('de', 'en', 'en-GB', 'en-US', 'root')
    inheritance = ParentGraph(tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    base = '''<dateFormatItem id="y">y</dateFormatItem>
      <dateFormatItem id="d">dd</dateFormatItem><dateFormatItem id="M">MMMM</dateFormatItem>
      <dateFormatItem id="j">h a</dateFormatItem><dateFormatItem id="Hm">HH:mm</dateFormatItem>
      <dateFormatItem id="y" count="other">yy</dateFormatItem>
      <dateFormatItem id="y" alt="variant">yyyy</dateFormatItem>'''
    child = '''<dateFormatItem id="y">Q</dateFormatItem>
      <dateFormatItem id="d">''' + BLOCK + '''</dateFormatItem><dateFormatItem id="M"/>
      <dateFormatItem id="j">'literal'</dateFormatItem>
      <dateFormatItem id="y" alt="variant">'child'</dateFormatItem>'''
    root_append = '' if empty else group('<appendItem request="Year">{0} ({2}: {1})</appendItem>', 'appendItems')
    source.set('root', calendar_formats(group('' if empty else base) + root_append))
    if not empty:
        source.set('en', calendar_formats(group(child) +
                   group('<appendItem request="Year">' + BLOCK + '</appendItem>', 'appendItems')))
        source.set('en_GB', calendar_formats(group('<dateFormatItem id="y" alt="variant">&apos;GB&apos;</dateFormatItem>')))
        source.set('en_US', calendar_formats(group(base)))
        source.set('en_1901_1996', calendar_formats(group('<dateFormatItem id="y">&apos;external&apos;</dateFormatItem>')))
        source.set('de', calendar('generic', '<dateTimeFormats>' +
                   alias("../../calendar[@type='gregorian']/dateTimeFormats", 'en_1901_1996') + '</dateTimeFormats>'))
    else:
        for tag in ('en', 'en_GB', 'en_US', 'de', 'en_1901_1996'):
            source.set(tag, calendar_formats(group('')))
    data = wire.select(date_available_formats.collect(source, inheritance), tags)
    if empty:
        data['groups'] = (('gregorian', 'availableFormats', ''),)
    pool = tuple(sorted(wire.strings(data) | set(tags), key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                        bundle.DATE_AVAILABLE_FORMATS: wire.encode(data, tags, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()) + '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'available_source_empty')
    atomic_write(args.output, text.encode('ascii'))
