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

"""Independent names for field widths, exact masking and semantic substitution."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from acquire import atomic_write
import bundle
import calendar_names
import calendar_name_tables as wire
import embed
from resolve import BLOCK
import tables
from test_intl_calendar_names import Sources, alias, calendar, group
from test_intl_list_patterns import graph


def pack():
    source = Sources()
    source.set('root', calendar('gregorian', '''<months><monthContext type="format">
      <monthWidth type="wide"><month type="1">January</month><month type="6">ordinary-six</month>
        <month type="7">ordinary-seven</month><month type="7" yeartype="standard">common-seven</month>
        <month type="7" yeartype="leap">leap-seven</month><month type="8">ordinary-eight</month>
        <month type="8" yeartype="leap">parent-leap-eight</month><month type="9">ordinary-nine</month>
        <month type="9" yeartype="leap">parent-leap-nine</month></monthWidth>
      <monthWidth type="abbreviated">''' + alias("../monthWidth[@type='wide']") + '''</monthWidth>
      <monthWidth type="narrow"><month type="1">J</month></monthWidth></monthContext>
      <monthContext type="stand-alone"><monthWidth type="wide"><month type="1">standalone-January</month>
      </monthWidth></monthContext></months>
      <days><dayContext type="format"><dayWidth type="wide"><day type="sun">Sunday</day></dayWidth>
        <dayWidth type="abbreviated"><day type="sun">Sun</day><day type="mon">Mon</day>
          <day type="tue">Tue</day><day type="wed">Wed</day></dayWidth>
        <dayWidth type="narrow"><day type="sun">S</day></dayWidth>
        <dayWidth type="short"><day type="sun">Su</day><day type="mon"/>
          <day type="wed">We</day></dayWidth></dayContext>
      <dayContext type="stand-alone"><dayWidth type="short"><day type="sun">standalone-Su</day>
      </dayWidth></dayContext></days>
      <eras><eraNames><era type="1">AD-wide</era></eraNames>
        <eraAbbr><era type="1">AD</era><era type="1" alt="variant">CE</era>
          <era type="0" alt="variant">BCE-only</era></eraAbbr>
        <eraNarrow><era type="1">A</era></eraNarrow></eras>
      <dayPeriods><dayPeriodContext type="format"><dayPeriodWidth type="wide">
        <dayPeriod type="am">AM-wide</dayPeriod><dayPeriod type="pm">PM-wide</dayPeriod>
        <dayPeriod type="morning1">morning-wide</dayPeriod></dayPeriodWidth>
      <dayPeriodWidth type="abbreviated"><dayPeriod type="am">AM</dayPeriod><dayPeriod type="pm">PM</dayPeriod>
        <dayPeriod type="morning1">parent-morning</dayPeriod><dayPeriod type="noon">parent-noon</dayPeriod>
        <dayPeriod type="midnight">parent-midnight</dayPeriod></dayPeriodWidth>
      <dayPeriodWidth type="narrow"><dayPeriod type="am">a</dayPeriod><dayPeriod type="pm">p</dayPeriod>
      </dayPeriodWidth></dayPeriodContext></dayPeriods>''') +
      calendar('japanese', group('eras', '<era type="236">Reiwa</era>')) +
      calendar('chinese', '''<cyclicNameSets><cyclicNameSet type="years"><cyclicNameContext type="format">
        <cyclicNameWidth type="abbreviated"><cyclicName type="1">cycle-one</cyclicName>
          <cyclicName type="60">cycle-sixty</cyclicName></cyclicNameWidth>
        <cyclicNameWidth type="wide">''' + alias("../cyclicNameWidth[@type='abbreviated']") + '''</cyclicNameWidth>
        <cyclicNameWidth type="narrow"><cyclicName type="1">C</cyclicName></cyclicNameWidth>
        </cyclicNameContext></cyclicNameSet></cyclicNameSets>'''))
    source.set('en', calendar('gregorian', group('months', '''<month type="1">child-January</month>
        <month type="7">child-seven</month><month type="8" yeartype="leap">''' + BLOCK + '''</month>
        <month type="9" yeartype="leap"/>''') +
        group('days', '<day type="wed">' + BLOCK + '</day>', width='short') +
        group('dayPeriods', '<dayPeriod type="morning1">' + BLOCK + '</dayPeriod><dayPeriod type="noon"/>'
              '<dayPeriod type="midnight">' + BLOCK + '</dayPeriod>', width='abbreviated')))
    tags = graph().tags
    data = wire.select(calendar_names.collect(source, graph()), tags)
    pool = tuple(sorted(wire.strings(data) | set(tags), key=lambda text: text.encode('utf-8')))
    ids = {text: index for index, text in enumerate(pool)}
    binary = bundle.pack({bundle.STRINGS: tables.encode_strings(pool),
                          bundle.LOCALES: tables.encode_locales(tags, graph(), ids),
                          bundle.CALENDAR_NAMES: wire.encode(data, tags, ids)}, bytes(32), 'minimal')
    tables.validate(binary)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    atomic_write(args.output, embed.render(pack()).encode('ascii'))
