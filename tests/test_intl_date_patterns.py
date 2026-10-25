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

"""Authored date/time styles, calendar aliases and independent skeleton metadata."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import date_patterns as patterns
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph

DTD = b'''<!ELEMENT ldml ANY><!ELEMENT dates ANY><!ELEMENT calendars ANY>
<!ELEMENT calendar ANY><!ATTLIST calendar type NMTOKEN #REQUIRED>
<!ATTLIST calendar alt NMTOKENS #IMPLIED>
<!ELEMENT dateFormats ANY><!ELEMENT timeFormats ANY>
<!ELEMENT dateFormatLength ANY><!ELEMENT timeFormatLength ANY>
<!ATTLIST dateFormatLength type (full | long | medium | short) #REQUIRED>
<!ATTLIST timeFormatLength type (full | long | medium | short) #REQUIRED>
<!ELEMENT dateFormat ANY><!ELEMENT timeFormat ANY>
<!ATTLIST dateFormat type NMTOKEN "standard">
<!ATTLIST timeFormat type NMTOKEN "standard">
<!ELEMENT pattern (#PCDATA)><!ATTLIST pattern type NMTOKEN "standard">
<!ATTLIST pattern alt NMTOKENS #IMPLIED><!ATTLIST pattern numbers CDATA #IMPLIED><!--@VALUE-->
<!ATTLIST pattern count NMTOKEN #IMPLIED><!ATTLIST pattern draft NMTOKEN #IMPLIED><!--@METADATA-->
<!ELEMENT datetimeSkeleton (#PCDATA)><!ATTLIST datetimeSkeleton alt NMTOKENS #IMPLIED>
<!ATTLIST datetimeSkeleton numbers CDATA #IMPLIED><!--@VALUE-->
<!ELEMENT alias EMPTY><!ATTLIST alias source NMTOKEN #REQUIRED><!--@VALUE-->
<!ATTLIST alias path CDATA #REQUIRED><!--@VALUE-->
<!ELEMENT special ANY>'''


def calendar(code, body):
    return '<calendar type="' + code + '">' + body + '</calendar>'


def style(body, family='date', width='long'):
    return ('<' + family + 'Formats><' + family + 'FormatLength type="' + width + '">' +
            '<' + family + 'Format>' + body + '</' + family + 'Format></' +
            family + 'FormatLength></' + family + 'Formats>')


def styles(family, entries):
    prefix, suffix = '<' + family + 'Formats>', '</' + family + 'Formats>'
    return prefix + ''.join(style(body, family, width)[len(prefix):-len(suffix)]
                            for width, body in entries) + suffix


def alias(family, code, source='locale'):
    return ('<' + family + 'Formats><alias source="' + source +
            '" path="../../calendar[@type=\'' + code + '\']/' + family + 'Formats"/>' +
            '</' + family + 'Formats>')


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldml.dtd': DTD}
        for locale in ('root', 'en', 'en_GB', 'de'):
            self.set(locale, '')

    def set(self, locale, body):
        self.files['common/main/' + locale + '.xml'] = (
            '<ldml><dates><calendars>' + body + '</calendars></dates></ldml>').encode('utf-8')

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(name for name in self.files if name.startswith(prefix))


def values(source, locale='en-GB'):
    return {row[1:6]: row[6:] for row in patterns.collect(source, graph())['rows']
            if row[0] == locale}


class DatePatternsTests(unittest.TestCase):
    def test_all_styles_and_families_preserve_exact_text(self):
        source = Sources()
        source.set('root', calendar('gregorian', ''.join(styles(family, (
            (width, '<pattern> y-MM-dd </pattern><datetimeSkeleton>yMMdd</datetimeSkeleton>')
            for width in patterns.WIDTHS)) for family in patterns.FAMILIES)))
        result = values(source)
        self.assertEqual(len(result), 16)
        for family in patterns.FAMILIES:
            for width in patterns.WIDTHS:
                self.assertEqual(result['gregorian', family, width, 'pattern', ''],
                                 ('value', ' y-MM-dd ', '', 'root', 'gregorian', family, width, 'pattern', ''))
        self.assertEqual(patterns.collect(source, graph()), patterns.collect(source, graph()))

    def test_calendar_and_family_aliases_restart_at_requesting_locale(self):
        source = Sources()
        source.set('root', calendar('gregorian', style('<pattern>y</pattern>')) +
                   calendar('generic', alias('date', 'gregorian')) +
                   calendar('japanese', '<alias source="locale" path="../calendar[@type=\'generic\']"/>'))
        source.set('en', calendar('gregorian', style('<pattern>y MMMM d</pattern>')))
        self.assertEqual(patterns.collect(source, graph())['source_aliases'], 2)
        self.assertEqual(values(source)['japanese', 'date', 'long', 'pattern', ''],
                         ('value', 'y MMMM d', '', 'en', 'gregorian', 'date', 'long', 'pattern', ''))
        self.assertEqual(values(source, 'de')['japanese', 'date', 'long', 'pattern', ''][1], 'y')

    def test_explicit_source_and_same_calendar_alias(self):
        source = Sources()
        source.set('root', calendar('gregorian', style('<pattern>y</pattern>')))
        source.set('en', calendar('gregorian', '<dateFormats><alias source="de" path="../dateFormats"/></dateFormats>'))
        source.set('de', calendar('gregorian', style('<pattern>dd.MM.y</pattern>')))
        self.assertEqual(values(source)['gregorian', 'date', 'long', 'pattern', ''][1:4],
                         ('dd.MM.y', '', 'de'))

    def test_numbers_and_alternates_belong_to_their_own_leaf(self):
        source = Sources()
        source.set('root', calendar('gregorian', style('''<pattern numbers="y=hebr">y-MM-dd</pattern>
            <pattern alt="variant" numbers="latn" draft="unconfirmed">dd/MM/y</pattern>
            <datetimeSkeleton numbers="y=hebr">yMMdd</datetimeSkeleton>''')))
        source.set('en', calendar('gregorian', style('<datetimeSkeleton alt="variant">yMd</datetimeSkeleton>')))
        result = values(source)
        self.assertEqual(result['gregorian', 'date', 'long', 'pattern', ''][1:3], ('y-MM-dd', 'y=hebr'))
        self.assertEqual(result['gregorian', 'date', 'long', 'pattern', 'variant'][1:4], ('dd/MM/y', 'latn', 'root'))
        self.assertEqual(result['gregorian', 'date', 'long', 'skeleton', 'variant'][1:4], ('yMd', '', 'en'))

    def test_missing_blocked_inherit_and_empty_remain_distinct(self):
        source = Sources()
        source.set('root', calendar('gregorian', style('<pattern>y</pattern>')))
        source.set('en', calendar('gregorian', styles('date', (
                   ('long', '<pattern>' + INHERIT + '</pattern>'),
                   ('short', '<pattern>' + BLOCK + '</pattern>'))) +
                   style('<pattern/>', family='time')))
        result = values(source)
        self.assertEqual(result['gregorian', 'date', 'long', 'pattern', ''][1:4], ('y', '', 'root'))
        self.assertEqual(result['gregorian', 'date', 'short', 'pattern', ''][0], 'blocked')
        self.assertEqual(result['gregorian', 'time', 'long', 'pattern', ''][:3], ('value', '', ''))
        self.assertEqual(result['gregorian', 'date', 'full', 'pattern', ''][0], 'missing')
        self.assertTrue(all(len(value) == 9 for value in result.values()))

    def test_other_calendar_owners_are_not_interpreted_as_styles(self):
        source = Sources()
        source.set('root', calendar('gregorian', '<dateTimeFormats><unknown/></dateTimeFormats>' +
                   '<months><unknown/></months>' + style('<pattern>y</pattern>')))
        self.assertEqual(patterns.collect(source, graph())['source_records'], 1)

    def test_reject_duplicate_and_unsupported_source_structure(self):
        cases = (
            calendar('gregorian', style('<pattern>y</pattern><pattern draft="approved">d</pattern>')),
            calendar('gregorian', style('<pattern count="one">y</pattern>')),
            calendar('gregorian', style('<pattern>j</pattern>')),
            calendar('gregorian', style('<pattern><child/></pattern>')),
            calendar('gregorian', style('<pattern>y</pattern>') * 2),
            calendar('gregorian', '<dateFormats>text</dateFormats>'),
            calendar('gregorian', alias('date', 'unknown')),
            calendar('gregorian', alias('date', 'gregorian')),
            calendar('gregorian', '<dateFormats><alias source="locale" path="unsupported"/></dateFormats>'),
            calendar('gregorian', '<alias source="locale" path="../calendar[@type=\'gregorian\']"/>'),
            calendar('gregorian', '<alias source="locale" path="../calendar[@type=\'gregorian\']"/>' + style('<pattern>y</pattern>')),
            '<calendar type="gregorian" alt="variant"/>',
            '<calendar type="gregorian"/><calendar type="gregorian"/>',
            '<alias source="locale" path="../calendars"/>',
        )
        for body in cases:
            source = Sources()
            source.set('root', body)
            with self.subTest(body=body), self.assertRaises(DataError):
                patterns.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
