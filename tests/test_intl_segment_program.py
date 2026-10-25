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

"""Aggregate program gates, mode-aware owners and first-rule selection."""
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from segment_dfa import acceptance
from segment_lower import Character, Language, LoweredProgram, LoweredRule
import segment_program as programs
from intl_segment_language_fixture import char, node


def lowered(rows):
    owner = object()
    rules = tuple(LoweredRule(object(), left, right, breaks, False) for left, right, breaks in rows)
    return LoweredProgram(rules, owner)


def decisions(program, text):
    masks = tuple(acceptance(item.dfa, program.alphabet, text, reverse=item.reverse)
                  for item in program.machines)
    result = []
    for position in range(len(text) + 1):
        selected = (True, None)
        if position != 0 and position != len(text):
            for index, rule in enumerate(program.rules):
                if masks[rule.left][position] and masks[rule.right][position]:
                    selected = rule.breaks, index
                    break
        result.append(selected)
    return tuple(result)


class SegmentProgramTests(unittest.TestCase):
    def test_priority_side_mapping_and_retained_source_context(self):
        source = lowered(((char('a'), char('b'), False),
                          (node('epsilon'), node('epsilon'), True),
                          (node('epsilon'), node('epsilon'), False)))
        program = programs.assemble(source)
        self.assertIs(program.lowered, source)
        self.assertIs(program.rules[0].source, source.rules[0])
        self.assertEqual(decisions(program, 'cabd'), ((True, None), (True, 1),
                                                    (False, 0), (True, 1), (True, None)))
        self.assertTrue(all(not program.machines[rule.left].reverse for rule in program.rules))
        self.assertTrue(all(program.machines[rule.right].reverse for rule in program.rules))
        self.assertGreater(program.totals.work, 0)

    def test_identical_modes_share_owners_but_direction_does_not(self):
        a = char('a')
        source = lowered(((a, a, False), (a, a, True)))
        program = programs.assemble(source)
        self.assertEqual(len(program.machines), 2)
        self.assertEqual(program.rules[0].left, program.rules[1].left)
        self.assertEqual(program.rules[0].right, program.rules[1].right)
        self.assertNotEqual(program.rules[0].left, program.rules[0].right)

    def test_bos_policy_is_part_of_identity_even_for_equal_tables(self):
        any_character = Language('char', (), Character(((0, 0x110000),), False, None), 1)
        all_text = node('star', any_character)
        source = lowered(((all_text, node('epsilon'), False),
                          (node('concat', node('start'), all_text), node('epsilon'), True)))
        program = programs.assemble(source)
        first = program.machines[program.rules[0].left]
        second = program.machines[program.rules[1].left]
        self.assertEqual(first.dfa.rows, second.dfa.rows)
        self.assertEqual(first.dfa.accepted, second.dfa.accepted)
        self.assertNotEqual(first.anchored, second.anchored)
        self.assertNotEqual(program.rules[0].left, program.rules[1].left)

    def test_semantic_key_excludes_provenance_and_construction_statistics(self):
        first = programs.assemble(lowered(((char('a'), char('b'), False),)))
        second = programs.assemble(lowered(((char('a'), char('b'), False),)))
        self.assertEqual(programs.semantic_key(first), programs.semantic_key(second))
        item = first.machines[0]
        changed = item._replace(dfa=item.dfa._replace(construction_work=999, subsets=()))
        altered = first._replace(machines=(changed,) + first.machines[1:])
        self.assertEqual(programs.semantic_key(first), programs.semantic_key(altered))
        self.assertNotEqual(programs.semantic_key(first), programs.semantic_key(
            programs.assemble(lowered(((char('a'), char('b'), True),)))))

    def test_empty_program_represents_default_break_without_side_tables(self):
        program = programs.assemble(lowered(()))
        self.assertEqual(program.machines, ())
        self.assertEqual(program.rules, ())
        self.assertEqual(decisions(program, 'ab'), ((True, None),) * 3)

    def test_aggregate_rule_storage_and_work_limits(self):
        source = lowered(((char('a'), char('b'), False), (char('c'), char('d'), True)))
        for limits in (programs.DEFAULT_LIMITS._replace(rules=1),
                       programs.DEFAULT_LIMITS._replace(nfa_states=1),
                       programs.DEFAULT_LIMITS._replace(dfa_states=1),
                       programs.DEFAULT_LIMITS._replace(cells=1),
                       programs.DEFAULT_LIMITS._replace(members=1),
                       programs.DEFAULT_LIMITS._replace(predicate_pairs=1),
                       programs.DEFAULT_LIMITS._replace(work=1)):
            with self.subTest(limits=limits), self.assertRaisesRegex(DataError, 'budget'):
                programs.assemble(source, limits)
        with self.assertRaises(DataError):
            programs.assemble(source, programs.DEFAULT_LIMITS._replace(work=True))

    def test_remaining_work_is_delegated_before_starting_an_expensive_component(self):
        source = lowered(((char('a'), char('b'), False),))
        original = programs.compile_language
        seen = []
        def compile(language, **options):
            seen.append(options['limits'].work)
            return original(language, **options)
        with mock.patch.object(programs, 'compile_language', compile):
            with self.assertRaisesRegex(DataError, 'budget'):
                programs.assemble(source, programs.DEFAULT_LIMITS._replace(work=2))
        self.assertEqual(seen, [1])  # One visit was already spent verifying the side.

    def test_retained_nfas_share_canonical_predicate_tuple_owners(self):
        a = char('a')
        source = lowered(((a, a, False), (a, a, True)))
        original, owners = programs.compile_machine, []
        def compile(program, alphabet, **options):
            owners.append(program.predicates[0])
            return original(program, alphabet, **options)
        with mock.patch.object(programs, 'compile_machine', compile):
            result = programs.assemble(source, programs.DEFAULT_LIMITS._replace(predicate_pairs=1))
        self.assertEqual(len(owners), 4)
        self.assertTrue(all(owner is owners[0] for owner in owners))
        self.assertEqual(result.totals.predicate_pairs, 1)

    def test_auxiliary_predicates_split_classes_without_changing_languages(self):
        source = lowered(((char('ab'), node('epsilon'), False),))
        program = programs.assemble(source, extra_predicates=(((97, 98),),))
        self.assertIn(((97, 98),), program.alphabet.predicates)
        self.assertEqual(decisions(program, 'ab'), ((True, None), (False, 0), (True, None)))
        with self.assertRaisesRegex(DataError, 'predicate-pair budget'):
            programs.assemble(source, programs.DEFAULT_LIMITS._replace(predicate_pairs=1),
                              extra_predicates=(((97, 98),),))


if __name__ == '__main__':
    unittest.main()
