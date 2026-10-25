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

"""Collation-specific parents, type fallback and bounded ordered imports."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import collation_programs as programs
import collation_rules as rules
import parents


def program(locale, kind='standard', text='', alternate=''):
    return rules.Program(locale, kind, alternate, '', '', rules.parse(text, locale + '#' + kind))


def keyword(name, aliases=(), preferred=''):
    return 'u', 'co', name, aliases, preferred, False, False, '', ''


def catalog(extra=(), defaults=None, keywords=()):
    graph = parents.ParentGraph(
        ('root', 'xx', 'yy', 'zh', 'zh-Hant', 'xx-AA', 'da', 'el', 'ko'),
        {'xx-AA': 'yy'}, {'collations': {'xx-AA': 'zh-Hant'}},
        {'und': 'en-Latn-US'}, (), False)
    records = [program('root'), program('root', 'search', '&a<b')] + list(extra)
    return programs.Catalog({programs.identity(p): p for p in records},
                            {'root': 'standard'} if defaults is None else defaults,
                            graph, keywords)


class CatalogTests(unittest.TestCase):
    def test_component_parent_and_empty_override(self):
        c = catalog((program('yy', text='&y<z'), program('zh', text='&c<d'),
                     program('zh-Hant', text='&h<i')))
        self.assertEqual(c.find('xx-AA', 'standard').locale, 'zh-Hant')
        self.assertEqual(tuple(c.graph.chain('xx-AA', 'main')), ('xx-AA', 'yy', 'root'))
        c = catalog((program('zh', text='&c<d'), program('zh-Hant')))
        selected = c.find('xx-AA', 'standard')
        self.assertEqual(selected.locale, 'zh-Hant')
        self.assertEqual(c.expand(selected), ())  # Do not concatenate parent rules.
        self.assertEqual(c.find('UND', 'standard').locale, 'root')

    def test_aliases_use_keyword_data_and_preferred_chains(self):
        c = catalog((program('xx', 'phonebook', '&a<b'),
                     program('xx', 'longmodern', '&c<d')),
                    keywords=(keyword('phonebk', ('phonebook',)),
                              keyword('modern', ('longmodern',)),
                              keyword('old', ('legacy',), 'modern')))
        self.assertEqual(c.find('xx', 'phonebk').collation, 'phonebook')
        self.assertEqual(c.find('xx', 'phonebook'), c.find('xx', 'phonebk'))
        self.assertEqual(c.find('xx', 'legacy').collation, 'longmodern')
        with self.assertRaises(TypeError):
            c.programs[('xx', 'other', '')] = program('xx', 'other')
        c = catalog((program('xx', 'phonebook'),),
                    keywords=(keyword('phonebk', ('phonebk', 'phonebook')),))
        self.assertEqual(c.find('xx', 'phonebk').collation, 'phonebook')

    def test_ambiguous_aliases_and_broken_keyword_chains_fail(self):
        c = catalog((program('xx', 'phonebk'), program('xx', 'phonebook')),
                    keywords=(keyword('phonebk', ('phonebook',)),))
        with self.assertRaises(DataError):
            c.find('xx', 'phonebk')
        for entries in ((keyword('aaa', preferred='bbb'),),
                        (keyword('aaa', preferred='bbb'), keyword('bbb', preferred='aaa')),
                        (keyword('aaa', ('same',)), keyword('bbb', ('same',))),
                        (keyword('aaa'), keyword('aaa'))):
            with self.subTest(entries=entries), self.assertRaises(DataError):
                catalog(keywords=entries)

    def test_type_fallback_matches_tr35_examples(self):
        c = catalog((program('da', text='&d<e'), program('da', 'search', '&g<h'),
                     program('el', text='&e<f'), program('ko', text='&k<l'),
                     program('ko', 'search', '&m<n'), program('ko', 'searchjl', '&o<p'),
                     program('zh', 'pinyin', '&q<r'), program('zh', 'stroke', '&s<t')),
                    defaults={'root': 'standard', 'zh': 'pinyin', 'zh-Hant': 'stroke'})
        cases = (('da', 'phonebook', 'da', 'standard'),
                 ('zh', None, 'zh', 'pinyin'), ('zh', 'standard', 'root', 'standard'),
                 ('zh', 'phonebook', 'zh', 'pinyin'),
                 ('zh-Hant', 'phonebook', 'zh', 'stroke'),
                 ('da', 'searchjl', 'da', 'search'), ('el', 'search', 'root', 'search'),
                 ('el', 'searchjl', 'root', 'search'), ('ko', 'searchjl', 'ko', 'searchjl'))
        for locale, requested, actual_locale, actual_type in cases:
            selected = c.select(locale, requested)
            self.assertEqual((selected.locale, selected.collation), (actual_locale, actual_type))
        # Search fallback precedes the locale's default, even for a Chinese default.
        self.assertEqual(c.select('zh', 'searchxx').collation, 'search')

    def test_alternates_do_not_substitute_for_normal_programs(self):
        c = catalog((program('xx', text='&a<b', alternate='proposed'),
                     program('xx', 'special', '&b<a', 'short')))
        self.assertEqual(c.select('xx').locale, 'root')
        self.assertIsNone(c.find('xx', 'special'))
        self.assertEqual(c.find('xx', 'special', 'short').alternate, 'short')
        self.assertIsNone(c.find('xx', 'standard', 'short'))

    def test_imports_repeat_and_settings_and_suppression_remain_ordered(self):
        c = catalog((program('xx', text='[caseFirst upper][import yy][import yy][caseFirst off]'),
                     program('yy', text='[suppressContractions [a-c]]&a<b')))
        nodes = c.expand(c.select('xx'))
        self.assertEqual([type(n) for n in nodes],
                         [rules.Setting, rules.SuppressContractions, rules.Reset, rules.Relation,
                          rules.SuppressContractions, rules.Reset, rules.Relation, rules.Setting])
        self.assertEqual(nodes[0].values, ('upper',))
        self.assertEqual(nodes[-1].values, ('off',))
        self.assertIs(nodes[1], nodes[4])  # Immutable syntax can be cached, execution cannot.
        self.assertEqual(nodes[1].position.source, 'yy#standard')

    def test_import_without_co_is_standard_not_default(self):
        c = catalog((program('xx', text='[import zh]'),
                     program('zh', 'pinyin', '&a<b')),
                    defaults={'root': 'standard', 'zh': 'pinyin'})
        self.assertEqual(c.expand(c.select('xx')), ())
        self.assertEqual(c.select('zh').collation, 'pinyin')

    def test_private_imports_allowed_missing_import_not_public_fallback(self):
        c = catalog((program('xx', text='[import yy-u-co-private-test]'),
                     program('yy', 'private-test', '&a<b')))
        self.assertEqual(len(c.expand(c.select('xx'))), 2)
        self.assertEqual(c.select('yy', 'private-test').collation, 'standard')
        self.assertEqual(c.find('yy', 'private-test').collation, 'private-test')
        with self.assertRaises(DataError):
            catalog((program('yy', 'private-test'),), defaults={'yy': 'private-test'})
        c = catalog((program('xx', text='[import yy-u-co-missing]'),
                     program('yy', 'pinyin', '&a<b')),
                    defaults={'root': 'standard', 'yy': 'pinyin'})
        self.assertEqual(c.select('yy', 'missing').collation, 'pinyin')
        with self.assertRaisesRegex(DataError, 'xx#standard:1:1: missing imported'):
            c.expand(c.select('xx'))

    def test_import_cycles_depth_and_both_node_and_empty_dag_budgets(self):
        c = catalog((program('xx', text='[import yy]'), program('yy', text='[import xx]')))
        with self.assertRaisesRegex(DataError, 'cycle'):
            c.expand(c.select('xx'))
        c = catalog((program('xx', text='[import yy][import yy]'), program('yy', text='&a<b')))
        with self.assertRaisesRegex(DataError, 'step budget'):
            c.expand(c.select('xx'), max_steps=5)
        self.assertEqual(len(c.expand(c.select('xx'), max_steps=6)), 4)
        extra = [program('xx', 'private-p{:02}'.format(i),
                         '[import xx-u-co-private-p{0:02}][import xx-u-co-private-p{0:02}]'.format(i + 1))
                 for i in range(8)] + [program('xx', 'private-p08')]
        c = catalog(extra)
        start = c.find('xx', 'private-p00')
        with self.assertRaisesRegex(DataError, 'depth budget'):
            c.expand(start, max_depth=3)
        with self.assertRaisesRegex(DataError, 'step budget'):
            c.expand(start, max_steps=100)  # Every leaf is empty, but work is not free.
        self.assertEqual(c.expand(start, max_steps=1000), ())
        for value in (0, -1, True, 1.5):
            with self.subTest(value=value), self.assertRaises(DataError):
                c.expand(start, max_steps=value)
            with self.subTest(value=value), self.assertRaises(DataError):
                c.expand(start, max_depth=value)

    def test_root_semantic_contract_and_foreign_program(self):
        c = catalog()
        with self.assertRaises(DataError):
            c.expand(program('xx', text='&a<b'))
        for records in ({}, {('root', 'standard', ''): program('root', text='&a<b')},
                        {('root', 'standard', ''): program('xx')}):
            with self.subTest(records=records), self.assertRaises(DataError):
                programs.Catalog(records, {}, c.graph, ())


if __name__ == '__main__':
    unittest.main()
