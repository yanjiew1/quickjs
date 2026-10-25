#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Authored field labels and templates independent of prepared CLDR data."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import calendar_names
import calendar_name_tables as names_wire
import calendar_era_tables as era_wire
import embed
from resolve import BLOCK
import tables
from test_intl_calendar_names import Sources, calendar, group
from test_intl_list_patterns import graph


def pack():
    source = Sources()
    eras = '<eras><eraAbbr><era type="0">BCE</era><era type="1">CE</era></eraAbbr></eras>'
    lunar = '''<months><monthContext type="format"><monthWidth type="wide">
      <month type="2">second月</month></monthWidth>
      <monthWidth type="abbreviated"><month type="2">second-abbr</month></monthWidth></monthContext>
      <monthContext type="stand-alone"><monthWidth type="wide"><month type="2">standalone月</month>
      </monthWidth></monthContext></months>
      <monthPatterns><monthPatternContext type="format"><monthPatternWidth type="wide">
      <monthPattern type="leap">閏'{0}'!</monthPattern></monthPatternWidth>
      <monthPatternWidth type="abbreviated"><monthPattern type="leap">{1}</monthPattern>
      </monthPatternWidth></monthPatternContext>
      <monthPatternContext type="stand-alone"><monthPatternWidth type="wide">
      <monthPattern type="leap">({0})閏</monthPattern></monthPatternWidth>
      </monthPatternContext>
      <monthPatternContext type="numeric"><monthPatternWidth type="all">
      <monthPattern type="leap">[閏{0}]</monthPattern></monthPatternWidth>
      </monthPatternContext></monthPatterns>
      <cyclicNameSets><cyclicNameSet type="years"><cyclicNameContext type="format">
      <cyclicNameWidth type="abbreviated"><cyclicName type="40">cycle四十</cyclicName>
      </cyclicNameWidth></cyclicNameContext></cyclicNameSet></cyclicNameSets>'''
    # Complete reachable lunar domains while retaining the special month/year
    # labels used by rendering regressions below.
    other = ''.join('<month type="' + str(i) + '">month' + str(i) + '</month>'
                    for i in range(1, 13) if i != 2)
    lunar = lunar.replace('<month type="2">second月</month>', other + '<month type="2">second月</month>')
    lunar = lunar.replace('<month type="2">second-abbr</month>', other + '<month type="2">second-abbr</month>')
    lunar = lunar.replace('<month type="2">standalone月</month>', other + '<month type="2">standalone月</month>')
    cycles = ''.join('<cyclicName type="' + str(i) + '">cycle' + str(i) + '</cyclicName>'
                     for i in range(1, 61) if i != 40)
    lunar = lunar.replace('<cyclicName type="40">', cycles + '<cyclicName type="40">')
    hebrew = ''.join('<month type="' + str(i) + '">month' + str(i) + '</month>'
                     for i in range(1, 14) if i not in (6, 7))
    periods = ''.join('<dayPeriod type="' + code + '">' + code + '</dayPeriod>'
                      for code in ('midnight', 'noon', 'morning1', 'morning2', 'afternoon1',
                                   'afternoon2', 'evening1', 'evening2', 'night1', 'night2'))
    source.set('root', calendar('gregorian', eras +
        group('months', '<month type="1">January</month>') +
        '<days><dayContext type="format"><dayWidth type="wide"><day type="sun">Sunday</day>'
        '</dayWidth><dayWidth type="abbreviated"><day type="sun">Sun</day></dayWidth>'
        '</dayContext></days>' +
        group('dayPeriods', '<dayPeriod type="am">AM</dayPeriod><dayPeriod type="pm">PM</dayPeriod>'
              '<dayPeriod type="noon">NOON</dayPeriod><dayPeriod type="morning1">morning</dayPeriod>',
              width='abbreviated')) +
        calendar('iso8601', eras.replace('>CE</era>', '>ISO-CE</era>')) +
        calendar('japanese', '<eras><eraAbbr><era type="236">Reiwa</era>'
                 '<era type="999">Future</era></eraAbbr></eras>') +
        calendar('hebrew', group('months', hebrew + '<month type="6" yeartype="leap">Adar I</month>'
                 '<month type="7" yeartype="leap">Adar II</month><month type="7">Adar</month>')) +
        calendar('chinese', lunar) + calendar('dangi', lunar) +
        ''.join(calendar(name, '<monthPatterns><monthPatternContext type="numeric">'
            '<monthPatternWidth type="all"><monthPattern type="leap">' + text +
            '</monthPattern></monthPatternWidth></monthPatternContext></monthPatterns>')
            for name, text in (('numericbad', '{1}'), ('numericblocked', BLOCK), ('numericempty', ''))) +
        calendar('periodonly', group('dayPeriods', periods, width='abbreviated')))
    source.set('en', calendar('gregorian', group('months', '<month type="2">' + BLOCK + '</month>'
               '<month type="3"/>') + group('dayPeriods', '<dayPeriod type="noon"/>', width='abbreviated')))
    tags = graph().tags
    names = names_wire.select(calendar_names.collect(source, graph()), tags)
    eras = {'rows': (('gregorian', 'bce', '0'), ('gregorian', 'ce', '1'),
                     ('japanese', 'next-era', '999'), ('japanese', 'reiwa', '236'))}
    pool = tuple(sorted(names_wire.strings(names) | era_wire.strings(eras) | set(tags),
                        key=lambda text: text.encode('utf-8')))
    ids = {text: i for i, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                          bundle.CALENDAR_NAMES: names_wire.encode(names, tags, ids),
                          bundle.CALENDAR_ERAS: era_wire.encode(eras, ids)}, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = embed.render(pack()).replace('qjs_intl_native', 'qjs_intl_date_text_fixture')
    atomic_write(args.output, text.encode('ascii'))
