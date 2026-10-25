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

"""Authored available-pattern identities, append grammar and sparse aliases."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import date_available_formats as formats
from resolve import BLOCK, INHERIT
from test_intl_date_patterns import Sources as StyleSources, calendar
from test_intl_list_patterns import graph


class Sources(StyleSources):
    def __init__(self):
        super().__init__()
        dtd = b'<!ELEMENT dateTimeFormats ANY>'
        for tag in formats.FAMILIES:
            dtd += ('<!ELEMENT ' + tag + ' ANY><!ATTLIST ' + tag + ' alt NMTOKENS #IMPLIED>'
                    '<!ATTLIST ' + tag + ' draft NMTOKEN #IMPLIED><!--@METADATA-->').encode()
        for tag, identity in (('dateFormatItem', 'id'), ('appendItem', 'request')):
            dtd += ('<!ELEMENT ' + tag + ' ANY><!ATTLIST ' + tag + ' ' + identity + ' CDATA #REQUIRED>'
                    '<!ATTLIST ' + tag + ' alt NMTOKENS #IMPLIED>'
                    '<!ATTLIST ' + tag + ' draft NMTOKEN #IMPLIED><!--@METADATA-->'
                    '<!ATTLIST ' + tag + ' references CDATA #IMPLIED><!--@METADATA-->').encode()
        dtd += b'<!ATTLIST dateFormatItem count NMTOKEN #IMPLIED>'
        dtd += b'<!ATTLIST dateFormatItem numbers CDATA #IMPLIED><!--@VALUE-->'
        self.files['common/dtd/ldml.dtd'] += dtd


def group(body, family='availableFormats', alt=''):
    return '<' + family + (' alt="' + alt + '"' if alt else '') + '>' + body + '</' + family + '>'


def calendar_formats(body, code='gregorian'):
    return calendar(code, '<dateTimeFormats>' + body + '</dateTimeFormats>')


def alias(path, source='locale'):
    return '<alias source="' + source + '" path="' + path + '"/>'


def layers(data, locale, family='availableFormats', alt='', code='gregorian'):
    key = code, family, alt
    head = next((head for tag, group, head in data['rows'] if (tag, group) == (locale, key)), 0)
    while head:
        tag, origin, index, head = data['nodes'][head - 1]
        yield tag, origin, data['maps'][index]


def query(data, locale, identifier, count='', leaf_alt='', **kwargs):
    for tag, origin, fields in layers(data, locale, **kwargs):
        for field in fields:
            if field[:3] == (identifier, count, leaf_alt):
                return field[3], tag, origin
    return None


class DateAvailableFormatsTests(unittest.TestCase):
    def test_exact_selectors_and_separate_template_grammar(self):
        source = Sources()
        source.set('root', calendar_formats(
            group('''<dateFormatItem id="yMd"> y-MM-dd </dateFormatItem>
                <dateFormatItem id="yMd" count="other">y/MM/dd</dateFormatItem>
                <dateFormatItem id="yMd" count="one" alt="variant">y 'year' M d</dateFormatItem>
                <dateFormatItem id="yMd" alt="ascii" draft="approved" references="x">y.M.d</dateFormatItem>''') +
            group('<dateFormatItem id="yMd" alt="ascii">dd/MM/y</dateFormatItem>', alt='variant') +
            group('''<appendItem request="Time-Day-Of-Week">{1} ob {0}</appendItem>
                <appendItem request="Year">{0} ({2}: {1})</appendItem>
                <appendItem request="Year" alt="variant">{1} [{0}]</appendItem>''', 'appendItems')))
        data = formats.collect(source, graph())
        self.assertEqual(query(data, 'en-GB', 'yMd')[:2], (' y-MM-dd ', 'root'))
        self.assertEqual(query(data, 'en-GB', 'yMd', count='other')[0], 'y/MM/dd')
        self.assertEqual(query(data, 'en-GB', 'yMd', count='one', leaf_alt='variant')[0], "y 'year' M d")
        self.assertEqual(query(data, 'en-GB', 'yMd', leaf_alt='ascii')[0], 'y.M.d')
        self.assertEqual(query(data, 'en-GB', 'yMd', leaf_alt='ascii', alt='variant')[0], 'dd/MM/y')
        self.assertEqual(query(data, 'en-GB', 'Time-Day-Of-Week', family='appendItems')[0], '{1} ob {0}')
        self.assertEqual(query(data, 'en-GB', 'Year', family='appendItems')[0], '{0} ({2}: {1})')
        self.assertEqual(query(data, 'en-GB', 'Year', family='appendItems', leaf_alt='variant')[0], '{1} [{0}]')
        self.assertEqual(data, formats.collect(source, graph()))

    def test_sparse_child_layers_keep_siblings_and_states(self):
        source = Sources()
        source.set('root', calendar_formats(group('''<dateFormatItem id="y">y</dateFormatItem>
            <dateFormatItem id="M">M</dateFormatItem><dateFormatItem id="d">d</dateFormatItem>''')))
        source.set('en', calendar_formats(group('<dateFormatItem id="y">' + INHERIT + '</dateFormatItem>'
            '<dateFormatItem id="M">' + BLOCK + '</dateFormatItem><dateFormatItem id="d"/>'
            '<dateFormatItem id="y" alt="variant">yy</dateFormatItem>')))
        data = formats.collect(source, graph())
        self.assertEqual(query(data, 'en-GB', 'y')[:2], ('y', 'root'))
        self.assertEqual(query(data, 'en-GB', 'M')[:2], (BLOCK, 'en'))
        self.assertEqual(query(data, 'en-GB', 'd')[:2], ('', 'en'))
        self.assertIsNone(query(data, 'en-GB', 'Q'))
        chain = list(layers(data, 'en-GB'))
        self.assertEqual([tag for tag, origin, fields in chain], ['en', 'root'])
        self.assertEqual(chain[0][2][-1], ('y', '', 'variant', 'yy'))

    def test_ancestor_aliases_keep_child_overrides_and_requester_anchor(self):
        source = Sources()
        source.set('root', calendar_formats(group('<dateFormatItem id="y">y</dateFormatItem>') +
            group('<appendItem request="Year">{0} {1}</appendItem>', 'appendItems')) +
            calendar('generic', '<dateTimeFormats>' +
                     alias("../../calendar[@type='gregorian']/dateTimeFormats") + '</dateTimeFormats>') +
            calendar('japanese', alias("../calendar[@type='generic']")))
        source.set('en', calendar_formats(group('<dateFormatItem id="y">yyyy</dateFormatItem>')))
        source.set('en_GB', calendar_formats(group('<dateFormatItem id="M">MMM</dateFormatItem>'), code='japanese'))
        data = formats.collect(source, graph())
        self.assertEqual(query(data, 'en-GB', 'y', code='japanese'),
                         ('yyyy', 'en', ('gregorian', 'availableFormats', '')))
        self.assertEqual(query(data, 'en-GB', 'M', code='japanese')[1], 'en-GB')
        self.assertEqual(query(data, 'en-GB', 'Year', code='japanese', family='appendItems')[0], '{0} {1}')
        self.assertEqual(query(data, 'de', 'y', code='japanese')[0], 'y')

    def test_explicit_source_and_family_aliases_preserve_origins(self):
        source = Sources()
        source.set('root', calendar_formats(group('<dateFormatItem id="y">y</dateFormatItem>') +
            group('<appendItem request="Year">{0} {1}</appendItem>', 'appendItems')))
        source.set('de', calendar_formats(group('<dateFormatItem id="y">yy</dateFormatItem>') +
            group('<appendItem request="Year">{1} {0}</appendItem>', 'appendItems')))
        source.set('en', calendar_formats(group(alias(
            "../../../calendar[@type='gregorian']/dateTimeFormats/appendItems", 'de'), 'appendItems')))
        data = formats.collect(source, graph())
        self.assertEqual(query(data, 'en-GB', 'Year', family='appendItems'),
                         ('{1} {0}', 'de', ('gregorian', 'appendItems', '')))
        self.assertEqual(query(data, 'en-GB', 'y')[0], 'y')

    def test_container_alternate_alias_and_original_skeleton_spelling(self):
        source = Sources()
        source.set('root', calendar_formats(group('<dateFormatItem id="yMMdd">y/MM/dd</dateFormatItem>',
                                                 alt='variant')))
        source.set('en', calendar_formats(group(alias("../availableFormats[@alt='variant']"))))
        data = formats.collect(source, graph())
        self.assertEqual(query(data, 'en-GB', 'yMMdd'),
                         ('y/MM/dd', 'root', ('gregorian', 'availableFormats', 'variant')))
        self.assertIsNone(query(data, 'en-GB', 'yMd'))

    def test_unowned_subtrees_are_left_to_their_collectors(self):
        source = Sources()
        source.set('root', calendar('gregorian', '<months><unknown/></months><dateFormats><unknown/></dateFormats>' +
            '<dateTimeFormats><intervalFormats><unknown/></intervalFormats>' +
            '<numericSeparators><unknown/></numericSeparators>' +
            '<dateTimeFormatLength><unknown/></dateTimeFormatLength>' +
            group('<dateFormatItem id="y">y</dateFormatItem>') + '</dateTimeFormats>'))
        self.assertEqual(formats.collect(source, graph())['source_records'], 1)

    def test_reject_duplicate_or_unsupported_structure(self):
        bodies = (
            group('<dateFormatItem id="y">y</dateFormatItem>' * 2),
            group('') * 2,
            group('<dateFormatItem id="y">y</dateFormatItem>') + group(''),
            group('', alt='variant') * 2,
            group('<dateFormatItem id="">y</dateFormatItem>'),
            group('<dateFormatItem id="y-M">y</dateFormatItem>'),
            group('<dateFormatItem id="y" count="invalid">y</dateFormatItem>'),
            group('<dateFormatItem id="y" numbers="latn">y</dateFormatItem>'),
            group('<dateFormatItem id="y">j</dateFormatItem>'),
            group('<dateFormatItem id="y"><cp hex="41"/></dateFormatItem>'),
            group('<appendItem request="Unknown">{0} {1}</appendItem>', 'appendItems'),
            group('<appendItem request="Year">{0} {1}</appendItem>' * 2, 'appendItems'),
            group('unexpected text'),
            group(alias('../appendItems')),
            group(alias("../availableFormats[@type='variant']")),
            group(alias('../../../calendar')),
            group(alias('../../../../calendar[@type=\'gregorian\']/dateTimeFormats/availableFormats')),
            group(alias("../../../calendar[@type='unknown']/dateTimeFormats/availableFormats")),
            group(alias('../availableFormats') + '<dateFormatItem id="y">y</dateFormatItem>'),
            '<unknown/>',
        )
        for body in bodies:
            source = Sources()
            source.set('root', calendar_formats(body))
            with self.subTest(body=body), self.assertRaises(DataError):
                formats.collect(source, graph())
        source = Sources()
        source.set('root', calendar('gregorian', '<dateTimeFormats/>' * 2))
        with self.assertRaises(DataError):
            formats.collect(source, graph())

    def test_alias_cycles_and_explicit_source_chains_fail(self):
        source = Sources()
        source.set('root', calendar_formats(group(alias('../availableFormats'))))
        with self.assertRaises(DataError):
            formats.collect(source, graph())
        source.set('root', calendar_formats(group('<dateFormatItem id="y">y</dateFormatItem>')))
        source.set('en', calendar_formats(group(alias('../availableFormats', 'de'))))
        source.set('de', calendar_formats(group(alias('../availableFormats', 'en'))))
        with self.assertRaises(DataError):
            formats.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
