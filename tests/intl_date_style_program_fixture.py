#!/usr/bin/env python3
"""Exact styles and independently indexed connector facts for composition."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_patterns
import date_style_tables as style_wire
import date_combinations
import date_combination_tables as connector_wire
import embed
from resolve import BLOCK
import tables
from test_intl_date_combinations import Sources, calendar
from test_intl_date_patterns import styles
from test_intl_list_patterns import graph


def connectors(entries):
    result = '<dateTimeFormats>'
    for width, standard, at_time in entries:
        result += '<dateTimeFormatLength type="' + width + '">'
        for form, text in (('standard', standard), ('atTime', at_time)):
            if text is not None:
                result += '<dateTimeFormat type="' + form + '"><pattern>' + text + '</pattern></dateTimeFormat>'
        result += '</dateTimeFormatLength>'
    return result + '</dateTimeFormats>'


def pack():
    source = Sources()
    source.set('root', calendar('gregorian', styles('date', (
        ('full', '<pattern>EEEE, d MMMM y</pattern><datetimeSkeleton>yMMMMEEEEd</datetimeSkeleton>'),
        ('long', '<pattern numbers="M=romanlow">d MMMM y</pattern>'),
        ('medium', '<pattern>y-MM-dd</pattern>'),
        ('short', '<pattern>y/M/d</pattern>'))) + styles('time', (
        ('full', '<pattern>h:mm:ss a zzzz</pattern><pattern alt="ascii">' + BLOCK + '</pattern>'),
        ('long', '<pattern>h:mm:ss a z</pattern><pattern alt="ascii"/>'),
        ('medium', '<pattern>h:mm a</pattern><pattern alt="ascii">h:mm &apos;ASCII&apos; a</pattern>'
                   '<datetimeSkeleton numbers="latn">hm</datetimeSkeleton>'),
        ('short', '<pattern>Bh:mm [z]</pattern><pattern alt="ascii">Q</pattern><datetimeSkeleton alt="ascii">Bh</datetimeSkeleton>'))) + connectors((
        ('full', '{1}, {0}', "{1} 'at' {0}"),
        ('long', '{0} / {1}', ''),
        ('medium', '{1} ~ {0}', BLOCK),
        ('short', "'{0}' {1} {0}", None)))) +
        calendar('generic', styles('date', (('short', '<pattern>dd/MM/Y G</pattern>'),
            ('long', '<pattern>y</pattern>'))) + styles('time', (
            ('full', '<pattern>y h:mm a</pattern>'), ('long', '<pattern>m</pattern>'),
            ('medium', '<pattern>h:mm a</pattern>'))) + connectors((
            ('long', "a {1} {0}", None), ('short', BLOCK, '')))) +
        calendar('japanese', styles('date', (('long', '<pattern>r(U)</pattern>'),)) +
            styles('time', (('medium', '<pattern>h:mm a</pattern>'),)) +
            connectors((('long', 'a {1} {0}', None),))))
    source.set('en', calendar('gregorian', connectors((('full', '{1}, {0}', BLOCK),))))
    source.set('en_GB', calendar('generic', styles('date', (
        ('full', '<pattern>' + BLOCK + '</pattern>'), ('medium', '<pattern/>')))))
    tags = graph().tags
    style = style_wire.select(date_patterns.collect(source, graph()), tags)
    combination = connector_wire.select(date_combinations.collect(source, graph()), tags)
    # Keep the valid catalogs independent: omit the generic connector catalog
    # while styles and their provenance retain all three source calendars.
    combination['calendars'] = tuple(c for c in combination['calendars'] if c != 'generic')
    combination['rows'] = tuple(row for row in combination['rows'] if row[1] != 'generic')
    pool = tuple(sorted(style_wire.strings(style) | connector_wire.strings(combination) | set(tags),
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                          bundle.DATE_STYLES: style_wire.encode(style, tags, ids),
                          bundle.DATE_COMBINATIONS: connector_wire.encode(combination, tags, ids)},
                         bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack()).encode('ascii'))
