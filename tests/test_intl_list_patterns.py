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
    def test_contextual_replacements_require_matching_individual_templates(self):
        cases = (
            ('es-MX', 'standard', ('{0} y {1}', '{0}{1}', '{0}{1}', '{0} & {1}'),
             (1, '{0} e {1}', '{0} & {1}')),
            ('es', 'unit', ('{0} & {1}', '{0}{1}', '{0}{1}', '{0} y {1}'),
             (1, '{0} & {1}', '{0} e {1}')),
            ('es', 'or', ('{0} o {1}', '{0}{1}', '{0}{1}', '{0} o {1}'),
             (2, '{0} u {1}', '{0} u {1}')),
            ('he-IL', 'standard', ('{0} ו{1}', '{0}{1}', '{0}{1}', '{0} ו{1}'),
             (3, '{0} ו-{1}', '{0} ו-{1}')),
            ('es', 'standard-narrow', ('{0}, {1}',) * 4, (0, '', '')),
            ('es', 'or', ('{0} y {1}',) * 4, (0, '', '')),
            ('en', 'standard', ('{0} y {1}',) * 4, (0, '', '')),
            ('he', 'or', ('{0} או {1}',) * 4, (0, '', '')),
        )
        for locale, variant, values, expected in cases:
            self.assertEqual(lists.context(locale, variant, values), expected)

    def test_resolved_contexts_follow_inherited_request_locale_patterns(self):
        sources = Sources()
        sources.set('es', ET.fromstring('''<ldml><listPatterns><listPattern>
            <listPatternPart type="2">{0} y {1}</listPatternPart>
        </listPattern></listPatterns></ldml>'''))
        sources.set('es_MX', ET.Element('ldml'))
        expanded = ParentGraph(graph().tags + ('es', 'es-MX'), {}, {},
                               {'und': 'en-Latn-US'}, (), False)
        result = lists.collect(sources, expanded)
        contexts = {(locale, variant): (kind, pair, end)
                    for locale, variant, kind, pair, end in result['contexts']}
        self.assertEqual(contexts[('es-MX', 'standard')], (1, '{0} e {1}', ' {0}\u00a0& {1} '))
        self.assertEqual(contexts[('es-MX', 'unit')], (1, '{0} e {1}', ' {0}\u00a0& {1} '))
        self.assertEqual(contexts[('es-MX', 'or')], (0, '', ''))

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
