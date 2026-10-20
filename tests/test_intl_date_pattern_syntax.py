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
    def test_combination_slots_and_quoted_connector(self):
        self.assertEqual(syntax.compile_combination("{1} 'at' {0}"),
                         (('1', 0, ''), ('', 0, ' at '), ('0', 0, '')))
        self.assertEqual(syntax.compile_combination('{0}, {1}'),
                         (('0', 0, ''), ('', 0, ', '), ('1', 0, '')))
        self.assertEqual(syntax.compile_combination('{1}{0}'),
                         (('1', 0, ''), ('0', 0, '')))

    def test_combination_quotes_apply_to_markers_and_fields(self):
        self.assertEqual(syntax.compile_combination("'{0}' {1} 'o''clock' {0}"),
                         (('', 0, '{0} '), ('1', 0, ''), ('', 0, " o'clock "), ('0', 0, '')))
        self.assertEqual(syntax.compile_combination("{1}'{'{0}'}'"),
                         (('1', 0, ''), ('', 0, '{'), ('0', 0, ''), ('', 0, '}')))
        self.assertEqual(syntax.compile_combination("''{1}'' {0}"),
                         (('', 0, "'"), ('1', 0, ''), ('', 0, "' "), ('0', 0, '')))
        self.assertEqual(syntax.compile_combination('{1}\u202f\u200f年😀 {0}'),
                         (('1', 0, ''), ('', 0, '\u202f\u200f年😀 '), ('0', 0, '')))

    def test_combination_keeps_fields_without_selecting_format_policy(self):
        self.assertEqual(syntax.compile_combination('{1} HH:mm {0}'),
                         (('1', 0, ''), ('', 0, ' '), ('H', 2, ''), ('', 0, ':'),
                          ('m', 2, ''), ('', 0, ' '), ('0', 0, '')))
        # Ordinary date patterns still treat braces as literal text.
        self.assertEqual(syntax.compile_pattern('{0}'), (('', 0, '{0}'),))
        for suffix in (' }', ' {2}', ' {01}', ' {}', ' {'):
            self.assertEqual(syntax.compile_combination('{1}{0}' + suffix),
                             (('1', 0, ''), ('0', 0, ''), ('', 0, suffix)))

    def test_combination_rejects_missing_repeated_or_invalid_slots(self):
        for value in ('', None, 1, b'{1}{0}', '{1}', '{0}', "'{1}' {0}",
                      '{0}{1}{0}', '{1}{1}{0}', '{2}{1}', '{}{1}',
                      '{01}{1}', '{ 0}{1}', '{0 {1}',
                      "{0}{1}'", '{0}j{1}', '{0}\0{1}', '{0}\ud800{1}'):
            with self.subTest(value=repr(value)), self.assertRaises(DataError):
                syntax.compile_combination(value)

    def test_combination_audit_keeps_forms_and_markers_separate(self):
        source = Sources('''<dateTimeFormats><dateTimeFormatLength type="long">
            <dateTimeFormat><pattern>{1}, {0}</pattern><pattern alt="variant">{1}, {0}</pattern>
            <pattern>''' + INHERIT + '''</pattern></dateTimeFormat>
            <dateTimeFormat type="atTime"><pattern>{1} 'at' {0}</pattern>
            <pattern>''' + BLOCK + '''</pattern></dateTimeFormat>
            </dateTimeFormatLength><availableFormats><dateFormatItem id="y">y</dateFormatItem>
            </availableFormats></dateTimeFormats>''')
        self.assertEqual(syntax.audit_combinations(source),
                         {'records': 3, 'markers': 2, 'patterns': 2, 'tokens': 6})

    def test_combination_audit_rejects_bad_leaves(self):
        for value in ('<pattern>{1}</pattern>', '<pattern><child/></pattern>'):
            source = Sources('<dateTimeFormats><dateTimeFormatLength><dateTimeFormat>' + value +
                             '</dateTimeFormat></dateTimeFormatLength></dateTimeFormats>')
            with self.subTest(value=value), self.assertRaises(DataError):
                syntax.audit_combinations(source)

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

    def test_interval_directives_preserve_order_without_splitting_fields(self):
        pattern = "LLL d – MMM d, y"
        tokens = syntax.compile_pattern(pattern)
        for prefix in ('', 'earliestFirst:', 'latestFirst:'):
            self.assertEqual(syntax.compile_interval(prefix + pattern), (prefix[:-1], tokens))
        self.assertEqual(syntax.compile_interval("'latestFirst:'d–d"),
                         ('', (('', 0, 'latestFirst:'), ('d', 1, ''), ('', 0, '–'), ('d', 1, ''))))
        for value in ('', None, 1, b'd-d', 'latestFirst:', 'earliestFirst:',
                      'latestFirst:earliestFirst:d–d', 'latestFirst:j', 'latestFirst:\0'):
            with self.subTest(value=repr(value)), self.assertRaises(DataError):
                syntax.compile_interval(value)

    def test_audit_uses_interval_directives_only_for_interval_leaves(self):
        body = '''<dateTimeFormats><intervalFormats><intervalFormatItem>
            <greatestDifference id="d">latestFirst:d–d</greatestDifference>
            </intervalFormatItem></intervalFormats></dateTimeFormats>'''
        source = Sources(body)
        self.assertEqual(syntax.audit(source)['records'], 1)
        # A later calendar cannot reuse the interval grammar's cached result.
        path = 'common/main/en.xml'
        source.files[path] = source.files[path].replace(b'</calendars>',
            b'''<calendar type="japanese"><dateFormats><dateFormatLength><dateFormat>
            <pattern>latestFirst:d-d</pattern></dateFormat></dateFormatLength></dateFormats>
            </calendar></calendars>'''.replace(b'd-d', 'd–d'.encode('utf-8')))
        with self.assertRaises(DataError):
            syntax.audit(source)

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
