#!/usr/bin/env python3
"""Authored unsupported week-year styles and real calendar-year alternatives."""
import argparse
from pathlib import Path
import sys
from xml.sax.saxutils import escape

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


GALICIAN = "EEEE, d 'de' MMMM 'de' Y G"
ALTERNATIVE = "E, d 'de' MMM 'de' y G"


def pack():
    cases = {
        'exact': ('y-MM-dd', {'yMd': 'y/M/d'}),
        'galician': (GALICIAN, {'GyMMMEd': ALTERNATIVE}),
        'kekchi': ('dd/MM/Y G', {'GyMd': 'dd-MM-y G'}),
        'priority': (GALICIAN, {'GyMMMEd': ALTERNATIVE,
                              'GyMMMMEEEEd': 'EEEE [d] MMMM y G'}),
        'override': (GALICIAN, {'GyMMMMEEEEd': ALTERNATIVE}),
        'context': (GALICIAN, {'GyLLLEd': 'E, d LLL y G'}),
        'numeric': (GALICIAN, {'GyMEd': 'E, d M y G'}),
        'quarter': ('Y Q d', {'yMd': 'y/M/d'}),
        'twodigit': ('YY/M/d', {'yMd': 'y/M/d'}),
        'duplicate': ('Y/Y/M/d', {'yMd': 'y/M/d'}),
        'mixed': ('Y/y/M/d', {'yMd': 'y/M/d'}),
        'related': ('Y/r/M/d', {'yMd': 'y/M/d'}),
        'time': ('Y/M/d H', {'yMd': 'y/M/d'}),
        'blocked': (GALICIAN, {'GyMMMEd': ALTERNATIVE}),
        'empty': (GALICIAN, {'GyMMMEd': ALTERNATIVE}),
        'missing': ('Y/M/d', {'yMd': 'y/M/d'}),
    }
    source, roots = Sources(), []
    for code, (pattern, choices) in sorted(cases.items()):
        widths = ('short',) if code == 'missing' else ('full', 'long', 'medium', 'short')
        skeleton = '<datetimeSkeleton>GYMMMMEEEEd</datetimeSkeleton>' if code == 'galician' else ''
        date = styles('date', tuple((width, '<pattern>' + escape(pattern) + '</pattern>' + skeleton)
                                   for width in widths))
        available = ''.join('<dateFormatItem id="' + key + '">' + escape(value) + '</dateFormatItem>'
                            for key, value in sorted(choices.items()))
        roots.append(calendar(code, date + '<dateTimeFormats>' + group(available) + '</dateTimeFormats>'))
    source.set('root', ''.join(roots))
    source.set('en', ''.join(calendar(code, styles('date', (('full', '<pattern>' + value + '</pattern>'),)))
                            for code, value in (('blocked', BLOCK), ('empty', ''))) +
               calendar('kekchi', styles('date', (('full', '<datetimeSkeleton numbers="latn">GGGGGyMMdd</datetimeSkeleton>'),))))
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
