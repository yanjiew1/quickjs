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

"""Prepare historical bindings, boundary rules and the two CLDR ignore forms.

These typed records retain priority and provenance. They do not apply remaps to
input or move an ignore phase ahead of earlier raw-adjacency rules.
"""
from collections import namedtuple
import re

from cldr_xml import DataError
from segment_expression import (DEFAULT_LIMITS, ExpressionError, SPACE, is_set,
                                parse_expression)
from segment_sources import KINDS

PreparedVariables = namedtuple('PreparedVariables', 'arena visible bindings')
BoundaryRule = namedtuple('BoundaryRule', 'source left right right_start breaks')
IgnoreRule = namedtuple('IgnoreRule', 'source excluded ignored excluded_start ignored_start')
MAX_BINDINGS = 1024
MAX_RULES = 1024
MAX_NODES = 65536
WS = '[' + re.escape(''.join(sorted(SPACE))) + ']*'


def _ignore_pattern(excluded, ignored):
    # Deliberately recognize the two pinned shapes, not arbitrary captures,
    # replacement expressions or a host regular-expression language.
    excluded = r'\[\^' + WS + WS.join(map(re.escape, excluded)) + WS + r'\]'
    ignored = r'\(' + WS + (WS + r'\|' + WS).join(map(re.escape, ignored)) + WS + r'\)'
    return re.compile(WS + r'\(\?<X>(?P<excluded>' + excluded + r')' + WS + r'\)' + WS +
                      r'(?P<ignored>' + ignored + r')' + WS + r'\*' + WS)


IGNORE = {
    ('WordBreak', '4'): _ignore_pattern(('$CR', '$LF', '$Newline'),
                                      ('$Extend', '$Format', '$ZWJ')),
    ('SentenceBreak', '5'): _ignore_pattern(('$ParaSep',), ('$Extend', '$Format')),
}


def _context(source):
    return '{}:{}:{}'.format(source.source, source.locale, source.identifier)


def _parse(text, visible, arena, limits, source, offset=0):
    try:
        return parse_expression(text, visible, arena, limits)
    except ExpressionError as error:
        raise DataError('{}: {} (original offset {})'.format(
            _context(source), error, offset + error.offset))


def _count(node):
    # References are leaves, not textual expansions of historical bindings.
    count, pending = 0, [node]
    while pending:
        item = pending.pop()
        count += 1
        pending.extend(item.children)
    return count


def prepare_variables(captured, limits=DEFAULT_LIMITS):
    """Parse each declaration using exactly the earlier captured environment."""
    if len(captured.bindings) > MAX_BINDINGS:
        raise DataError('segmentation binding budget exceeded')
    arena, visible, nodes = [], {}, 0
    for binding in captured.bindings:
        if binding.environment != tuple(sorted(visible.items())):
            raise DataError('inconsistent historical binding environment: ' + _context(binding))
        node = _parse(binding.expression, binding.environment, tuple(arena), limits, binding)
        nodes += _count(node)
        if nodes > MAX_NODES:
            raise DataError('segmentation binding node budget exceeded')
        visible[binding.identifier] = len(arena)
        arena.append(node)
    if captured.visible != tuple(sorted(visible.items())):
        raise DataError('inconsistent final segmentation environment')
    return PreparedVariables(tuple(arena), captured.visible, captured.bindings)


def _operator(text, source, limits):
    if not isinstance(text, str) or len(text) > limits.source:
        raise DataError('segmentation rule source budget exceeded: ' + _context(source))
    pending, found, pos = [], [], 0
    while pos < len(text):
        char, start = text[pos], pos
        pos += 1
        if char == '\\':
            # Escapes are parsed and validated by the expression owner. This
            # scanner only prevents an escaped boundary marker becoming syntax.
            pos += 1
            continue
        if char == '[':
            pending.append(']')
        elif pending and pending[-1] == ']':
            if char == ']':
                pending.pop()
            continue
        elif char == '(':
            pending.append(')')
        elif char in '])':
            if not pending or pending.pop() != char:
                raise DataError('unbalanced rule delimiter: ' + _context(source))
        elif char in '\u00d7\u00f7\u2192' and not pending:
            found.append((start, char))
        if len(pending) > limits.depth:
            raise DataError('segmentation rule nesting budget exceeded: ' + _context(source))
    if pending or len(found) != 1:
        raise DataError('expected one top-level boundary/remap operator: ' + _context(source))
    return found[0]


def prepare_rule(source, variables, kind, limits=DEFAULT_LIMITS):
    """Prepare one ordered ordinary boundary or exact WB4/SB5 ignore record."""
    if kind not in KINDS:
        raise DataError('unsupported segmentation kind')
    position, operator = _operator(source.expression, source, limits)
    left, right = source.expression[:position], source.expression[position + 1:]
    if operator != '\u2192':
        before = _parse(left, variables.visible, variables.arena, limits, source)
        after = _parse(right, variables.visible, variables.arena, limits, source, position + 1)
        return BoundaryRule(source, before, after, position + 1, operator == '\u00f7')
    pattern = IGNORE.get((kind, source.identifier))
    match = pattern.fullmatch(left) if pattern is not None else None
    if match is None or right.strip(''.join(SPACE)) != '${X}':
        raise DataError('unsupported segmentation remap: ' + _context(source))
    excluded_start, ignored_start = match.start('excluded'), match.start('ignored')
    negative = _parse(match.group('excluded'), variables.visible, variables.arena,
                      limits, source, excluded_start)
    ignored = _parse(match.group('ignored'), variables.visible, variables.arena,
                     limits, source, ignored_start)
    if not is_set(ignored):
        raise DataError('sequence-valued ignored set: ' + _context(source))
    # The exact recognized syntax guarantees bracket(complement(positive)).
    # Retain that positive exclusion expression rather than complementing an
    # evaluated set twice and losing the source phase's identity.
    return IgnoreRule(source, negative.children[0].children[0], ignored,
                      excluded_start, ignored_start)


def prepare_rules(sources, variables, kind, limits=DEFAULT_LIMITS):
    """Preserve caller-supplied inherited priority; do not sort textual IDs."""
    if kind not in KINDS:
        raise DataError('unsupported segmentation kind')
    if len(sources) > MAX_RULES:
        raise DataError('segmentation rule budget exceeded')
    result, nodes = [], 0
    for source in sources:
        rule = prepare_rule(source, variables, kind, limits)
        expressions = (rule.left, rule.right) if isinstance(rule, BoundaryRule) else (rule.excluded, rule.ignored)
        nodes += sum(_count(node) for node in expressions)
        if nodes > MAX_NODES:
            raise DataError('segmentation rule node budget exceeded')
        result.append(rule)
    return tuple(result)
