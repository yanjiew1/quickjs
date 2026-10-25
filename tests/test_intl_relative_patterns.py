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

"""Authored RelativeTimeFormat source aliases, markers and literal/template roles."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from resolve import BLOCK, INHERIT
import relative_patterns as relative
from test_intl_list_patterns import graph

DTD = b'''<!ELEMENT ldml ANY><!ELEMENT dates ANY><!ELEMENT fields ANY>
<!ATTLIST dates alt NMTOKENS #IMPLIED>
<!ATTLIST dates validSubLocales NMTOKENS #IMPLIED><!--@VALUE-->
<!ATTLIST fields alt NMTOKENS #IMPLIED>
<!ATTLIST fields validSubLocales NMTOKENS #IMPLIED><!--@VALUE-->
<!ELEMENT field ANY><!ATTLIST field type NMTOKEN #REQUIRED>
<!ATTLIST field alt NMTOKENS #IMPLIED>
<!ELEMENT displayName (#PCDATA)><!ELEMENT relativePeriod (#PCDATA)>
<!ELEMENT relative (#PCDATA)><!ATTLIST relative type NMTOKEN #REQUIRED>
<!ATTLIST relative alt NMTOKENS #IMPLIED>
<!ATTLIST relative draft NMTOKEN #IMPLIED><!--@METADATA-->
<!ATTLIST relative validSubLocales NMTOKENS #IMPLIED><!--@VALUE-->
<!ELEMENT relativeTime ANY><!ATTLIST relativeTime type NMTOKEN #REQUIRED>
<!ATTLIST relativeTime alt NMTOKENS #IMPLIED>
<!ELEMENT relativeTimePattern (#PCDATA)><!ATTLIST relativeTimePattern count NMTOKEN #REQUIRED>
<!ATTLIST relativeTimePattern alt NMTOKENS #IMPLIED>
<!ELEMENT alias EMPTY><!ATTLIST alias source NMTOKEN #REQUIRED><!--@VALUE-->
<!ATTLIST alias path CDATA #REQUIRED><!--@VALUE-->
<!ATTLIST alias alt NMTOKENS #IMPLIED>
<!ELEMENT special ANY><!ELEMENT unknown ANY>'''


def field(name, content):
    return '<field type="' + name + '">' + content + '</field>'


def pattern(tense, count, text):
    return '<relativeTime type="' + tense + '"><relativeTimePattern count="' + count + '">' + text + '</relativeTimePattern></relativeTime>'


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldml.dtd': DTD}
        root = ''
        for unit in relative.UNITS:
            root += field(unit, pattern('past', 'other', '{0} ' + unit + ' ago') +
                          pattern('future', 'other', 'in {0} ' + unit))
            root += field(unit + '-short', '<alias source="locale" path="../field[@type=\'' + unit + '\']"/>')
            root += field(unit + '-narrow', '<alias source="locale" path="../field[@type=\'' + unit + '-short\']"/>')
        self.set('root', root)
        for locale in ('en', 'en_GB', 'de'):
            self.set(locale, '')

    def set(self, locale, text):
        self.files['common/main/' + locale + '.xml'] = ('<ldml><dates><fields>' + text + '</fields></dates></ldml>').encode('utf-8')

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))


class RelativePatternTests(unittest.TestCase):
    def values(self, data, locale='en', unit='day', width='long'):
        index = next(i for tag, u, w, i in data['rows'] if (tag, u, w) == (locale, unit, width))
        return {(kind, selector): (blocked, text, origin, u, w)
                for kind, selector, blocked, text, origin, u, w in data['maps'][index]}

    def test_inventory_and_determinism(self):
        data = relative.collect(Sources(), graph())
        self.assertEqual(len(data['rows']), 4 * 8 * 3)
        self.assertEqual(data['source_records'], 16)
        self.assertEqual(data['source_aliases'], 16)
        self.assertEqual(data['alias_fields'], 16 * 12)
        self.assertEqual(data['maps'], tuple(sorted(set(data['maps']))))
        self.assertEqual(data, relative.collect(Sources(), graph()))
        self.assertEqual(self.values(data, width='narrow')[('past', 'other')],
                         (False, '{0} day ago', 'root', 'day', 'long'))

    def test_inherited_alias_restarts_request_and_retains_child_override(self):
        source = Sources()
        source.set('en', field('day-short', pattern('future', 'one', 'EN {0}')))
        source.set('en_GB', field('day-short', pattern('future', 'one', 'GB {0}')) +
                   field('day-narrow', '<relative type="0">British today</relative>'))
        data = relative.collect(source, graph())
        values = self.values(data, 'en-GB', width='narrow')
        self.assertEqual(values[('future', 'one')], (False, 'GB {0}', 'en-GB', 'day', 'short'))
        self.assertEqual(values[('literal', '0')], (False, 'British today', 'en-GB', 'day', 'narrow'))
        self.assertEqual(values[('past', 'other')][2:], ('root', 'day', 'long'))

    def test_explicit_source_alias_and_changed_unit(self):
        source = Sources()
        source.set('en', field('hour', pattern('future', 'one', 'English {0}')))
        source.set('de', field('day-narrow', '<alias source="en" path="../field[@type=\'hour\']"/>'))
        value = self.values(relative.collect(source, graph()), 'de', width='narrow')
        self.assertEqual(value[('future', 'one')], (False, 'English {0}', 'en', 'hour', 'long'))

    def test_literal_keys_and_literal_only_numeric_forms(self):
        source = Sources()
        content = ''.join('<relative type="' + str(key) + '">' + str(key) + ' {not a template}</relative>'
                          for key in (-2, -1, 0, 1, 2, 3, 42))
        content += pattern('past', 'one', 'one day ago')
        content += pattern('future', 'two', 'in two days')
        source.set('en', field('day', content))
        data = relative.collect(source, graph())
        self.assertEqual(data['literal_keys'], ('-1', '-2', '0', '1', '2', '3', '42'))
        values = self.values(data)
        self.assertEqual(values[('literal', '42')][1], '42 {not a template}')
        self.assertEqual(values[('past', 'one')][1], 'one day ago')
        self.assertEqual(values[('future', 'two')][1], 'in two days')

    def test_markers_empty_and_absent_are_distinct(self):
        source = Sources()
        source.set('en', field('day', '<relative type="-1">' + BLOCK + '</relative><relative type="0"/>' +
                   pattern('future', 'one', BLOCK) + pattern('past', 'one', '')))
        source.set('en_GB', field('day', '<relative type="-1">' + INHERIT + '</relative>'))
        values = self.values(relative.collect(source, graph()), 'en-GB')
        self.assertEqual(values[('literal', '-1')][:3], (True, '', 'en'))
        self.assertEqual(values[('literal', '0')][:2], (False, ''))
        self.assertEqual(values[('past', 'one')][:2], (False, ''))
        self.assertTrue(values[('future', 'one')][0])
        self.assertNotIn(('literal', '1'), values)

    def test_unrelated_fields_and_alternates(self):
        source = Sources()
        source.set('en', field('weekday', '<unknown/>') +
                   '<field type="day" alt="variant"><unknown/></field>' +
                   field('day', '<displayName>day label</displayName><relativePeriod>period</relativePeriod>' +
                         '<relative type="0" alt="variant">ignored</relative>' +
                         '<relative type="0" draft="unconfirmed">today</relative>'))
        data = relative.collect(source, graph())
        self.assertEqual(data['skipped_alternates'], 2)
        self.assertEqual(self.values(data)[('literal', '0')][1], 'today')

    def test_container_applicability(self):
        source = Sources()
        content = field('day', '<relative type="0">selected</relative>')
        ignored = field('day', '<relative type="0">alternate</relative>')
        source.files['common/main/en.xml'] = (
            '<ldml><dates alt="variant"><fields>' + ignored + '</fields></dates>' +
            '<dates><alias source="locale" path="../dates" alt="variant"/>' +
            '<fields alt="variant">' + ignored + '</fields><fields>' +
            content + '</fields></dates></ldml>').encode('utf-8')
        data = relative.collect(source, graph())
        self.assertEqual(data['skipped_alternates'], 3)
        self.assertEqual(self.values(data)[('literal', '0')][1], 'selected')
        cases = (
            '<dates validSubLocales="en"><fields/></dates>',
            '<dates><fields validSubLocales="en"/></dates>',
            '<dates/><dates/>',
            '<dates><fields/><fields/></dates>',
            '<dates><alias source="locale" path="../dates"/></dates>',
            '<dates><fields><alias source="locale" path="../fields"/></fields></dates>',
        )
        for content in cases:
            source.files['common/main/en.xml'] = ('<ldml>' + content + '</ldml>').encode('utf-8')
            with self.subTest(content=content), self.assertRaises(DataError):
                relative.collect(source, graph())

    def test_literal_key_boundaries(self):
        for key in ('9007199254740992', '-9007199254740992'):
            self.assertEqual(relative.literal_key(key), key)
        for key in ('9007199254740993', '-9007199254740993'):
            with self.subTest(key=key), self.assertRaises(DataError):
                relative.literal_key(key)

    def test_bad_source_structures_and_keys(self):
        cases = [
            field('day', '<relative type="0">x</relative><relative type="0">y</relative>'),
            field('day', '<relative type="0" validSubLocales="en">scoped</relative>'),
            field('day', '<alias source="locale" path="../field[@type=\'hour\']"/><displayName>x</displayName>'),
            field('day', '<alias source="locale" path="../field[@type=\'weekday\']"/>'),
            field('day', '<alias source="zz" path="../field[@type=\'hour\']"/>'),
            field('day', '<unknown/>'),
            field('day', pattern('past', 'other', 'a') + pattern('past', 'one', 'b')),
            field('day', '<relativeTime type="past"><alias source="locale" path="../relativeTime"/></relativeTime>'),
            field('day', pattern('future', 'invalid', 'x')),
            field('day', '<relativeTime type="past"><relativeTimePattern count="one">x</relativeTimePattern><relativeTimePattern count="one">y</relativeTimePattern></relativeTime>'),
        ]
        cases += [field('day', '<relative type="' + key + '">x</relative>')
                  for key in ('-0', '01', '+1', '1.0', 'NaN', '9007199254740993')]
        cases += [field('day', pattern('future', 'one', value))
                  for value in ('{0}{0}', '{1}', '{', '}')]
        cases += [field('day', '') + field('day', '')]
        for content in cases:
            source = Sources()
            source.set('en', content)
            with self.subTest(content=content), self.assertRaises(DataError):
                relative.collect(source, graph())

    def test_cycles_and_required_long_coverage(self):
        source = Sources()
        source.set('en', field('day', '<alias source="locale" path="../field[@type=\'day-short\']"/>'))
        with self.assertRaisesRegex(DataError, 'cycle'):
            relative.collect(source, graph())
        source = Sources()
        source.set('en', field('day', pattern('past', 'other', BLOCK)))
        with self.assertRaisesRegex(DataError, 'required'):
            relative.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
