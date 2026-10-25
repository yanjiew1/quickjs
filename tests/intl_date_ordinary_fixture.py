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

"""Required date subsets with real append/connector and flexible clock evidence."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_available_tables as available
import date_combination_tables as combinations
import date_combinations
import embed
from parents import ParentGraph
from resolve import BLOCK
import tables


def pack(blocked=False, padded=False, partial_connectors=False, month_hints=False):
    tags = ('en', 'root')
    graph = ParentGraph(tags, {}, {}, {'und': 'en-Latn-US'}, (), False)
    ordinary, append = ('gregorian', 'availableFormats', ''), ('gregorian', 'appendItems', '')
    patterns = (('M', 'L'), ('Md', 'M/d'), ('GyM', 'y M G'), ('GyMd', 'y M d G'),
                ('Hm', 'H:mm'), ('Hms', 'H:mm:ss'), ('Bh', 'B h'), ('Bhm', 'B h:mm'),
                ('Bhms', 'B h:mm:ss'), ('Eh', 'E h a'), ('ms', 'mm:ss'))
    if month_hints:
        # yMMMd sorts first, but yMd deliberately requests a numeric month
        # through the same textual pattern. Different raw text is not an alias.
        patterns += (('yMMMd', 'd MMM y'), ('yMd', 'd MMM y'),
                     ('yMMMMdd', 'dd MMMM y'), ('yMdd', 'MMMM dd, y'),
                     ('yMMMdHm', 'd MMM y HH:mm'), ('yMdHm', 'd MMM y HH:mm'),
                     ('yMMMdhm', 'd MMM y h:mm a'),
                     # Preserve intentional B5 -> B1 overrides and raw B3
                     # lengths; a b skeleton cannot supply B width evidence.
                     ('BBBBBh', 'B h'), ('bbhm', 'B h:mm'),
                     ('BBBhms', 'BBBB h:mm:ss'))
    if padded:
        # Canonical source order puts the named date before the padded numeric
        # date. Composition should choose the latter without narrowing it.
        patterns += (('yMMMd', 'MMM d y'), ('yMd', 'y-MM-dd'))
    source = dict(calendars=('gregorian',), groups=tuple(sorted((ordinary, append))),
                  maps=(tuple((key, '', '', value) for key, value in patterns),
                        (('Year', '', '', BLOCK if blocked else '{1} [year] {0}'),
                         ('Day-Of-Week', '', '', '{1}, {0}'),
                         ('Date-Timezone', '', '', '{1} [zone] {0}'))),
                  nodes=(('root', ordinary, 0, 0), ('root', append, 1, 0)),
                  rows=tuple((tag, key, head) for tag in tags for key, head in ((ordinary, 1), (append, 2))))
    def connector_row(tag, width, form):
        # Preserve blocked and explicit-empty width records; mixed construction
        # must not silently substitute the otherwise usable short connector.
        state = 'blocked' if partial_connectors and width == 'full' else 'value'
        text = '' if partial_connectors and width in ('full', 'medium') else "{1} 'at' {0}"
        return (tag, 'gregorian', width, form, '', state, text, '',
                'root', 'gregorian', width, form, '')

    connector = dict(calendars=('gregorian',), alternates=('',), widths=date_combinations.WIDTHS,
                     forms=date_combinations.FORMS,
                     rows=tuple(connector_row(tag, width, form)
                                for tag in tags for width in date_combinations.WIDTHS
                                for form in date_combinations.FORMS))
    pool = tuple(sorted(available.strings(source) | combinations.strings(connector) | set(tags),
                        key=lambda text: text.encode('utf-8')))
    ids = {text: i for i, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, graph, ids),
                          bundle.DATE_AVAILABLE_FORMATS: available.encode(source, tags, ids),
                          bundle.DATE_COMBINATIONS: combinations.encode(connector, tags, ids)}, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()).replace('qjs_intl_native', 'qjs_intl_date_ordinary_fixture')
    text += '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'qjs_intl_date_ordinary_blocked')
    text += '\n' + embed.render(pack(padded=True)).replace('qjs_intl_native', 'qjs_intl_date_ordinary_padded')
    text += '\n' + embed.render(pack(partial_connectors=True)).replace('qjs_intl_native',
                                                                       'qjs_intl_date_ordinary_partial')
    text += '\n' + embed.render(pack(month_hints=True)).replace('qjs_intl_native',
                                                                  'qjs_intl_date_ordinary_month_hints')
    atomic_write(args.output, text.encode('ascii'))
