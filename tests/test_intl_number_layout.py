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

"""Number layout alias scope, sparse names, exact text and invalid structure."""
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import number_layout as layout
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph
from test_intl_number_patterns import Sources as PatternSources


class Sources(PatternSources):
    def __init__(self):
        super().__init__()
        self.files['common/dtd/ldml.dtd'] += b'''<!ELEMENT miscPatterns ANY>
          <!ATTLIST miscPatterns numberSystem NMTOKEN #IMPLIED>
          <!ATTLIST miscPatterns alt NMTOKENS #IMPLIED>
          <!ELEMENT unitPattern (#PCDATA)><!ATTLIST unitPattern count NMTOKEN #REQUIRED>
          <!ATTLIST unitPattern alt NMTOKENS #IMPLIED><!ATTLIST unitPattern case NMTOKEN #IMPLIED>
          <!ELEMENT currencyPatternAppendISO (#PCDATA)>
          <!ELEMENT currencySpacing ANY><!ATTLIST currencySpacing alt NMTOKENS #IMPLIED>
          <!ELEMENT beforeCurrency ANY><!ELEMENT afterCurrency ANY>
          <!ELEMENT currencyMatch (#PCDATA)><!ELEMENT surroundingMatch (#PCDATA)>
          <!ELEMENT insertBetween (#PCDATA)><!ELEMENT unknown ANY>'''
        root = ET.fromstring(self.files['common/main/root.xml'])
        numbers = root.find('numbers')
        currency = numbers.find("currencyFormats[@numberSystem='latn']")
        ET.SubElement(currency, 'unitPattern', count='other').text = '{0} {1}'
        ET.SubElement(currency, 'currencyPatternAppendISO').text = '{0} ¤¤'
        spacing = ET.SubElement(currency, 'currencySpacing')
        for side in layout.SIDES:
            child = ET.SubElement(spacing, side)
            for field, value in zip(layout.SPACING, ('[[:^S:]&[:^Z:]]', '[:digit:]', ' ')):
                ET.SubElement(child, field).text = value
        group = ET.SubElement(numbers, 'miscPatterns', numberSystem='latn')
        for field, value in zip(layout.MISC, ('{0}–{1}', '~{0}', '{0}+', '≤{0}')):
            ET.SubElement(group, 'pattern', type=field).text = value
        group = ET.SubElement(numbers, 'miscPatterns', numberSystem='mathbold')
        ET.SubElement(group, 'alias', source='locale', path="../miscPatterns[@numberSystem='latn']")
        self.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')


class NumberLayoutTests(unittest.TestCase):
    def rows(self, source):
        return {(locale, system): dict(zip(layout.FIELDS, values))
                for locale, system, values in layout.collect(source, graph())['rows']}

    def test_requesting_aliases_and_exact_boundary_text(self):
        source = Sources()
        source.set('en', '''<ldml><numbers>
          <miscPatterns numberSystem="latn"><pattern type="range">{0} à {1}</pattern></miscPatterns>
          <currencyFormats numberSystem="latn"><unitPattern count="one">{1} {0}</unitPattern>
            <currencySpacing><beforeCurrency><insertBetween> </insertBetween></beforeCurrency>
            </currencySpacing></currencyFormats></numbers></ldml>''')
        source.set('en_GB', '<ldml><numbers><miscPatterns numberSystem="latn">'
                   '<pattern type="range">' + INHERIT + '</pattern></miscPatterns></numbers></ldml>')
        rows = self.rows(source)
        for system in ('latn', 'mathbold'):
            values = rows[('en-GB', system)]
            self.assertEqual(values['range'], '{0} à {1}')
            self.assertEqual(values['approximately'], '~{0}')
            self.assertEqual(values['currency-unit-one'], '{1} {0}')
            self.assertEqual(values['beforeCurrency-insertBetween'], ' ')
            self.assertEqual(values['afterCurrency-insertBetween'], ' ')
            self.assertEqual(values['beforeCurrency-currencyMatch'], '[[:^S:]&[:^Z:]]')

    def test_explicit_alias_optional_states_and_no_invented_fallback(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <unitPattern count="one">{1}{0}</unitPattern></currencyFormats></numbers></ldml>''')
        source.set('de', '''<ldml><numbers><currencyFormats numberSystem="mathbold">
          <alias source="en" path="../currencyFormats[@numberSystem='latn']"/>
          </currencyFormats></numbers></ldml>''')
        source.set('en_GB', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <unitPattern count="one">''' + BLOCK + '''</unitPattern><unitPattern count="two"/>
          <currencyPatternAppendISO/></currencyFormats></numbers></ldml>''')
        rows = self.rows(source)
        self.assertEqual(rows[('de', 'mathbold')]['currency-unit-one'], '{1}{0}')
        self.assertEqual(rows[('en-GB', 'mathbold')]['currency-unit-one'], BLOCK)
        self.assertEqual(rows[('en-GB', 'latn')]['currency-unit-two'], '')
        self.assertEqual(rows[('en-GB', 'latn')]['currencyPatternAppendISO'], '')
        self.assertIsNone(rows[('en-GB', 'latn')]['currency-unit-few'])
        # Real root arab likewise has spacing/standard patterns without a name unitPattern.
        root = ET.fromstring(source.files['common/main/root.xml'])
        group = root.find("numbers/currencyFormats[@numberSystem='mathbold']")
        group.clear()
        group.set('numberSystem', 'mathbold')
        spacing = ET.SubElement(group, 'currencySpacing')
        ET.SubElement(spacing, 'alias', source='locale',
                      path="../../currencyFormats[@numberSystem='latn']/currencySpacing")
        source.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        rows = self.rows(source)
        self.assertIsNone(rows[('root', 'mathbold')]['currency-unit-other'])
        self.assertEqual(rows[('root', 'mathbold')]['afterCurrency-insertBetween'], ' ')

    def test_alternates_do_not_override_baseline(self):
        source = Sources()
        source.set('en', '''<ldml><numbers alt="variant"><unknown/></numbers><numbers>
          <miscPatterns numberSystem="latn" alt="variant"><unknown/></miscPatterns>
          <miscPatterns numberSystem="latn"><pattern type="range" alt="variant">bad</pattern>
          </miscPatterns><currencyFormats numberSystem="latn">
          <unitPattern count="other" alt="variant">bad</unitPattern>
          <currencySpacing alt="variant"><unknown/></currencySpacing>
          </currencyFormats></numbers></ldml>''')
        data = layout.collect(source, graph())
        self.assertEqual(data['skipped_alternates'], 5)
        self.assertEqual(self.rows(source)[('en', 'mathbold')]['range'], '{0}–{1}')

    def test_template_origin_tracks_terminal_alias_leaf(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <unitPattern count="other">{0} {1}</unitPattern></currencyFormats></numbers></ldml>''')
        source.set('de', '''<ldml><numbers><currencyFormats numberSystem="mathbold">
          <alias source="root" path="../currencyFormats[@numberSystem='latn']"/>
          </currencyFormats></numbers></ldml>''')
        data = layout.collect(source, graph())
        roots = {(locale, system): mask for locale, system, mask in data['root_fields']}
        other = 1 << layout.FIELDS.index('currency-unit-other')
        self.assertFalse(roots.get(('en', 'latn'), 0) & other)
        self.assertFalse(roots.get(('en', 'mathbold'), 0) & other)
        self.assertFalse(roots.get(('en-GB', 'mathbold'), 0) & other)
        self.assertTrue(roots[('de', 'mathbold')] & other)
        rows = self.rows(source)
        self.assertEqual(rows[('en', 'latn')]['currency-unit-other'],
                         rows[('de', 'mathbold')]['currency-unit-other'])

    def test_currency_template_projection_uses_each_parent_default(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><defaultNumberingSystem>mathbold</defaultNumberingSystem>
          <currencyFormats numberSystem="mathbold"><unitPattern count="one">M {0} {1}</unitPattern>
          </currencyFormats><currencyFormats numberSystem="latn">
          <unitPattern count="one">L {0} {1}</unitPattern></currencyFormats></numbers></ldml>''')
        source.set('en_GB', '''<ldml><numbers><defaultNumberingSystem>latn</defaultNumberingSystem>
          <currencyFormats numberSystem="latn"><unitPattern count="one">''' + INHERIT +
          '''</unitPattern></currencyFormats></numbers></ldml>''')
        rows = {locale: (dict(zip(layout.COUNTS, values)), mask)
                for locale, values, mask in layout.collect(source, graph())['currency_units']}
        self.assertEqual(rows['en-GB'][0]['one'], 'M {0} {1}')
        self.assertEqual(rows['en-GB'][0]['other'], '{0} {1}')
        self.assertFalse(rows['en-GB'][1] & (1 << layout.COUNTS.index('one')))
        self.assertTrue(rows['en-GB'][1] & (1 << layout.COUNTS.index('other')))
        self.assertEqual(self.rows(source)[('en-GB', 'latn')]['currency-unit-one'], 'L {0} {1}')
        before = layout.collect(source, graph())
        source.set('en_GB', '''<ldml><numbers><defaultNumberingSystem>latn</defaultNumberingSystem>
          <currencyFormats numberSystem="latn"><unitPattern count="one">L {0} {1}</unitPattern>
          </currencyFormats></numbers></ldml>''')
        after = layout.collect(source, graph())
        self.assertEqual(before['rows'], after['rows'])
        self.assertEqual(before['root_fields'], after['root_fields'])
        self.assertNotEqual(before['currency_units'], after['currency_units'])

    def test_currency_projection_aliases_keep_the_requesting_locale(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><defaultNumberingSystem>mathbold</defaultNumberingSystem>
          <currencyFormats numberSystem="mathbold"><alias source="locale"
          path="../currencyFormats[@numberSystem='latn']"/></currencyFormats>
          <currencyFormats numberSystem="latn"><unitPattern count="one">parent {0} {1}</unitPattern>
          </currencyFormats></numbers></ldml>''')
        source.set('en_GB', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <unitPattern count="one">child {0} {1}</unitPattern></currencyFormats></numbers></ldml>''')
        source.set('de', '''<ldml><numbers><defaultNumberingSystem>mathbold</defaultNumberingSystem>
          <currencyFormats numberSystem="mathbold"><alias source="root"
          path="../currencyFormats[@numberSystem='latn']"/></currencyFormats></numbers></ldml>''')
        rows = {locale: (dict(zip(layout.COUNTS, values)), mask)
                for locale, values, mask in layout.collect(source, graph())['currency_units']}
        self.assertEqual(rows['en-GB'][0]['one'], 'child {0} {1}')
        self.assertEqual(rows['de'][0]['other'], '{0} {1}')
        self.assertTrue(rows['de'][1] & (1 << layout.COUNTS.index('other')))

    def test_currency_projection_root_alias_cannot_capture_child_raw_override(self):
        source = Sources()
        root = ET.fromstring(source.files['common/main/root.xml'])
        currency = root.find("numbers/currencyFormats[@numberSystem='latn']")
        ET.SubElement(currency, 'unitPattern', count='one').text = 'root {0} {1}'
        source.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        source.set('en', '''<ldml><numbers><defaultNumberingSystem>mathbold</defaultNumberingSystem>
          <currencyFormats numberSystem="mathbold"><alias source="root"
          path="../currencyFormats[@numberSystem='latn']"/></currencyFormats></numbers></ldml>''')
        source.set('en_GB', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <unitPattern count="one">child raw {0} {1}</unitPattern></currencyFormats>
          <currencyFormats numberSystem="mathbold"><unitPattern count="other">child {0} {1}</unitPattern>
          </currencyFormats></numbers></ldml>''')
        rows = {locale: (dict(zip(layout.COUNTS, values)), mask)
                for locale, values, mask in layout.collect(source, graph())['currency_units']}
        self.assertEqual(rows['en-GB'][0]['one'], 'root {0} {1}')
        self.assertTrue(rows['en-GB'][1] & (1 << layout.COUNTS.index('one')))
        self.assertEqual(rows['en-GB'][0]['other'], 'child {0} {1}')
        self.assertFalse(rows['en-GB'][1] & (1 << layout.COUNTS.index('other')))

    def test_required_fields_reject_missing_blocked_and_empty_matches(self):
        for value in ('', BLOCK):
            source = Sources()
            source.set('en', '<ldml><numbers><miscPatterns numberSystem="latn">'
                       '<pattern type="range">' + value + '</pattern></miscPatterns></numbers></ldml>')
            with self.assertRaises(DataError):
                layout.collect(source, graph())
        source = Sources()
        source.set('en', '<ldml><numbers><currencyFormats numberSystem="latn"><currencySpacing>'
                   '<beforeCurrency><insertBetween/></beforeCurrency></currencySpacing>'
                   '</currencyFormats></numbers></ldml>')
        self.assertEqual(self.rows(source)[('en', 'mathbold')]['beforeCurrency-insertBetween'], '')

    def test_invalid_structure_selectors_and_aliases(self):
        contents = (
            '<miscPatterns numberSystem="roman"/>',
            '<miscPatterns numberSystem="latn"/><miscPatterns numberSystem="latn"/>',
            '<miscPatterns numberSystem="latn"><pattern type="range" count="one">x</pattern></miscPatterns>',
            '<miscPatterns numberSystem="latn"><pattern type="other">x</pattern></miscPatterns>',
            '<miscPatterns numberSystem="latn"><alias source="locale" path="../unknown"/></miscPatterns>',
            '<miscPatterns numberSystem="latn"><alias source="locale" path="../miscPatterns[@numberSystem=\'latn\']"/></miscPatterns>',
            '<miscPatterns numberSystem="latn">text<pattern type="range">x</pattern></miscPatterns>',
            '<miscPatterns numberSystem="latn"><pattern type="range"><unknown/></pattern></miscPatterns>',
            '<currencyFormats numberSystem="latn"><unitPattern count="unknown">x</unitPattern></currencyFormats>',
            '<currencyFormats numberSystem="latn"><unitPattern count="one" case="genitive">x</unitPattern></currencyFormats>',
            '<currencyFormats numberSystem="latn"><unitPattern count="one">x</unitPattern><unitPattern count="one">y</unitPattern></currencyFormats>',
            '<currencyFormats numberSystem="latn"><currencySpacing/><currencySpacing/></currencyFormats>',
            '<currencyFormats numberSystem="latn"><currencySpacing><unknown/></currencySpacing></currencyFormats>',
            '<currencyFormats numberSystem="latn"><currencySpacing><beforeCurrency/><beforeCurrency/></currencySpacing></currencyFormats>',
            '<currencyFormats numberSystem="latn"><currencySpacing><beforeCurrency><unknown/></beforeCurrency></currencySpacing></currencyFormats>',
            '<currencyFormats numberSystem="latn"><currencySpacing><alias source="locale" path="../currencySpacing"/></currencySpacing></currencyFormats>',
            '<currencyFormats numberSystem="latn"><currencySpacing><alias source="locale" path="../../currencyFormats[@numberSystem=\'latn\']/currencySpacing"/><beforeCurrency/></currencySpacing></currencyFormats>',
        )
        for contents in contents:
            source = Sources()
            source.set('en', '<ldml><numbers>' + contents + '</numbers></ldml>')
            with self.assertRaises(DataError, msg=contents):
                layout.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
