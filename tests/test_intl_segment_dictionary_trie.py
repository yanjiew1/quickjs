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

"""Dictionary sharing preserves terminal values, prefixes and scalar labels."""
from bisect import bisect_left
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import segment_dictionary as dictionary
import segment_dictionary_trie as trie


def prepared(text, valued=False):
    return trie.prepare(dictionary.parse(text.encode('utf-8'), valued))


def lookup(data, word):
    state = data.root
    for char in word:
        scalar = ord(char)
        first, last = data.edge_starts[state:state + 2]
        labels = [data.alphabet[data.labels[index]] for index in range(first, last)]
        at = bisect_left(labels, scalar)
        if at == len(labels) or labels[at] != scalar:
            return False, None
        state = data.targets[first + at]
    terminal = data.terminals[state]
    return (True, data.values[terminal - 1]) if terminal else (False, None)


class TrieTests(unittest.TestCase):
    def test_suffix_sharing_and_input_order_determinism(self):
        data = prepared('ab\ncb\nad\ncd\n')
        self.assertEqual(data, prepared('cd\nad\ncb\nab\n'))
        self.assertEqual(len(data.terminals), 3)
        self.assertEqual(len(data.labels), 4)
        for word in ['ab', 'ad', 'cb', 'cd']:
            self.assertEqual(lookup(data, word), (True, 0))
        for word in ['', 'a', 'b', 'c', 'abc', 'cc', 'da']:
            self.assertEqual(lookup(data, word), (False, None))
        for state in range(len(data.terminals)):
            first, last = data.edge_starts[state:state + 2]
            self.assertTrue(all(target < state for target in data.targets[first:last]))
            self.assertEqual(list(data.labels[first:last]), sorted(set(data.labels[first:last])))

    def test_costs_prevent_false_state_merging(self):
        data = prepared('ab\t1\ncb\t2\nad\t1\ncd\t1\n', True)
        for word, cost in [('ab', 1), ('cb', 2), ('ad', 1), ('cd', 1)]:
            self.assertEqual(lookup(data, word), (True, cost))
        self.assertGreater(len(data.terminals), 3)
        self.assertTrue(data.valued)

    def test_prefix_terminals_zero_and_full_cost_range(self):
        data = prepared('a\t0\nab\t4294967295\nb\t0\n', True)
        self.assertEqual(lookup(data, 'a'), (True, 0))
        self.assertEqual(lookup(data, 'ab'), (True, 0xffffffff))
        self.assertEqual(lookup(data, 'b'), (True, 0))
        self.assertEqual(lookup(data, 'aba'), (False, None))
        self.assertEqual((data.words, data.max_scalars, data.max_utf16), (3, 2, 2))

    def test_scalar_keys_are_not_normalized_or_split(self):
        keys = ['é', 'é', '𠀀a', '𠀀b', 'a\u2028b']
        data = prepared('\n'.join(keys))
        self.assertEqual(data.words, len(keys))
        self.assertEqual((data.max_scalars, data.max_utf16), (3, 3))
        self.assertIn(0x20000, data.alphabet)
        self.assertNotIn(0xd840, data.alphabet)
        for word in keys:
            self.assertEqual(lookup(data, word), (True, 0))

    def test_exact_sixteen_bit_id_domains(self):
        # Alphabet IDs are zero-based; terminal cost IDs reserve zero.
        # Single-scalar keys share one terminal state, keeping this fixture small.
        keys = [chr(value) for value in range(0x10000, 0x20000)]
        data = prepared('\n'.join(keys))
        self.assertEqual((len(data.alphabet), data.labels[-1]), (65536, 65535))
        self.assertEqual(lookup(data, keys[-1]), (True, 0))
        with self.assertRaises(DataError):
            prepared('\n'.join(keys + [chr(0x20000)]))
        with self.assertRaises(DataError):
            prepared('\n'.join('{}\t{}'.format(word, index) for index, word in enumerate(keys)), True)

    def test_rejects_inconsistent_prepared_records(self):
        base = dictionary.parse(b'a\t1\n', True)
        cases = [base._replace(entries=(('b', 1), ('a', 1))),
                 base._replace(entries=(('a', 1), ('a', 1))),
                 base._replace(entries=(('a', -1),)), base._replace(entries=(('a', True),)),
                 base._replace(entries=(('\ud800', 1),)), base._replace(entries=(('', 1),)),
                 base._replace(max_scalars=2), base._replace(max_utf16=2),
                 base._replace(valued=False)]
        for data in cases:
            with self.subTest(data=data), self.assertRaises(DataError):
                trie.prepare(data)

    def test_total_scalar_gate_precedes_policy_scan_and_shared_construction(self):
        class Unvisited(str):
            def __iter__(self):
                raise AssertionError('over-budget key was scanned')

        data = dictionary.Dictionary((dictionary.Entry('aa', None),
                                      dictionary.Entry(Unvisited('bb'), None)), False, 2, 2, ())
        with patch.object(trie.scalar_trie, 'DEFAULT_LIMITS', trie.scalar_trie.DEFAULT_LIMITS._replace(scalars=3)), \
                patch.object(trie.scalar_trie, 'prepare') as shared:
            with self.assertRaisesRegex(DataError, 'scalar budget'):
                trie.prepare(data)
            shared.assert_not_called()


if __name__ == '__main__':
    unittest.main()
