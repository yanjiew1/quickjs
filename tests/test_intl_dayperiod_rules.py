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

"""Raw day-period semantics, independent rule kinds and explicit source origins."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import dayperiod_rules as source

DTD = b'''<!ELEMENT supplementalData ANY><!ELEMENT dayPeriodRuleSet ANY>
<!ATTLIST dayPeriodRuleSet type NMTOKEN #IMPLIED>
<!ELEMENT dayPeriodRules ANY><!ATTLIST dayPeriodRules locales NMTOKENS #REQUIRED>
<!ELEMENT dayPeriodRule EMPTY><!ATTLIST dayPeriodRule type NMTOKEN #REQUIRED>
<!ATTLIST dayPeriodRule at NMTOKEN #IMPLIED><!ATTLIST dayPeriodRule from NMTOKEN #IMPLIED>
<!ATTLIST dayPeriodRule before NMTOKEN #IMPLIED><!ATTLIST dayPeriodRule after NMTOKEN #IMPLIED>
<!ATTLIST dayPeriodRule to NMTOKEN #IMPLIED>'''
PATH = 'common/supplemental/dayPeriods.xml'
VARIABLE = '''<dayPeriodRule type="morning1" from="06:00" before="12:15"/>
<dayPeriodRule type="afternoon1" from="12:15" before="18:00"/>
<dayPeriodRule type="night1" from="18:00" before="06:00"/>'''
XML = ('''<supplementalData><dayPeriodRuleSet>
<dayPeriodRules locales="root"><dayPeriodRule type="am" from="00:00" before="12:00"/>
<dayPeriodRule type="pm" from="12:00" before="24:00"/></dayPeriodRules>
<dayPeriodRules locales="en sr hi_Latn">''' + VARIABLE + '''
<dayPeriodRule type="midnight" at="00:00"/><dayPeriodRule type="noon" at="12:00"/>
</dayPeriodRules></dayPeriodRuleSet><dayPeriodRuleSet type="selection">
<dayPeriodRules locales="en">''' + VARIABLE + '''</dayPeriodRules>
</dayPeriodRuleSet></supplementalData>''').encode()


class Sources:
    def __init__(self, xml=XML):
        self.files = {'common/dtd/ldmlSupplemental.dtd': DTD, PATH: xml}

    def read(self, name):
        return self.files[name]


class DayPeriodRulesTests(unittest.TestCase):
    def test_typed_kinds_explicit_origins_and_shared_lists(self):
        rows = source.collect(Sources())['rows']
        groups = {(kind, tag): rules for kind, tag, rules in rows}
        self.assertEqual(len(rows), 5)
        self.assertIs(groups['format', 'en'], groups['format', 'sr'])
        self.assertEqual(groups['format', 'en'], groups['format', 'hi-Latn'])
        self.assertIn(('night1', 'range', 64800, 21600), groups['format', 'en'])
        self.assertIn(('morning1', 'range', 21600, 44100), groups['format', 'en'])
        self.assertIn(('midnight', 'at', 0, 0), groups['format', 'en'])
        self.assertNotIn(('midnight', 'at', 0, 0), groups['selection', 'en'])
        self.assertEqual(groups['format', 'root'], (('am','range',0,43200), ('pm','range',43200,86400)))
        self.assertNotIn(('format', 'sr-Latn'), groups)  # Do not expand main-data inheritance.

    def test_boundaries_and_fixed_period_definitions(self):
        self.assertEqual(source.boundary('01:30'), 5400)
        self.assertEqual(source.boundary('24:00', True), 86400)
        for text, before in (('24:00', False), ('24:01', True), ('25:00', True), ('12:60', False),
                             ('1:00', False), ('00:00:00', False), (' 00:00', False)):
            with self.subTest(text=text), self.assertRaises(DataError):
                source.boundary(text, before)
        for code, form, start, end in (('midnight','at',60,60), ('noon','range',0,43200),
                                       ('am','range',0,43140), ('pm','range',43200,0),
                                       ('morning1','at',0,0)):
            with self.subTest(code=code), self.assertRaises(DataError):
                source.validate_rules('format', ((code,form,start,end),))

    def test_variable_partition_gaps_overlaps_and_format_ambiguity(self):
        for rules in ((('morning1','range',60,43200), ('night1','range',43200,0)),
                      (('morning1','range',0,43260), ('night1','range',43200,86400)),
                      (('morning1','range',0,0),),
                      (('morning1','range',0,46800), ('night1','range',46800,86400)),
                      (('morning1','range',22500,65700), ('night1','range',65700,22500)),
                      (('morning1','range',22500,44100), ('afternoon1','range',44100,65700),
                       ('night1','range',65700,22500))):
            with self.subTest(rules=rules), self.assertRaises(DataError):
                source.validate_rules('format', rules)
        # Selection is not constrained by twelve-hour clock parsing.
        self.assertEqual(len(source.validate_rules('selection',
            (('morning1','range',0,46800), ('night1','range',46800,86400)))), 2)
        self.assertEqual(len(source.validate_rules('selection',
            (('morning1','range',22500,65700), ('night1','range',65700,22500)))), 2)
        # Fixed AM/PM may coexist with a complete variable partition.
        rules = (('morning1','range',0,43200), ('night1','range',43200,86400),
                 ('am','range',0,43200), ('pm','range',43200,86400))
        self.assertEqual(len(source.validate_rules('format', rules)), 4)

    def test_reject_unknown_duplicate_and_unreviewed_source_forms(self):
        for old, new in ((b'type="selection"', b'type="format"'),
                         (b'type="selection"', b'type="unknown"'),
                         (b'locales="root"', b'locales=""'),
                         (b'locales="root"', b'locales="root root"'),
                         (b'locales="en sr hi_Latn"', b'locales="en EN"'),
                         (b'locales="en sr hi_Latn"', b'locales="en sr\xc2\xa0hi_Latn"'),
                         (b'type="night1"', b'type="morning1"'),
                         (b'type="night1"', b'type="unknown"'),
                         (b'from="18:00"', b'after="18:00"'),
                         (b'before="06:00"', b'to="06:00"'),
                         (b'at="00:00"', b'at="00:00" before="01:00"'),
                         (b'at="00:00"', b''),
                         (b'<dayPeriodRuleSet>', b'<dayPeriodRuleSet>text'),
                         (b'<dayPeriodRuleSet>', b'<dayPeriodRuleSet extra="x">'),
                         (b'<dayPeriodRule type="midnight" at="00:00"/>',
                          b'<dayPeriodRule type="midnight" at="00:00"><x/></dayPeriodRule>')):
            with self.subTest(new=new), self.assertRaises(DataError):
                source.collect(Sources(XML.replace(old, new)))

    def test_empty_and_point_only_facts_do_not_fabricate_am_pm(self):
        self.assertEqual(source.collect(Sources(b'<supplementalData/>')), {'rows': ()})
        xml = b'''<supplementalData><dayPeriodRuleSet type="selection">
          <dayPeriodRules locales="gsw"><dayPeriodRule type="midnight" at="00:00"/>
          </dayPeriodRules></dayPeriodRuleSet></supplementalData>'''
        self.assertEqual(source.collect(Sources(xml)),
                         {'rows': (('selection', 'gsw', (('midnight','at',0,0),)),)})


if __name__ == '__main__':
    unittest.main()
