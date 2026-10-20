#!/usr/bin/env python3
"""Emit interval layers with external origins, selector aliases and empty data."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import date_intervals
import date_interval_tables as wire
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
from test_intl_date_intervals import Sources, alias, data_calendar, formats, item, leaf
from test_intl_list_patterns import graph


def pack(empty=False):
    source = Sources()
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',), {}, {}, {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'en', 'en-GB', 'root')
    if empty:
        source.set('root', data_calendar(formats('')))
    else:
        source.set('root', data_calendar(formats(leaf('intervalFormatFallback', '{0}–{1}') +
            item(''.join(leaf('greatestDifference', 'd–d', id=code) for code in sorted('BGHahm')) +
                 leaf('greatestDifference', "latestFirst:M d – M d", id='M') +
                 leaf('greatestDifference', '', id='y') + leaf('greatestDifference', BLOCK, id='d')) +
            '<intervalFormatRanges>' + ''.join(leaf('intervalFormatRange', '{1} to {0}', type=code)
                                              for code in sorted(date_intervals.RANGES)) + '</intervalFormatRanges>') +
            formats(item(leaf('greatestDifference', 'dd–dd', id='d', alt='variant'), alt='ascii'), alt='variant')) +
            data_calendar(formats(alias("../../../calendar[@type='gregorian']/dateTimeFormats/intervalFormats")),
                          code='generic') +
            data_calendar(formats(item(leaf('greatestDifference', 'd–d', id='d'), skeleton='y' * 4096)) +
                          formats(item(leaf('greatestDifference', 'dd–dd', id='d'), skeleton='y' * 4096), alt='variant'),
                          code='chinese'))
        source.set('en', data_calendar(formats(item(leaf('greatestDifference', "'child'", id='d', alt='variant')))))
        source.set('en_GB', data_calendar(formats(item(leaf('greatestDifference', "'own'", id='M', alt='variant'))),
                                          code='generic'))
        source.set('en_1901_1996', data_calendar(formats(item(leaf('greatestDifference', "'external'", id='d')))))
        source.set('de', data_calendar(formats(alias("../../../calendar[@type='gregorian']/dateTimeFormats/"
                                                    'intervalFormats', 'en_1901_1996')), code='generic'))
    data = wire.select(date_intervals.collect(source, inheritance), tags)
    if empty:
        data['groups'] = (('gregorian', '', 'fallback', '', ''),)
    pool = tuple(sorted(wire.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'Generic', 'En', 'en-us', 'bad_tag', 'en-1901-1901', 'bad!', 'Unknown'},
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                        bundle.DATE_INTERVALS: wire.encode(data, tags, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()) + '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'date_interval_empty')
    atomic_write(args.output, text.encode('ascii'))
