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

"""Time styles with independently authored visible Timezone append templates."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_available_formats
import date_available_tables as available_wire
import date_patterns
import date_style_tables as style_wire
import embed
from resolve import BLOCK
import tables
from test_intl_date_available_formats import Sources, calendar, group
from test_intl_date_patterns import styles
from test_intl_list_patterns import graph


def pack():
    source = Sources()
    templates = {'plain': '{0} {1}', 'paren': '{0} ({1})', 'reverse': '{1} {0}',
                 'comma': '{0}, {1}', 'cyrillic': '{0} ч. {1}', 'letters': "'at' {0} + {1}",
                 'blocked': '{0} {1}', 'empty': '{0} {1}', 'missing': None,
                 'malformed': '{0} {0} {1}', 'recovery': '{0} {1}', 'unmatched': '{0} {1}'}
    time = styles('time', (
        ('full', '<pattern>h:mm:ss a zzzz</pattern>'),
        ('long', '<pattern>HH:mm:ss z</pattern>'),
        ('medium', '<pattern>H:m:s</pattern>'),
        ('short', '<pattern>Bh:mm [z]</pattern>')))
    roots = []
    for code, text in sorted(templates.items()):
        leaf = '' if text is None else '<appendItem request="Timezone">' + text + '</appendItem>'
        own_time = time
        if code == 'recovery':
            own_time = styles('time', (
                ('full', '<pattern>HH:mm:ss zzzz</pattern>'),
                ('long', '<pattern>HH:mm:ss z</pattern>'),
                ('medium', '<pattern>HH:mm:ss</pattern>'),
                ('short', '<pattern>HH:mm</pattern>')))
        elif code == 'unmatched':
            own_time = time.replace('HH:mm:ss z', 'HH:mm:ss [z]')
        roots.append(calendar(code, own_time + '<dateTimeFormats>'  + group(leaf, 'appendItems') + '</dateTimeFormats>'))
    source.set('root', ''.join(roots))
    source.set('en', ''.join(calendar(code, '<dateTimeFormats>' + group(
        '<appendItem request="Timezone">' + text + '</appendItem>', 'appendItems') + '</dateTimeFormats>')
        for code, text in (('blocked', BLOCK), ('empty', ''))))
    tags = graph().tags
    available = available_wire.select(date_available_formats.collect(source, graph()), tags)
    style = style_wire.select(date_patterns.collect(source, graph()), tags)
    pool = tuple(sorted(available_wire.strings(available) | style_wire.strings(style) | set(tags),
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                          bundle.DATE_STYLES: style_wire.encode(style, tags, ids),
                          bundle.DATE_AVAILABLE_FORMATS: available_wire.encode(available, tags, ids)},
                         bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack()).encode('ascii'))
