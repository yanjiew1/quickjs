#!/usr/bin/env python3
"""Emit authored name layers, external origins and an empty calendar-name pack."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import calendar_names
import calendar_name_tables as wire
import embed
from parents import ParentGraph
from resolve import BLOCK, INHERIT
import tables
from test_intl_calendar_names import Sources, alias, calendar, group
from test_intl_list_patterns import graph


def pack(empty=False):
    source = Sources()
    inheritance = ParentGraph(graph().tags + ('en-1901-1996',), {}, {},
                              {'und': 'en-Latn-US'}, (), False)
    tags = ('de', 'en', 'en-GB', 'root')
    if empty:
        source.set('root', calendar('gregorian', '<months/>'))
    else:
        source.set('root', calendar('gregorian', group('months',
            '<month type="1">root</month><month type="2">' + BLOCK + '</month><month type="3"/>'
            '<month type="7" yeartype="standard">ordinary</month>'
            '<month type="7" yeartype="leap" alt="variant">leap😀</month>') +
            group('days', '<day type="mon">Monday</day>', width='short') +
            group('quarters', '<quarter type="4">Q4</quarter>', context='stand-alone') +
            group('dayPeriods', '<dayPeriod type="midnight">night</dayPeriod>', width='narrow')) +
            calendar('japanese', '<eras><eraNames><era type="236">Reiwa</era></eraNames>'
                '<eraAbbr><era type="' + '1' * 4096 + '">raw long era</era></eraAbbr>'
                '<eraNarrow><era type="' + '1' * 4096 + '">another layer</era></eraNarrow></eras>') +
            calendar('chinese', '''<cyclicNameSets><cyclicNameSet type="years">
                <cyclicNameContext type="format"><cyclicNameWidth type="abbreviated">
                <cyclicName type="1">year</cyclicName></cyclicNameWidth></cyclicNameContext></cyclicNameSet>
                <cyclicNameSet type="solarTerms"><cyclicNameContext type="format">
                <cyclicNameWidth type="narrow">''' + alias("../../../cyclicNameSet[@type='years']/"
                "cyclicNameContext[@type='format']/cyclicNameWidth[@type='abbreviated']") +
                '''</cyclicNameWidth></cyclicNameContext></cyclicNameSet></cyclicNameSets>''' +
                group('monthPatterns', '<monthPattern type="leap">leap {0}</monthPattern>',
                      context='numeric', width='all')) +
            calendar('generic', '<months>' + alias("../../calendar[@type='gregorian']/months") + '</months>'))
        source.set('en', calendar('gregorian', group('months', '<month type="1" alt="variant">child</month>')))
        source.set('en_GB', calendar('generic', group('months', '<month type="4">own</month>')))
        source.set('en_1901_1996', calendar('gregorian', group('months', '<month type="5">external</month>')))
        source.set('de', calendar('generic', '<months>' +
            alias("../../calendar[@type='gregorian']/months", source='en_1901_1996') + '</months>'))
    data = wire.select(calendar_names.collect(source, inheritance), tags)
    if empty:
        data['groups'] = (('gregorian', 'months', '', 'format', 'wide'),)
    pool = tuple(sorted(wire.strings(data) | set(tags) |
                        {BLOCK, INHERIT, 'Generic', 'japanese-', 'En', 'en-us',
                         'bad_tag', 'en-1901-1901', 'bad!', '0', '01', '14'},
                        key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    return bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                        bundle.LOCALES: tables.encode_locales(tags, inheritance, ids),
                        bundle.CALENDAR_NAMES: wire.encode(data, tags, ids)}, bytes(32), 'minimal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()) + '\n' + embed.render(pack(True)).replace('qjs_intl_native', 'calendar_name_empty')
    atomic_write(args.output, text.encode('ascii'))
