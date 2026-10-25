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

"""Sparse compact maps: aliases, requesting locale, markers and sharing."""
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import compact_patterns as compact
from parents import ParentGraph
from resolve import BLOCK, INHERIT
from test_intl_list_patterns import graph
from test_intl_number_patterns import Sources as PatternSources


class Sources(PatternSources):
    def __init__(self):
        super().__init__()
        root = ET.fromstring(self.files['common/main/root.xml'])
        for style in ('decimal', 'currency'):
            group = root.find('./numbers/' + style + 'Formats[@numberSystem="latn"]')
            if style == 'decimal':
                length = ET.SubElement(group, 'decimalFormatLength', type='long')
                ET.SubElement(length, 'alias', source='locale',
                              path="../decimalFormatLength[@type='short']")
            length = ET.SubElement(group, style + 'FormatLength', type='short')
            form = ET.SubElement(length, style + 'Format')
            for power, value in (('1000', '0K'), ('10000', '00K'), ('1000000', '0M')):
                ET.SubElement(form, 'pattern', type=power, count='other').text = (
                    '¤' + value if style == 'currency' else value)
        self.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')


class CompactPatternTests(unittest.TestCase):
    def rows(self, data):
        return {(locale, system, compact.KINDS[kind]): data['maps'][index]
                for locale, system, kind, index in data['rows']}

    def test_requesting_aliases_and_shared_sparse_maps(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><decimalFormats numberSystem="latn">
          <decimalFormatLength type="short"><decimalFormat>
            <pattern type="1" count="other">0</pattern>
            <pattern type="1" count="0">zero</pattern>
            <pattern type="1000" count="other">0 English</pattern>
            <pattern type="1000" count="one">0 single</pattern>
            <pattern type="1000" count="1">single</pattern>
          </decimalFormat></decimalFormatLength></decimalFormats></numbers></ldml>''')
        data = compact.collect(source, graph())
        rows = self.rows(data)
        self.assertEqual(len(rows), len(graph().tags) * 2 * len(compact.KINDS))
        self.assertLess(len(data['maps']), len(rows))
        for system in ('latn', 'mathbold'):
            values = rows[('en-GB', system, ('decimal', 'long'))]
            self.assertIn((0, 'other', 'standard', '0'), values)
            self.assertIn((0, '0', 'standard', 'zero'), values)
            self.assertIn((3, 'other', 'standard', '0 English'), values)
            self.assertIn((3, '1', 'standard', 'single'), values)
            self.assertNotIn((3, 'few', 'standard', '0 English'), values)
            self.assertEqual(values, rows[('en-GB', system, ('decimal', 'short'))])
        self.assertEqual(data, compact.collect(source, graph()))

    def test_explicit_alias_source_and_leaf_markers(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><decimalFormats numberSystem="latn">
          <decimalFormatLength type="short"><decimalFormat>
            <pattern type="1000" count="other">0 English</pattern>
          </decimalFormat></decimalFormatLength></decimalFormats></numbers></ldml>''')
        source.set('en_GB', '''<ldml><numbers><decimalFormats numberSystem="latn">
          <decimalFormatLength type="short"><decimalFormat>
            <pattern type="1000" count="other">''' + INHERIT + '''</pattern>
            <pattern type="1000" count="one">''' + BLOCK + '''</pattern>
          </decimalFormat></decimalFormatLength></decimalFormats></numbers></ldml>''')
        source.set('de', '''<ldml><numbers><decimalFormats numberSystem="latn">
          <decimalFormatLength type="short"><decimalFormat>
            <pattern type="1000" count="other">0 Deutsch</pattern>
          </decimalFormat></decimalFormatLength></decimalFormats>
          <decimalFormats numberSystem="mathbold">
            <alias source="en" path='../decimalFormats[@numberSystem="latn"]'/>
          </decimalFormats></numbers></ldml>''')
        rows = self.rows(compact.collect(source, graph()))
        self.assertIn((3, 'other', 'standard', '0 English'),
                      rows[('de', 'mathbold', ('decimal', 'long'))])
        inherited = rows[('en-GB', 'mathbold', ('decimal', 'long'))]
        self.assertIn((3, 'other', 'standard', '0 English'), inherited)
        self.assertIn((3, 'one', 'standard', BLOCK), inherited)

    def test_currency_variants_and_alternate_aliases(self):
        source = Sources()
        source.set('en', '''<ldml><numbers><currencyFormats numberSystem="latn">
          <currencyFormatLength type="short"><currencyFormat>
            <pattern type="1000" count="other">¤0x</pattern>
            <pattern type="1000" count="other" alt="alphaNextToNumber">¤ 0x</pattern>
            <pattern type="1000" count="other" alt="noCurrency"/>
            <pattern type="1000" count="other" alt="variant">ignore</pattern>
            <alias alt="alphaNextToNumber" source="locale" path="ignore"/>
          </currencyFormat></currencyFormatLength></currencyFormats></numbers></ldml>''')
        data = compact.collect(source, graph())
        self.assertEqual(data['skipped_alternates'], 2)
        values = self.rows(data)[('en', 'mathbold', ('currency', 'short'))]
        self.assertIn((3, 'other', 'alphaNextToNumber', '¤ 0x'), values)
        self.assertIn((3, 'other', 'noCurrency', ''), values)

    def test_absent_currency_map_stays_absent(self):
        source = Sources()
        root = ET.fromstring(source.files['common/main/root.xml'])
        group = root.find('./numbers/currencyFormats[@numberSystem="mathbold"]')
        group.remove(group.find('alias'))
        source.files['common/main/root.xml'] = ET.tostring(root, encoding='utf-8')
        rows = self.rows(compact.collect(source, graph()))
        self.assertEqual(rows[('en', 'mathbold', ('currency', 'short'))], ())
        self.assertTrue(rows[('en', 'latn', ('currency', 'short'))])

    def test_iterative_long_parent_and_alias_resolution(self):
        tags = tuple('en-' + str(i).zfill(4) for i in range(1100))
        general = {tags[i]: tags[i+1] for i in range(len(tags)-1)}
        general[tags[-1]] = 'root'
        parents = ParentGraph(tags + ('root',), general, {}, {'und': 'en-Latn-US'}, (), False)
        records = compact.SparseMaps(parents,
                                     ((system, style, width) for system in ('latn', 'mathbold')
                                      for style, width in compact.KINDS), 'compact')
        key = ('latn', 'decimal', 'short')
        records.add('root', key, (3, 'other', 'standard'), '0K')
        records.alias('root', ('mathbold', 'decimal', 'short'), key, None)
        records.add(tags[0], key, (3, 'other', 'standard'), '0 local')
        value = records.resolve(tags[0], ('mathbold', 'decimal', 'short'), {})
        self.assertEqual(value, {(3, 'other', 'standard'): '0 local'})

    def test_reject_malformed_scopes_and_cycles(self):
        invalid = (
            '<decimalFormats numberSystem="roman"/>',
            '<decimalFormats numberSystem="latn"/><decimalFormats numberSystem="latn"/>',
            '<decimalFormats numberSystem="latn"><unknown/></decimalFormats>',
            '<decimalFormats numberSystem="latn"><alias source="locale" path="../unknown"/></decimalFormats>',
            '<decimalFormats numberSystem="latn"><decimalFormatLength type="short">text</decimalFormatLength></decimalFormats>',
            '<decimalFormats numberSystem="latn"><decimalFormatLength type="short"/></decimalFormats>',
            '<decimalFormats numberSystem="latn"><decimalFormatLength type="short"><alias source="locale" '
            'path="../decimalFormatLength[@type=&#39;long&#39;]"/></decimalFormatLength></decimalFormats>',
        )
        for xml in invalid:
            source = Sources()
            source.set('en', '<ldml><numbers>' + xml + '</numbers></ldml>')
            with self.assertRaises(DataError, msg=xml):
                compact.collect(source, graph())
        for fields in ('type="1001" count="other"', 'type="1000" count="unknown"',
                       'type="1000"', 'type="1000" count="other" numbers="latn"'):
            source = Sources()
            source.set('en', '<ldml><numbers><decimalFormats numberSystem="latn">'
                       '<decimalFormatLength type="short"><decimalFormat><pattern ' + fields +
                       '>0K</pattern></decimalFormat></decimalFormatLength></decimalFormats></numbers></ldml>')
            with self.assertRaises(DataError):
                compact.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
