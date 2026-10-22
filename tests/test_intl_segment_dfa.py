#!/usr/bin/env python3
"""Restarted suffix/prefix matching and anchored DFA equivalence, not UTF-16 IO."""
import itertools
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from segment_alphabet import partition
from segment_lower import Character, Language
from segment_nfa import compile_language, endpoints
import segment_dfa as dfa


def node(kind, *children):
    return Language(kind, tuple(children), None, 1 + max((child.height for child in children), default=0))


def char(text):
    values, ranges = sorted(set(map(ord, text))), []
    for value in values:
        if ranges and ranges[-1][1] == value:
            ranges[-1] = ranges[-1][0], value + 1
        else:
            ranges.append((value, value + 1))
    return Language('char', (), Character(tuple(ranges), False, None), 1)


def machine(language, side, limits=dfa.DEFAULT_LIMITS):
    production = dfa.production_side(language, side)
    program = compile_language(production.language, reverse=production.reverse)
    alphabet = partition(program.predicates)
    result = dfa.compile_machine(program, alphabet, restart=not production.anchored, limits=limits)
    return result, alphabet


class SegmentDFATests(unittest.TestCase):
    def test_short_exhaustive_left_suffix_and_right_prefix_equivalence(self):
        a, b = char('a'), char('b')
        languages = [node('epsilon'), char(''), a, char('ab'), node('concat', a, b),
                     node('alt', a, b), node('star', a), node('optional', b),
                     node('concat', node('star', node('alt', a, b)), a),
                     node('star', node('optional', a))]
        texts = [''.join(value) for size in range(5) for value in itertools.product('ab', repeat=size)]
        for language in languages:
            reference = compile_language(language)
            left, left_alphabet = machine(language, 'left')
            right, right_alphabet = machine(language, 'right')
            for text in texts:
                expected_left = tuple(any(position in endpoints(reference, text, start)
                                          for start in range(position + 1))
                                      for position in range(len(text) + 1))
                expected_right = tuple(bool(endpoints(reference, text, position))
                                       for position in range(len(text) + 1))
                with self.subTest(kind=language.kind, text=text):
                    self.assertEqual(dfa.acceptance(left, left_alphabet, text), expected_left)
                    self.assertEqual(dfa.acceptance(right, right_alphabet, text, reverse=True), expected_right)

    def test_absolute_left_anchor_seeds_only_at_original_bos(self):
        ri = char('R')
        language = node('concat', node('start'), node('star', node('concat', ri, ri)), ri)
        program, alphabet = machine(language, 'left')
        text = 'R' * 1001
        accepted = dfa.acceptance(program, alphabet, text)
        self.assertEqual(accepted, tuple(bool(position % 2) for position in range(1002)))
        self.assertEqual(dfa.acceptance(program, alphabet, 'xRRR'), (False,) * 5)
        self.assertEqual(dfa.acceptance(program, alphabet, '\nRRR'), (False,) * 5)
        self.assertFalse(program.restart)
        self.assertEqual(language.children[0].kind, 'start')  # Original proof language survives.

    def test_long_right_context_runs_backward_without_a_lookahead_cutoff(self):
        # An authored SB8-shaped prefix needs an arbitrarily distant final L.
        language = node('concat', node('star', char('x')), char('L'))
        program, alphabet = machine(language, 'right')
        text = 'x' * 5000 + 'L'
        self.assertEqual(dfa.acceptance(program, alphabet, text, reverse=True),
                         (True,) * len(text) + (False,))
        self.assertFalse(any(dfa.acceptance(program, alphabet, 'x' * 5000, reverse=True)))

    def test_nullable_dead_states_and_complete_deterministic_rows(self):
        for language in (node('epsilon'), char(''), node('star', node('optional', char('a')))):
            program, alphabet = machine(language, 'left')
            self.assertEqual(program, machine(language, 'left')[0])
            self.assertEqual(len(program.rows), len(program.accepted))
            self.assertTrue(all(len(row) == len(alphabet.masks) for row in program.rows))
            self.assertTrue(all(0 <= target < len(program.rows) for row in program.rows for target in row))
        anchored = node('concat', node('start'), node('epsilon'))
        program, alphabet = machine(anchored, 'left')
        self.assertEqual(dfa.acceptance(program, alphabet, 'aaa'), (True, False, False, False))

    def test_global_alphabet_retains_overlaps_and_other_machines_predicates(self):
        language = node('concat', char('ab'), char('bc'))
        nfa = compile_language(language)
        alphabet = partition(nfa.predicates + (char('z').character.ranges,))
        program = dfa.compile_machine(nfa, alphabet)
        self.assertEqual(dfa.acceptance(program, alphabet, 'zzabc'), (False, False, False, False, True, True))
        with self.assertRaisesRegex(DataError, 'absent from alphabet'):
            dfa.compile_machine(nfa, partition(()))

    def test_reject_unreviewed_production_anchor_positions(self):
        start, a = node('start'), char('a')
        for language, side in [(start, 'left'), (node('concat', start, a), 'right'),
                               (node('concat', a, start), 'left'),
                               (node('star', start), 'left'),
                               (node('concat', node('start', a), a), 'left'),
                               (node('concat', start._replace(character=a.character), a), 'left'),
                               (node('concat', start, start, a), 'left')]:
            with self.subTest(side=side, kind=language.kind), self.assertRaisesRegex(DataError, 'anchor'):
                dfa.production_side(language, side)
        with self.assertRaisesRegex(DataError, 'start assertion'):
            dfa.compile_machine(compile_language(node('concat', start, a)), partition((a.character.ranges,)))
        with mock.patch.object(dfa, 'MAX_ANCHOR_VISITS', 2), self.assertRaisesRegex(DataError, 'anchor.*budget'):
            dfa.production_side(node('concat', a, a, a), 'left')

    def test_state_cell_member_and_work_limits(self):
        language = node('concat', char('a'), char('b'))
        for limits, message in [(dfa.DEFAULT_LIMITS._replace(states=1), 'state budget'),
                                (dfa.DEFAULT_LIMITS._replace(cells=1), 'transition budget'),
                                (dfa.DEFAULT_LIMITS._replace(members=1), 'subset-member budget'),
                                (dfa.DEFAULT_LIMITS._replace(work=1), 'work budget')]:
            with self.subTest(message=message), self.assertRaisesRegex(DataError, message):
                machine(language, 'left', limits)
        with self.assertRaisesRegex(DataError, 'invalid.*limits'):
            machine(language, 'left', dfa.DEFAULT_LIMITS._replace(states=True))

    def test_code_point_domain_and_invalid_targets(self):
        language = char('\0\n\ud800\U0010ffff')
        program, alphabet = machine(language, 'left')
        self.assertEqual(dfa.acceptance(program, alphabet, '\0\n\ud800\U0010ffff'), (False, True, True, True, True))
        nfa = compile_language(char('a'))
        bad = nfa._replace(instructions=(nfa.instructions[0]._replace(first=999),) + nfa.instructions[1:])
        with self.assertRaisesRegex(DataError, 'invalid.*target'):
            dfa.compile_machine(bad, partition(nfa.predicates))


if __name__ == '__main__':
    unittest.main()
