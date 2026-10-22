"""Phase-aware segmentation language IR, before automata or runtime execution.

UAX #29 ignore replacement applies only to subsequent rules. Keep original
typed expressions beside derived atoms; evaluated membership alone cannot
recover the distinction between positive values and negations.
"""
from collections import namedtuple

from cldr_xml import DataError
from segment_expression import is_set
from segment_rules import MAX_RULES, BoundaryRule, IgnoreRule
from segment_sets import DOMAIN_END, SetEvaluator

Character = namedtuple('Character', 'ranges negative source')
Language = namedtuple('Language', 'kind children character height')
LoweredRule = namedtuple('LoweredRule', 'source left right breaks ignore_replacement')
LoweredProgram = namedtuple('LoweredProgram', 'rules variables')
LowerLimits = namedtuple('LowerLimits', 'nodes edges height visits')
DEFAULT_LIMITS = LowerLimits(65536, 131072, 128, 262144)


class _Lowerer:
    def __init__(self, variables, resolver, limits):
        if (not isinstance(limits, LowerLimits) or
                any(type(value) is not int or value < 1 for value in limits)):
            raise DataError('invalid segmentation lowering limits')
        self.arena, self.limits = variables.arena, limits
        self.evaluator = SetEvaluator(self.arena, resolver)
        self.cache, self.polarity = {}, {}
        self.nodes = self.edges = self.visits = 0
        self.ignored = None

    def visit(self):
        self.visits += 1
        if self.visits > self.limits.visits:
            raise DataError('segmentation lowering work budget exceeded')

    def node(self, kind, children=(), character=None):
        height = 1 + max((child.height for child in children), default=0)
        if (self.nodes >= self.limits.nodes or self.edges + len(children) > self.limits.edges or
                height > self.limits.height):
            raise DataError('segmentation lowering output budget exceeded')
        self.nodes += 1
        self.edges += len(children)
        return Language(kind, tuple(children), character, height)

    def negative(self, root):
        """Classify the reviewed maximal-atom shapes without losing aliases.

        A complemented class stays one negative atom. Positive differences
        stay one predicate. Mixed/nested negation beyond the pinned profile
        fails rather than inventing ignore semantics for Boolean operands.
        """
        pending = [(root, False)]
        while pending:
            node, finish = pending.pop()
            self.visit()
            identity = id(node)
            if identity in self.polarity:
                continue
            children = self.evaluator.children(node)
            if not finish:
                pending.append((node, True))
                pending.extend((child, False) for child in reversed(children))
                continue
            flags = [self.polarity[id(child)] for child in children]
            if node.kind == 'complement':
                if flags[0]:
                    raise DataError('unreviewed nested segmentation negation')
                value = True
            elif node.kind in ('bracket', 'set_ref'):
                value = flags[0]
            else:
                if any(flags):
                    raise DataError('unreviewed mixed-polarity segmentation set')
                value = False
            self.polarity[identity] = value
        return self.polarity[id(root)]

    def lower(self, root):
        # Keep raw and ignore-aware uses separate even when they share a
        # historical declaration. A generated ignored closure bypasses this
        # walker, so it cannot recursively acquire another copy of itself.
        phase = self.ignored is not None
        pending, active = [(root, False)], set()
        while pending:
            node, finish = pending.pop()
            self.visit()
            key = id(node), phase
            if key in self.cache:
                continue
            if not finish:
                if key in active:
                    raise DataError('cyclic segmentation lowering reference')
                self.evaluator.shape(node)
                if is_set(node):
                    ranges = self.evaluator.evaluate(node)
                    negative = self.negative(node)
                    result = self.node('char', character=Character(ranges, negative, node))
                    if phase and not negative:
                        result = self.node('concat', (result, self.ignored))
                    self.cache[key] = result
                    continue
                children = node.children
                if node.kind == 'seq_ref':
                    slot = node.value
                    if type(slot) is not int or not 0 <= slot < len(self.arena):
                        raise DataError('invalid segmentation lowering reference')
                    children = (self.arena[slot],)
                active.add(key)
                pending.append((node, True))
                pending.extend((child, False) for child in reversed(children))
                continue
            if node.kind == 'seq_ref':
                result = self.cache[id(self.arena[node.value]), phase]
            else:
                children = tuple(self.cache[id(child), phase] for child in node.children)
                result = self.node(node.kind, children)
            self.cache[key] = result
            active.remove(key)
        return self.cache[id(root), phase]

    def ignore(self, rule):
        if self.ignored is not None:
            raise DataError('repeated segmentation ignore phase')
        excluded = self.evaluator.evaluate(rule.excluded)
        ignored = self.evaluator.evaluate(rule.ignored)
        before = self.evaluator.difference(((0, DOMAIN_END),), excluded)
        self.evaluator.retain(before)
        left = self.node('char', character=Character(before, True, rule.excluded))
        right = self.node('char', character=Character(ignored, False, rule.ignored))
        self.ignored = self.node('star', (right,))
        return LoweredRule(rule, left, right, False, True)


def lower_rules(rules, variables, resolver, limits=DEFAULT_LIMITS):
    """Return immutable rule IR and its matching historical provenance owner.

    The returned languages can share DAG nodes. A later Thompson compiler must
    give repeated occurrences fresh continuations; shared AST identity alone
    does not make sharing mutable NFA fragments correct. No input is stripped
    or boundary result produced by this preparation stage.
    """
    if len(rules) > MAX_RULES:
        raise DataError('segmentation lowering rule budget exceeded')
    lowerer, result = _Lowerer(variables, resolver, limits), []
    for rule in rules:
        if not isinstance(rule, (BoundaryRule, IgnoreRule)):
            raise DataError('unknown prepared segmentation rule')
        try:
            if isinstance(rule, IgnoreRule):
                result.append(lowerer.ignore(rule))
            else:
                result.append(LoweredRule(rule, lowerer.lower(rule.left), lowerer.lower(rule.right),
                                          rule.breaks, False))
        except DataError as error:
            source = rule.source
            raise DataError('{}:{}:{}: {}'.format(source.source, source.locale, source.identifier, error))
    # Shared character sources may belong to declaration spans, not the using
    # rule's substring. Keep its arena/binding context with the derived program.
    return LoweredProgram(tuple(result), variables)
