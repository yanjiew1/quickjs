#!/usr/bin/env python3
"""Typed numeric symbols: exact text, aliases, inheritance and missing values."""
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import number_symbols as symbols
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph
from test_intl_numbering import Sources as NumberingSources


class Sources(NumberingSources):
    def __init__(self):
        super().__init__()
        self.files['common/dtd/ldml.dtd'] += b'''<!ELEMENT symbols ANY>
          <!ATTLIST symbols numberSystem CDATA #IMPLIED>
          <!ATTLIST symbols alt NMTOKENS #IMPLIED>
          <!ATTLIST alias path CDATA #IMPLIED>
          <!ATTLIST alias alt NMTOKENS #IMPLIED>'''
        for name in symbols.FIELDS:
            self.files['common/dtd/ldml.dtd'] += ('''<!ELEMENT {0} (#PCDATA)>
              <!ATTLIST {0} alt NMTOKENS #IMPLIED>
              <!ATTLIST {0} numberSystem CDATA #IMPLIED>'''.format(name)).encode('ascii')
        root = ET.fromstring(self.files['common/main/root.xml'])
        numbers = root.find('numbers')
        group = ET.SubElement(numbers, 'symbols', numberSystem='latn')
        for field in symbols.CORE_FIELDS:
            ET.SubElement(group, field).text = {'decimal': '.', 'group': '\u00a0'}.get(field, field)
        group = ET.SubElement(numbers, 'symbols', numberSystem='mathbold')
        ET.SubElement(group, 'alias', source='locale', path="../symbols[@numberSystem='latn']")
        self.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')


class SymbolTests(unittest.TestCase):
    def rows(self, sources):
        return {(locale, system): dict(zip(symbols.FIELDS, values))
                for locale, system, values in symbols.collect(sources, graph())['rows']}

    def test_exact_text_inheritance_and_requesting_locale_alias(self):
        sources = Sources()
        sources.set('en', '<ldml><numbers><symbols numberSystem="latn">'
                    '<decimal>,</decimal><group>  </group><plusSign/>'
                    '</symbols></numbers></ldml>')
        rows = self.rows(sources)
        for system in ('latn', 'mathbold'):
            self.assertEqual(rows[('en-GB', system)]['decimal'], ',')
            self.assertEqual(rows[('en-GB', system)]['group'], '  ')
            self.assertEqual(rows[('en-GB', system)]['plusSign'], '')
            self.assertIsNone(rows[('en-GB', system)]['currencyDecimal'])
        self.assertEqual(rows[('de', 'latn')]['group'], '\u00a0')
        self.assertEqual(len(rows), 8)

    def test_optional_block_inheritance_and_explicit_source_alias(self):
        sources = Sources()
        sources.set('en', '<ldml><numbers><symbols numberSystem="latn">'
                    '<currencyDecimal>;</currencyDecimal><currencyGroup>:</currencyGroup>'
                    '</symbols></numbers></ldml>')
        sources.set('en_GB', '<ldml><numbers><symbols numberSystem="latn">'
                    '<currencyDecimal>' + INHERIT + '</currencyDecimal>'
                    '<currencyGroup>' + BLOCK + '</currencyGroup>'
                    '</symbols></numbers></ldml>')
        sources.set('de', '<ldml><numbers><symbols numberSystem="mathbold">'
                    '<alias source="en" path="../symbols[@numberSystem=\'latn\']"/>'
                    '</symbols></numbers></ldml>')
        rows = self.rows(sources)
        self.assertEqual(rows[('en-GB', 'mathbold')]['currencyDecimal'], ';')
        self.assertEqual(rows[('en-GB', 'mathbold')]['currencyGroup'], BLOCK)
        self.assertEqual(rows[('de', 'mathbold')]['currencyDecimal'], ';')

    def test_double_quoted_alias_path(self):
        sources = Sources()
        root = ET.fromstring(sources.files['common/main/root.xml'])
        alias = root.find("./numbers/symbols[@numberSystem='mathbold']/alias")
        alias.set('path', '../symbols[@numberSystem="latn"]')
        sources.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        self.assertEqual(self.rows(sources)[('en', 'mathbold')]['decimal'], '.')

    def test_alternates_do_not_override_or_create_duplicates(self):
        sources = Sources()
        sources.set('en', '''<ldml>
          <numbers alt="variant"><symbols numberSystem="latn"><decimal>bad</decimal></symbols></numbers>
          <numbers><alias source="root" alt="variant"/>
          <symbols numberSystem="latn" alt="variant"><decimal>bad</decimal></symbols>
          <symbols numberSystem="latn"><decimal alt="variant">bad</decimal><decimal>,</decimal></symbols>
          </numbers></ldml>''')
        result = symbols.collect(sources, graph())
        self.assertEqual(result['skipped_alternates'], 4)
        self.assertEqual(self.rows(sources)[('en', 'mathbold')]['decimal'], ',')

    def test_malformed_duplicate_unknown_and_cyclic_fields(self):
        invalid = (
            '<symbols><decimal>.</decimal></symbols>',
            '<symbols numberSystem="roman"/>',
            '<symbols numberSystem="unknown"/>',
            '<symbols numberSystem="latn"/><symbols numberSystem="latn"/>',
            '<symbols numberSystem="latn"><decimal>.</decimal><decimal>,</decimal></symbols>',
            '<symbols numberSystem="latn"><unknown>.</unknown></symbols>',
            '<symbols numberSystem="latn"><decimal><unknown/></decimal></symbols>',
            '<symbols numberSystem="latn">junk<decimal>.</decimal></symbols>',
            '<symbols numberSystem="latn">\u00a0<decimal>.</decimal></symbols>',
            '<symbols numberSystem="latn"><decimal>.</decimal>junk</symbols>',
            '<symbols numberSystem="latn"><decimal numberSystem="mathbold">.</decimal></symbols>',
            '<symbols numberSystem="latn"><alias source="locale" path="../unknown"/></symbols>',
            '<symbols numberSystem="latn"><alias source="locale"/></symbols>',
            '<symbols numberSystem="latn"><alias path="../symbols[@numberSystem=\'latn\']"/></symbols>',
            '<symbols numberSystem="latn"><alias source="locale" path="../symbols[@numberSystem=\'latn&quot;]"/></symbols>',
            '<symbols numberSystem="latn"><alias source="locale" path="../symbols[@numberSystem=\'latn\']"><decimal>.</decimal></alias></symbols>',
            '<symbols numberSystem="latn"><alias source="locale" path="../symbols[@numberSystem=\'latn\']"/><alias source="locale" path="../symbols[@numberSystem=\'latn\']"/></symbols>',
            '<symbols numberSystem="latn"><alias source="locale" path="../symbols[@numberSystem=\'roman\']"/></symbols>',
            '<symbols numberSystem="latn"><alias source="unknown" path="../symbols[@numberSystem=\'latn\']"/></symbols>',
            '<symbols numberSystem="latn"><alias source="locale" path="../symbols[@numberSystem=\'mathbold\']"/></symbols>',
            '<symbols numberSystem="latn"><alias source="locale" path="../symbols[@numberSystem=\'latn\']"/><decimal>.</decimal></symbols>',
            '<symbols numberSystem="latn"><alias source="locale" path="../symbols[@numberSystem=\'latn\']">junk</alias></symbols>',
            '<symbols numberSystem="latn"><decimal>' + BLOCK + '</decimal></symbols>',
            '<alias source="root"/>',
        )
        for xml in invalid:
            sources = Sources()
            sources.set('en', '<ldml><numbers>' + xml + '</numbers></ldml>')
            with self.assertRaises(DataError, msg=xml):
                symbols.collect(sources, graph())
        sources = Sources()
        sources.set('en', '<ldml><numbers/><numbers/></ldml>')
        with self.assertRaises(DataError):
            symbols.collect(sources, graph())
        sources = Sources()
        sources.set('root', '<ldml/>')
        with self.assertRaises(DataError):
            symbols.collect(sources, graph())


if __name__ == '__main__':
    unittest.main()
