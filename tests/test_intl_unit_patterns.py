#!/usr/bin/env python3
"""Unit width/group aliases, plural/case identity, exact states and bad scopes."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from resolve import BLOCK, INHERIT
import unit_patterns as units
from test_intl_list_patterns import graph


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldml.dtd': b'''<!ELEMENT ldml ANY><!ELEMENT units ANY>
          <!ATTLIST units alt NMTOKENS #IMPLIED><!ELEMENT unitLength ANY>
          <!ATTLIST unitLength type (long | short | narrow) #REQUIRED>
          <!ATTLIST unitLength alt NMTOKENS #IMPLIED>
          <!ELEMENT unit ANY><!ATTLIST unit type NMTOKEN #REQUIRED>
          <!ATTLIST unit alt NMTOKENS #IMPLIED><!ELEMENT compoundUnit ANY>
          <!ATTLIST compoundUnit type NMTOKEN #REQUIRED>
          <!ATTLIST compoundUnit alt NMTOKENS #IMPLIED>
          <!ELEMENT displayName (#PCDATA)><!ATTLIST displayName alt NMTOKENS #IMPLIED>
          <!ELEMENT gender (#PCDATA)><!ATTLIST gender alt NMTOKENS #IMPLIED>
          <!ELEMENT unitPattern (#PCDATA)><!ATTLIST unitPattern count NMTOKEN #REQUIRED>
          <!ATTLIST unitPattern case NMTOKENS #IMPLIED>
          <!ATTLIST unitPattern alt NMTOKENS #IMPLIED>
          <!ATTLIST unitPattern draft NMTOKEN #IMPLIED><!--@METADATA-->
          <!ELEMENT perUnitPattern (#PCDATA)><!ELEMENT compoundUnitPattern (#PCDATA)>
          <!ATTLIST compoundUnitPattern case NMTOKENS #IMPLIED>
          <!ELEMENT alias EMPTY><!ATTLIST alias source NMTOKEN #REQUIRED>
          <!ATTLIST alias path CDATA #IMPLIED><!ELEMENT unknown ANY>'''}
        for locale in ('en', 'en_GB', 'de'):
            self.set(locale, '')
        self.set('root', '''<unitLength type="long">
          <unit type="length-meter"><displayName>meters</displayName><gender>masculine</gender>
            <unitPattern count="one">{0} meter</unitPattern><unitPattern count="other">{0} meters</unitPattern>
            <unitPattern count="one" case="genitive">{0} meter-gen</unitPattern>
            <perUnitPattern>{0} per meter</perUnitPattern></unit>
          <unit type="duration-second"><displayName>seconds</displayName>
            <unitPattern count="other">{0} seconds</unitPattern></unit>
          <compoundUnit type="per"><compoundUnitPattern>{0}/{1}</compoundUnitPattern></compoundUnit>
          </unitLength><unitLength type="short"><unit type="length-meter">
            <displayName>m</displayName><unitPattern count="other">{0} m</unitPattern></unit>
          </unitLength><unitLength type="narrow"><alias source="locale"
            path="../unitLength[@type='short']"/></unitLength>''')

    def set(self, locale, content):
        self.files['common/main/' + locale + '.xml'] = ('<ldml><units>' + content + '</units></ldml>').encode('utf-8')

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))


class UnitPatternTests(unittest.TestCase):
    def values(self, data, locale, width, kind, name):
        index = next(index for tag, w, k, n, index in data['rows']
                     if (tag, w, k, n) == (locale, width, kind, name))
        return {(field, count, case): text for field, count, case, text in data['maps'][index]}

    def test_plural_case_gender_division_and_exact_states(self):
        source = Sources()
        source.set('en', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="one"> {0} meter </unitPattern>
          <unitPattern count="one" case="genitive">''' + BLOCK + '''</unitPattern>
          <unitPattern count="two"/><gender>common</gender></unit></unitLength>''')
        source.set('en_GB', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="one">''' + INHERIT + '''</unitPattern></unit></unitLength>''')
        data = units.collect(source, graph())
        values = self.values(data, 'en-GB', 'long', 'unit', 'length-meter')
        self.assertEqual(values[('unitPattern', 'one', '')], ' {0} meter ')
        self.assertEqual(values[('unitPattern', 'one', 'genitive')], BLOCK)
        self.assertEqual(values[('unitPattern', 'two', '')], '')
        self.assertEqual(values[('gender', '', '')], 'common')
        self.assertNotIn(('unitPattern', 'few', ''), values)
        self.assertEqual(values[('perUnitPattern', '', '')], '{0} per meter')
        self.assertEqual(self.values(data, 'en', 'long', 'compound', 'per'),
                         {('compoundUnitPattern', '', ''): '{0}/{1}'})

    def test_requesting_width_and_unit_aliases(self):
        source = Sources()
        source.set('en', '''<unitLength type="short"><unit type="length-meter">
          <unitPattern count="other">{0} mt</unitPattern></unit></unitLength>
          <unitLength type="long"><unit type="duration-second-person">
          <alias source="locale" path="../unit[@type='duration-second']"/></unit></unitLength>''')
        source.set('en_GB', '''<unitLength type="long"><unit type="duration-second">
          <unitPattern count="other">{0} seconds-local</unitPattern></unit></unitLength>''')
        data = units.collect(source, graph())
        self.assertEqual(self.values(data, 'en-GB', 'narrow', 'unit', 'length-meter')[('unitPattern', 'other', '')], '{0} mt')
        self.assertEqual(self.values(data, 'en-GB', 'long', 'unit', 'duration-second-person')[('unitPattern', 'other', '')], '{0} seconds-local')
        self.assertEqual(self.values(data, 'root', 'long', 'unit', 'duration-second-person'), {})

    def test_explicit_source_and_alternates(self):
        source = Sources()
        source.set('en', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="other" draft="unconfirmed">{0} English meters</unitPattern>
          <unitPattern count="other" alt="variant">ignored</unitPattern></unit></unitLength>''')
        source.set('de', '''<unitLength type="long"><unit type="length-meter">
          <alias source="en" path="../unit[@type='length-meter']"/></unit>
          <unit type="length-mile" alt="variant"><unknown/></unit></unitLength>
          <unitLength type="short" alt="variant"><unknown/></unitLength>''')
        data = units.collect(source, graph())
        self.assertEqual(self.values(data, 'de', 'long', 'unit', 'length-meter')[('unitPattern', 'other', '')], '{0} English meters')
        self.assertNotIn(('unit', 'length-mile'), data['groups'])
        self.assertEqual(data['skipped_alternates'], 3)

    def test_explicit_width_source_and_inherited_cross_width_cycle(self):
        source = Sources()
        source.set('en', '''<unitLength type="short"><unit type="length-meter">
          <unitPattern count="other">{0} English</unitPattern></unit></unitLength>''')
        source.set('de', '''<unitLength type="narrow"><alias source="en"
          path="../unitLength[@type='short']"/></unitLength>''')
        data = units.collect(source, graph())
        self.assertEqual(self.values(data, 'de', 'narrow', 'unit', 'length-meter')[('unitPattern', 'other', '')], '{0} English')
        source.set('en', '''<unitLength type="short"><alias source="locale"
          path="../unitLength[@type='narrow']"/></unitLength>''')
        with self.assertRaisesRegex(DataError, 'alias/parent cycle'):
            units.collect(source, graph())

    def test_repeated_invalid_scopes_selectors_and_aliases(self):
        invalid = (
            '<unitLength type="long"/><unitLength type="long"/>',
            '<unitLength type="wide"/>',
            '<unitLength type="long"><unit type="bad identifier"/></unitLength>',
            '<unitLength type="long"><unit type="length-meter"/><unit type="length-meter"/></unitLength>',
            '<unitLength type="long"><unit type="length-meter"><unitPattern count="bad">x</unitPattern></unit></unitLength>',
            '<unitLength type="long"><unit type="length-meter"><unitPattern count="one" case="genitive dative">x</unitPattern></unit></unitLength>',
            '<unitLength type="long"><unit type="length-meter"><unitPattern count="one">x</unitPattern><unitPattern count="one">y</unitPattern></unit></unitLength>',
            '<unitLength type="long"><unit type="length-meter"><gender>bad gender</gender></unit></unitLength>',
            '<unitLength type="long"><unit type="length-meter"><displayName><unknown/></displayName></unit></unitLength>',
            '<unitLength type="long"><unit type="length-meter">text<displayName>x</displayName></unit></unitLength>',
            '<unitLength type="long"><alias source="locale" path="../unknown"/></unitLength>',
            '<unitLength type="long"><unit type="length-meter"><alias source="locale" path="../unit[@type=\'missing-unit\']"/></unit></unitLength>',
            '<unitLength type="long"><unit type="length-meter"><alias source="locale" path="../unit[@type=\'length-meter\']"/></unit></unitLength>',
            '<unitLength type="long"><unit type="length-meter"><alias source="locale" path="../unit[@type=\'duration-second\']"/><displayName>x</displayName></unit></unitLength>',
            '<unitLength type="long"><unknown/></unitLength>',
            '<alias source="locale" path="../units"/>',
        )
        for value in invalid:
            source = Sources()
            source.set('en', value)
            with self.assertRaises(DataError, msg=value):
                units.collect(source, graph())
        source = Sources()
        source.files['common/dtd/ldml.dtd'] += b'<!ATTLIST unitPattern gender NMTOKEN #IMPLIED>'
        source.set('en', '<unitLength type="long"><unit type="length-meter">'
                   '<unitPattern count="one" gender="masculine">x</unitPattern></unit></unitLength>')
        with self.assertRaises(DataError):
            units.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
