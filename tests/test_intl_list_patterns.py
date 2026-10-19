#!/usr/bin/env python3
"""Authored ListFormat collector fixtures, with no downloaded data."""
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

TOOLS = Path(__file__).resolve().parents[1] / 'tools' / 'intl'
sys.path.insert(0, str(TOOLS))
from cldr_xml import DataError
import list_patterns as lists
from parents import ParentGraph
from resolve import BLOCK, INHERIT

DTD = b'''<!ELEMENT ldml ANY><!ELEMENT listPatterns ANY><!ELEMENT listPattern ANY>
<!ATTLIST listPatterns alt NMTOKENS #IMPLIED>
<!ATTLIST listPattern type NMTOKEN #IMPLIED>
<!ATTLIST listPattern alt NMTOKENS #IMPLIED>
<!ELEMENT listPatternPart (#PCDATA)>
<!ATTLIST listPatternPart type (2 | start | middle | end) #REQUIRED>
<!ATTLIST listPatternPart alt NMTOKENS #IMPLIED>
<!ELEMENT alias EMPTY>
<!ATTLIST alias source NMTOKEN #REQUIRED><!--@VALUE-->
<!ATTLIST alias path CDATA #IMPLIED><!--@VALUE-->
<!ATTLIST alias alt NMTOKENS #IMPLIED>
'''


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldml.dtd': DTD}
        root = ET.Element('ldml')
        container = ET.SubElement(root, 'listPatterns')
        standard = ET.SubElement(container, 'listPattern')
        for part in lists.PARTS:
            ET.SubElement(standard, 'listPatternPart', type=part).text = ' {0}\u00a0& {1} '
        for variant in lists.VARIANTS[1:]:
            group = ET.SubElement(container, 'listPattern', type=variant)
            ET.SubElement(group, 'alias', source='locale', path="../listPattern")
        self.set('root', root)
        for locale in ('en', 'en_GB', 'de'):
            self.set(locale, ET.Element('ldml'))

    def set(self, locale, root):
        self.files['common/main/' + locale + '.xml'] = ET.tostring(root, encoding='utf-8')

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(sorted(name for name in self.files if name.startswith(prefix)))


def graph():
    return ParentGraph(('root', 'en', 'en-GB', 'de'), {}, {},
                       {'und': 'en-Latn-US'}, (), False)


class ListPatternTests(unittest.TestCase):
    def test_alias_uses_requested_locale_and_exact_text(self):
        sources = Sources()
        sources.set('en', ET.fromstring('''<ldml><listPatterns><listPattern>
            <listPatternPart type="2"> {1} ; {0} </listPatternPart>
        </listPattern></listPatterns></ldml>'''))
        result = lists.collect(sources, graph())
        rows = {(locale, variant): values for locale, variant, values in result['rows']}
        self.assertEqual(len(rows), 4 * 9)
        for variant in lists.VARIANTS:
            self.assertEqual(rows[('en-GB', variant)][0], ' {1} ; {0} ')
            self.assertEqual(rows[('en-GB', variant)][1:], (' {0}\u00a0& {1} ',) * 3)

    def test_default_alias_path_and_explicit_locale(self):
        sources = Sources()
        sources.set('en', ET.fromstring('''<ldml><listPatterns><listPattern>
            <alias source="de"/>
        </listPattern></listPatterns></ldml>'''))
        sources.set('de', ET.fromstring('''<ldml><listPatterns><listPattern>
            <listPatternPart type="2">{0} plus {1}</listPatternPart>
        </listPattern></listPatterns></ldml>'''))
        rows = lists.collect(sources, graph())['rows']
        self.assertEqual(next(values for locale, variant, values in rows
                              if (locale, variant) == ('en-GB', 'unit')), ('{0} plus {1}',) + (' {0}\u00a0& {1} ',) * 3)

    def test_selection_and_explicit_inheritance(self):
        sources = Sources()
        sources.set('en', ET.fromstring('''<ldml><listPatterns><listPattern>
            <listPatternPart type="2">''' + INHERIT + '''</listPatternPart>
            <alias source="en" alt="variant"/>
        </listPattern><listPattern alt="variant"/></listPatterns></ldml>'''))
        result = lists.collect(sources, graph())
        self.assertEqual(result['skipped_alternates'], 2)
        self.assertEqual(result['rows'][0][2][0], ' {0}\u00a0& {1} ')

    def test_invalid_data_is_not_replaced_with_default_text(self):
        invalid = (
            '<listPattern type="unknown"/>',
            '<listPattern/><listPattern/>',
            '<listPattern><listPatternPart type="2">{0} and {2}</listPatternPart></listPattern>',
            '<listPattern><listPatternPart type="2">' + BLOCK + '</listPatternPart></listPattern>',
            '<listPattern><alias source="locale"/></listPattern>',
            '<listPattern><alias source="locale" path="../unknown"/></listPattern>',
            '<listPattern><alias source="locale" path="../listPattern[@type=\'or&quot;]"/></listPattern>',
            '<listPattern><alias source="locale"/><listPatternPart type="2">{0}{1}</listPatternPart></listPattern>',
            '<listPattern><listPatternPart type="2">{0}{1}</listPatternPart><listPatternPart type="2">{0}{1}</listPatternPart></listPattern>',
        )
        for group in invalid:
            sources = Sources()
            sources.set('en', ET.fromstring('<ldml><listPatterns>' + group + '</listPatterns></ldml>'))
            with self.assertRaises(DataError):
                lists.collect(sources, graph())

    def test_alternate_containers_do_not_supply_standard_data(self):
        for standard in ('', '''<listPatterns><listPattern>
                <listPatternPart type="2">{0} plus {1}</listPatternPart>
                </listPattern></listPatterns>'''):
            sources = Sources()
            sources.set('en', ET.fromstring('''<ldml><listPatterns alt="variant">
                <listPattern><listPatternPart type="2">{0} alternate {1}</listPatternPart>
                </listPattern></listPatterns>''' + standard + '</ldml>'))
            result = lists.collect(sources, graph())
            self.assertEqual(result['skipped_alternates'], 1)
            rows = {(locale, variant): values for locale, variant, values in result['rows']}
            expected = '{0} plus {1}' if standard else ' {0}\u00a0& {1} '
            self.assertEqual(rows[('en-GB', 'standard')][0], expected)


if __name__ == '__main__':
    unittest.main()
