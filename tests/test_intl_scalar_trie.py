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

"""Literal scalar keys, terminal identity, suffix sharing and resource gates."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import scalar_trie as graphs


def lookup(graph, key):
    state = graph.root
    for character in key:
        code = ord(character)
        edges = range(graph.edge_starts[state], graph.edge_starts[state + 1])
        found = [edge for edge in edges if graph.alphabet[graph.labels[edge]] == code]
        if not found:
            return None
        state = graph.targets[found[0]]
    terminal = graph.terminals[state]
    return graph.values[terminal - 1] if terminal else None


class ScalarTrieTests(unittest.TestCase):
    def test_literals_prefixes_payloads_and_shared_suffixes(self):
        entries = (('a', 0), ('ab', 7), ('cb', 7), ('n. Chr.', None), ('𠀀𠀁', 0xffffffff))
        graph = graphs.prepare(entries)
        for key, value in entries:
            self.assertEqual(lookup(graph, key), 0 if value is None else value)
        for key in ('', 'b', 'ac', 'n.', 'n.  Chr.', 'N. Chr.', '𠀀'):
            self.assertIsNone(lookup(graph, key))
        self.assertEqual((graph.words, graph.max_scalars, graph.max_utf16), (5, 7, 7))
        first = graphs.prepare((('ab', 1), ('cb', 1)))
        second = graphs.prepare((('ab', 1), ('cb', 2)))
        self.assertLess(len(first.terminals), len(second.terminals))
        self.assertTrue(all(target < state for state in range(len(graph.terminals))
                            for target in graph.targets[graph.edge_starts[state]:graph.edge_starts[state + 1]]))

    def test_common_keys_are_scalar_literals_not_dictionary_syntax(self):
        entries = (('\0', None), (' ', None), ('a b', None), ('é', None), ('𠀀𠀁𠀂', None))
        graph = graphs.prepare(entries)
        self.assertEqual((graph.max_scalars, graph.max_utf16), (3, 6))
        for key, value in entries:
            self.assertEqual(lookup(graph, key), 0)
        self.assertIsNone(lookup(graph, 'e\u0301'))

    def test_reject_invalid_order_scalar_payload_and_owner(self):
        for entries in ((), [], (('', None),), (('b', None), ('a', None)),
                        (('a', None), ('a', None)), (('\ud800', None),),
                        (('a', True),), (('a', 1.0),), (('a', -1),), (('a', 1 << 32),),
                        (('a',),), (('a', None, 0),)):
            with self.subTest(entries=entries), self.assertRaises(DataError):
                graphs.prepare(entries)

    def test_each_resource_cap_is_enforced(self):
        entries = (('ab', 1), ('cb', 2))
        for field, value in (('entries', 1), ('scalars', 3), ('key_length', 1),
                             ('states', 1), ('edges', 1), ('alphabet', 1), ('values', 1)):
            limits = graphs.DEFAULT_LIMITS._replace(**{field: value})
            with self.subTest(field=field), self.assertRaisesRegex(DataError, 'budget'):
                graphs.prepare(entries, limits)
        for value in (True, 0, graphs.DEFAULT_LIMITS.states + 1):
            with self.assertRaises(DataError):
                graphs.prepare(entries, graphs.DEFAULT_LIMITS._replace(states=value))

    def test_active_frontier_edges_share_the_retained_edge_budget(self):
        entries = tuple((key, None) for key in ('ab', 'ad', 'cb', 'cd'))
        # The final shared DAG has four edges, but construction temporarily
        # holds a duplicate two-edge suffix beside its canonical owner.
        self.assertEqual(len(graphs.prepare(entries).labels), 4)
        with self.assertRaisesRegex(DataError, 'active-edge budget'):
            graphs.prepare(entries, graphs.DEFAULT_LIMITS._replace(edges=4))
        self.assertEqual(len(graphs.prepare(entries, graphs.DEFAULT_LIMITS._replace(edges=5)).labels), 4)


if __name__ == '__main__':
    unittest.main()
