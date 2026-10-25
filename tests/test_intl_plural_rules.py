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

"""Authored CLDR plural conditions, component inheritance and range records."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from parents import ParentGraph
import plural_rules as plurals

DTD = b'''<!ELEMENT supplementalData ANY><!ELEMENT plurals ANY>
<!ATTLIST plurals type (cardinal | ordinal) #IMPLIED>
<!ELEMENT pluralRules ANY><!ATTLIST pluralRules locales NMTOKENS #REQUIRED>
<!ELEMENT pluralRule (#PCDATA)>
<!ATTLIST pluralRule count (zero | one | two | few | many | other) #REQUIRED>
<!ATTLIST pluralRule draft (approved | contributed | provisional | unconfirmed) #IMPLIED>
<!ELEMENT pluralRanges ANY><!ATTLIST pluralRanges locales NMTOKENS #REQUIRED>
<!ELEMENT pluralRange EMPTY>
<!ATTLIST pluralRange start (zero | one | two | few | many | other) #IMPLIED>
<!ATTLIST pluralRange end (zero | one | two | few | many | other) #IMPLIED>
<!ATTLIST pluralRange result (zero | one | two | few | many | other) #REQUIRED>'''


def graph(components=None):
    return ParentGraph(('root', 'en', 'en-GB', 'fr'), {'en-GB': 'fr'},
                       {'plurals': {} if components is None else components},
                       {'und': 'en-Latn-US'}, (), False)


class Sources:
    def __init__(self):
        self.files = {'common/dtd/ldmlSupplemental.dtd': DTD}
        self.set(0, '''<plurals type="cardinal">
          <pluralRules locales="root"><pluralRule count="other">@integer 0, 1, …</pluralRule></pluralRules>
          <pluralRules locales="en"><pluralRule count="one">i = 1 and v = 0 @integer 1</pluralRule>
            <pluralRule count="other"/></pluralRules>
          <pluralRules locales="fr"><pluralRule count="one" draft="provisional">i = 0,1</pluralRule>
            <pluralRule count="other"/></pluralRules>
        </plurals>''')
        self.set(1, '''<plurals type="ordinal">
          <pluralRules locales="root"><pluralRule count="other"/></pluralRules>
          <pluralRules locales="en"><pluralRule count="few">n % 10 = 3</pluralRule>
            <pluralRule count="one">n % 10 = 1</pluralRule>
            <pluralRule count="other"/><pluralRule count="two">n % 10 = 2</pluralRule></pluralRules>
        </plurals>''')
        self.set(2, '''<plurals><pluralRanges locales="en">
          <pluralRange start="one" end="other" result="other"/>
          <pluralRange start="one" end="one" result="one"/>
        </pluralRanges></plurals>''')

    def set(self, index, xml):
        self.files[plurals.PATHS[index]] = ('<supplementalData>' + xml + '</supplementalData>').encode('utf-8')

    def read(self, filename):
        return self.files[filename]


class PluralSourceTests(unittest.TestCase):
    def test_conditions_samples_normalization_and_adjacent_tokens(self):
        for condition in ('n = 1', 'imod10=1', 'iisnot1', 'iin1', 'i=2orv=0',
                          'nnotwithin1..2', 'i = 0,1..3 and n != 4 or e is 0',
                          'n = 4294967295', 'i % 0000000000010 = 1'):
            self.assertEqual(plurals.relation(condition), condition)
        self.assertEqual(plurals.relation(' \t\nn\u200e=\x851\u2029 @integer 1'), 'n = 1')
        self.assertEqual(plurals.relation(' @integer 0, 1, … @decimal 0.0'), '')
        for condition in ('x = 0', 'n = -1', 'n = 1.0', 'n = 2..1', 'n % 0 = 0',
                          'n = 4294967296', 'n = 1 or', 'n = 1,', 'n is 1..2',
                          'n = 1 or x = 0', 'n = 2 and x = 0', 'n\u00a0=1',
                          'n = 1\0', 'n = 1 andy = 1'):
            with self.assertRaises(DataError, msg=condition):
                plurals.relation(condition)

    def test_component_inheritance_categories_and_sparse_ranges(self):
        data = plurals.collect(Sources(), graph())
        rows = {(locale, kind): (rules, ranges) for locale, kind, rules, ranges in data['rows']}
        self.assertEqual(rows[('en-GB', 0)][0], ((1, 'i = 1 and v = 0'), (5, '')))
        self.assertEqual(rows[('en-GB', 1)][0],
                         ((1, 'n % 10 = 1'), (2, 'n % 10 = 2'), (3, 'n % 10 = 3'), (5, '')))
        self.assertEqual(rows[('en-GB', 0)][1], ((1, 1, 1), (1, 5, 5)))
        self.assertEqual(rows[('fr', 1)], (((5, ''),), ()))
        self.assertIn(('fr', 1, 'root', None), data['origins'])
        overridden = plurals.collect(Sources(), graph({'en-GB': 'fr'}))
        self.assertIn(('en-GB', 0, 'fr', None), overridden['origins'])
        self.assertEqual(data['source_locales'], (3, 2, 1))
        self.assertEqual(data['conditions'], tuple(sorted(set(data['conditions']))))

    def test_explicit_empty_range_map_stops_parent_inheritance(self):
        source = Sources()
        source.set(2, '''<plurals>
          <pluralRanges locales="en"><pluralRange start="one" end="one" result="one"/></pluralRanges>
          <pluralRanges locales="en_GB"/>
        </plurals>''')
        data = plurals.collect(source, graph())
        self.assertIn(('en-GB', 0, 'en', 'en-GB'), data['origins'])
        self.assertEqual(next(row[3] for row in data['rows'] if row[:2] == ('en-GB', 0)), ())

    def test_reject_malformed_rule_groups_and_missing_root(self):
        invalid = (
            '<plurals/>', '<plurals type="ordinal"/>',
            '<plurals><unknown/></plurals>',
            '<plurals><pluralRules locales="root"><pluralRule count="one">n=1</pluralRule></pluralRules></plurals>',
            '<plurals><pluralRules locales="en"><pluralRule count="other"/></pluralRules></plurals>',
            '<plurals><pluralRules locales="root root"><pluralRule count="other"/></pluralRules></plurals>',
            '<plurals><pluralRules locales="root"><pluralRule count="other">n=1</pluralRule></pluralRules></plurals>',
            '<plurals><pluralRules locales="root"><pluralRule count="one"/><pluralRule count="other"/></pluralRules></plurals>',
            '<plurals><pluralRules locales="root"><pluralRule count="other"/><pluralRule count="other"/></pluralRules></plurals>',
            '<plurals><pluralRules locales="root"><pluralRule count="unknown"/></pluralRules></plurals>',
            '<plurals><pluralRules locales="root"><pluralRule count="other"><unknown/></pluralRule></pluralRules></plurals>',
            '<plurals><pluralRules locales="root">junk<pluralRule count="other"/></pluralRules></plurals>',
            '<plurals><pluralRules locales="root"><pluralRule count="other"/>junk</pluralRules></plurals>',
            '<plurals><pluralRules locales="root"><pluralRule count="other"/></pluralRules>'
            '<pluralRules locales="root"><pluralRule count="other"/></pluralRules></plurals>',
            '<plurals/><plurals/>',
        )
        for xml in invalid:
            source = Sources()
            source.set(0, xml)
            with self.assertRaises(DataError, msg=xml):
                plurals.collect(source, graph())
        with self.assertRaises(DataError):
            plurals.collect(Sources(), ParentGraph(('root',), {}, {},
                                                  {'und': 'en-Latn-US'}, (), False))

    def test_reject_unreviewed_or_conflicting_range_records(self):
        for record in ('<pluralRange result="one"/>',
                       '<pluralRange start="one" end="other" result="unknown"/>',
                       '<pluralRange start="one" end="other" result="one"><unknown/></pluralRange>',
                       '<pluralRange start="one" end="other" result="one">junk</pluralRange>',
                       '<pluralRange start="one" end="other" result="one"/>'
                       '<pluralRange start="one" end="other" result="one"/>'):
            source = Sources()
            source.set(2, '<plurals><pluralRanges locales="en">' + record + '</pluralRanges></plurals>')
            with self.assertRaises(DataError, msg=record):
                plurals.collect(source, graph())


if __name__ == '__main__':
    unittest.main()
