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

"""Rule priority, historical environments and exact ignore-phase preparation."""
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from segment_inheritance import Binding, InheritedRule, VariableEnvironment
import segment_rules as rules


def environment(definitions):
    bindings, visible = [], {}
    for name, text in definitions:
        bindings.append(Binding(name, text, 'root', 'fixture.xml', tuple(sorted(visible.items()))))
        visible[name] = len(bindings) - 1
    return VariableEnvironment(tuple(bindings), tuple(sorted(visible.items())))


def source(text, identifier='4'):
    return InheritedRule(identifier, text, 'root', 'fixture.xml')


def variables(*extra):
    names = ('CR', 'LF', 'Newline', 'Extend', 'Format', 'ZWJ', 'ParaSep')
    return rules.prepare_variables(environment(tuple(('$' + name, '[' + chr(97 + i) + ']')
                                                     for i, name in enumerate(names)) + extra))


WB = '(?<X>[^$CR $LF $Newline]) ($Extend | $Format | $ZWJ)* → ${X}'
SB = '(?<X>[^$ParaSep]) ( $Extend | $Format )* → ${X}'


class SegmentRulePreparationTests(unittest.TestCase):
    def test_complete_historical_arena_and_contextual_unknown_name(self):
        captured = environment((('$A', '[a]'), ('$B', '$A'), ('$A', '($A $B*)')))
        prepared = rules.prepare_variables(captured)
        self.assertIs(prepared.bindings, captured.bindings)
        self.assertEqual(prepared.visible, (('$A', 2), ('$B', 1)))
        self.assertEqual(prepared.arena[1].value, 0)
        self.assertEqual(prepared.arena[2].children[0].value, 0)
        with self.assertRaisesRegex(DataError, r'fixture.xml:root:\$A.*unknown variable \$FE'):
            rules.prepare_variables(environment((('$A', '$FE'),)))

    def test_inconsistent_snapshots_and_final_map_are_rejected(self):
        captured = environment((('$A', 'a'), ('$B', '$A')))
        changed = captured.bindings[1]._replace(environment=())
        for corrupt in (captured._replace(bindings=(captured.bindings[0], changed)),
                        captured._replace(visible=(('$A', 1), ('$B', 1)))):
            with self.subTest(corrupt=corrupt), self.assertRaisesRegex(DataError, 'inconsistent'):
                rules.prepare_variables(corrupt)

    def test_ordinary_rules_keep_priority_original_source_and_offsets(self):
        sources = (source('^ (a a)* a × b', '10.15'), source('a ÷ b', '10.2'))
        result = rules.prepare_rules(sources, variables(), 'WordBreak')
        self.assertEqual([row.source.identifier for row in result], ['10.15', '10.2'])
        self.assertIs(result[0].source, sources[0])
        self.assertFalse(result[0].breaks)
        self.assertTrue(result[1].breaks)
        self.assertEqual(result[0].right_start, sources[0].expression.index('×') + 1)
        self.assertEqual(result[0].left.children[0].kind, 'start')

    def test_empty_sides_are_epsilon_not_any_or_empty_set(self):
        for text, side in [('× a', 'left'), ('a ÷', 'right')]:
            with self.subTest(text=text):
                result = rules.prepare_rule(source(text), variables(), 'WordBreak')
                self.assertEqual(getattr(result, side).kind, 'epsilon')
        result = rules.prepare_rule(source('[] × .'), variables(), 'WordBreak')
        self.assertEqual(result.left.kind, 'bracket')
        self.assertEqual(result.right.kind, 'any')

    def test_only_top_level_unescaped_markers_split_the_rule(self):
        for text in ('[×] ÷ [÷]', r'\× × \÷', r'\u00D7 × a'):
            with self.subTest(text=text):
                result = rules.prepare_rule(source(text), variables(), 'WordBreak')
                self.assertEqual(result.source.expression, text)
        for text in ('a', 'a × b ÷ c', '(a × b)', '([a] × b', '[a ÷ b', 'a) × b'):
            with self.subTest(text=text), self.assertRaises(DataError):
                rules.prepare_rule(source(text), variables(), 'WordBreak')

    def test_exact_word_and_sentence_ignores_remain_separate_records(self):
        for text, kind, identifier in [(WB, 'WordBreak', '4'), (SB, 'SentenceBreak', '5')]:
            with self.subTest(kind=kind):
                src = source(text, identifier)
                result = rules.prepare_rule(src, variables(), kind)
                self.assertIsInstance(result, rules.IgnoreRule)
                self.assertIs(result.source, src)
                self.assertEqual(result.excluded_start, text.index('[^'))
                self.assertEqual(result.ignored_start, text.index('(', text.index('])') + 2))
                self.assertNotEqual(result.excluded.kind, 'complement')
                self.assertEqual(result.ignored.kind, 'union')
        rows = (source('a × b', '3.3'), source(WB, '4'), source('a ÷ b', '5'))
        prepared = rules.prepare_rules(rows, variables(), 'WordBreak')
        self.assertEqual([type(row) for row in prepared],
                         [rules.BoundaryRule, rules.IgnoreRule, rules.BoundaryRule])

    def test_remap_near_misses_and_sequence_sets_are_not_general_captures(self):
        bad = [WB.replace('${X}', '${Y}'), WB.replace('?<X>', '?<Y>'),
               WB.replace('* →', '? →'), WB.replace('$ZWJ', '$CR'),
               WB.replace('→', '→ a'), WB + ' trailing']
        for text in bad:
            with self.subTest(text=text), self.assertRaisesRegex(DataError, 'unsupported segmentation remap'):
                rules.prepare_rule(source(text), variables(), 'WordBreak')
        for kind, identifier in [('SentenceBreak', '4'), ('WordBreak', '5'), ('GraphemeClusterBreak', '4')]:
            with self.subTest(kind=kind), self.assertRaises(DataError):
                rules.prepare_rule(source(WB, identifier), variables(), kind)
        with self.assertRaisesRegex(DataError, 'sequence-valued ignored set'):
            rules.prepare_rule(source(WB), variables(('$Extend', 'ab')), 'WordBreak')

    def test_right_error_reports_original_expression_offset(self):
        with self.assertRaisesRegex(DataError, r'fixture.xml:root:4.*original offset 5'):
            rules.prepare_rule(source('a ×  $Missing'), variables(), 'WordBreak')

    def test_aggregate_budgets_and_incomplete_results_are_not_published(self):
        captured = environment((('$A', 'a'), ('$B', 'b')))
        with mock.patch.object(rules, 'MAX_BINDINGS', 1), self.assertRaisesRegex(DataError, 'binding budget'):
            rules.prepare_variables(captured)
        with mock.patch.object(rules, 'MAX_NODES', 1), self.assertRaisesRegex(DataError, 'binding node budget'):
            rules.prepare_variables(captured)
        prepared = variables()
        sources = (source('a × b'), source('b ÷ c'))
        with mock.patch.object(rules, 'MAX_RULES', 1), self.assertRaisesRegex(DataError, 'rule budget'):
            rules.prepare_rules(sources, prepared, 'WordBreak')
        with mock.patch.object(rules, 'MAX_NODES', 1), self.assertRaisesRegex(DataError, 'rule node budget'):
            rules.prepare_rules(sources, prepared, 'WordBreak')
        limited = rules.DEFAULT_LIMITS._replace(source=2)
        with self.assertRaisesRegex(DataError, 'source budget'):
            rules.prepare_rule(source('a × b'), prepared, 'WordBreak', limited)
        with self.assertRaisesRegex(DataError, 'nesting budget'):
            rules.prepare_rule(source('(' * 65 + 'a' + ')' * 65 + ' × b'), prepared, 'WordBreak')
        with self.assertRaisesRegex(DataError, 'unsupported segmentation kind'):
            rules.prepare_rules(sources, prepared, 'LineBreak')
        with self.assertRaisesRegex(DataError, 'unsupported segmentation kind'):
            rules.prepare_rules((), prepared, 'LineBreak')


if __name__ == '__main__':
    unittest.main()
