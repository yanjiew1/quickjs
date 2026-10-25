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

"""Executable type support must not be inferred from permissive type fallback."""
import unittest
from unittest.mock import patch

from test_intl_collation_programs import catalog, program, keyword
import availability
import collation_bindings as bindings
from cldr_xml import DataError
import parents


class BindingTests(unittest.TestCase):
    def test_component_parents_and_exact_support(self):
        c = catalog((program('yy', 'phonebook'), program('zh-Hant', 'stroke')),
                    keywords=(keyword('phonebk', ('phonebook',)), keyword('stroke')))
        rows = bindings.resolve(c, ('xx-AA',))
        self.assertEqual([row.collation for row in rows], ['', 'stroke'])
        self.assertEqual(rows[1].sort, ('zh-Hant', 'stroke', ''))
        self.assertIsNone(rows[1].search)
        self.assertEqual(c.select('xx-AA', 'phonebk').collation, 'standard')

        # An explicitly empty component graph still has structural collation
        # parents; it must not inherit the exceptional main parent yy.
        graph = parents.ParentGraph(('root', 'xx', 'xx-AA', 'yy'),
                                    {'xx-AA': 'yy'}, {'collations': {}},
                                    {'und': 'en-Latn-US'}, (), False)
        c = type(c)(dict(c.programs), {}, graph, (keyword('phonebk', ('phonebook',)),))
        self.assertEqual([row.collation for row in bindings.resolve(c, ('xx-AA',))], [''])

    def test_aliases_defaults_and_independent_search(self):
        c = catalog((program('zh', 'pinyin'), program('zh', 'stroke'),
                     program('zh', 'search'), program('xx', 'phonebook')),
                    defaults={'root': 'standard', 'zh': 'pinyin', 'zh-Hant': 'stroke'},
                    keywords=(keyword('pinyin'), keyword('stroke'),
                              keyword('phonebk', ('phonebook',)),
                              keyword('old', ('legacy',), 'phonebk')))
        rows = bindings.resolve(c, ('xx', 'zh', 'zh-Hant'))
        by_key = {(row.locale, row.collation): row for row in rows}
        self.assertEqual(by_key['zh', ''].sort, ('zh', 'pinyin', ''))
        self.assertEqual(by_key['zh', ''].search, ('zh', 'search', ''))
        self.assertEqual(by_key['zh-Hant', ''].sort, ('zh', 'stroke', ''))
        self.assertEqual(by_key['zh', 'pinyin'].sort, by_key['zh', ''].sort)
        self.assertEqual(by_key['xx', 'phonebk'].sort, ('xx', 'phonebook', ''))
        self.assertNotIn(('xx', 'old'), by_key)
        self.assertNotIn(('xx', 'phonebook'), by_key)

    def test_private_and_alternate_programs_are_not_support(self):
        c = catalog((program('xx', 'private-helper'),
                     program('xx', 'special', alternate='short'),
                     program('xx', 'other', alternate='proposed')),
                    keywords=tuple(keyword(name) for name in
                                   ('standard', 'search', 'default', 'private-helper', 'special', 'other')))
        rows = bindings.resolve(c, ('xx',))
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0].sort, ('root', 'standard', ''))
        self.assertEqual(rows[0].search, ('root', 'search', ''))

    def test_profile_projection_and_synthetic_source_identity(self):
        graph = parents.ParentGraph(('root', 'en', 'en-US', 'zz', 'zz-Latn-AA'),
                                    {}, {'collations': {}},
                                    {'und': 'en-Latn-US', 'zz': 'zz-Latn-AA'}, (), False)
        completed, aliases = availability.complete(graph)
        self.assertEqual(availability.profile_locales(completed, 'minimal'), ('en', 'en-US', 'root'))
        self.assertIn('zz-AA', availability.profile_locales(completed, 'native'))

        original = catalog((program('zz-Latn-AA', 'special'),), keywords=(keyword('special'),))
        c = type(original)(dict(original.programs), {}, graph, (keyword('special'),))
        rows = bindings.resolve(c, ('zz-AA',), aliases)
        self.assertEqual(rows[1].sort, ('zz-Latn-AA', 'special', ''))
        self.assertEqual(rows[1].locale, 'zz-AA')
        # A plain source lookup has no such explicit synthetic identity mapping.
        self.assertEqual(len(bindings.resolve(c, ('zz-AA',))), 1)

        with patch.object(bindings.collation_programs, 'load', return_value=c):
            minimal = bindings.collect(None, 'minimal')
            native = bindings.collect(None, 'native')
        self.assertEqual([row.locale for row in minimal if not row.collation], ['en', 'en-US'])
        self.assertNotIn('root', {row.locale for row in native})

    def test_invalid_profiles_and_missing_search_fail_closed(self):
        c = catalog()
        programs = dict(c.programs)
        del programs['root', 'search', '']
        missing = type(c)(programs, {}, c.graph, ())
        with self.assertRaisesRegex(DataError, 'default/search'):
            bindings.resolve(missing, ('xx',))

        for locales in ((), ('root',), ('xx', 'xx'), ('yy', 'xx'), ('xx_AA',)):
            with self.subTest(locales=locales), self.assertRaises(DataError):
                bindings.resolve(c, locales)
        with self.assertRaises(DataError):
            bindings.resolve(c, ('xx',), (('xx', 'yy'), ('yy', 'xx')))
        with self.assertRaises(ValueError):
            availability.profile_locales(c.graph, 'unknown')


if __name__ == '__main__':
    unittest.main()
