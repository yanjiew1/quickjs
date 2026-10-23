#!/usr/bin/env python3
"""Authored zone presentation with real rule IDs and deliberately sparse labels."""
import argparse
from pathlib import Path
import sys
import struct

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import embed
from resolve import BLOCK
import tables
import timezone_names
import timezone_name_tables as names_wire
import timezone_presentation_tables as periods_wire
from test_intl_timezone_names import Sources, entity, width, leaf, graph


def pack(variant=None):
    source = Sources()
    root = (leaf('hourFormat', '+HH:mm;-HH:mm') + leaf('gmtFormat', 'GMT{0}') +
        leaf('regionFormat', "'{0}'") + leaf('fallbackFormat', '{0} — {1}') +
        entity(width(leaf('generic', 'Direct zone')), 'Asia/Tokyo') +
        entity(width(leaf('generic', 'Pacific')), 'Pacific', 'metazone') +
        entity(width(leaf('standard', 'Mountain standard') + leaf('daylight', 'Mountain summer')),
               'Mountain', 'metazone') +
        entity(width(leaf('standard', 'Eastern standard') + leaf('daylight', 'Eastern summer')),
               'Eastern', 'metazone') +
        entity(width(leaf('standard', 'Winter label') + leaf('daylight', 'Summer label')),
               'Europe/Dublin') +
        entity(leaf('exemplarCity', 'New_York_literal'), 'America/New_York') +
        entity(leaf('exemplarCity', 'Root Phoenix'), 'America/Phoenix') +
        entity(leaf('exemplarCity', 'Unknown place'), 'Etc/Unknown'))
    english = (entity(leaf('exemplarCity', BLOCK), 'America/Phoenix') +
               entity(leaf('exemplarCity', ''), 'America/New_York'))
    if variant == 'gmt':
        root += (entity(width(leaf('standard', 'Zone standard') + leaf('generic', 'Zone generic')), 'Etc/GMT') +
                 entity(width(leaf('standard', 'Meta standard') + leaf('generic', 'Meta generic')), 'GMT', 'metazone'))
        english += entity(width(leaf('standard', BLOCK) + leaf('generic', '')), 'Etc/GMT')
    source.set('root', root)
    source.set('en', english)
    tags = graph().tags
    names = names_wire.select(timezone_names.collect(source, graph()), tags)
    periods = {
        'zones': ('America/Los_Angeles', 'America/New_York', 'America/Phoenix', 'America/Vancouver',
                  'Europe/Dublin'),
        'ids': (('east', 'Eastern'), ('moun', 'Mountain'), ('paci', 'Pacific')),
        'periods': (
            ('America/Los_Angeles', None, None, 'Pacific', None, None),
            ('America/New_York', None, None, 'Eastern', None, None),
            ('America/Phoenix', None, None, 'Mountain', None, None),
            ('America/Vancouver', None, None, 'Pacific', None, None),
            ('Europe/Dublin', None, None, 'Eastern', 0, 3600),
        ),
        'preferred': (('Eastern', '001', 'America/New_York'), ('Mountain', '001', 'America/Phoenix'),
                      ('Pacific', '001', 'America/Los_Angeles'), ('Pacific', 'CA', 'America/Vancouver'),
                      ('Pacific', 'US', 'America/Los_Angeles')),
        'primary': (('CN', 'Asia/Shanghai'),),
    }
    if variant and variant != 'gmt':
        rows = []
        for row in periods['periods']:
            if row[0] != 'America/Phoenix':
                rows.append(row)
            elif variant == 'conflict':
                rows.extend((('America/Phoenix', None, 1704067200, 'Mountain', -28800, -25200),
                             ('America/Phoenix', 1704067200, None, 'Mountain', -25200, -21600)))
            elif variant == 'gap':
                rows.extend((('America/Phoenix', None, 1704067200, 'Mountain', -25200, -21600),
                             ('America/Phoenix', 1704153600, None, 'Mountain', -25200, -21600)))
            elif variant == 'ambiguous':
                rows.append(('America/Phoenix', None, None, 'Mountain', -25200, -25200))
            else:
                raise ValueError('unknown fixture variant')
        periods['periods'] = tuple(rows)
    pool = tuple(sorted(names_wire.strings(names) | periods_wire.strings(periods) | set(tags),
                        key=lambda text: text.encode('utf-8')))
    ids = {text: i for i, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                          bundle.TIMEZONE_NAMES: names_wire.encode(names, tags, ids),
                          bundle.TIMEZONE_PRESENTATION: periods_wire.encode(periods, ids)}, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()).replace('qjs_intl_native', 'qjs_intl_date_zone_fixture')
    for variant in ('conflict', 'gap', 'ambiguous', 'gmt'):
        text += '\n' + embed.render(pack(variant)).replace('qjs_intl_native', 'qjs_intl_date_zone_' + variant)
    header = b'TZif3' + bytes(15) + struct.pack('>6I', 0, 0, 0, 0, 1, 4)
    block = struct.pack('>lBB', -28800, 0, 0) + b'STD\0'
    perpetual = header + block + header + block + b'\nSTD8DST7,0/0,J365/25\n'
    text += '\nconst uint8_t qjs_intl_date_zone_perpetual_bytes[] = {' + ','.join(str(byte) for byte in perpetual) + '};\n'
    text += 'const size_t qjs_intl_date_zone_perpetual_length = sizeof(qjs_intl_date_zone_perpetual_bytes);\n'
    atomic_write(args.output, text.encode('ascii'))
