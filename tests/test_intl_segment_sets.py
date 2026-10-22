#!/usr/bin/env python3
"""Independent finite-set oracle and retained segmentation expression structure."""
from pathlib import Path
import random
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from segment_expression import parse_expression
import segment_sets as sets


def ranges(values):
    result = []
    for value in sorted(values):
        if result and result[-1][1] == value:
            result[-1] = (result[-1][0], value + 1)
        else:
            result.append((value, value + 1))
    return tuple(result)


def finite(values):
    return {value for start, end in values for value in range(start, end)}


def evaluate(text, properties=None, limits=sets.DEFAULT_LIMITS):
    properties = {} if properties is None else properties
    return sets.SetEvaluator((), properties.__getitem__, limits).evaluate(parse_expression(text))


class SegmentSetTests(unittest.TestCase):
    def test_union_and_difference_against_independent_finite_oracle(self):
        randoms = random.Random(79049)
        union = parse_expression(r'[\p{Left}\p{Right}]')
        difference = parse_expression(r'[\p{Left}-\p{Right}]')
        for _ in range(250):
            left = {i for i in range(32) if randoms.randrange(2)}
            right = {i for i in range(32) if randoms.randrange(2)}
            properties = {'Left': ranges(left), 'Right': ranges(right)}
            evaluator = sets.SetEvaluator((), properties.__getitem__)
            self.assertEqual(finite(evaluator.evaluate(union)), left | right)
            self.assertEqual(finite(evaluator.evaluate(difference)), left - right)

    def test_full_code_point_complement_keeps_nul_lf_and_surrogates(self):
        self.assertEqual(evaluate('.'), ((0, 0x110000),))
        self.assertEqual(evaluate('[^]'), ((0, 0x110000),))
        self.assertEqual(evaluate('[]'), ())
        self.assertEqual(evaluate(r'[^\u000A]'), ((0, 10), (11, 0x110000)))
        self.assertEqual(evaluate(r'[\uD800\uDFFF]'), ((0xd800, 0xd801), (0xdfff, 0xe000)))
        self.assertEqual(evaluate(r'[^\u0000\U0010FFFF]'), ((1, 0x10ffff),))

    def test_difference_preserves_operator_order_and_partial_overlaps(self):
        self.assertEqual(evaluate('[[a][b]-[a][c]]'), ranges(map(ord, 'bc')))
        properties = {'Left': ((0, 2), (4, 6), (8, 12)), 'Right': ((1, 5), (9, 10))}
        self.assertEqual(evaluate(r'[\p{Left}-\p{Right}]', properties),
                         ((0, 1), (5, 6), (8, 9), (10, 12)))

    def test_query_cache_is_shared_without_replacing_source_polarity(self):
        queried = []
        def resolver(query):
            queried.append(query)
            return ((97, 99),)
        node = parse_expression(r'[[\p{Letter}]-[^\p{Letter}]]')
        original = repr(node)
        evaluator = sets.SetEvaluator((), resolver)
        self.assertEqual(evaluator.evaluate(node), ((97, 99),))
        self.assertEqual(evaluator.evaluate(parse_expression(r'\p{Letter}')), ((97, 99),))
        self.assertEqual(queried, ['Letter'])
        self.assertEqual(repr(node), original)

    def test_long_prior_binding_chain_is_evaluated_without_recursion(self):
        arena = [parse_expression('[a]')]
        for i in range(1000):
            arena.append(parse_expression('$A', (('$A', i),), tuple(arena)))
        evaluator = sets.SetEvaluator(arena, {}.__getitem__)
        self.assertEqual(evaluator.evaluate(arena[-1]), ((97, 98),))
        self.assertEqual(evaluator.retained_pairs, 1)

    def test_sequence_walker_retains_maximal_atoms_and_shares_arena_targets(self):
        arena = (parse_expression('a [^b]*'),)
        root = parse_expression('^ $A $A?', (('$A', 0),), arena)
        evaluator = sets.SetEvaluator(arena, {}.__getitem__)
        operands = evaluator.operands(root)
        self.assertEqual(len(operands), 2)
        self.assertIs(operands[0][0], arena[0].children[0])
        self.assertEqual(operands[0][1], ((97, 98),))
        self.assertEqual(operands[1][0].kind, 'bracket')
        self.assertEqual(operands[1][0].children[0].kind, 'complement')
        cyclic = parse_expression('$A', (('$A', 0),), arena)
        with self.assertRaisesRegex(DataError, 'cyclic.*sequence'):
            sets.SetEvaluator((cyclic,), {}.__getitem__).operands(cyclic)

    def test_empty_properties_are_valid_but_noncanonical_ranges_fail(self):
        self.assertEqual(evaluate(r'\p{Empty}', {'Empty': ()}), ())
        bad = [None, 'a', [(True, 2)], [(1.0, 2)], [(0, 0)], [(-1, 2)],
               [(1, 0x110001)], [(2, 3), (0, 1)], [(0, 2), (1, 3)],
               [(0, 1), (1, 2)], [(1, 2, 3)]]
        for value in bad:
            with self.subTest(value=value), self.assertRaises(DataError):
                evaluate(r'\p{Bad}', {'Bad': value})

    def test_property_failures_propagate_without_fallback(self):
        for error in (DataError('unknown property'), OSError('tool failed'), MemoryError()):
            def fail(query):
                raise error
            with self.subTest(error=type(error)), self.assertRaises(type(error)):
                sets.SetEvaluator((), fail).evaluate(parse_expression(r'\p{Missing}'))

    def test_reject_sequence_nodes_and_corrupt_or_cyclic_references(self):
        for text in ('ab', '', '^', 'a*'):
            with self.subTest(text=text), self.assertRaisesRegex(DataError, 'sequence'):
                evaluate(text)
        reference = parse_expression('$A', (('$A', 0),), (parse_expression('a'),))
        with self.assertRaisesRegex(DataError, 'invalid.*reference'):
            sets.SetEvaluator((), {}.__getitem__).evaluate(reference)
        with self.assertRaisesRegex(DataError, 'cyclic'):
            sets.SetEvaluator((reference,), {}.__getitem__).evaluate(reference)
        corrupt = [parse_expression('[a]')._replace(children=()),
                   parse_expression('a')._replace(children=(parse_expression('b'),)),
                   parse_expression('a')._replace(kind='unknown'),
                   parse_expression('[a]')._replace(children=(False,))]
        for node in corrupt:
            with self.subTest(node=node), self.assertRaises(DataError):
                sets.SetEvaluator((), {}.__getitem__).evaluate(node)
        sequence = parse_expression('$A', (('$A', 0),), (parse_expression('ab'),))
        with self.assertRaises(DataError):
            sets.SetEvaluator((False,), {}.__getitem__).operands(sequence)

    def test_budgets_cover_inputs_results_work_queries_and_nodes(self):
        properties = {'Left': ((0, 1), (4, 5)), 'Right': ((2, 3), (6, 7))}
        for text, limits, message in [
                (r'\p{Left}', sets.DEFAULT_LIMITS._replace(ranges=1), 'range budget'),
                (r'[\p{Left}\p{Right}]', sets.DEFAULT_LIMITS._replace(ranges=2), 'result budget'),
                (r'[\p{Left}\p{Right}]', sets.DEFAULT_LIMITS._replace(properties=1), 'query budget'),
                (r'\p{Left}', sets.DEFAULT_LIMITS._replace(retained_pairs=1), 'retained range budget'),
                ('[a b]', sets.DEFAULT_LIMITS._replace(steps=1), 'work budget'),
                ('[a b]', sets.DEFAULT_LIMITS._replace(nodes=1), 'node budget')]:
            with self.subTest(text=text, message=message), self.assertRaisesRegex(DataError, message):
                evaluate(text, properties, limits)
        with self.assertRaisesRegex(DataError, 'invalid.*limits'):
            sets.SetEvaluator((), {}.__getitem__, sets.DEFAULT_LIMITS._replace(nodes=True))
        evaluator = sets.SetEvaluator((), properties.__getitem__,
                                      sets.DEFAULT_LIMITS._replace(retained_pairs=1))
        with self.assertRaisesRegex(DataError, 'retained range budget'):
            evaluator.evaluate(parse_expression(r'\p{Left}'))
        self.assertEqual(evaluator.properties, {})
        self.assertEqual(evaluator.retained_pairs, 0)

    def test_repeated_cached_operand_walks_charge_cumulative_work(self):
        evaluator = sets.SetEvaluator((), {}.__getitem__, sets.DEFAULT_LIMITS._replace(steps=80))
        root = parse_expression('a b')
        with self.assertRaisesRegex(DataError, 'work budget'):
            for _ in range(100):
                evaluator.operands(root)


if __name__ == '__main__':
    unittest.main()
