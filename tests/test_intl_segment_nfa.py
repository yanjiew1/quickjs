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

"""Thompson equivalence, occurrence ownership and original-position anchors."""
import itertools
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import segment_nfa as nfa
from intl_segment_reference import ends
from intl_segment_language_fixture import char, node


class SegmentNFATests(unittest.TestCase):
    def test_forward_and_reverse_equivalence_over_short_authored_alphabet(self):
        a, b = char('a'), char('b')
        languages = [node('epsilon'), char(''), a, char('ab'), node('concat', a, b),
                     node('alt', a, b), node('star', a), node('optional', b),
                     node('concat', node('star', node('alt', a, b)), a),
                     node('star', node('optional', a)),
                     node('concat', node('start'), node('star', a), b)]
        texts = [''.join(parts) for size in range(5) for parts in itertools.product('ab', repeat=size)]
        for language in languages:
            forward = nfa.compile_language(language)
            backward = nfa.compile_language(language, reverse=True)
            for text in texts:
                for start in range(len(text) + 1):
                    with self.subTest(kind=language.kind, text=text, start=start):
                        self.assertEqual(nfa.endpoints(forward, text, start), ends(language, text, start))
                        expected = frozenset(position for position in range(start + 1)
                                             if start in ends(language, text, position))
                        self.assertEqual(nfa.endpoints(backward, text, start, reverse=True), expected)

    def test_shared_dag_occurrences_receive_fresh_continuations(self):
        shared = node('concat', char('a'), char('b'))
        language = node('concat', shared, char('c'), shared)
        program = nfa.compile_language(language)
        self.assertEqual(nfa.endpoints(program, 'abcab', 0), frozenset((5,)))
        for text in ('ab', 'abc', 'abab', 'abccab'):
            self.assertEqual(nfa.endpoints(program, text, 0), frozenset())
        self.assertEqual(nfa.endpoints(program, 'abcabcab', 0), frozenset((5,)))
        self.assertEqual(len(program.predicates), 3)

    def test_anchored_parity_and_long_loops_do_not_use_a_context_cutoff(self):
        ri = char('R')
        language = node('concat', node('start'), node('star', node('concat', ri, ri)), ri)
        forward = nfa.compile_language(language)
        text = 'R' * 1001
        self.assertEqual(nfa.endpoints(forward, text, 0), frozenset(range(1, 1002, 2)))
        self.assertEqual(nfa.endpoints(forward, 'xRRR', 1), frozenset())
        backward = nfa.compile_language(language, reverse=True)
        self.assertEqual(nfa.endpoints(backward, text, 1001, reverse=True), frozenset((0,)))
        self.assertEqual(nfa.endpoints(backward, 'xRRR', 4, reverse=True), frozenset())

    def test_full_code_point_endpoints_and_original_start_position(self):
        language = node('concat', char('\0'), char('\n'), char('\ud800'), char('\U0010ffff'))
        program = nfa.compile_language(language)
        self.assertEqual(nfa.endpoints(program, '\0\n\ud800\U0010ffff', 0), frozenset((4,)))
        anchor = nfa.compile_language(node('concat', node('start'), char('a')))
        self.assertEqual(nfa.endpoints(anchor, '\na', 1), frozenset())

    def test_nullable_cycles_terminate_and_state_numbering_is_deterministic(self):
        language = node('star', node('optional', node('epsilon')))
        program = nfa.compile_language(language)
        self.assertEqual(nfa.endpoints(program, 'abc', 0), frozenset((0,)))
        self.assertEqual(program, nfa.compile_language(language))
        self.assertTrue(all(instruction.first is not None for instruction in program.instructions
                            if instruction.kind != 'accept'))

    def test_compilation_and_reference_budgets_fail_explicitly(self):
        language = node('concat', char('a'), char('b'))
        for limits, message in [(nfa.DEFAULT_LIMITS._replace(states=1), 'state budget'),
                                (nfa.DEFAULT_LIMITS._replace(work=1), '(work|pending) budget'),
                                (nfa.DEFAULT_LIMITS._replace(predicates=1), 'predicate budget'),
                                (nfa.DEFAULT_LIMITS._replace(retained_pairs=1), 'retained range budget')]:
            with self.subTest(message=message), self.assertRaisesRegex(DataError, message):
                nfa.compile_language(language, limits=limits)
        # This DAG is small, but its Thompson occurrences are exponential.
        doubled = node('optional', char('a'))
        for _ in range(20):
            doubled = node('concat', doubled, doubled)
        with self.assertRaisesRegex(DataError, 'budget'):
            nfa.compile_language(doubled)
        program = nfa.compile_language(language)
        with self.assertRaisesRegex(DataError, 'reference work budget'):
            nfa.endpoints(program, 'ab', 0, work_limit=1)
        nested = char('a')
        for _ in range(100):
            nested = node('alt', nested, char('b'))
        with self.assertRaisesRegex(DataError, 'work budget'):
            nfa.compile_language(nested, limits=nfa.DEFAULT_LIMITS._replace(work=1000))
        self.assertEqual(nfa.endpoints(nfa.compile_language(nested), 'a', 0), frozenset((1,)))
        with self.assertRaises(DataError):
            nfa.compile_language(language, limits=nfa.DEFAULT_LIMITS._replace(states=True))
        with self.assertRaises(DataError):
            nfa.compile_language(language, reverse=1)

    def test_invalid_language_shapes_and_reference_inputs(self):
        for language in (node('unknown'), node('concat'), node('star'),
                         node('epsilon', char('a')), node('char')):
            with self.subTest(language=language.kind), self.assertRaises(DataError):
                nfa.compile_language(language)
        program = nfa.compile_language(char('a'))
        for start in (-1, 2, True):
            with self.subTest(start=start), self.assertRaises(DataError):
                nfa.endpoints(program, 'a', start)


if __name__ == '__main__':
    unittest.main()
