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

"""Pinned native refinement roles and sentence context, beside base machines.

These flags authorize later dictionary/suppression decisions; they do not
execute refinement or advertise native locale coverage on their own.
"""
from collections import namedtuple

from cldr_xml import DataError
from segment_program import DEFAULT_LIMITS, assemble, semantic_key
from segment_rules import IgnoreRule
from segment_sets import DEFAULT_LIMITS as CONTEXT_LIMITS, SetEvaluator
from segment_sources import KINDS

Profile = namedtuple('Profile', 'kind program roles class_flags requirements')
PRESERVE, WORD_LEXICAL, SENTENCE_SOFT = range(3)
NEEDS_DICTIONARY = 1
NEEDS_SUPPRESSIONS = 2
NEEDS_WORD_STATUS = 4
NEEDS_GRAPHEMES = 8
TERM, CLOSE, SPACE, SEPARATOR, IGNORED = (1, 2, 4, 8, 16)

# Explicit source IDs, never an open numeric interval that would authorize a
# newly added upstream rule. Japanese's two lexical rows have a separate origin.
WORD_LEXICAL_IDS = frozenset(('5', '6', '7', '7.1', '7.2', '7.3', '8', '9',
                              '10', '11', '12', '13', '13.1', '13.2'))
JAPANESE_LEXICAL_IDS = frozenset(('13.3', '13.4'))


def _roles(kind, rules):
    result = []
    for rule in rules:
        source = rule.source.source
        lexical = ((source.locale == 'root' and source.identifier in WORD_LEXICAL_IDS) or
                   (source.locale == 'ja' and source.identifier in JAPANESE_LEXICAL_IDS))
        role = PRESERVE
        if kind == 'WordBreak' and lexical:
            if rule.breaks:
                raise DataError('changed native lexical rule action: ' + source.identifier)
            role = WORD_LEXICAL
        elif kind == 'SentenceBreak' and source.locale == 'root' and source.identifier == '11':
            if not rule.breaks:
                raise DataError('changed native sentence rule 11 action')
            role = SENTENCE_SOFT
        result.append(role)
    return tuple(result)


def build(kind, lowered, resolver, limits=DEFAULT_LIMITS):
    """Pair the base program with closed native-v1 refinement metadata.

    Input provenance is the authenticated, explicitly corrected CLDR profile.
    Unrecognized rules stay protected. Required refinement modules remain
    unresolved requirements until the runtime service checks them separately.
    """
    if kind not in KINDS:
        raise DataError('unsupported segmentation profile granularity')
    roles = _roles(kind, lowered.rules)
    contexts = []
    if kind == 'SentenceBreak':
        variables = lowered.variables
        visible = dict(variables.visible)
        # Context sets share one evaluator budget across all five queries.
        # This fixed preparation cap is separate from base-machine limits.
        evaluator = SetEvaluator(variables.arena, resolver, CONTEXT_LIMITS)
        for name, flag in (('$SATerm', TERM), ('$Close', CLOSE), ('$Sp', SPACE), ('$ParaSep', SEPARATOR)):
            if name not in visible:
                raise DataError('missing native sentence context variable: ' + name)
            contexts.append((evaluator.evaluate(variables.arena[visible[name]]), flag))
        ignores = [row.source for row in lowered.rules if row.ignore_replacement]
        if len(ignores) != 1 or not isinstance(ignores[0], IgnoreRule):
            raise DataError('missing native sentence ignore descriptor')
        contexts.append((evaluator.evaluate(ignores[0].ignored), IGNORED))
    program = assemble(lowered, limits, extra_predicates=tuple(values for values, flag in contexts))
    predicate_ids = {values: index for index, values in enumerate(program.alphabet.predicates)}
    # Range-tuple keys require linear hashing. Resolve each context once, not
    # once per alphabet class; the class loop then performs five bit tests.
    context_masks = tuple((1 << predicate_ids[values], flag) for values, flag in contexts)
    flags = []
    for mask in program.alphabet.masks:
        value = 0
        for context_mask, flag in context_masks:
            if mask & context_mask:
                value |= flag
        # The selected UAX sentence categories are disjoint in this profile.
        # A changed source must review tail-anchor semantics before admission.
        if value & (value - 1):
            raise DataError('overlapping native sentence context categories')
        flags.append(value)
    requirements = 0
    if kind == 'WordBreak':
        requirements = NEEDS_DICTIONARY | NEEDS_WORD_STATUS | NEEDS_GRAPHEMES
    elif kind == 'SentenceBreak':
        requirements = NEEDS_SUPPRESSIONS | NEEDS_GRAPHEMES
    return Profile(kind, program, roles, tuple(flags), requirements)


def profile_key(profile):
    """Execution identity includes policy, not just base boundary languages."""
    return (profile.kind, semantic_key(profile.program), profile.roles,
            profile.class_flags, profile.requirements)
