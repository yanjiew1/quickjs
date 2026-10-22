#!/usr/bin/env python3
"""Earlier definition snapshots and segmentation-specific parent selection."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from parents import ParentGraph
from segment_sources import Definition, Segmentation
import segment_variables as variables


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
        result = variables.capture(records, graph(), 'ja', 'WordBreak')
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
        result = variables.capture(records, graph(), 'en-GB', 'WordBreak')
        self.assertEqual(result.bindings[-1].expression, 'en')
        self.assertEqual(result.bindings[-1].locale, 'en')
        self.assertEqual(variables.capture(records, graph(), 'en-GB', 'SentenceBreak'),
                         variables.Environment((), ()))

    def test_unknown_expression_references_remain_for_the_semantic_compiler(self):
        # Raw capture must not guess a missing variable or turn source regex
        # syntax into a recursively expanded Python regular expression.
        result = variables.capture((row('root', ('$A', '$NotDefined')),), graph(), 'ja', 'WordBreak')
        self.assertEqual(result.bindings[0].expression, '$NotDefined')
        self.assertEqual(result.bindings[0].environment, ())

    def test_reject_unknown_or_ambiguous_source_and_component(self):
        for records, locale, kind in [
                ((row('root'), row('root')), 'en', 'WordBreak'),
                ((row('xx'),), 'en', 'WordBreak'),
                ((row('root')._replace(kind='LineBreak'),), 'en', 'WordBreak'),
                ((), 'xx', 'WordBreak'), ((), 'en', 'LineBreak')]:
            with self.subTest(records=records, locale=locale, kind=kind), self.assertRaises(DataError):
                variables.capture(records, graph(), locale, kind)
        no_component = ParentGraph(('root', 'en'), {}, {}, {'und': 'en-Latn-US'}, (), False)
        with self.assertRaises(DataError):
            variables.capture((), no_component, 'en', 'WordBreak')


if __name__ == '__main__':
    unittest.main()
