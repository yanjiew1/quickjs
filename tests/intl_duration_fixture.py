#!/usr/bin/env python3
"""Emit raw duration states and external, multi-variant alias provenance."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import duration_patterns
import duration_tables
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
from test_intl_duration_patterns import Sources, alias, pattern
from test_intl_list_patterns import graph


def pack():
    source = Sources()
    source.set('root', pattern('hm', 'h:mm') + pattern('hms', 'h.mm:ss'))
    source.set('en', pattern('hm', BLOCK))
    source.set('en_1901_1996', pattern('hms', "'ga' hh:mm:ss"))
    source.set('de', alias('hm', 'hm', 'en') +
               alias('hms', 'hms', 'en_1901_1996') + pattern('ms', ''))
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'root')
    data = duration_tables.select(duration_patterns.collect(source, inheritance), tags)
    pool = tuple(sorted(duration_tables.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'En', 'en-us', 'bad_tag', 'en-1901-1901'},
                        key=lambda value: value.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                          bundle.DURATION_PATTERNS: duration_tables.encode(data, tags, ids)},
                         bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack()).encode('ascii'))
