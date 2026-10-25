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

"""Authored date/time connectors, typed aliases and independent source records."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import date_combinations as combinations
from resolve import BLOCK, INHERIT
from test_intl_date_patterns import Sources as StyleSources, calendar
from test_intl_list_patterns import graph


class Sources(StyleSources):
    def __init__(self):
        super().__init__()
        self.files['common/dtd/ldml.dtd'] += b'''<!ELEMENT dateTimeFormats ANY>
          <!ELEMENT dateTimeFormatLength ANY>
          <!ATTLIST dateTimeFormatLength type (full | long | medium | short) #REQUIRED>
          <!ELEMENT dateTimeFormat ANY><!ATTLIST dateTimeFormat type NMTOKEN "standard">'''


def connector(body, width='long', form='standard'):
    return ('<dateTimeFormats><dateTimeFormatLength type="' + width + '">' +
            '<dateTimeFormat type="' + form + '">' + body + '</dateTimeFormat>' +
            '</dateTimeFormatLength></dateTimeFormats>')


def group_alias(code, source='locale'):
    return ('<dateTimeFormats><alias source="' + source +
            '" path="../../calendar[@type=\'' + code + '\']/dateTimeFormats"/></dateTimeFormats>')


def values(source, locale='en-GB'):
    return {row[1:5]: row[5:] for row in combinations.collect(source, graph())['rows']
            if row[0] == locale}


class DateCombinationTests(unittest.TestCase):
    def test_all_forms_widths_and_exact_source_text(self):
        source = Sources()
        body = '<dateTimeFormats>'
        for width in combinations.WIDTHS:
            body += '<dateTimeFormatLength type="' + width + '">'
            for form in combinations.FORMS:
                body += '<dateTimeFormat type="' + form + '"><pattern> {1} 年😀 {0} </pattern></dateTimeFormat>'
            body += '</dateTimeFormatLength>'
        source.set('root', calendar('gregorian', body + '</dateTimeFormats>'))
        result = values(source)
        self.assertEqual(len(result), 12)
        for width in combinations.WIDTHS:
            for form in combinations.FORMS:
                self.assertEqual(result['gregorian', width, form, ''],
                                 ('value', ' {1} 年😀 {0} ', '', 'root', 'gregorian', width, form, ''))
        self.assertEqual(combinations.collect(source, graph()), combinations.collect(source, graph()))

    def test_calendar_and_group_aliases_restart_at_requesting_locale(self):
        source = Sources()
        source.set('root', calendar('gregorian', connector('<pattern>{1}, {0}</pattern>')) +
                   calendar('generic', group_alias('gregorian')) +
                   calendar('japanese', '<alias source="locale" path="../calendar[@type=\'generic\']"/>'))
        source.set('en', calendar('gregorian', connector('<pattern>{1} {0}</pattern>')))
        result = values(source)
        self.assertEqual(result['japanese', 'long', 'standard', ''],
                         ('value', '{1} {0}', '', 'en', 'gregorian', 'long', 'standard', ''))
        self.assertEqual(result['generic', 'long', 'standard', ''][1], '{1} {0}')

    def test_form_aliases_keep_target_metadata_and_explicit_anchor(self):
        source = Sources()
        source.set('root', calendar('gregorian', '''<dateTimeFormats>
            <dateTimeFormatLength type="long"><dateTimeFormat>
            <pattern numbers="latn">{1}, {0}</pattern></dateTimeFormat>
            <dateTimeFormat type="atTime"><alias source="locale"
              path="../dateTimeFormat[@type='standard']"/></dateTimeFormat>
            <dateTimeFormat type="relative"><alias source="de"
              path="../dateTimeFormat[@type='standard']"/></dateTimeFormat>
            </dateTimeFormatLength></dateTimeFormats>'''))
        source.set('en', calendar('gregorian', connector('<pattern>{1} {0}</pattern>')))
        source.set('de', calendar('gregorian', connector('<pattern>{0} – {1}</pattern>')))
        result = values(source)
        self.assertEqual(result['gregorian', 'long', 'atTime', ''],
                         ('value', '{1} {0}', '', 'en', 'gregorian', 'long', 'standard', ''))
        self.assertEqual(result['gregorian', 'long', 'relative', ''],
                         ('value', '{0} – {1}', '', 'de', 'gregorian', 'long', 'standard', ''))

    def test_explicit_group_anchor_survives_inherited_locale_alias(self):
        source = Sources()
        source.set('root', calendar('gregorian', connector('<pattern>{1}, {0}</pattern>')) +
                   calendar('generic', group_alias('gregorian')))
        source.set('en', calendar('gregorian', connector('<pattern>{1}{0}</pattern>')) +
                   calendar('japanese', group_alias('generic', source='de')))
        source.set('de', calendar('gregorian', connector('<pattern>{0} – {1}</pattern>')))
        self.assertEqual(values(source)['japanese', 'long', 'standard', ''],
                         ('value', '{0} – {1}', '', 'de', 'gregorian', 'long', 'standard', ''))

    def test_shared_calendar_reader_rejects_repeated_containers(self):
        for xml in ('<ldml><dates/><dates/></ldml>',
                    '<ldml><dates><calendars/><calendars/></dates></ldml>'):
            source = Sources()
            source.files['common/main/root.xml'] = xml.encode('utf-8')
            with self.subTest(xml=xml), self.assertRaises(DataError):
                combinations.collect(source, graph())

    def test_numbers_alternates_and_default_form_are_independent(self):
        source = Sources()
        source.set('root', calendar('gregorian', '''<dateTimeFormats>
            <dateTimeFormatLength type="long"><dateTimeFormat>
            <pattern numbers="y=hebr">{1}, {0}</pattern>
            <pattern alt="variant" numbers="latn" draft="unconfirmed">{0} – {1}</pattern>
            </dateTimeFormat></dateTimeFormatLength></dateTimeFormats>'''))
        source.set('en', calendar('gregorian', connector('<pattern alt="variant">{1}{0}</pattern>')))
        result = values(source)
        self.assertEqual(result['gregorian', 'long', 'standard', ''][1:4], ('{1}, {0}', 'y=hebr', 'root'))
        self.assertEqual(result['gregorian', 'long', 'standard', 'variant'][1:4], ('{1}{0}', '', 'en'))
        # A consumer may fall back atTime to standard, after typed resolution.
        self.assertEqual(result['gregorian', 'long', 'atTime', ''][0], 'missing')

    def test_markers_empty_and_alias_inherited_block_preserve_provenance(self):
        source = Sources()
        source.set('root', calendar('gregorian', connector('<pattern>{1}, {0}</pattern>')) +
                   calendar('generic', group_alias('gregorian')))
        source.set('en', calendar('gregorian', '''<dateTimeFormats>
            <dateTimeFormatLength type="long"><dateTimeFormat><pattern>''' + INHERIT + '''</pattern>
            </dateTimeFormat><dateTimeFormat type="atTime"><pattern>''' + BLOCK + '''</pattern>
            </dateTimeFormat><dateTimeFormat type="relative"><pattern/></dateTimeFormat>
            </dateTimeFormatLength></dateTimeFormats>'''))
        result = values(source)
        self.assertEqual(result['gregorian', 'long', 'standard', ''][3], 'root')
        self.assertEqual(result['generic', 'long', 'atTime', ''],
                         ('blocked', '', '', 'en', 'gregorian', 'long', 'atTime', ''))
        self.assertEqual(result['gregorian', 'long', 'relative', ''][:4], ('value', '', '', 'en'))
        self.assertEqual(result['gregorian', 'short', 'relative', ''], ('missing',) + ('',) * 7)

    def test_other_calendar_and_format_owners_are_not_interpreted(self):
        source = Sources()
        source.set('root', calendar('gregorian', '''<months><unknown/></months><dateFormats><unknown/>
            </dateFormats><dateTimeFormats><availableFormats><unknown/></availableFormats>
            <intervalFormats><unknown/></intervalFormats><appendItems><unknown/></appendItems>
            <numericSeparators><unknown/></numericSeparators></dateTimeFormats>'''))
        self.assertEqual(combinations.collect(source, graph())['source_records'], 0)

    def test_reject_duplicate_unknown_and_mixed_source_records(self):
        forms = '''<dateTimeFormats><dateTimeFormatLength type="long">{}</dateTimeFormatLength></dateTimeFormats>'''
        cases = (
            connector('<pattern>{1}</pattern>'), connector('<pattern>j{1}{0}</pattern>'),
            connector('<pattern><child/></pattern>'), connector('<pattern count="one">{1}{0}</pattern>'),
            connector('<pattern>{1}{0}</pattern><pattern draft="approved">{1}{0}</pattern>'),
            connector('<pattern>{1}{0}</pattern>', form='unknown'),
            forms.format('<dateTimeFormat/><dateTimeFormat type="standard"/>'),
            forms.format('<alias source="locale" path="unsupported"/>'),
            connector('<alias source="locale" path="../dateTimeFormat[@type=\'standard\']"/>'
                      '<pattern>{1}{0}</pattern>', form='atTime'),
            connector('<alias source="locale" path="unsupported"/>', form='atTime'),
            forms.format('''<dateTimeFormat type="atTime"><alias source="locale"
                path="../dateTimeFormat[@type='relative']"/></dateTimeFormat>
                <dateTimeFormat type="relative"><alias source="locale"
                path="../dateTimeFormat[@type='atTime']"/></dateTimeFormat>'''),
            group_alias('unknown'), group_alias('gregorian'),
            '<dateTimeFormats><alias source="locale" path="unsupported"/></dateTimeFormats>',
            connector('<pattern>{1}{0}</pattern>') * 2,
            '<dateTimeFormats>text</dateTimeFormats>',
            '<dateTimeFormats><unknown/></dateTimeFormats>',
        )
        for body in cases:
            source = Sources()
            source.set('root', calendar('gregorian', body))
            with self.subTest(body=body), self.assertRaises(DataError):
                combinations.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
