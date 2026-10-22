#!/usr/bin/env python3
"""Closed refinement permissions and data-driven sentence context classes."""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
from segment_alphabet import classify
from segment_inheritance import Binding, InheritedRule, VariableEnvironment
from segment_lower import lower_rules
from segment_rules import prepare_rules, prepare_variables
import segment_profile as profiles


def source(kind, definitions, rows):
    bindings, visible = [], {}
    for name, text in definitions:
        bindings.append(Binding(name, text, 'root', 'fixture.xml', tuple(sorted(visible.items()))))
        visible[name] = len(bindings) - 1
    variables = prepare_variables(VariableEnvironment(tuple(bindings), tuple(sorted(visible.items()))))
    rules = tuple(InheritedRule(key, text, locale, 'fixture.xml') for key, text, locale in rows)
    return lower_rules(prepare_rules(rules, variables, kind), variables, {}.__getitem__)


SENTENCE_VARIABLES = (('$SATerm', '[.!]'), ('$Close', r'[\u0029]'), ('$Sp', r'[\u0020]'),
                      ('$ParaSep', r'[\u000A]'), ('$Extend', '[~]'), ('$Format', '[`]'))
SENTENCE_RULES = (('4', '$ParaSep ÷', 'root'),
                  ('5', '(?<X>[^$ParaSep]) ($Extend | $Format)* → ${X}', 'root'),
                  ('11', '$SATerm $Close* $Sp* $ParaSep? ÷', 'root'), ('998', '× .', 'root'))


class SegmentProfileTests(unittest.TestCase):
    def test_word_whitelist_does_not_authorize_neighbors_or_unreviewed_origins(self):
        rows = (('3', 'a × b', 'root'), ('4', 'a × b', 'root'), ('5', 'a × b', 'root'),
                ('5', 'a × b', 'fr'),
                ('13.2', 'a × b', 'root'), ('13.15', 'a × b', 'root'),
                ('13.3', 'a × b', 'root'), ('13.3', 'a × b', 'ja'),
                ('15', 'a × b', 'root'), ('16', 'a × b', 'root'))
        profile = profiles.build('WordBreak', source('WordBreak', (), rows), {}.__getitem__)
        self.assertEqual(profile.roles, (0, 0, 1, 0, 1, 0, 0, 1, 0, 0))
        self.assertFalse(any(profile.class_flags))
        self.assertEqual(profile.requirements, profiles.NEEDS_DICTIONARY |
                         profiles.NEEDS_WORD_STATUS | profiles.NEEDS_GRAPHEMES)

    def test_sentence_context_is_homogeneous_for_every_class(self):
        lowered = source('SentenceBreak', SENTENCE_VARIABLES, SENTENCE_RULES)
        profile = profiles.build('SentenceBreak', lowered, {}.__getitem__)
        self.assertEqual(profile.roles, (0, 0, profiles.SENTENCE_SOFT, 0))
        for text, expected in [('.', profiles.TERM), ('!', profiles.TERM), (')', profiles.CLOSE),
                               (' ', profiles.SPACE), ('\n', profiles.SEPARATOR),
                               ('~', profiles.IGNORED), ('`', profiles.IGNORED), ('A', 0)]:
            index = classify(profile.program.alphabet, ord(text))
            self.assertEqual(profile.class_flags[index], expected)
        self.assertIs(profile.program.lowered, lowered)
        self.assertEqual(profile.requirements, profiles.NEEDS_SUPPRESSIONS | profiles.NEEDS_GRAPHEMES)

    def test_graphemes_are_preserved_without_refinement_requirements(self):
        profile = profiles.build('GraphemeClusterBreak',
                                 source('GraphemeClusterBreak', (), (('5', 'a × b', 'root'),)),
                                 {}.__getitem__)
        self.assertEqual(profile.roles, (profiles.PRESERVE,))
        self.assertEqual(profile.requirements, 0)
        self.assertFalse(any(profile.class_flags))

    def test_changed_actions_missing_context_and_overlapping_categories_fail(self):
        with self.assertRaisesRegex(DataError, 'lexical rule action'):
            profiles.build('WordBreak', source('WordBreak', (), (('5', 'a ÷ b', 'root'),)), {}.__getitem__)
        changed = tuple((key, text.replace('÷', '×') if key == '11' else text, locale)
                        for key, text, locale in SENTENCE_RULES)
        with self.assertRaisesRegex(DataError, 'sentence rule 11 action'):
            profiles.build('SentenceBreak', source('SentenceBreak', SENTENCE_VARIABLES, changed), {}.__getitem__)
        with self.assertRaisesRegex(DataError, 'missing.*context variable'):
            profiles.build('SentenceBreak', source('SentenceBreak', (), ()), {}.__getitem__)
        changed = SENTENCE_VARIABLES + (('$Close', '[.!]'),)
        with self.assertRaisesRegex(DataError, 'overlapping.*context'):
            profiles.build('SentenceBreak', source('SentenceBreak', changed, SENTENCE_RULES), {}.__getitem__)
        with self.assertRaisesRegex(DataError, 'ignore descriptor'):
            profiles.build('SentenceBreak', source('SentenceBreak', SENTENCE_VARIABLES,
                                                   (SENTENCE_RULES[0], SENTENCE_RULES[2])), {}.__getitem__)

    def test_profile_identity_includes_policy_beyond_base_tables(self):
        profile = profiles.build('WordBreak', source('WordBreak', (), (('5', 'a × b', 'root'),)), {}.__getitem__)
        self.assertNotEqual(profiles.profile_key(profile), profiles.profile_key(profile._replace(roles=(0,))))
        self.assertNotEqual(profiles.profile_key(profile), profiles.profile_key(profile._replace(requirements=0)))
        with self.assertRaises(DataError):
            profiles.build('LineBreak', profile.program.lowered, {}.__getitem__)

    def test_unreviewed_same_id_sentence_override_remains_protected(self):
        rows = tuple((key, text, 'fr' if key == '11' else locale) for key, text, locale in SENTENCE_RULES)
        profile = profiles.build('SentenceBreak', source('SentenceBreak', SENTENCE_VARIABLES, rows), {}.__getitem__)
        self.assertEqual(profile.roles, (profiles.PRESERVE,) * len(rows))

    def test_sentence_context_uses_one_separately_bounded_evaluator(self):
        lowered = source('SentenceBreak', SENTENCE_VARIABLES, SENTENCE_RULES)
        with patch.object(profiles, 'CONTEXT_LIMITS', profiles.CONTEXT_LIMITS._replace(steps=1)):
            with self.assertRaisesRegex(DataError, 'set work budget'):
                profiles.build('SentenceBreak', lowered, {}.__getitem__)


if __name__ == '__main__':
    unittest.main()
