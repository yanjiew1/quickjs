#!/usr/bin/env python3
"""Emit authored explicit era maps, including a hypothetical noncontiguous era."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import calendar_era_tables as wire
import embed
import tables
from test_intl_calendar_eras import fixture


def pack(empty=False, missing=False):
    data, pool, ids, section = fixture({'rows': ()} if empty else None)
    if not empty:
        calendar, raw = 'long-' + 'a' * 2048, '9' * 2048
        rows = data['rows'] + ((calendar, 'am', '0'), (calendar, 'be', '1'),
                               ('other', 'am', raw), ('third', 'am', raw))
        data, pool, ids, section = fixture({'rows': rows})
    sections = {bundle.STRINGS: tables.encode_strings(pool),
                bundle.LOCALES: (1, struct.pack('<II', ids['root'], 0xffffffff))}
    if not missing:
        sections[bundle.CALENDAR_ERAS] = section
    return bundle.pack(sections, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack())
    text += '\n' + embed.render(pack(empty=True)).replace('qjs_intl_native', 'era_empty')
    text += '\n' + embed.render(pack(missing=True)).replace('qjs_intl_native', 'era_missing')
    atomic_write(args.output, text.encode('ascii'))
