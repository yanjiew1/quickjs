#!/usr/bin/env python3
"""Authored shared date-field labels and requesting-context width aliases."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import date_field_names as names
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph

DTD = b'''<!ELEMENT ldml ANY><!ELEMENT dates ANY><!ELEMENT fields ANY>
<!ATTLIST fields validSubLocales NMTOKENS #IMPLIED><!--@VALUE-->
<!ELEMENT field ANY><!ATTLIST field type NMTOKEN #REQUIRED>
<!ATTLIST field alt NMTOKENS #IMPLIED>
<!ATTLIST field draft NMTOKEN #IMPLIED><!--@METADATA-->
<!ELEMENT displayName (#PCDATA)><!ATTLIST displayName alt NMTOKENS #IMPLIED>
<!ATTLIST displayName count NMTOKEN #IMPLIED>
<!ATTLIST displayName draft NMTOKEN #IMPLIED><!--@METADATA-->
<!ATTLIST displayName references CDATA #IMPLIED><!--@METADATA-->
<!ELEMENT alias EMPTY><!ATTLIST alias source NMTOKEN #REQUIRED><!--@VALUE-->
<!ATTLIST alias path CDATA #REQUIRED><!--@VALUE-->
<!ATTLIST alias alt NMTOKENS #IMPLIED>
<!ELEMENT relative ANY><!ELEMENT relativeTime ANY><!ELEMENT relativePeriod ANY>
<!ELEMENT special ANY><!ELEMENT unknown ANY>'''


def field(code, text):
    return '<field type="' + code + '"><displayName>' + text + '</displayName></field>'


def alias(code, target, source='locale'):
    return ('<field type="' + code + '"><alias source="' + source +
            '" path="../field[@type=\'' + target + '\']"/></field>')


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldml.dtd': DTD}
        for locale in ('root', 'en', 'en_GB', 'de'):
            self.set(locale, '')

    def set(self, locale, content):
        self.files['common/main/' + locale + '.xml'] = (
            '<ldml><dates><fields>' + content + '</fields></dates></ldml>').encode('utf-8')

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))


def values(source, locale='en'):
    return {row[1:4]: row[4:] for row in names.collect(source, graph())['rows'] if row[0] == locale}


class DateFieldNamesTests(unittest.TestCase):
    def test_all_ecma_fields_map_to_exact_source_keys(self):
        source = Sources()
        source.set('root', ''.join(field(raw + ('' if width == 'long' else '-' + width),
                                               name + ':' + width)
                                   for name, raw in names.SOURCE_NAMES.items() for width in names.WIDTHS))
        data = names.collect(source, graph())
        self.assertEqual(len(data['rows']), 4 * 12 * 3)
        self.assertEqual(data['source_records'], 36)
        result = values(source)
        for name in names.FIELDS:
            for width in names.WIDTHS:
                self.assertEqual(result[name, width, ''],
                                 ('value', name + ':' + width, 'root', name, width, ''))
        self.assertEqual(data, names.collect(source, graph()))

    def test_inherited_width_alias_restarts_at_requesting_locale(self):
        source = Sources()
        source.set('root', field('year', 'root year') + alias('year-short', 'year') +
                   alias('year-narrow', 'year-short'))
        source.set('en_GB', field('year', 'British year'))
        result = values(source, 'en-GB')
        self.assertEqual(result['year', 'narrow', ''],
                         ('value', 'British year', 'en-GB', 'year', 'long', ''))
        self.assertEqual(values(source, 'de')['year', 'narrow', ''],
                         ('value', 'root year', 'root', 'year', 'long', ''))

    def test_explicit_source_alias(self):
        source = Sources()
        source.set('en', field('zone', 'time zone'))
        source.set('de', alias('zone-short', 'zone', 'en'))
        self.assertEqual(values(source, 'de')['timeZoneName', 'short', ''],
                         ('value', 'time zone', 'en', 'timeZoneName', 'long', ''))

    def test_alternates_are_independent_and_follow_aliases(self):
        source = Sources()
        source.set('root', alias('dayperiod-short', 'dayperiod'))
        source.set('en', '''<field type="dayperiod"><displayName> day period </displayName>
            <displayName alt="variant" draft="unconfirmed">am/pm</displayName></field>''')
        data = names.collect(source, graph())
        self.assertEqual(data['alternates'], ('', 'variant'))
        self.assertEqual(values(source, 'en-GB')['dayPeriod', 'short', 'variant'],
                         ('value', 'am/pm', 'en', 'dayPeriod', 'long', 'variant'))
        self.assertEqual(values(source, 'en-GB')['dayPeriod', 'short', ''],
                         ('value', ' day period ', 'en', 'dayPeriod', 'long', ''))

    def test_missing_blocked_empty_and_inherit_are_distinct(self):
        source = Sources()
        source.set('root', field('year', 'year'))
        source.set('en', field('year', INHERIT) + field('month', BLOCK) + field('era', ''))
        result = values(source, 'en-GB')
        self.assertEqual(result['year', 'long', ''], ('value', 'year', 'root', 'year', 'long', ''))
        self.assertEqual(result['month', 'long', ''], ('blocked', '', 'en', 'month', 'long', ''))
        self.assertEqual(result['era', 'long', ''], ('value', '', 'en', 'era', 'long', ''))
        self.assertEqual(result['quarter', 'long', ''], ('missing', '', '', '', '', ''))

    def test_relative_patterns_are_owned_separately(self):
        source = Sources()
        source.set('en', '''<field type="year"><displayName>year</displayName>
            <relative><unknown/></relative><relativeTime><unknown/></relativeTime>
            <relativePeriod><unknown/></relativePeriod></field>
            <field type="sun"><unknown/></field>''')
        self.assertEqual(names.collect(source, graph())['source_records'], 1)

    def test_reject_duplicate_or_unsupported_structures(self):
        cases = (
            field('year', 'a') + field('year', 'b'),
            '<field type="year"><displayName>a</displayName><displayName draft="contributed">b</displayName></field>',
            '<field type="year" alt="variant"/>',
            '<field type="year"><displayName count="one">year</displayName></field>',
            '<field type="year"><displayName><unknown/></displayName></field>',
            '<field type="year">text<displayName>year</displayName></field>',
            '<field type="year"><unknown/></field>',
            '<field type="year"><alias source="locale" path="../field[@type=\'sun\']"/></field>',
            '<field type="year"><alias source="en" path="unsupported"/></field>',
            '<field type="year"><alias source="de" path="../field[@type=\'year\']"/><displayName>year</displayName></field>',
            alias('year', 'month', 'zz'),
            '<alias source="locale" path="../fields"/>',
        )
        for content in cases:
            source = Sources()
            source.set('en', content)
            with self.subTest(content=content), self.assertRaises(DataError):
                names.collect(source, graph())
        for content in ('<dates/><dates/>', '<dates><fields/><fields/></dates>',
                        '<dates><fields validSubLocales="en"/></dates>',
                        '<dates><alias source="locale" path="../dates"/></dates>'):
            source.files['common/main/en.xml'] = ('<ldml>' + content + '</ldml>').encode('utf-8')
            with self.subTest(content=content), self.assertRaises(DataError):
                names.collect(source, graph())

    def test_alias_cycles_are_rejected(self):
        source = Sources()
        source.set('en', alias('year', 'year-short') + alias('year-short', 'year'))
        with self.assertRaisesRegex(DataError, 'cycle'):
            names.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
