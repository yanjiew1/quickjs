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

    def overlays(self, data, locale, width, kind, name):
        head = next(index for tag, w, k, n, index in data['overlay_rows']
                    if (tag, w, k, n) == (locale, width, kind, name))
        result = []
        while head:
            origin, source_width, source_kind, source_name, index, tail = data['overlay_nodes'][head - 1]
            self.assertIn((source_kind, source_name), data['groups'])
            self.assertIn(source_width, units.WIDTHS)
            self.assertLess(tail, head)
            result.append({(field, count, case): text
                           for field, count, case, text in data['overlay_maps'][index]})
            head = tail
        return result

    def test_overlay_lateral_candidates_precede_inherited_exact_leaves(self):
        source = Sources()
        source.set('en', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="one">PARENT_ONE {0}</unitPattern>
          <unitPattern count="one" case="genitive">PARENT_GENITIVE {0}</unitPattern>
          </unit></unitLength>''')
        source.set('en_GB', '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="other">CHILD_OTHER {0}</unitPattern>
          <unitPattern count="one" case="genitive">''' + INHERIT + '''</unitPattern>
          <unitPattern count="two"/><unitPattern count="few">''' + BLOCK + '''</unitPattern>
          </unit></unitLength>''')
        data = units.collect(source, graph())
        layers = self.overlays(data, 'en-GB', 'long', 'unit', 'length-meter')
        self.assertEqual(layers[0], {('unitPattern', 'other', ''): 'CHILD_OTHER {0}',
                                     ('unitPattern', 'two', ''): '', ('unitPattern', 'few', ''): BLOCK})
        self.assertEqual(layers[1][('unitPattern', 'one', '')], 'PARENT_ONE {0}')
        self.assertEqual(layers[1][('unitPattern', 'one', 'genitive')], 'PARENT_GENITIVE {0}')
        for row in data['overlay_rows']:
            flattened = {}
            for layer in reversed(self.overlays(data, *row[:-1])):
                flattened.update(layer)
            self.assertEqual(flattened, self.values(data, *row[:-1]))
        self.assertEqual(tuple(sorted(set(data['overlay_maps']))), data['overlay_maps'])
        self.assertEqual(len(set(data['overlay_nodes'])), len(data['overlay_nodes']))

    def test_overlay_aliases_preserve_original_child_before_restart(self):
        source = Sources()
        source.set('en', '''<unitLength type="short"><unit type="length-meter">
          <unitPattern count="one">ENGLISH_SHORT {0}</unitPattern></unit></unitLength>''')
        source.set('en_GB', '''<unitLength type="narrow"><unit type="length-meter">
          <unitPattern count="other">BRITISH_NARROW {0}</unitPattern></unit></unitLength>''')
        source.set('de', '''<unitLength type="narrow"><alias source="en"
          path="../unitLength[@type='short']"/></unitLength>''')
        data = units.collect(source, graph())
        layers = self.overlays(data, 'en-GB', 'narrow', 'unit', 'length-meter')
        self.assertEqual(layers[0], {('unitPattern', 'other', ''): 'BRITISH_NARROW {0}'})
        self.assertEqual(layers[1], {('unitPattern', 'one', ''): 'ENGLISH_SHORT {0}'})
        self.assertEqual(self.overlays(data, 'de', 'narrow', 'unit', 'length-meter'), layers[1:])
        head = next(row[-1] for row in data['overlay_rows']
                    if row[:-1] == ('en-GB', 'narrow', 'unit', 'length-meter'))
        node = data['overlay_nodes'][head - 1]
        self.assertEqual(node[:4], ('en-GB', 'narrow', 'unit', 'length-meter'))
        self.assertEqual(data['overlay_nodes'][node[-1] - 1][:4],
                         ('en', 'short', 'unit', 'length-meter'))

    def test_overlay_deduplication_retains_identical_text_origins(self):
        source = Sources()
        content = '''<unitLength type="long"><unit type="length-meter">
          <unitPattern count="other">IDENTICAL {0}</unitPattern></unit></unitLength>'''
        source.set('en', content)
        source.set('de', content)
        data = units.collect(source, graph())
        heads = [next(row[-1] for row in data['overlay_rows']
                      if row[:-1] == (locale, 'long', 'unit', 'length-meter'))
                 for locale in ('en', 'de')]
        self.assertNotEqual(*heads)
        nodes = [data['overlay_nodes'][head - 1] for head in heads]
        self.assertEqual([node[0] for node in nodes], ['en', 'de'])
        self.assertEqual(nodes[0][1:], nodes[1][1:])

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
