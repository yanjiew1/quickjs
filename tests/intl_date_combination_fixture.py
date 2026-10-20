#!/usr/bin/env python3
"""Emit authored connector facts with all states and an external source locale."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_combinations
import date_combination_tables
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_combinations import Sources, group_alias, calendar, connector
from test_intl_list_patterns import graph


def pack():
    source = Sources()
    source.set('root', calendar('gregorian', """<dateTimeFormats>
        <dateTimeFormatLength type="long"><dateTimeFormat>
        <pattern numbers="y=hebr">{1}, {0}</pattern></dateTimeFormat>
        <dateTimeFormat type="atTime"><pattern>""" + BLOCK + """</pattern></dateTimeFormat>
        </dateTimeFormatLength></dateTimeFormats>""") +
        calendar('generic', group_alias('gregorian')) +
        calendar('japanese', '<alias source="locale" path="../calendar[@type=\'generic\']"/>'))
    source.set('en', calendar('gregorian', connector("""<pattern/>
               <pattern alt="variant" numbers="latn">{1} 'at' {0}</pattern>""")))
    source.set('en_GB', calendar('gregorian', connector('<pattern>{0} – {1}</pattern>')))
    source.set('en_1901_1996', calendar('gregorian', connector(
        """<pattern numbers="latn">{1} 'external' 😀 {0}</pattern>""")))
    source.set('de', calendar('gregorian', group_alias('gregorian', 'en_1901_1996')))
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'en', 'en-GB', 'root')
    data = date_combination_tables.select(date_combinations.collect(source, inheritance), tags)
    pool = tuple(sorted(date_combination_tables.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'Generic', 'japanese-', 'En', 'en-us',
                         'bad_tag', 'en-1901-1901', 'bad!'}, key=lambda s: s.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                          bundle.DATE_COMBINATIONS: date_combination_tables.encode(data, tags, ids)},
                         bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack()).encode('ascii'))
