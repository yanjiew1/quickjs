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

"""Ordered tailoring syntax, quoting boundaries and XML source identity."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import collation_rules as rules
from cldr_xml import DataError


class RuleTests(unittest.TestCase):
    def test_ordered_relations_context_and_settings(self):
        nodes = rules.parse('&ae < x | y / z << q [caseFirst upper] <<< Q = r <<<< s')
        self.assertEqual([type(n) for n in nodes],
                         [rules.Reset, rules.Relation, rules.Relation, rules.Setting,
                          rules.Relation, rules.Relation, rules.Relation])
        self.assertEqual(nodes[0].text, (ord('a'), ord('e')))
        self.assertEqual(nodes[1][1:], (1, (ord('x'),), (ord('y'),), (ord('z'),)))
        self.assertEqual([nodes[i].strength for i in (1, 2, 4, 5, 6)], [1, 2, 3, 0, 4])
        self.assertEqual(nodes[3][1:], ('caseFirst', ('upper',)))

    def test_before_and_special_resets(self):
        for strength in (1, 2, 3):
            nodes = rules.parse('&[before {}]a{}b'.format(strength, '<' * strength))
            self.assertEqual(nodes[0].before, strength)
        nodes = rules.parse('&[last primary ignorable]<<x &[last regular]<z')
        self.assertEqual(nodes[0].special, 'last primary ignorable')
        self.assertEqual(nodes[2].special, 'last regular')
        for text in ('&[before 2]a<b', '&[before 4]a<<<<b', '&[before 1]a=b'):
            with self.subTest(text=text), self.assertRaises(DataError):
                rules.parse(text)

    def test_tool_unescaping_precedes_grammar(self):
        # Decoded syntax is still syntax. Decoded whitespace needs apostrophes.
        self.assertEqual(rules.parse(r'&a \u003c b')[1].text, (ord('b'),))
        self.assertEqual(rules.parse(r"&a<'\u003c'")[1].text, (ord('<'),))
        self.assertEqual(rules.parse(r"&a<'\u0020'")[1].text, (ord(' '),))
        self.assertEqual(rules.parse(r"&a<'\t'")[1].text, (ord('\t'),))
        self.assertEqual(rules.parse(r"&a<'\\'")[1].text, (ord('\\'),))
        self.assertEqual(rules.parse(r'&a<\u0000')[1].text, (0,))
        self.assertEqual(rules.parse(r'&a<\U0001F600')[1].text, (0x1f600,))
        self.assertEqual(rules.parse(r'&a<\uD83D\uDE00')[1].text, (0x1f600,))
        for text in (r'&a<\u003c', r'&a<\u0020', r'&a<\uD800', r'&a<\uDC00',
                     r'&a<\U00110000', r'&a<\uGGGG', '&a<\\'):
            with self.subTest(text=text), self.assertRaises(DataError):
                rules.parse(text)

    def test_quotes_comments_and_source_positions(self):
        nodes = rules.parse("# head\r\n&'a b'<'#'='' # tail\n<c", 'authored')
        self.assertEqual(nodes[0].text, tuple(map(ord, 'a b')))
        self.assertEqual(nodes[1].text, (ord('#'),))
        self.assertEqual(nodes[2].text, (ord("'"),))
        self.assertEqual(nodes[0].position, rules.Position('authored', 2, 1))
        self.assertEqual(nodes[3].position.line, 3)
        for newline in ('\r', '\n', '\r\n', '\f', '\x85', '\u2028', '\u2029'):
            nodes = rules.parse('# comment' + newline + '&a<b', 'newlines')
            self.assertEqual(len(nodes), 2)
            self.assertEqual(nodes[0].position, rules.Position('newlines', 2, 1))
        self.assertFalse(rules.parse('# comment\v&a<b'))
        text = r"&'\u0061'<b"
        self.assertEqual(rules.parse(text)[1].position.column, text.index('<') + 1)
        with self.assertRaises(DataError):
            rules.parse('&a b<c')  # Whitespace ends a string; it does not join a+b.
        with self.assertRaises(DataError):
            rules.parse("&a<'unterminated")

    def test_starred_order_ranges_and_protected_hyphens(self):
        nodes = rules.parse(r"&A<*'\u0020'-'/'<*0-'@'<*AZ<*'['-'`'")
        self.assertEqual(nodes[1].ranges, ((0x20, 0x2f),))
        self.assertEqual(nodes[2].ranges, ((0x30, 0x40),))
        self.assertEqual(nodes[3].ranges, ((ord('A'), ord('A')), (ord('Z'), ord('Z'))))
        self.assertEqual(rules.parse("&a<*'a-'b")[1].ranges,
                         ((97, 97), (45, 45), (98, 98)))
        for text in ('&a<*z-a', '&a<*a-', '&a<*a-d-g', '&a<*a|b', '&a<*a/b'):
            with self.subTest(text=text), self.assertRaises(DataError):
                rules.parse(text)

    def test_set_commands_are_ordered_not_syntax_from_comments(self):
        nodes = rules.parse(r'[suppressContractions [a-c\u19B5-\u19B7]] &a<b '
                            r'[optimize [Ии]] [suppressContractions [··]]')
        self.assertEqual([type(n) for n in nodes], [rules.SuppressContractions,
                         rules.Reset, rules.Relation, rules.Optimize, rules.SuppressContractions])
        self.assertEqual(nodes[0].ranges, ((97, 99), (0x19b5, 0x19b7)))
        self.assertEqual(nodes[3].ranges, ((0x418, 0x418), (0x438, 0x438)))
        self.assertFalse(rules.parse('# [suppressContractions [[:dt=none:]]]\n'))
        for text in ('[optimize [[:L:]]]', '[optimize [{ab}]]', '[optimize [^a]]',
                     '[optimize [a-z]', '[suppressContractions a]', '[optimize [#]]',
                     "[optimize ['a']]"):
            with self.subTest(text=text), self.assertRaises(DataError):
                rules.parse(text)

    def test_repeated_imports_and_settings_are_not_deduplicated(self):
        nodes = rules.parse('[import und-u-co-search][import hr][import hr]'
                            '[caseFirst upper][caseFirst off][reorder others digit]')
        self.assertEqual(nodes[0][1:], ('root', 'search'))
        self.assertEqual(rules.import_target('UND-u-co-search'), ('root', 'search'))
        self.assertEqual(nodes[1][1:], ('hr', 'standard'))
        self.assertEqual(nodes[2][1:], nodes[1][1:])
        self.assertEqual(nodes[3].values, ('upper',))
        self.assertEqual(nodes[4].values, ('off',))
        self.assertEqual(nodes[5].values, ('others', 'digit'))
        # Alias resolution is a catalog concern: preserve the requested co type.
        self.assertEqual(rules.parse('[import de-u-co-phonebk]')[0].collation, 'phonebk')

    def test_reject_unknown_executable_syntax_and_bound_work(self):
        for text in ('<a', '&a', '&a &b<c', '&[before 1]a', '&a<<<<<b', '[unknown on]', '[caseFirst sideways]',
                     '[reorder Latn Latn]', '[import en-u-kn-true]', '&a!b<c',
                     '[normalization on off]', '&[unknown place]<a', '\ud800',
                     '& # incomplete\n a<b', '&a< # incomplete\n b', '&a<x # done\n |y'):
            with self.subTest(text=text), self.assertRaises(DataError):
                rules.parse(text)
        previous = rules.MAX_NODES
        try:
            rules.MAX_NODES = 2
            with self.assertRaises(DataError):
                rules.parse('&a<b<c')
        finally:
            rules.MAX_NODES = previous


DTD = b'''<!ELEMENT ldml ANY><!ELEMENT collations ANY><!ELEMENT collation ANY>
<!ELEMENT cr ANY><!ELEMENT defaultCollation ANY>
<!ATTLIST collation type CDATA #REQUIRED>
<!ATTLIST collation alt CDATA #IMPLIED>
<!ATTLIST collation draft (approved|provisional) "approved">
<!ATTLIST collation references CDATA #IMPLIED>
<!ATTLIST collation visibility CDATA #IMPLIED>
<!ATTLIST collations draft CDATA #IMPLIED>
<!ATTLIST cr alt CDATA #IMPLIED>
<!ATTLIST cr draft CDATA #IMPLIED>
<!ATTLIST cr references CDATA #IMPLIED>
<!ATTLIST defaultCollation alt CDATA #IMPLIED>
<!ATTLIST defaultCollation draft CDATA #IMPLIED>'''


class Sources:
    def __init__(self, files):
        self.files = dict(files, **{'common/dtd/ldml.dtd': DTD})

    def read(self, name):
        return self.files[name]

    def paths(self, prefix):
        return sorted(name for name in self.files if name.startswith(prefix))


def xml(body):
    return ('<ldml><collations>' + body + '</collations></ldml>').encode('utf-8')


class SourceTests(unittest.TestCase):
    def sources(self, body):
        return Sources({'common/collation/root.xml': xml(
            '<defaultCollation>standard</defaultCollation><collation type="standard"/>'),
            'common/collation/xx.xml': xml(body)})

    def test_empty_is_present_missing_and_alternates_remain_distinct(self):
        programs, defaults = rules.collect(self.sources(
            '<collation type="standard"><cr>&amp;a&lt;b</cr></collation>'
            '<collation type="standard" alt="short"><cr>&amp;b&lt;a</cr></collation>'
            '<collation type="private-test" draft="provisional" references="authored"/>'))
        self.assertEqual(programs['root', 'standard', ''].nodes, ())
        self.assertNotIn(('xx', 'search', ''), programs)
        self.assertEqual(programs['xx', 'standard', 'short'].nodes[0].text, (ord('b'),))
        private = programs['xx', 'private-test', '']
        self.assertEqual((private.draft, private.references), ('provisional', 'authored'))
        self.assertEqual(defaults, {'root': 'standard'})

    def test_reject_ambiguous_or_unknown_records(self):
        for body in ('<collation type="standard"/><collation type="standard"/>',
                     '<collation type="standard"><cr/><cr/></collation>',
                     '<collation type="standard"><alias/></collation>',
                     '<collation type="standard">lost text</collation>',
                     '<defaultCollation>bad type</defaultCollation>',
                     '<collation type="standard" mystery="yes"/>',
                     '<collation type="standard" visibility="private"/>',
                     '<collation type="standard"><cr alt="short">&amp;a&lt;b</cr></collation>',
                     '<collation type="standard"><cr draft="provisional"/></collation>',
                     '<collation type="standard"><cr references="hidden"/></collation>',
                     '<defaultCollation alt="short">standard</defaultCollation>',
                     '<defaultCollation draft="provisional">standard</defaultCollation>'):
            with self.subTest(body=body), self.assertRaises(DataError):
                rules.collect(self.sources(body))
        with self.assertRaises(DataError):
            rules.collect(Sources({'common/collation/en.xml': b'<ldml/>'}))


if __name__ == '__main__':
    unittest.main()
