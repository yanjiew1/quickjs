#!/usr/bin/env python3
"""Earlier definition snapshots and segmentation-specific parent selection."""
from pathlib import Path
from decimal import localcontext
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from parents import ParentGraph
from segment_sources import Definition, Segmentation
import segment_inheritance as inheritance


def graph():
    return ParentGraph(('root', 'en', 'en-GB', 'en-US', 'ja'),
                       {'en-GB': 'en-US'}, {'segmentations': {}},
                       {'und': 'en-Latn-US'}, (), False)


def row(locale, *definitions):
    return Segmentation(locale, 'WordBreak', tuple(Definition(*value) for value in definitions),
                        None, None, 'common/segments/' + locale + '.xml')


class SegmentVariableTests(unittest.TestCase):
    def test_redefinitions_capture_previous_own_and_other_bindings(self):
        records = (row('root', ('$A', 'a'), ('$B', '$A')),
                   row('ja', ('$A', '($A $B*)'), ('$B', '$A')))
        original = repr(records)
        result = inheritance.capture_variables(records, graph(), 'ja', 'WordBreak')
        self.assertEqual(result.visible, (('$A', 2), ('$B', 3)))
        self.assertEqual(result.bindings[0].environment, ())
        self.assertEqual(result.bindings[1].environment, (('$A', 0),))
        self.assertEqual(result.bindings[2].environment, (('$A', 0), ('$B', 1)))
        self.assertEqual(result.bindings[3].environment, (('$A', 2), ('$B', 1)))
        self.assertEqual(result.bindings[2].expression, '($A $B*)')
        self.assertEqual(result.bindings[2].source, 'common/segments/ja.xml')
        self.assertEqual(repr(records), original)
        for slot, binding in enumerate(result.bindings):
            self.assertTrue(all(previous < slot for name, previous in binding.environment))

    def test_component_parents_empty_groups_and_kind_isolation(self):
        records = (row('root', ('$A', 'root')), row('en', ('$A', 'en')),
                   row('en-US', ('$A', 'US')), row('en-GB'))
        result = inheritance.capture_variables(records, graph(), 'en-GB', 'WordBreak')
        self.assertEqual(result.bindings[-1].expression, 'en')
        self.assertEqual(result.bindings[-1].locale, 'en')
        self.assertEqual(inheritance.capture_variables(records, graph(), 'en-GB', 'SentenceBreak'),
                         inheritance.VariableEnvironment((), ()))

    def test_unknown_expression_references_remain_for_the_semantic_compiler(self):
        # Raw capture must not guess a missing variable or turn source regex
        # syntax into a recursively expanded Python regular expression.
        result = inheritance.capture_variables((row('root', ('$A', '$NotDefined')),), graph(), 'ja', 'WordBreak')
        self.assertEqual(result.bindings[0].expression, '$NotDefined')
        self.assertEqual(result.bindings[0].environment, ())

    def test_reject_unknown_or_ambiguous_source_and_component(self):
        for records, locale, kind in [
                ((row('root'), row('root')), 'en', 'WordBreak'),
                ((row('xx'),), 'en', 'WordBreak'),
                ((row('root')._replace(kind='LineBreak'),), 'en', 'WordBreak'),
                ((), 'xx', 'WordBreak'), ((), 'en', 'LineBreak')]:
            with self.subTest(records=records, locale=locale, kind=kind), self.assertRaises(DataError):
                inheritance.capture_variables(records, graph(), locale, kind)
        no_component = ParentGraph(('root', 'en'), {}, {}, {'und': 'en-Latn-US'}, (), False)
        with self.assertRaises(DataError):
            inheritance.capture_variables((), no_component, 'en', 'WordBreak')


def rule_row(locale, *rules):
    return row(locale)._replace(rules=tuple(Definition(*value) for value in rules))


class SegmentRuleTests(unittest.TestCase):
    def test_numeric_order_override_deletion_and_source_identity(self):
        records = (rule_row('root', ('10.2', 'c × d'), ('10.1', 'a × b'), ('3', 'x ÷ y')),
                   rule_row('en', ('10.15', 'b × c'), ('10.10', 'a ÷ b')),
                   rule_row('en-GB', ('3', '')),
                   rule_row('en-US', ('1', 'not the segmentation parent')))
        result = inheritance.ordered_rules(records, graph(), 'en-GB', 'WordBreak')
        self.assertEqual([rule.identifier for rule in result], ['10.10', '10.15', '10.2'])
        self.assertEqual([rule.expression for rule in result], ['a ÷ b', 'b × c', 'c × d'])
        self.assertEqual([rule.locale for rule in result], ['en', 'en', 'root'])
        self.assertEqual(result[0].source, 'common/segments/en.xml')

    def test_ordering_never_rounds_rule_identifiers(self):
        first = '1.000000000000000000000000000001'
        second = '1.000000000000000000000000000002'
        with localcontext() as context:
            context.prec = 1
            result = inheritance.ordered_rules(
                (rule_row('root', (second, 'b ÷ c'), (first, 'a ÷ b')),), graph(), 'en', 'WordBreak')
        self.assertEqual([rule.identifier for rule in result], [first, second])

    def test_empty_containers_inherit_but_empty_rule_contents_delete(self):
        records = (rule_row('root', ('3', 'a × b')), rule_row('en'))
        self.assertEqual(len(inheritance.ordered_rules(records, graph(), 'en', 'WordBreak')), 1)
        self.assertEqual(inheritance.ordered_rules(records, graph(), 'en', 'SentenceBreak'), ())
        records = (records[0], rule_row('en', ('3', '\n  \t'), ('999', '')))
        self.assertEqual(inheritance.ordered_rules(records, graph(), 'en', 'WordBreak'), ())
        records = (rule_row('root', ('3', ' a × b '), ('4', '\u00a0')),)
        result = inheritance.ordered_rules(records, graph(), 'en', 'WordBreak')
        self.assertEqual([rule.expression for rule in result], [' a × b ', '\u00a0'])

    def test_reject_invalid_or_ambiguous_numeric_identifiers(self):
        for rules in [(('1', 'a'), ('1.00', 'b')), (('bad', 'a'),)]:
            with self.subTest(rules=rules), self.assertRaises(DataError):
                inheritance.ordered_rules((rule_row('root', *rules),), graph(), 'en', 'WordBreak')


if __name__ == '__main__':
    unittest.main()
