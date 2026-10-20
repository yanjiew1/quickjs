#!/usr/bin/env python3
"""Emit localized zone names with selector aliases and an external source."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import embed
from resolve import BLOCK, INHERIT
import tables
import timezone_name_tables as wire
from test_intl_timezone_name_tables import fixture
from test_intl_timezone_names import graph


def pack(empty=False):
    data, tags, _, _ = fixture()
    tags = ('de', 'en', 'en-GB', 'root')
    if empty:
        data = {'groups': (('city', 'Europe/London', '', '', ''),), 'maps': (), 'nodes': (), 'rows': ()}
    else:
        data = wire.select(data, tags)
        # A source outside the requester set exercises validation scratch for
        # unique multi-variant source identities as well as external provenance.
        data['nodes'] = tuple(('en-1901-1996' if origin == 'en' else origin, group, fields, parent)
                              for origin, group, fields, parent in data['nodes'])
    pool = tuple(sorted(wire.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'Bad ID', 'bad_tag', 'en-1901-1901', 'Unknown'},
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                        bundle.TIMEZONE_NAMES: wire.encode(data, tags, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()) + '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'timezone_name_empty')
    atomic_write(args.output, text.encode('ascii'))
