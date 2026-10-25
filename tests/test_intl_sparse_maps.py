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

"""Shared sparse group alias resolution, immutable sharing and bad states."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from resolve import BLOCK, INHERIT, SparseMaps
from test_intl_list_patterns import graph


class SparseMapTests(unittest.TestCase):
    def records(self):
        return SparseMaps(graph(), (('a',), ('b',)), 'authored')

    def test_alias_requesting_context_sharing_and_sparse_overlays(self):
        records = self.records()
        records.add('root', ('a',), ('value',), 'root')
        records.add('root', ('a',), ('blocked',), BLOCK)
        records.alias('root', ('b',), ('a',), None)
        records.add('en', ('a',), ('value',), 'English')
        records.add('en', ('a',), ('empty',), '')
        records.add('en-GB', ('a',), ('value',), INHERIT)
        cache = {}
        result = records.resolve('en-GB', ('b',), cache)
        self.assertEqual(result, {('value',): 'English', ('blocked',): BLOCK, ('empty',): ''})
        self.assertIs(result, records.resolve('en-GB', ('a',), cache))
        self.assertEqual(records.resolve('root', ('a',), cache),
                         {('value',): 'root', ('blocked',): BLOCK})

    def test_explicit_alias_source_and_absent_group(self):
        records = self.records()
        self.assertEqual(records.resolve('en', ('a',), {}), {})
        records.add('en', ('a',), ('value',), 'English')
        records.add('de', ('a',), ('value',), 'German')
        records.alias('de', ('b',), ('a',), 'en')
        self.assertEqual(records.resolve('de', ('b',), {}), {('value',): 'English'})

    def test_local_overlay_order_and_exact_flattening(self):
        records = self.records()
        records.add('root', ('a',), ('one', 'genitive'), 'parent one genitive')
        records.add('root', ('a',), ('one', ''), 'parent one')
        records.add('en', ('a',), ('other', ''), 'local other')
        records.add('en', ('a',), ('one', 'genitive'), INHERIT)
        records.add('en', ('a',), ('blocked', ''), BLOCK)
        records.add('en', ('a',), ('empty', ''), '')
        cache = {}
        head = records.overlays('en-GB', ('a',), cache)
        self.assertEqual((head.locale, head.group), ('en', ('a',)))
        self.assertEqual(head.fields, {('other', ''): 'local other',
                                        ('blocked', ''): BLOCK, ('empty', ''): ''})
        self.assertIsNot(head.fields, records.values[('en', ('a',))])
        self.assertEqual(head.parent.locale, 'root')
        self.assertEqual(head.parent.fields[('one', 'genitive')], 'parent one genitive')
        self.assertIsNone(head.parent.parent)
        self.assertIs(head, records.overlays('en-GB', ('a',), cache))
        # The raw exact-leaf view remains unchanged. Lateral consumers can
        # now inspect child OTHER before considering an inherited parent ONE.
        layers = []
        while head is not None:
            layers.append(head.fields)
            head = head.parent
        flattened = {}
        for fields in reversed(layers):
            flattened.update(fields)
        self.assertEqual(flattened, records.resolve('en-GB', ('a',), {}))

    def test_overlay_alias_restart_order_and_shared_suffix(self):
        records = self.records()
        records.add('root', ('a',), ('one',), 'root one')
        records.add('en', ('a',), ('other',), 'English other')
        records.add('en-GB', ('a',), ('one',), INHERIT)
        records.add('en-GB', ('b',), ('other',), 'British other')
        records.alias('root', ('b',), ('a',), None)
        cache = {}
        head = records.overlays('en-GB', ('b',), cache)
        self.assertEqual((head.locale, head.group, head.fields),
                         ('en-GB', ('b',), {('other',): 'British other'}))
        self.assertIs(head.parent, records.overlays('en-GB', ('a',), cache))
        self.assertEqual(head.parent.fields, {('other',): 'English other'})
        self.assertEqual(head.parent.parent.fields, {('one',): 'root one'})
        self.assertIsNone(head.parent.parent.parent)

    def test_overlay_explicit_source_resets_requester_and_empty_cache(self):
        records = self.records()
        cache = {}
        self.assertIsNone(records.overlays('en', ('a',), cache))
        self.assertTrue(cache and all(value is None for value in cache.values()))
        self.assertIsNone(records.overlays('en', ('a',), cache))
        records = SparseMaps(graph(), (('a',), ('b',), ('c',)), 'authored')
        records.add('en', ('a',), ('value',), 'English')
        records.add('de', ('a',), ('value',), 'German')
        records.alias('de', ('c',), ('b',), 'en')
        records.alias('root', ('b',), ('a',), None)
        head = records.overlays('de', ('c',), {})
        self.assertEqual((head.locale, head.group, head.fields),
                         ('en', ('a',), {('value',): 'English'}))
        self.assertIsNone(head.parent)

    def test_deep_overlay_chain_without_recursion(self):
        class DeepGraph:
            tags = tuple('level%d' % i for i in range(2200))

            def parent(self, locale):
                index = int(locale[5:])
                return 'level%d' % (index - 1) if index else None

        records = SparseMaps(DeepGraph(), (('a',),), 'deep')
        for i in range(2200):
            records.add('level%d' % i, ('a',), ('value',), str(i))
        head = records.overlays('level2199', ('a',), {})
        count = 0
        while head is not None:
            self.assertEqual(head.fields[('value',)], str(2199 - count))
            head = head.parent
            count += 1
        self.assertEqual(count, 2200)

    def test_unknown_duplicate_mixed_and_cyclic_states(self):
        records = self.records()
        for locale, key in (('unknown', ('a',)), ('en', ('unknown',))):
            with self.assertRaises(DataError):
                records.add(locale, key, ('value',), '')
            with self.assertRaises(DataError):
                records.resolve(locale, key, {})
            with self.assertRaises(DataError):
                records.overlays(locale, key, {})
        records.add('en', ('a',), ('value',), '')
        with self.assertRaises(DataError):
            records.add('en', ('a',), ('value',), '')
        with self.assertRaises(DataError):
            records.alias('en', ('a',), ('b',), None)
        records.alias('en', ('b',), ('a',), None)
        with self.assertRaises(DataError):
            records.add('en', ('b',), ('value',), '')
        with self.assertRaises(DataError):
            records.alias('en', ('b',), ('a',), None)
        records = self.records()
        records.alias('root', ('a',), ('b',), None)
        records.alias('root', ('b',), ('a',), None)
        with self.assertRaisesRegex(DataError, 'alias/parent cycle'):
            records.resolve('en', ('a',), {})
        with self.assertRaisesRegex(DataError, 'alias/parent cycle'):
            records.overlays('en', ('a',), {})


if __name__ == '__main__':
    unittest.main()
