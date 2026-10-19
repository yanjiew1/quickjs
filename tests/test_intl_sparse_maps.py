#!/usr/bin/env python3
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

    def test_unknown_duplicate_mixed_and_cyclic_states(self):
        records = self.records()
        for locale, key in (('unknown', ('a',)), ('en', ('unknown',))):
            with self.assertRaises(DataError):
                records.add(locale, key, ('value',), '')
            with self.assertRaises(DataError):
                records.resolve(locale, key, {})
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


if __name__ == '__main__':
    unittest.main()
