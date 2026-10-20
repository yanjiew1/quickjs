#!/usr/bin/env python3
"""Literal slot templates and quoted localized offset source subpatterns."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import literal_template
import zone_pattern_syntax as syntax


class ZonePatternSyntaxTests(unittest.TestCase):
    def test_literals_quotes_signs_spaces_and_field_widths(self):
        for value in ('+HH:mm; -HH:mm', '+HH:mm;−HH:mm', '+HH:mm;–HH:mm',
                      '+H:mm;-H:mm', '+HHmm;-HHmm', '\u200e+HH:mm;\u200e-HH:mm',
                      "'+'HH';'mm;'-'HH';'mm", "+HH:mm'';-HH:mm''"):
            patterns = syntax.compile_hour_format(value)
            self.assertEqual(len(patterns), 2)
            self.assertEqual([token.symbol for token in patterns[0] if token.symbol], ['H', 'm'])
        positive, negative = syntax.compile_hour_format('+HH:mm; -HH:mm')
        self.assertEqual(negative[0].text, ' -')
        self.assertEqual(positive[0].text, '+')
        self.assertIn(';', [token.text for token in syntax.compile_hour_format("+HH';'mm;-HH';'mm")[0]])

    def test_reject_malformed_or_other_date_fields(self):
        for value in (None, '', '+HH:mm', '+HH:mm;', ';-HH:mm', '+HH:mm;-HH:mm;',
                      "+HH:mm;'-HH:mm", '+hh:mm;-hh:mm', '+HHH:mm;-HHH:mm',
                      '+HH:m;-HH:m', '+HH:mm:ss;-HH:mm:ss', '+HH:mm y;-HH:mm', '+HH:mm;-HH:\0mm'):
            with self.subTest(value=value), self.assertRaises(DataError):
                syntax.compile_hour_format(value)

    def test_fixed_operand_templates_have_literal_apostrophes(self):
        for value, count in (("l'heure {0}", 1), ('{0}/{1}', 2), ("'{1}' and '{0}'", 2)):
            self.assertEqual(literal_template.validate(value, count), value)
        for value, count in (('{1}', 1), ('{0}{0}', 1), ('{0}{1}', 1), ('{0}', 2),
                             ('{{0}', 1), ('{0}}', 1), ('{0}\0', 1), ('{0}\ud800', 1),
                             ('{0}', 0), ('{0}', True), ('{0}', 11)):
            with self.subTest(value=repr(value), count=count), self.assertRaises(DataError):
                literal_template.validate(value, count)


if __name__ == '__main__':
    unittest.main()
