#!/usr/bin/env python3
"""Required formatter locale closure, distinct from source inheritance."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import availability
from parents import ParentGraph


def graph(extra=(), likely=None):
    return ParentGraph({'root', 'sr', 'sr-Cyrl-RS', 'sr-Latn-RS'} | set(extra), {}, {},
                       likely or {'und': 'en-Latn-US', 'sr': 'sr-Cyrl-RS'}, (), False)


class AvailabilityTests(unittest.TestCase):
    def test_required_scriptless_region_prefers_likely_script(self):
        source = graph()
        result, aliases = availability.complete(source)
        self.assertEqual(aliases, (('sr-RS', 'sr-Cyrl-RS'),))
        self.assertEqual(tuple(result.chain('sr-RS')),
                         ('sr-RS', 'sr-Cyrl-RS', 'sr-Cyrl', 'sr', 'root'))
        self.assertNotIn('sr-RS', source.tags)
        self.assertEqual(source.general, {})
        self.assertEqual(result.source_locales, source.source_locales)
        for tag in source.tags:
            self.assertEqual(tuple(result.chain(tag)), tuple(source.chain(tag)))

    def test_region_likely_override_and_existing_source_precedence(self):
        result, aliases = availability.complete(graph(likely={
            'und': 'en-Latn-US', 'sr': 'sr-Cyrl-RS', 'sr-RS': 'sr-Latn-RS'}))
        self.assertEqual(aliases, (('sr-RS', 'sr-Latn-RS'),))
        result, aliases = availability.complete(graph(('sr-RS',)))
        self.assertEqual(aliases, ())
        self.assertEqual(result.parent('sr-RS'), 'sr')

    def test_stable_choice_without_matching_likely_script_and_idempotence(self):
        result, aliases = availability.complete(graph(likely={'und': 'en-Latn-US',
                                                              'sr': 'sr-Arab-RS'}))
        self.assertEqual(aliases, (('sr-RS', 'sr-Cyrl-RS'),))
        second, aliases = availability.complete(result)
        self.assertEqual(aliases, ())
        self.assertEqual(result.tags, second.tags)
        self.assertEqual(result.general, second.general)

    def test_variant_source_and_component_chains_are_preserved(self):
        source = ParentGraph({'root', 'sl-Latn-SI-rozaj'},
                             {'sl-Latn-SI-rozaj': 'root'},
                             {'collations': {'sl-Latn-SI': 'root'}},
                             {'und': 'en-Latn-US', 'sl': 'sl-Latn-SI'}, (), False)
        result, aliases = availability.complete(source)
        self.assertEqual(aliases, (('sl-SI', 'sl-Latn-SI'),))
        for tag in source.tags:
            for component in ('main', 'collations'):
                self.assertEqual(tuple(result.chain(tag, component)),
                                 tuple(source.chain(tag, component)))


if __name__ == '__main__':
    unittest.main()
