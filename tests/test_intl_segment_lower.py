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

"""Ignore-phase language semantics with a bounded independent short oracle."""
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from segment_inheritance import Binding, InheritedRule, VariableEnvironment
from segment_rules import prepare_rules, prepare_variables
import segment_lower as lower
from intl_segment_reference import ends, winning


def prepare(definitions=(), source_rules=()):
    definitions = (('$CR', r'[\u000D]'), ('$LF', r'[\u000A]'), ('$Newline', r'[\u0085]'),
                   ('$Extend', '[~]'), ('$Format', '[`]'), ('$ZWJ', '[z]'),
                   ('$ExtPict', '[E]'), ('$A', '[A]')) + definitions
    bindings, visible = [], {}
    for name, text in definitions:
        bindings.append(Binding(name, text, 'root', 'fixture.xml', tuple(sorted(visible.items()))))
        visible[name] = len(bindings) - 1
    variables = prepare_variables(VariableEnvironment(tuple(bindings), tuple(sorted(visible.items()))))
    rows = tuple(InheritedRule(key, text, 'root', 'fixture.xml') for key, text in source_rules)
    return variables, prepare_rules(rows, variables, 'WordBreak')


IGNORE = ('4', '(?<X>[^$CR $LF $Newline]) ($Extend | $Format | $ZWJ)* → ${X}')


def compile_rules(definitions=(), rows=(), limits=lower.DEFAULT_LIMITS):
    variables, prepared = prepare(definitions, rows)
    return lower.lower_rules(prepared, variables, {}.__getitem__, limits).rules


class SegmentLoweringTests(unittest.TestCase):
    def test_raw_rules_precede_ignore_and_later_rules_skip_only_at_phase(self):
        rules = compile_rules(rows=(('3.1', '($CR | $LF | $Newline) ÷'),
                                    ('3.3', '$ZWJ × $ExtPict'), IGNORE,
                                    ('5', '$A × $A')))
        self.assertEqual(winning(rules, 'zE', 1), (False, '3.3'))
        self.assertEqual(winning(rules, 'z~E', 1), (False, '4'))
        self.assertEqual(winning(rules, 'z~E', 2), (True, None))
        self.assertEqual(winning(rules, 'A~~A', 3), (False, '5'))
        self.assertEqual(winning(rules, '\r~~A', 1), (True, '3.1'))
        self.assertEqual(winning(rules, '\r~~A', 2), (False, '4'))
        self.assertEqual(winning(rules, '\r~~A', 3), (True, None))
        self.assertEqual(winning(rules, '~~A', 1), (False, '4'))
        self.assertEqual(winning(rules, '~~A', 2), (True, None))

    def test_positive_and_complemented_atoms_have_different_languages(self):
        rules = compile_rules((('$Extend', '[a]'), ('$Format', '[]'), ('$ZWJ', '[]')),
                              (IGNORE, ('5', '[^a] ×'), ('6', '[b] ×')))
        self.assertEqual(ends(rules[1].left, 'ba', 0), frozenset((1,)))
        self.assertEqual(ends(rules[2].left, 'ba', 0), frozenset((1, 2)))
        self.assertTrue(rules[1].left.character.negative)
        self.assertEqual(rules[2].left.kind, 'concat')

    def test_single_character_union_and_alias_are_one_padded_atom(self):
        rules = compile_rules((('$Both', '([A]|[B])'), ('$Alias', '$Both')),
                              (IGNORE, ('5', '$Alias ×')))
        left = rules[1].left
        self.assertEqual(left.kind, 'concat')
        self.assertEqual(left.children[0].kind, 'char')
        self.assertEqual(ends(left, 'A~~', 0), frozenset((1, 2, 3)))
        self.assertEqual(ends(left, 'B~', 0), frozenset((1, 2)))

    def test_same_declaration_can_be_used_raw_and_after_ignore(self):
        variables, prepared = prepare(source_rules=(('3', '$A ×'), IGNORE, ('5', '$A ×')))
        before = repr(variables), repr(prepared)
        result = lower.lower_rules(prepared, variables, {}.__getitem__)
        self.assertIs(result.variables, variables)
        result = result.rules
        self.assertEqual(result[0].left.kind, 'char')
        self.assertEqual(result[2].left.kind, 'concat')
        self.assertEqual((repr(variables), repr(prepared)), before)
        self.assertIs(result[1].source, prepared[1])
        self.assertTrue(result[1].ignore_replacement)
        self.assertFalse(result[0].ignore_replacement)

    def test_sequence_aliases_optional_loops_anchors_and_epsilon(self):
        rules = compile_rules((('$Pair', '$A $A'),),
                              (IGNORE, ('5', '^ $Pair* $A? ×')))
        self.assertEqual(ends(rules[1].left, 'A~A~~', 0), frozenset((0, 1, 2, 3, 4, 5)))
        self.assertEqual(ends(rules[1].left, 'AA', 1), frozenset())
        self.assertEqual(ends(rules[1].right, '', 0), frozenset((0,)))

    def test_positive_difference_stays_one_character_predicate(self):
        rules = compile_rules(rows=(IGNORE, ('5', '[[AB]-[A]] ×')))
        self.assertEqual(ends(rules[1].left, 'A~', 0), frozenset())
        self.assertEqual(ends(rules[1].left, 'B~', 0), frozenset((1, 2)))

    def test_unreviewed_mixed_negation_and_repeated_ignore_fail_with_source(self):
        for text in ('[[A][^B]] ×', '[[^A]-[B]] ×', '[^[^A]] ×'):
            with self.subTest(text=text), self.assertRaisesRegex(DataError, 'fixture.xml:root:5:.*unreviewed'):
                compile_rules(rows=(IGNORE, ('5', text)))
        with self.assertRaisesRegex(DataError, 'repeated.*ignore phase'):
            compile_rules(rows=(IGNORE, IGNORE))

    def test_limits_and_property_failure_propagation(self):
        for limits in (lower.DEFAULT_LIMITS._replace(nodes=1), lower.DEFAULT_LIMITS._replace(edges=1),
                       lower.DEFAULT_LIMITS._replace(height=1), lower.DEFAULT_LIMITS._replace(visits=1)):
            with self.subTest(limits=limits), self.assertRaisesRegex(DataError, 'budget'):
                compile_rules(rows=(IGNORE, ('5', '$A × $A')), limits=limits)
        with self.assertRaises(DataError):
            compile_rules(limits=lower.DEFAULT_LIMITS._replace(nodes=True))
        variables, prepared = prepare((('$P', r'\p{Unavailable}'),), (('5', '$P ×'),))
        def failure(query):
            raise DataError('property owner failed')
        with self.assertRaisesRegex(DataError, 'fixture.xml:root:5:.*property owner failed'):
            lower.lower_rules(prepared, variables, failure)
        with mock.patch.object(lower, 'MAX_RULES', 1), self.assertRaisesRegex(DataError, 'rule budget'):
            compile_rules(rows=(('1', 'a × b'), ('2', 'b ÷ c')))

    def test_short_oracle_shares_dag_work_and_has_a_cumulative_limit(self):
        definitions = [('$Pair', 'a?')]
        for _ in range(20):
            definitions.append(('$Pair', '$Pair $Pair'))
        rule = compile_rules(tuple(definitions), (('1', '$Pair ×'),))[0]
        self.assertEqual(ends(rule.left, '', 0), frozenset((0,)))
        with self.assertRaisesRegex(ValueError, 'reference work budget'):
            ends(rule.left, '', 0, work_limit=1)


if __name__ == '__main__':
    unittest.main()
