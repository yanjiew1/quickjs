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

"""Typed CLDR expressions, frozen references and bounded source diagnostics."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import segment_expression as expression


def parse(text, visible=(), arena=(), limits=expression.DEFAULT_LIMITS):
    return expression.parse_expression(text, visible, arena, limits)


def inside(node):
    return node.children[0] if node.kind == 'bracket' else node


class SegmentExpressionTests(unittest.TestCase):
    def test_set_and_sequence_types_survive_grouping_and_aliases(self):
        a = parse(r'\p{Word_Break=ALetter}')
        b = parse('(a | b)')
        arena = (a, b, parse('ab'), parse('a*'), parse('a?'))
        names = tuple(('$' + name, i) for i, name in enumerate('ABCDE'))
        for i, name in enumerate('ABCDE'):
            result = parse('($' + name + ')', names, arena)
            self.assertEqual(result.value, i)
            self.assertEqual(expression.is_set(result), i < 2)
        result = inside(parse('[$A $B]', names, arena))
        self.assertEqual(result.kind, 'union')
        self.assertEqual([node.value for node in result.children], [0, 1])
        for name in 'CDE':
            with self.subTest(name=name), self.assertRaisesRegex(DataError, 'sequence variable'):
                parse('[$' + name + ']', names, arena)

    def test_references_capture_slots_without_expanding_trees(self):
        a = parse('a')
        b = parse('$A', (('$A', 0),), (a,))
        c = parse('($A $B*)', (('$A', 0), ('$B', 1)), (a, b))
        self.assertEqual(c.kind, 'concat')
        self.assertEqual(c.children[0].value, 0)
        self.assertEqual(c.children[1].children[0].value, 1)
        # A final self-binding is sequence-valued; its older set slot remains
        # set-valued and usable in the earlier frozen alias.
        current = parse('$A', (('$A', 2),), (a, b, c))
        self.assertEqual(current.kind, 'seq_ref')
        self.assertEqual(b.kind, 'set_ref')
        with self.assertRaisesRegex(DataError, r'unknown variable \$FE'):
            parse('($A $FE*)', (('$A', 0),), (a,))

    def test_complement_structure_and_equal_precedence_are_preserved(self):
        result = inside(parse('[[a][b]-[c][d]]'))
        self.assertEqual(result.kind, 'union')
        difference, d = result.children
        self.assertEqual(difference.kind, 'difference')
        self.assertEqual(difference.children[0].kind, 'union')
        self.assertEqual(inside(d).value, ord('d'))
        result = parse('[^a b]*')
        self.assertEqual(result.kind, 'star')
        negative = inside(result.children[0])
        self.assertEqual(negative.kind, 'complement')
        self.assertEqual(negative.children[0].kind, 'union')

    def test_any_dot_start_epsilon_and_empty_set_are_distinct(self):
        self.assertEqual(parse('.').kind, 'any')
        self.assertEqual(inside(parse('[.]')).value, ord('.'))
        self.assertEqual(parse(r'\u002E').value, ord('.'))
        self.assertEqual(parse('^').kind, 'start')
        self.assertEqual(inside(parse('[^.]')).kind, 'complement')
        self.assertEqual(parse('').kind, 'epsilon')
        self.assertEqual(inside(parse('[]')).kind, 'union')
        self.assertTrue(expression.is_set(parse('[]')))
        self.assertFalse(expression.is_set(parse('()')))

    def test_escaped_metacharacters_remain_literal_in_both_contexts(self):
        for escape, value in [(r'\u002D', '-'), (r'\u005E', '^'),
                              (r'\u0024', '$'), (r'\u002E', '.'),
                              (r'\u005D', ']'), (r'\*', '*'), (r'\÷', '÷')]:
            for text in (escape, '[' + escape + ']'):
                with self.subTest(text=text):
                    result = inside(parse(text))
                    self.assertEqual(result.kind, 'literal')
                    self.assertEqual(result.value, ord(value))
        # Lone surrogates belong to the code-point set domain; unlike strict
        # UTF-8 dictionary keys, they can describe unpaired UTF-16 input.
        self.assertEqual(inside(parse(r'[\uD800]')).value, 0xd800)
        self.assertEqual(parse(r'\U0010FFFF').value, 0x10ffff)
        self.assertEqual(inside(parse('[×]')).value, ord('×'))

    def test_only_pattern_whitespace_is_ignored(self):
        self.assertEqual(parse(' \t\u200e a\u2028 ').value, ord('a'))
        self.assertEqual(parse('\u00a0').value, 0xa0)
        self.assertEqual(parse(r'\u0020').value, 0x20)
        with self.assertRaisesRegex(DataError, 'non-immediate'):
            parse('[ ^a]')

    def test_precedence_and_real_regional_indicator_shape(self):
        result = parse('^ (a a)* a | b?')
        self.assertEqual(result.kind, 'alt')
        left, right = result.children
        self.assertEqual([node.kind for node in left.children], ['start', 'star', 'literal'])
        self.assertEqual(left.children[1].children[0].kind, 'concat')
        self.assertEqual(right.kind, 'optional')

    def test_reject_unsupported_or_malformed_syntax_with_offsets(self):
        cases = ['[', '(', ')', 'a)', '[a', 'a**', 'a??', '^*',
                 '\\', r'\u123', r'\uZZZZ', r'\U00110000', r'\n',
                 r'\p{', r'\p{x-y}', r'\p {x}', r'\P{x}',
                 '[a-z]', '[a-[b]]', '[[a]b-[c]]', '[a&b]', 'a+', 'a{2}', '(?<X>a)', 'a × b',
                 'a ÷ b', 'a → b', '$', '\0']
        for text in cases:
            with self.subTest(text=text), self.assertRaises(expression.ExpressionError) as caught:
                parse(text)
            self.assertGreaterEqual(caught.exception.offset, 0)
            self.assertLessEqual(caught.exception.offset, len(text))
        try:
            parse('a | $Missing')
        except expression.ExpressionError as error:
            self.assertEqual(error.offset, 4)
        else:
            self.fail('unknown name accepted')

    def test_limits_bound_source_tokens_nodes_and_recursive_nesting(self):
        limits = expression.Limits(3, 3, 4, 2)
        self.assertEqual(parse('abc', limits=limits)._fields, expression.Expression._fields)
        for text, changed in [('abcd', limits), ('abc', limits._replace(tokens=2)),
                              ('abc', limits._replace(nodes=2)),
                              ('(((a)))', limits._replace(source=20, tokens=20, nodes=20))]:
            with self.subTest(text=text, limits=changed), self.assertRaisesRegex(DataError, 'budget'):
                parse(text, limits=changed)
        self.assertEqual(parse('((a))', limits=limits._replace(source=20, tokens=20)).value, ord('a'))
        for bad in [expression.Limits(1, 1, 1, 65), expression.Limits(True, 1, 1, 1)]:
            with self.assertRaises(DataError):
                parse('a', limits=bad)

    def test_environment_validation_and_deterministic_immutable_results(self):
        for names in [(('$A', -1),), (('$A', True),), (('A', 0),),
                      (('$A', 0), ('$A', 0))]:
            with self.subTest(names=names), self.assertRaises(DataError):
                parse('$A', names, (parse('a'),))
        first = parse(r'(\p{Hiragana}|[\u3005\u3007]) a*')
        self.assertEqual(first, parse(r'(\p{Hiragana}|[\u3005\u3007]) a*'))
        with self.assertRaises(AttributeError):
            first.kind = 'any'

    def test_subtraction_distinguishes_literal_and_singleton_set_aliases(self):
        arena = (parse('a'), parse('[a]'), parse('$A', (('$A', 0),), (parse('a'),)))
        names = (('$A', 0), ('$B', 1), ('$C', 2))
        self.assertEqual(inside(parse('[$B-[b]]', names, arena)).kind, 'difference')
        for text in ('[$A-[b]]', '[[b]-$A]', '[$C-[b]]'):
            with self.subTest(text=text), self.assertRaisesRegex(DataError, 'unsupported set range'):
                parse(text, names, arena)

    def test_flat_unions_stay_shallow_and_difference_depth_is_bounded(self):
        result = parse('[' + 'a ' * 600 + ']')
        self.assertEqual(result.height, 3)
        self.assertEqual(len(inside(result).children), 600)
        self.assertIn('bracket', repr(result))
        with self.assertRaisesRegex(DataError, 'tree depth budget'):
            parse('[[a]' + '-[b]' * 64 + ']')
        with self.assertRaisesRegex(DataError, 'nesting budget'):
            parse('(' * 65 + 'a' + ')' * 65)
        self.assertEqual(parse('(' * 64 + 'a' + ')' * 64).value, ord('a'))


if __name__ == '__main__':
    unittest.main()
