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

"""Authored digital-duration source selection, inheritance and provenance."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from resolve import BLOCK, INHERIT
import duration_patterns as duration
from test_intl_list_patterns import graph

DTD = b'''<!ELEMENT ldml ANY><!ELEMENT units ANY>
<!ATTLIST units alt NMTOKENS #IMPLIED>
<!ATTLIST units validSubLocales NMTOKENS #IMPLIED><!--@VALUE-->
<!ELEMENT durationUnit ANY><!ATTLIST durationUnit type NMTOKEN #REQUIRED>
<!ATTLIST durationUnit alt NMTOKENS #IMPLIED>
<!ELEMENT durationUnitPattern (#PCDATA)>
<!ATTLIST durationUnitPattern alt NMTOKENS #IMPLIED>
<!ATTLIST durationUnitPattern draft NMTOKEN #IMPLIED><!--@METADATA-->
<!ATTLIST durationUnitPattern validSubLocales NMTOKENS #IMPLIED><!--@VALUE-->
<!ELEMENT alias EMPTY><!ATTLIST alias source NMTOKEN #REQUIRED><!--@VALUE-->
<!ATTLIST alias path CDATA #REQUIRED><!--@VALUE-->
<!ATTLIST alias alt NMTOKENS #IMPLIED>
<!ELEMENT unitLength ANY><!ELEMENT special ANY><!ELEMENT unknown ANY>'''


def pattern(name, text):
    return '<durationUnit type="' + name + '"><durationUnitPattern>' + text + '</durationUnitPattern></durationUnit>'


def alias(name, target, source='locale'):
    return ('<durationUnit type="' + name + '"><alias source="' + source +
            '" path="../durationUnit[@type=\'' + target + '\']"/></durationUnit>')


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldml.dtd': DTD}
        self.set('root', pattern('hm', 'h:mm') + pattern('hms', 'h:mm:ss') + pattern('ms', 'm:ss'))
        for locale in ('en', 'en_GB', 'de'):
            self.set(locale, '')

    def set(self, locale, content):
        self.files['common/main/' + locale + '.xml'] = ('<ldml><units>' + content + '</units></ldml>').encode('utf-8')

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))


class DurationPatternsTests(unittest.TestCase):
    def values(self, source, locale='en'):
        return {name: (state, text, origin, key)
                for tag, name, state, text, origin, key in duration.collect(source, graph())['rows']
                if tag == locale}

    def test_inventory_and_determinism(self):
        source = Sources()
        result = duration.collect(source, graph())
        self.assertEqual(len(result['rows']), 12)
        self.assertEqual(result['source_records'], 3)
        self.assertEqual(result['source_aliases'], 0)
        self.assertEqual(result, duration.collect(source, graph()))
        self.assertEqual(self.values(source)['hms'], ('value', 'h:mm:ss', 'root', 'hms'))

    def test_exact_patterns_are_not_projected_to_equal_separators(self):
        source = Sources()
        source.set('en', pattern('hm', "'ga' h:mm") +
                   pattern('hms', ' hh.mm:ss ') + pattern('ms', 'aɖabaƒoƒo m:ss'))
        values = self.values(source, 'en-GB')
        for key, text in (('hm', "'ga' h:mm"), ('hms', ' hh.mm:ss '), ('ms', 'aɖabaƒoƒo m:ss')):
            self.assertEqual(values[key], ('value', text, 'en', key))

    def test_inherited_alias_restarts_at_requesting_locale(self):
        source = Sources()
        source.set('en', alias('hm', 'ms'))
        source.set('en_GB', pattern('ms', 'British m.ss'))
        self.assertEqual(self.values(source, 'en-GB')['hm'],
                         ('value', 'British m.ss', 'en-GB', 'ms'))

    def test_explicit_source_alias(self):
        source = Sources()
        source.set('en', pattern('ms', 'EN m.ss'))
        source.set('de', alias('hm', 'ms', 'en'))
        self.assertEqual(self.values(source, 'de')['hm'], ('value', 'EN m.ss', 'en', 'ms'))

    def test_missing_blocked_empty_inherit_are_distinct(self):
        source = Sources()
        source.set('root', '')
        source.set('en', pattern('hm', BLOCK) + pattern('hms', ''))
        source.set('en_GB', pattern('hm', INHERIT))
        values = self.values(source, 'en-GB')
        self.assertEqual(values['hm'], ('blocked', '', 'en', 'hm'))
        self.assertEqual(values['hms'], ('value', '', 'en', 'hms'))
        self.assertEqual(values['ms'], ('missing', '', '', ''))

    def test_alternates_selected_before_duplicate_validation(self):
        source = Sources()
        source.files['common/main/en.xml'] = b'''<ldml>
            <units alt="variant"><unknown/></units><units>
            <unitLength><unknown/></unitLength>
            <durationUnit type="hm" alt="variant"><unknown/></durationUnit>
            <durationUnit type="hm"><alias alt="variant" source="locale" path="unsupported"/>
            <durationUnitPattern alt="variant">ignored</durationUnitPattern>
            <durationUnitPattern draft="unconfirmed">selected h:mm</durationUnitPattern>
            <special/></durationUnit><special/></units></ldml>'''
        result = duration.collect(source, graph())
        self.assertEqual(result['skipped_alternates'], 4)
        self.assertEqual(self.values(source)['hm'], ('value', 'selected h:mm', 'en', 'hm'))

    def test_reject_ambiguous_or_unsupported_structures(self):
        cases = [
            pattern('hm', 'a') + pattern('hm', 'b'),
            pattern('unknown', 'h:mm'),
            '<durationUnit type="hm"><durationUnitPattern>a</durationUnitPattern><durationUnitPattern>b</durationUnitPattern></durationUnit>',
            '<durationUnit type="hm"><unknown/></durationUnit>',
            '<durationUnit type="hm">text<durationUnitPattern>h:mm</durationUnitPattern></durationUnit>',
            '<durationUnit type="hm"><durationUnitPattern validSubLocales="en">h:mm</durationUnitPattern></durationUnit>',
            '<durationUnit type="hm"><alias source="locale" path="../durationUnit[@type=\'ms\']"/><durationUnitPattern>h:mm</durationUnitPattern></durationUnit>',
            alias('hm', 'invalid'), alias('hm', 'ms', 'zz'),
            '<alias source="locale" path="../units"/>',
            '<unknown/>',
        ]
        for content in cases:
            source = Sources()
            source.set('en', content)
            with self.subTest(content=content), self.assertRaises(DataError):
                duration.collect(source, graph())
        for content in ('<units/><units/>', '<units validSubLocales="en"/>'):
            source.files['common/main/en.xml'] = ('<ldml>' + content + '</ldml>').encode('utf-8')
            with self.subTest(content=content), self.assertRaises(DataError):
                duration.collect(source, graph())

    def test_alias_parent_cycles(self):
        source = Sources()
        source.set('en', alias('hm', 'ms') + alias('ms', 'hm'))
        with self.assertRaisesRegex(DataError, 'cycle'):
            duration.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
