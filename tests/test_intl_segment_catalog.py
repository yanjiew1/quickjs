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

"""Final locale IDs, source/provenance sharing and bounded compilation work."""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from parents import ParentGraph
import availability
import segment_catalog as catalog
import segment_program_tables
import segment_suppression_tables
from segment_sources import Definition, Segmentation
from test_intl_segment_profile import SENTENCE_VARIABLES, SENTENCE_RULES


def sources():
    rows = [Segmentation('root', 'GraphemeClusterBreak', (), (Definition('5', 'a × b'),), None, 'root.xml'),
            Segmentation('root', 'WordBreak', (Definition('$A', 'a'),),
                         (Definition('5', '$A × b'),), None, 'root.xml'),
            Segmentation('root', 'SentenceBreak', tuple(Definition(*row) for row in SENTENCE_VARIABLES),
                         tuple(Definition(*row[:2]) for row in SENTENCE_RULES), ('Root.',), 'root.xml')]
    rows += [Segmentation('en', 'SentenceBreak', None, None, ('n. Chr.', 'Mr.'), 'en.xml'),
             Segmentation('en-US', 'SentenceBreak', None, None, ('US.',), 'en-US.xml'),
             Segmentation('fr', 'SentenceBreak', None, None, (), 'fr.xml'),
             Segmentation('ja', 'WordBreak', (Definition('$Unused', 'x'),), None, None, 'ja.xml')]
    return tuple(rows)


def graph():
    # The main exceptional parent must not redirect segmentation inheritance.
    return ParentGraph(('root', 'en', 'en-US', 'en-GB', 'fr', 'ja'),
                       {'en-GB': 'en-US'}, {'segmentations': {}}, {'und': 'en-Latn-US'}, (), False)


class SegmentCatalogTests(unittest.TestCase):
    def test_compile_distinct_streams_once_and_preserve_origins(self):
        tags = ('en-GB', 'fr', 'ja', 'root')
        with patch.object(catalog, 'build', wraps=catalog.build) as build:
            result = catalog.prepare(sources(), graph(), tags, {}.__getitem__)
        self.assertEqual(build.call_count, 4)  # Three root streams + unused ja binding.
        self.assertEqual(len(result.streams), 4)
        self.assertEqual(len(result.profiles), 3)  # Same executable word semantics.
        self.assertEqual(result.program_bindings, tuple((locale, kind, kind)
                                                       for locale in range(4) for kind in range(3)))
        self.assertNotEqual(result.stream_routes[1], result.stream_routes[7])
        self.assertEqual(result.suppression_bindings, ((0, 0), (1, 1), (2, 2), (3, 2)))
        origin = result.suppression_sources[result.suppression_origins[0]]
        self.assertEqual(origin.entries, ('n. Chr.', 'Mr.'))
        self.assertEqual(origin.origin, ('en', 'en.xml'))
        self.assertIsNone(result.lists[1])
        programs = segment_program_tables.encode(result.profiles, result.program_bindings, bytes(32), 4)
        lists = segment_suppression_tables.encode(result.lists, result.suppression_bindings, bytes(32), 4)
        self.assertEqual(segment_program_tables.decode(programs, 4).bindings, result.program_bindings)
        self.assertEqual(segment_suppression_tables.decode(lists, 4).bindings, result.suppression_bindings)

    def test_changed_binding_creates_new_execution_in_first_use_order(self):
        records = sources()[:-1] + (sources()[-1]._replace(variables=(Definition('$A', 'c'),)),)
        result = catalog.prepare(records, graph(), ('en-GB', 'ja', 'root'), {}.__getitem__)
        self.assertEqual(len(result.profiles), 4)
        self.assertEqual(result.program_bindings,
                         ((0, 0, 0), (0, 1, 1), (0, 2, 2),
                          (1, 0, 0), (1, 1, 3), (1, 2, 2),
                          (2, 0, 0), (2, 1, 1), (2, 2, 2)))

    def test_missing_and_explicit_empty_origins_share_only_execution(self):
        records = tuple(row._replace(suppressions=None) if row.locale in ('root', 'en') else row
                        for row in sources())
        result = catalog.prepare(records, graph(), ('en-GB', 'fr', 'root'), {}.__getitem__)
        self.assertEqual(result.lists, (None,))
        self.assertEqual(result.suppression_bindings, ((0, 0), (1, 0), (2, 0)))
        self.assertEqual(tuple(row.origin for row in result.suppression_sources), (None, ('fr', 'fr.xml')))
        self.assertEqual(result.suppression_origins, (0, 1, 0))

    def test_formatter_alias_keeps_segmentation_structural_parent(self):
        sparse = ParentGraph(('root', 'az', 'az-Arab-AZ'), {}, {'segmentations': {}},
                             {'und': 'en-Latn-US', 'az': 'az-Latn-AZ'}, (), False)
        complete, aliases = availability.complete(sparse)
        self.assertEqual(aliases, (('az-AZ', 'az-Arab-AZ'),))
        records = sources()[:3] + (
            Segmentation('az', 'WordBreak', (Definition('$A', 'b'),), None, None, 'az.xml'),
            Segmentation('az-Arab-AZ', 'WordBreak', (Definition('$A', 'c'),), None, None, 'az-Arab-AZ.xml'))
        result = catalog.prepare(records, complete, ('az-AZ', 'az-Arab-AZ', 'root'), {}.__getitem__)
        alias_stream = result.streams[result.stream_routes[1]]
        target_stream = result.streams[result.stream_routes[4]]
        self.assertEqual(alias_stream.variables.bindings[-1].locale, 'az')
        self.assertEqual(target_stream.variables.bindings[-1].locale, 'az-Arab-AZ')
        self.assertNotEqual(alias_stream.program, target_stream.program)

    def test_suppression_source_attempt_budget_precedes_equal_graph_work(self):
        records = tuple(row._replace(suppressions=('n. Chr.', 'Mr.')) if row.locale == 'fr' else row
                        for row in sources())
        with patch.object(catalog, 'MAX_SUPPRESSION_SOURCES', 1), \
                patch.object(catalog, 'prepare_suppressions', wraps=catalog.prepare_suppressions) as prepare:
            with self.assertRaisesRegex(DataError, 'suppression source budget'):
                catalog.prepare(records, graph(), ('en-GB', 'fr'), {}.__getitem__)
            self.assertEqual(prepare.call_count, 1)

    def test_inventory_and_attempt_budget_precede_compilation(self):
        for tags in ((), ['root'], ('root', 'fr'), ('root', 'root'), (1,), ('zz',)):
            with self.subTest(tags=tags), patch.object(catalog, 'build') as build:
                with self.assertRaises(DataError):
                    catalog.prepare(sources(), graph(), tags, {}.__getitem__)
                build.assert_not_called()
        with patch.object(catalog, 'MAX_STREAMS', 0), patch.object(catalog, 'build') as build:
            with self.assertRaisesRegex(DataError, 'distinct stream budget'):
                catalog.prepare(sources(), graph(), ('root',), {}.__getitem__)
            build.assert_not_called()
        with self.assertRaises(DataError):
            catalog.prepare(sources() + (sources()[0],), graph(), ('root',), {}.__getitem__)


if __name__ == '__main__':
    unittest.main()
