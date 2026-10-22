"""Bounded typed parser for the pinned CLDR segmentation expression profile.

This stage preserves source structure and historical binding slots. It does
not evaluate Unicode properties, lower ignore rules, or execute segmentation.
In particular, complements must survive until the later UAX #29 ignore pass.
"""
from collections import namedtuple
import re

from cldr_xml import DataError

Expression = namedtuple('Expression', 'kind children value start end height')
Limits = namedtuple('Limits', 'source tokens nodes depth')
DEFAULT_LIMITS = Limits(65536, 16384, 16384, 64)
Token = namedtuple('Token', 'kind value start end')
SET_KINDS = frozenset(('literal', 'property', 'any', 'bracket', 'union', 'complement',
                       'difference', 'set_ref'))
# Pattern_White_Space is stable and deliberately excludes NBSP. Host Python
# Unicode tables and isspace() would silently accept a different grammar.
SPACE = frozenset('\t\n\v\f\r \u0085\u200e\u200f\u2028\u2029')
NAME = re.compile(r'[A-Za-z0-9_]+')
PROPERTY = re.compile(r'[A-Za-z_][A-Za-z0-9_]*(?:=[A-Za-z_][A-Za-z0-9_]*)?')
HEX = frozenset('0123456789abcdefABCDEF')


class ExpressionError(DataError):
    def __init__(self, message, offset):
        self.offset = offset
        super().__init__('{} at expression offset {}'.format(message, offset))


def is_set(node):
    return node.kind in SET_KINDS


def _limits(limits):
    if (not isinstance(limits, Limits) or
            any(type(value) is not int or value < 1 for value in limits) or
            limits.depth > 64):
        raise DataError('invalid segmentation expression limits')


def _tokens(text, limits):
    if not isinstance(text, str) or len(text) > limits.source:
        raise ExpressionError('expression source budget exceeded', 0)
    result, pos = [], 0
    while pos < len(text):
        start, char = pos, text[pos]
        pos += 1
        if char in SPACE:
            continue
        if len(result) >= limits.tokens:
            raise ExpressionError('expression token budget exceeded', start)
        if char == '\\':
            if pos == len(text):
                raise ExpressionError('truncated escape', start)
            escape = text[pos]
            pos += 1
            if escape == 'p':
                if pos == len(text) or text[pos] != '{':
                    raise ExpressionError('missing property braces', start)
                end = text.find('}', pos + 1)
                if end < 0 or not PROPERTY.fullmatch(text[pos + 1:end]):
                    raise ExpressionError('unsupported property spelling', start)
                result.append(Token('property', text[pos + 1:end], start, end + 1))
                pos = end + 1
                continue
            if escape in ('u', 'U'):
                width = 4 if escape == 'u' else 8
                digits = text[pos:pos + width]
                if len(digits) != width or any(digit not in HEX for digit in digits):
                    raise ExpressionError('malformed code-point escape', start)
                value = int(digits, 16)
                if value > 0x10ffff:
                    raise ExpressionError('code point outside Unicode domain', start)
                pos += width
            elif escape in '[](){}.*?+|^-$\\\u00d7\u00f7\u2192' or escape in SPACE:
                value = ord(escape)
            else:
                raise ExpressionError('unsupported escape', start)
            # Decoded metacharacters never become operators or references.
            result.append(Token('literal', value, start, pos))
        elif char == '$':
            match = NAME.match(text, pos)
            if match is None:
                raise ExpressionError('missing variable name', start)
            pos = match.end()
            result.append(Token('reference', '$' + match.group(), start, pos))
        elif char in '[]()|*?^.-\u00d7\u00f7\u2192':
            result.append(Token(char, char, start, pos))
        elif char in '{}+&:' or char == '\0':
            raise ExpressionError('unsupported expression syntax', start)
        else:
            result.append(Token('literal', ord(char), start, pos))
    result.append(Token('end', None, len(text), len(text)))
    return tuple(result)


class _Parser:
    def __init__(self, text, visible, arena, limits):
        _limits(limits)
        self.tokens = _tokens(text, limits)
        self.pos = self.nodes = self.depth = 0
        self.limits, self.arena = limits, arena
        self.visible = {}
        for name, slot in visible:
            if (not isinstance(name, str) or not name.startswith('$') or
                    not NAME.fullmatch(name[1:]) or name in self.visible or
                    type(slot) is not int or not 0 <= slot < len(arena) or
                    not isinstance(arena[slot], Expression)):
                raise DataError('invalid historical expression environment')
            self.visible[name] = slot

    @property
    def token(self):
        return self.tokens[self.pos]

    def take(self):
        token = self.token
        self.pos += 1
        return token

    def node(self, kind, children=(), value=None, start=None, end=None):
        self.nodes += 1
        if self.nodes > self.limits.nodes:
            raise ExpressionError('expression node budget exceeded', self.token.start)
        height = 1 + max((child.height for child in children), default=0)
        if height > self.limits.depth:
            raise ExpressionError('expression tree depth budget exceeded', self.token.start)
        if start is None:
            start = children[0].start
        if end is None:
            end = children[-1].end
        return Expression(kind, tuple(children), value, start, end, height)

    def enter(self, token):
        self.depth += 1
        if self.depth > self.limits.depth:
            raise ExpressionError('expression nesting budget exceeded', token.start)

    def expect(self, kind):
        if self.token.kind != kind:
            raise ExpressionError('expected ' + kind, self.token.start)
        return self.take()

    def group(self, kind, children):
        if len(children) == 1:
            return children[0]
        return self.node(kind, children)

    def reference(self, token):
        slot = self.visible.get(token.value)
        if slot is None:
            raise ExpressionError('unknown variable ' + token.value, token.start)
        kind = 'set_ref' if is_set(self.arena[slot]) else 'seq_ref'
        return self.node(kind, value=slot, start=token.start, end=token.end)

    def set_atom(self):
        token = self.take()
        if token.kind == '[':
            return self.set_group(token)
        if token.kind == 'reference':
            result = self.reference(token)
            if not is_set(result):
                raise ExpressionError('sequence variable in a set', token.start)
            return result
        if token.kind == 'property':
            return self.node('property', value=token.value, start=token.start, end=token.end)
        if token.kind in ('literal', '.', '\u00d7', '\u00f7', '\u2192'):
            value = token.value if token.kind == 'literal' else ord(token.kind)
            return self.node('literal', value=value, start=token.start, end=token.end)
        raise ExpressionError('expected a set atom', token.start)

    def full_set(self, node):
        # Macro replacement distinguishes a character from a full set. Retain
        # bracket wrappers even for [a], and follow aliases by slot, not text.
        before = len(self.arena)
        while node.kind == 'set_ref':
            slot = node.value
            if type(slot) is not int or not 0 <= slot < before:
                raise DataError('invalid historical set alias')
            before = slot
            node = self.arena[slot]
        return node.kind != 'literal'

    def set_group(self, opening):
        self.enter(opening)
        negate = self.token.kind == '^'
        if negate:
            # UnicodeSet complement is the immediate [^ spelling. Whitespace
            # elsewhere is insignificant, but must not broaden this operator.
            if self.token.start != opening.end:
                raise ExpressionError('non-immediate set complement', self.token.start)
            self.take()
        if self.token.kind == ']':
            result = self.node('union', start=opening.start, end=self.token.end)
        else:
            last = self.set_atom()
            pending = [last]
            while self.token.kind != ']':
                if self.token.kind == 'end':
                    raise ExpressionError('unclosed set', opening.start)
                if self.token.kind == '-':
                    self.take()
                    # The pinned profile uses nested-set subtraction, not
                    # literal ranges. Reject ambiguous range syntax explicitly.
                    if (not self.full_set(last) or
                            self.token.kind not in ('[', 'reference', 'property')):
                        raise ExpressionError('unsupported set range', self.token.start)
                    right = self.set_atom()
                    if not self.full_set(right):
                        raise ExpressionError('unsupported set range', right.start)
                    result = self.node('difference', (self.group('union', pending), right))
                    pending = [result]
                    last = right
                else:
                    # Union and difference have equal precedence, left to
                    # right. Do not collect all union operands before minus.
                    last = self.set_atom()
                    pending.append(last)
            result = self.group('union', pending)
        closing = self.expect(']')
        if negate:
            result = self.node('complement', (result,), start=opening.start, end=closing.end)
        self.depth -= 1
        return self.node('bracket', (result,), start=opening.start, end=closing.end)

    def atom(self):
        token = self.take()
        if token.kind == '[':
            return self.set_group(token)
        if token.kind == '(':
            self.enter(token)
            result = self.alternation()
            self.expect(')')
            self.depth -= 1
            return result
        if token.kind == 'reference':
            return self.reference(token)
        if token.kind == '.':
            return self.node('any', start=token.start, end=token.end)
        if token.kind == '^':
            return self.node('start', start=token.start, end=token.end)
        if token.kind in ('property', 'literal'):
            return self.node(token.kind, value=token.value, start=token.start, end=token.end)
        raise ExpressionError('expected an expression atom', token.start)

    def concatenation(self):
        children = []
        while self.token.kind not in ('|', ')', 'end'):
            child = self.atom()
            if self.token.kind in ('*', '?'):
                if child.kind == 'start':
                    raise ExpressionError('quantified start assertion', self.token.start)
                token = self.take()
                child = self.node('star' if token.kind == '*' else 'optional',
                                  (child,), end=token.end)
            children.append(child)
        if not children:
            return self.node('epsilon', start=self.token.start, end=self.token.start)
        return self.group('concat', children)

    def alternation(self):
        children = [self.concatenation()]
        while self.token.kind == '|':
            self.take()
            children.append(self.concatenation())
        # A union of single-character languages is still set-valued, including
        # grouped aliases. Star/optional and epsilon remain sequence-valued.
        kind = 'union' if all(is_set(child) for child in children) else 'alt'
        return self.group(kind, children)


def parse_expression(text, visible=(), arena=(), limits=DEFAULT_LIMITS):
    """Return an immutable typed tree with original offsets and slot references.

    The arena contains already parsed earlier bindings. No reference expands
    its target tree, so repeated macros cannot grow the result exponentially.
    Property spellings remain symbolic; the pinned C resolver validates their
    meanings in the next preparation stage. Empty text denotes epsilon, not
    an empty character set. The set domain includes lone surrogate code points.
    """
    parser = _Parser(text, visible, arena, limits)
    result = parser.alternation()
    parser.expect('end')
    return result
