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

"""Independent raw calendar inventories over shared metadata identities."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import calendar_name_tables as names_wire
import date_available_tables as available_wire
import date_combination_tables as combination_wire
import date_combinations
import date_interval_tables as interval_wire
import date_patterns
import date_style_tables as style_wire
import embed
import locale_tables
from parents import ParentGraph
import tables


def pack(ambiguous=False):
    tags = ('root',)
    graph = ParentGraph(tags, {}, {}, {'und': 'en-Latn-US'}, (), False)
    metadata = dict(aliases=(), likely=(('und', 'en-Latn-US'),),
                    keys=(('u', 'ca', 'single', (), '', False),
                          ('u', 'zz', 'single', (), '', False)),
                    types=tuple(sorted((
                        ('u', 'ca', 'buddhist', (), '', False, False, '', ''),
                        ('u', 'ca', 'ethioaa', ('ethiopic-amete-alem',), '', False, False, '', ''),
                        ('u', 'ca', 'gregory', ('gregorian', 'shared', 'clash'), '', False, False, '', ''),
                        ('u', 'ca', 'hebrew', ('clash',), '', False, False, '', ''),
                        ('u', 'ca', 'islamic-civil', (), '', False, False, '', ''),
                        ('u', 'ca', 'islamicc', ('islamic-civil',), 'islamic-civil', True, False, '', ''),
                        ('u', 'ca', 'iso8601', (), '', False, False, '', ''),
                        ('u', 'ca', 'oldgreg', ('shared',), 'gregory', True, False, '', ''),
                        ('u', 'ca', 'oldname', ('earliest',), 'oldgreg', True, False, '', ''),
                        ('u', 'ca', 'retired', (), '', True, False, '', ''),
                        ('u', 'zz', 'CODEPOINTS', (), '', False, True, '', ''),
                    ))))
    available_calendars = ('generic', 'gregorian', 'iso8601') + (('oldgreg',) if ambiguous else ())
    available = dict(calendars=available_calendars,
                     groups=tuple((cal, 'availableFormats', '') for cal in available_calendars),
                     maps=(), nodes=(), rows=())
    style_calendars = ('buddhist', 'ethiopic-amete-alem', 'gregorian', 'iso8601')
    styles = dict(calendars=style_calendars, alternates=('',), fields=date_patterns.FIELDS,
                  widths=date_patterns.WIDTHS,
                  rows=tuple(('root',) + selector + ('missing',) + ('',) * 8
                             for selector in style_wire.selectors(style_calendars, ('',))))
    combinations = dict(calendars=('oldgreg',), alternates=('',), forms=date_combinations.FORMS,
                        widths=date_combinations.WIDTHS,
                        rows=tuple(('root',) + selector + ('missing',) + ('',) * 7
                                   for selector in combination_wire.selectors(('oldgreg',), ('',))))
    name_calendars = ('ethiopic-amete-alem', 'generic', 'gregorian', 'hebrew', 'iso8601')
    names = dict(calendars=name_calendars,
                 groups=tuple((cal, 'months', '', 'format', 'wide') for cal in name_calendars),
                 maps=(), nodes=(), rows=())
    intervals = dict(calendars=('generic', 'hebrew', 'iso8601'),
                     suffixes=(('', 'fallback', '', ''),), maps=(), nodes=(), rows=())
    pairs = ((bundle.DATE_AVAILABLE_FORMATS, available_wire, available),
             (bundle.DATE_STYLES, style_wire, styles),
             (bundle.DATE_COMBINATIONS, combination_wire, combinations),
             (bundle.CALENDAR_NAMES, names_wire, names),
             (bundle.DATE_INTERVALS, interval_wire, intervals))
    values = locale_tables.strings(metadata) | set(tags)
    for identity, wire, data in pairs:
        values.update(wire.strings(data))
    pool = tuple(sorted(values, key=lambda value: value.encode('utf-8')))
    ids = {value: index for index, value in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: tables.encode_locales(tags, graph, ids)}
    sections.update(locale_tables.encode(metadata, ids))
    for identity, wire, data in pairs:
        sections[identity] = wire.encode(data, tags, ids)
    return bundle.pack(sections, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()) + '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'calendar_ambiguous')
    atomic_write(args.output, text.encode('ascii'))
