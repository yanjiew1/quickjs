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

"""Emit authored date labels with external source identities and all states."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_field_names
import date_field_tables
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_field_names import Sources, alias, field
from test_intl_list_patterns import graph


def pack():
    source = Sources()
    source.set('root', field('year', 'root year') + alias('year-short', 'year') +
               alias('year-narrow', 'year-short'))
    source.set('en', field('era', '') + field('month', BLOCK) +
               '<field type="dayperiod"><displayName>AM/PM</displayName>' +
               '<displayName alt="variant">am/pm</displayName></field>')
    source.set('en_GB', field('year', 'British year'))
    source.set('en_1901_1996', field('zone', 'external time zone'))
    source.set('de', alias('zone-short', 'zone', 'en_1901_1996'))
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'en', 'en-GB', 'root')
    data = date_field_tables.select(date_field_names.collect(source, inheritance), tags)
    pool = tuple(sorted(date_field_tables.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'En', 'en-us', 'bad_tag', 'en-1901-1901', 'bad!'},
                        key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                          bundle.DATE_FIELD_NAMES: date_field_tables.encode(data, tags, ids)},
                         bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack()).encode('ascii'))
