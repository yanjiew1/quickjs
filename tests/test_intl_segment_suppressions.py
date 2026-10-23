#!/usr/bin/env python3
"""Whole-list inheritance and literal suppression graphs with strict owners."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from parents import ParentGraph
from segment_dictionary import Dictionary, Entry
from segment_dictionary_trie import prepare as dictionary_graph
from segment_inheritance import SuppressionEnvironment, capture_suppressions
from segment_sources import Segmentation
from segment_suppressions import prepare
from test_intl_scalar_trie import lookup


def row(locale, entries):
    return Segmentation(locale, 'SentenceBreak', None, None, entries,
                        'common/segments/' + locale + '.xml')


class SuppressionTests(unittest.TestCase):
    def test_nearest_whole_list_and_explicit_empty_ignore_main_parent(self):
        graph = ParentGraph(('root', 'en', 'en-GB', 'en-US'), {'en-GB': 'en-US'},
                            {'segmentations': {}}, {'und': 'en-Latn-US'}, (), False)
        rows = (row('root', ('Root.',)), row('en', ('Dr.', 'Mr.')), row('en-US', ('US.',)))
        environment = capture_suppressions(rows, graph, 'en-GB')
        self.assertEqual(environment, SuppressionEnvironment(('Dr.', 'Mr.'), 'en', 'common/segments/en.xml'))
        empty = capture_suppressions(rows + (row('en-GB', ()),), graph, 'en-GB')
        self.assertEqual(empty.entries, ())
        self.assertEqual(prepare(empty).origin, ('en-GB', 'common/segments/en-GB.xml'))
        self.assertIsNone(prepare(empty).graph)
        absent = capture_suppressions((), graph, 'en-GB')
        self.assertIsNone(prepare(absent).origin)
        with self.assertRaises(DataError):
            capture_suppressions(rows + (rows[0],), graph, 'en')

    def test_raw_order_space_case_and_complete_prefixes(self):
        entries = ('n. Chr.', 'n.', 'Mr.', '𠀀.')
        result = prepare(SuppressionEnvironment(entries, 'en', 'fixture.xml'))
        self.assertIs(result.entries, entries)
        for key in entries:
            self.assertEqual(lookup(result.graph, key), 0)
        for key in ('n', 'n. Chr', 'N.', 'n.  Chr.'):
            self.assertIsNone(lookup(result.graph, key))
        # Shared mechanics must not change dictionary key/cost policy.
        data = Dictionary((Entry('n. Chr.', None),), False, 7, 7, ())
        with self.assertRaises(DataError):
            dictionary_graph(data)

    def test_bad_keys_origins_and_budgets_never_become_empty_policy(self):
        for entries in (('',), ('a', 'a'), ('\ud800',), ('a' * 257,), (True,)):
            with self.subTest(entries=entries), self.assertRaises(DataError):
                prepare(SuppressionEnvironment(entries, 'en', 'fixture.xml'))
        for environment in (SuppressionEnvironment(('a',), None, None),
                            SuppressionEnvironment((), None, 'fixture.xml'),
                            SuppressionEnvironment((), 'en', None),
                            SuppressionEnvironment([], 'en', 'fixture.xml')):
            with self.subTest(environment=environment), self.assertRaises(DataError):
                prepare(environment)


if __name__ == '__main__':
    unittest.main()
