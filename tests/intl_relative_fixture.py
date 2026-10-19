#!/usr/bin/env python3
"""Emit authored relative forms, external alias origins and literal boundaries."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import relative_patterns
import relative_tables
import tables
from test_intl_list_patterns import graph
from test_intl_relative_patterns import Sources, field, pattern


def pack():
    source = Sources()
    source.set('en', field('day', '<relative type="0">today {literal}</relative>' +
               '<relative type="1">' + BLOCK + '</relative><relative type="2"/>' +
               pattern('past', 'one', 'yesterday')))
    source.set('en_1901_1996', field('day', '<relative type="42">far day</relative>' +
               '<relative type="9007199254740992">last positive key</relative>' +
               '<relative type="-9007199254740992">last negative key</relative>' +
               pattern('future', 'one', 'VARIANT {0}')))
    source.set('de', field('day-narrow', '<alias source="en_1901_1996" path="../field[@type=\'day\']"/>'))
    inheritance = graph()
    inheritance = ParentGraph(inheritance.tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    # The alias terminal is deliberately outside this selected profile.
    tags = ('de', 'root')
    data = relative_tables.select(relative_patterns.collect(source, inheritance), tags)
    pool = tuple(sorted(relative_tables.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'en-1901-1901', 'En', 'en-us', 'bad_tag', '-0',
                         '9007199254740993', '-9007199254740993', 'invalid {1}'},
                        key=lambda text: text.encode('utf-8')))
    ids = {text: i for i, text in enumerate(pool)}
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                bundle.RELATIVE_PATTERNS: relative_tables.encode(data, tags, ids)}
    result = bundle.pack(sections, bytes(32), 'minimal')
    tables.validate(result)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack()).encode('ascii'))
