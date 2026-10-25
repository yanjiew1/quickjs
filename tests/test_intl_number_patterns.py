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

"""Standard number pattern aliases, optional variants, grouping and bad scopes."""
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import number_patterns as patterns
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph
from test_intl_numbering import Sources as NumberingSources


class Sources(NumberingSources):
    def __init__(self):
        super().__init__()
        self.files['common/dtd/ldml.dtd'] += b'''<!ATTLIST alias path CDATA #IMPLIED>
          <!ATTLIST alias alt NMTOKENS #IMPLIED>
          <!ELEMENT minimumGroupingDigits (#PCDATA)>
          <!ATTLIST minimumGroupingDigits alt NMTOKENS #IMPLIED>
          <!ELEMENT pattern (#PCDATA)><!ATTLIST pattern type NMTOKEN "standard">
          <!ATTLIST pattern alt NMTOKENS #IMPLIED><!ATTLIST pattern count NMTOKEN #IMPLIED>
          <!ATTLIST pattern numbers CDATA #IMPLIED>'''
        root = ET.fromstring(self.files['common/main/root.xml'])
        numbers = root.find('numbers')
        ET.SubElement(numbers, 'minimumGroupingDigits').text = '1'
        values = ('#,##0.###', '[#E0]', '#,##0%', '¤#,##0.00')
        for style, value in zip(patterns.STYLES, values):
            for tag, fields in ((style + 'Formats', ('numberSystem', 'alt')),
                                (style + 'FormatLength', ('type', 'alt')),
                                (style + 'Format', ('alt',))):
                self.files['common/dtd/ldml.dtd'] += ('<!ELEMENT ' + tag + ' ANY>').encode('ascii')
                for field in fields:
                    self.files['common/dtd/ldml.dtd'] += (
                        '<!ATTLIST ' + tag + ' ' + field + ' NMTOKEN #IMPLIED>').encode('ascii')
            self.files['common/dtd/ldml.dtd'] += (
                '<!ATTLIST ' + style + 'Format type NMTOKEN "standard">').encode('ascii')
            group = ET.SubElement(numbers, style + 'Formats', numberSystem='latn')
            length = ET.SubElement(group, style + 'FormatLength')
            form = ET.SubElement(length, style + 'Format')
            ET.SubElement(form, 'pattern').text = value
            if style == 'currency':
                form = ET.SubElement(length, 'currencyFormat', type='accounting')
                ET.SubElement(form, 'alias', source='locale',
                              path="../currencyFormat[@type='standard']")
            group = ET.SubElement(numbers, style + 'Formats', numberSystem='mathbold')
            ET.SubElement(group, 'alias', source='locale',
                          path="../" + style + "Formats[@numberSystem='latn']")
        self.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')


class NumberPatternTests(unittest.TestCase):
    def rows(self, data):
        return {(locale, system): dict(zip(patterns.FIELDS, values))
                for locale, system, values in data['rows']}

    def test_required_patterns_requesting_aliases_and_grouping(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><minimumGroupingDigits>2</minimumGroupingDigits>
          <decimalFormats numberSystem="latn"><decimalFormatLength><decimalFormat>
            <pattern>#0.000</pattern></decimalFormat></decimalFormatLength></decimalFormats>
          <currencyFormats numberSystem="latn"><currencyFormatLength><currencyFormat>
            <pattern>¤ #,##0.00</pattern><pattern alt="alphaNextToNumber">¤'#' #,##0.00</pattern>
          </currencyFormat></currencyFormatLength></currencyFormats>
        </numbers></ldml>''')
        source.set('en_GB', '<ldml><numbers><minimumGroupingDigits>' + INHERIT +
                   '</minimumGroupingDigits></numbers></ldml>')
        data = patterns.collect(source, graph())
        rows = self.rows(data)
        for system in ('latn', 'mathbold'):
            values = rows[('en-GB', system)]
            self.assertEqual(values['decimal'], '#0.000')
            self.assertEqual(values['scientific'], '[#E0]')
            self.assertEqual(values['currency-accounting'], '¤ #,##0.00')
            self.assertEqual(values['currency-accounting-alpha'], "¤'#' #,##0.00")
            self.assertIsNone(values['currency-standard-none'])
        self.assertEqual(dict(data['grouping']), {'de': 1, 'en': 2, 'en-GB': 2, 'root': 1})

    def test_explicit_source_optional_block_and_empty(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <currencyFormatLength><currencyFormat>
            <pattern alt="alphaNextToNumber">¤ #0</pattern>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        source.set('en_GB', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <currencyFormatLength><currencyFormat>
            <pattern alt="alphaNextToNumber">''' + BLOCK + '''</pattern>
            <pattern alt="noCurrency"/>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        source.set('de', '''<ldml><numbers><currencyFormats numberSystem="mathbold">
          <alias source="en" path="../currencyFormats[@numberSystem='latn']"/>
        </currencyFormats></numbers></ldml>''')
        rows = self.rows(patterns.collect(source, graph()))
        self.assertEqual(rows[('en-GB', 'mathbold')]['currency-accounting-alpha'], BLOCK)
        self.assertEqual(rows[('en-GB', 'latn')]['currency-accounting-none'], '')
        self.assertEqual(rows[('de', 'mathbold')]['currency-standard-alpha'], '¤ #0')

    def test_alternates_and_compact_lengths_do_not_replace_standard(self):
        source = Sources()
        source.set('en', '''<ldml>
          <numbers alt="variant"><minimumGroupingDigits>bad</minimumGroupingDigits></numbers>
          <numbers><decimalFormats numberSystem="latn" alt="variant"/>
          <decimalFormats numberSystem="latn"><decimalFormatLength type="short">
            <decimalFormat><pattern type="1000" count="one">0K</pattern></decimalFormat>
          </decimalFormatLength><decimalFormatLength><decimalFormat>
            <pattern alt="variant">bad</pattern><pattern>#0</pattern>
          </decimalFormat></decimalFormatLength></decimalFormats></numbers></ldml>''')
        data = patterns.collect(source, graph())
        self.assertEqual(data['skipped_alternates'], 3)
        self.assertEqual(self.rows(data)[('en', 'latn')]['decimal'], '#0')

    def test_alternate_aliases_do_not_change_baseline(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <currencyFormatLength><currencyFormat><pattern>¤0</pattern>
            <alias alt="alphaNextToNumber" source="locale"
                   path="../currencyFormat[@type='accounting']"/>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        source.set('en_GB', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <currencyFormatLength><currencyFormat>
            <alias alt="noCurrency" source="locale"
                   path="../currencyFormat[@type='accounting']"/>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        data = patterns.collect(source, graph())
        self.assertEqual(data['skipped_alternates'], 2)
        rows = self.rows(data)
        self.assertEqual(rows[('en-GB', 'latn')]['currency-standard'], '¤0')
        self.assertEqual(rows[('en-GB', 'mathbold')]['currency-accounting'], '¤0')
        self.assertIsNone(rows[('en-GB', 'latn')]['currency-standard-alpha'])

    def test_optional_inheritance_and_double_quoted_aliases(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <currencyFormatLength><currencyFormat>
            <pattern alt="alphaNextToNumber">¤ 0</pattern>
          </currencyFormat><currencyFormat type="accounting">
            <alias source="locale" path='../currencyFormat[@type="standard"]'/>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        source.set('en_GB', '''<ldml><numbers><currencyFormats numberSystem="mathbold">
          <alias source="locale" path='../currencyFormats[@numberSystem="latn"]'/>
          </currencyFormats><currencyFormats numberSystem="latn"><currencyFormatLength>
          <currencyFormat><pattern alt="alphaNextToNumber">''' + INHERIT + '''</pattern>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        rows = self.rows(patterns.collect(source, graph()))
        self.assertEqual(rows[('en-GB', 'mathbold')]['currency-accounting-alpha'], '¤ 0')

    def test_required_empty_and_blocked_patterns(self):
        for value in ('', BLOCK):
            source = Sources()
            source.set('en', '''<ldml><numbers><decimalFormats numberSystem="latn">
              <decimalFormatLength><decimalFormat><pattern>''' + value + '''</pattern>
              </decimalFormat></decimalFormatLength></decimalFormats></numbers></ldml>''')
            with self.assertRaises(DataError):
                patterns.collect(source, graph())

    def test_reject_malformed_scopes_and_alias_cycles(self):
        invalid = (
            '<minimumGroupingDigits>0</minimumGroupingDigits>',
            '<minimumGroupingDigits>-1</minimumGroupingDigits>',
            '<minimumGroupingDigits>4294967296</minimumGroupingDigits>',
            '<minimumGroupingDigits>1</minimumGroupingDigits><minimumGroupingDigits>2</minimumGroupingDigits>',
            '<decimalFormats/>', '<decimalFormats numberSystem="roman"/>',
            '<decimalFormats numberSystem="latn"/><decimalFormats numberSystem="latn"/>',
            '<decimalFormats numberSystem="latn"><unknown/></decimalFormats>',
            '<decimalFormats numberSystem="latn">text</decimalFormats>',
            '<decimalFormats numberSystem="latn"><alias source="locale" '
            'path="../decimalFormats[@numberSystem=&#39;latn&#39;]">text</alias></decimalFormats>',
            '<decimalFormats numberSystem="latn"><alias source="locale" '
            'path="../decimalFormats[@numberSystem=&#39;latn&#39;]"><pattern/></alias></decimalFormats>',
            '<decimalFormats numberSystem="latn"><alias source="locale" path="../unknown"/></decimalFormats>',
            '<decimalFormats numberSystem="latn"><alias source="locale" path="../percentFormats[@numberSystem=&#39;latn&#39;]"/></decimalFormats>',
            '<decimalFormats numberSystem="latn"><decimalFormatLength/><decimalFormatLength/></decimalFormats>',
            '<decimalFormats numberSystem="latn"><decimalFormatLength><decimalFormat>'
            '<pattern>0</pattern><pattern>0</pattern></decimalFormat></decimalFormatLength></decimalFormats>',
            '<decimalFormats numberSystem="latn"><decimalFormatLength><decimalFormat>'
            '<pattern count="one">0</pattern></decimalFormat></decimalFormatLength></decimalFormats>',
            '<decimalFormats numberSystem="latn"><decimalFormatLength><decimalFormat>'
            '<pattern numbers="hant">0</pattern></decimalFormat></decimalFormatLength></decimalFormats>',
            '<currencyFormats numberSystem="latn"><currencyFormatLength>'
            '<currencyFormat><alias source="locale" path="../currencyFormat[@type=&#39;accounting&#39;]"/></currencyFormat>'
            '</currencyFormatLength></currencyFormats>',
            '<alias source="root"/>',
        )
        for xml in invalid:
            source = Sources()
            source.set('en', '<ldml><numbers>' + xml + '</numbers></ldml>')
            with self.assertRaises(DataError, msg=xml):
                patterns.collect(source, graph())
        source = Sources()
        source.set('root', '<ldml/>')
        with self.assertRaises(DataError):
            patterns.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
