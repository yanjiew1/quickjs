#!/usr/bin/env python3
"""LDML date pattern quoting and field identity, independently of format policy."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import date_pattern_syntax as syntax
from resolve import BLOCK, INHERIT


class Sources:
    def __init__(self, body):
        self.files = {
            'common/dtd/ldml.dtd': b'''<!ELEMENT pattern (#PCDATA)>
                <!ATTLIST pattern alt NMTOKENS #IMPLIED>
                <!ATTLIST pattern numbers CDATA #IMPLIED><!--@VALUE-->
                <!ELEMENT dateFormatItem (#PCDATA)>
                <!ATTLIST dateFormatItem id NMTOKEN #REQUIRED>
                <!ELEMENT greatestDifference (#PCDATA)>
                <!ATTLIST greatestDifference id NMTOKEN #REQUIRED>''',
            'common/main/en.xml': ('<ldml><dates><calendars><calendar type="gregorian">' +
                                   body + '</calendar></calendars></dates></ldml>').encode('utf-8'),
        }

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return tuple(name for name in self.files if name.startswith(prefix))


class DatePatternSyntaxTests(unittest.TestCase):
    def test_fields_keep_source_identity_width_and_order(self):
        self.assertEqual(syntax.compile_pattern('yyyy.MM.dd G'),
                         (('y', 4, ''), ('', 0, '.'), ('M', 2, ''),
                          ('', 0, '.'), ('d', 2, ''), ('', 0, ' '), ('G', 1, '')))
        fields = 'GyYuUrQqMLlwWdDFgEecabBhHKkmsSAzZOvVXx'
        self.assertEqual(syntax.compile_pattern(fields), tuple((c, 1, '') for c in fields))
        # Width legality and interpretation belong to the consuming compiler.
        self.assertEqual(syntax.compile_pattern('y' * 20), (('y', 20, ''),))

    def test_quote_rules_inside_and_outside_literals(self):
        self.assertEqual(syntax.compile_pattern("hh 'o''clock' a, zzzz"),
                         (('h', 2, ''), ('', 0, " o'clock "), ('a', 1, ''),
                          ('', 0, ', '), ('z', 4, '')))
        self.assertEqual(syntax.compile_pattern("EEE, MMM d, ''yy"),
                         (('E', 3, ''), ('', 0, ', '), ('M', 3, ''),
                          ('', 0, ' '), ('d', 1, ''), ('', 0, ", '"), ('y', 2, '')))
        self.assertEqual(syntax.compile_pattern("'jJC' 'unassigned p' ''"),
                         (('', 0, "jJC unassigned p '"),))
        self.assertEqual(syntax.compile_pattern("''''"), (('', 0, "''"),))

    def test_scalar_literals_whitespace_and_braces_are_preserved(self):
        self.assertEqual(syntax.compile_pattern('  y年M月d日\u202f\u200f😀 '),
                         (('', 0, '  '), ('y', 1, ''), ('', 0, '年'), ('M', 1, ''),
                          ('', 0, '月'), ('d', 1, ''), ('', 0, '日\u202f\u200f😀 ')))
        self.assertEqual(syntax.compile_pattern('{0} – {1}'), (('', 0, '{0} – {1}'),))
        self.assertEqual(syntax.compile_pattern('123é'), (('', 0, '123é'),))

    def test_interval_fields_remain_distinct(self):
        self.assertEqual(syntax.compile_pattern('MMM d–d, y'),
                         (('M', 3, ''), ('', 0, ' '), ('d', 1, ''), ('', 0, '–'),
                          ('d', 1, ''), ('', 0, ', '), ('y', 1, '')))
        self.assertEqual(syntax.compile_pattern("y'/'y"),
                         (('y', 1, ''), ('', 0, '/'), ('y', 1, '')))

    def test_invalid_inputs_and_reserved_letters(self):
        for value in ('', None, 1, b'yyyy', '\0', '\ud800', '\udfff',
                      "yyyy '", "'quoted''", 'j', 'J', 'C', 'p', 'yyyy at HH:mm'):
            with self.subTest(value=repr(value)), self.assertRaises(DataError):
                syntax.compile_pattern(value)

    def test_audit_checks_all_raw_paths_and_alternates(self):
        source = Sources('''<dateFormats><dateFormatLength><dateFormat>
            <pattern>y-M-d</pattern><pattern alt="variant" numbers="hanidec">y年M月d日</pattern>
            <pattern>''' + INHERIT + '''</pattern></dateFormat></dateFormatLength></dateFormats>
            <timeFormats><timeFormatLength><timeFormat><pattern>HH:mm</pattern>
            </timeFormat></timeFormatLength></timeFormats><dateTimeFormats>
            <availableFormats><dateFormatItem id="yMd">y-M-d</dateFormatItem>
            <dateFormatItem id="y">''' + BLOCK + '''</dateFormatItem></availableFormats>
            <intervalFormats><intervalFormatItem><greatestDifference id="d">d–d</greatestDifference>
            </intervalFormatItem></intervalFormats>
            <dateTimeFormatLength><dateTimeFormat><pattern>{1} 'at' {0}</pattern>
            </dateTimeFormat></dateTimeFormatLength></dateTimeFormats>''')
        self.assertEqual(syntax.audit(source), {'records': 5, 'markers': 2, 'patterns': 4,
                                               'tokens': 17, 'fields': 'HMdmy'})

    def test_audit_reports_bad_leaves_without_claiming_alias_resolution(self):
        source = Sources('''<dateFormats><alias source="locale" path="../dateFormats"/></dateFormats>''')
        self.assertEqual(syntax.audit(source)['records'], 0)
        prefix = '<dateTimeFormats><availableFormats>'
        suffix = '</availableFormats></dateTimeFormats>'
        for leaf in ('<dateFormatItem id="y">j</dateFormatItem>',
                     '<dateFormatItem id="y"><child/></dateFormatItem>',
                     '<dateFormatItem>y</dateFormatItem>',
                     '<dateFormatItem id="y" unknown="x">y</dateFormatItem>'):
            with self.subTest(leaf=leaf), self.assertRaises(DataError):
                syntax.audit(Sources(prefix + leaf + suffix))


if __name__ == '__main__':
    unittest.main()
