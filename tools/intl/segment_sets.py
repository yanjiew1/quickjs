"""Bounded code-point interval evaluation, alongside the retained source AST.

Membership ranges do not replace expression polarity/provenance: later ignore
lowering must still inspect the original complement/difference structure.
"""
from collections import namedtuple
import heapq

from cldr_xml import DataError
from segment_expression import Expression, is_set

DOMAIN_END = 0x110000
SetLimits = namedtuple('SetLimits', 'nodes ranges steps properties retained_pairs')
DEFAULT_LIMITS = SetLimits(131072, 16384, 8000000, 256, 262144)
ARITIES = {'literal': 0, 'property': 0, 'any': 0, 'set_ref': 0,
           'seq_ref': 0, 'start': 0, 'epsilon': 0,
           'bracket': 1, 'complement': 1, 'star': 1, 'optional': 1,
           'difference': 2}


def checked_ranges(values, limit):
    """Copy normalized nonadjacent half-open ranges, including surrogates."""
    if not isinstance(values, (tuple, list)) or len(values) > limit:
        raise DataError('segmentation property range budget exceeded')
    result, previous = [], -1
    for row in values:
        if (not isinstance(row, (tuple, list)) or len(row) != 2 or
                any(type(value) is not int for value in row)):
            raise DataError('invalid segmentation property range')
        start, end = row
        if not previous < start < end <= DOMAIN_END:
            raise DataError('unnormalized segmentation property ranges')
        result.append((start, end))
        previous = end
    return tuple(result)


class SetEvaluator:
    """One arena, injected pinned-property owner and bounded immutable cache.

    Evaluation is iterative even through historical aliases. Parser-local tree
    depth cannot bound the length of an otherwise valid prior-binding chain.
    Cache entries keep their AST objects alive so identity keys cannot be reused.
    """
    def __init__(self, arena, resolver, limits=DEFAULT_LIMITS):
        if (not isinstance(limits, SetLimits) or
                any(type(value) is not int or value < 1 for value in limits)):
            raise DataError('invalid segmentation set limits')
        self.arena = tuple(arena)
        self.resolver = resolver
        self.limits = limits
        self.cache, self.properties = {}, {}
        self.range_owners, self.retained_pairs = {}, 0
        self.steps = 0

    def consume(self, count=1):
        self.steps += count
        if self.steps > self.limits.steps:
            raise DataError('segmentation set work budget exceeded')

    def append(self, out, start, end):
        if start < end:
            if len(out) >= self.limits.ranges:
                raise DataError('segmentation set result budget exceeded')
            out.append((start, end))

    def union(self, inputs):
        out, start, end = [], None, None
        # Merge sorted streams without enumerating the Unicode domain or
        # repeatedly copying an ever-growing prefix for a wide union node.
        for first, last in heapq.merge(*inputs):
            self.consume()
            if start is None:
                start, end = first, last
            elif first <= end:
                end = max(end, last)
            else:
                self.append(out, start, end)
                start, end = first, last
        if start is not None:
            self.append(out, start, end)
        return tuple(out)

    def difference(self, left, right):
        out, index = [], 0
        for start, end in left:
            self.consume()
            while index < len(right) and right[index][1] <= start:
                self.consume()
                index += 1
            while index < len(right) and right[index][0] < end:
                self.consume()
                cut_start, cut_end = right[index]
                self.append(out, start, min(end, cut_start))
                start = max(start, cut_end)
                if start >= end:
                    break
                index += 1
            self.append(out, start, end)
        return tuple(out)

    def shape(self, node):
        if not isinstance(node, Expression) or not isinstance(node.children, tuple):
            raise DataError('invalid segmentation expression node')
        count = len(node.children)
        if node.kind in ARITIES:
            valid = count == ARITIES[node.kind]
        else:
            valid = node.kind == 'union' or node.kind in ('concat', 'alt') and count >= 2
        if (not valid or count > self.limits.nodes or
                any(not isinstance(child, Expression) for child in node.children)):
            raise DataError('invalid segmentation expression shape')
        if node.kind == 'property' and not isinstance(node.value, str):
            raise DataError('invalid segmentation property query')

    def children(self, node):
        self.shape(node)
        if not is_set(node):
            raise DataError('sequence used as a segmentation character set')
        if node.kind == 'set_ref':
            slot = node.value
            if type(slot) is not int or not 0 <= slot < len(self.arena):
                raise DataError('invalid segmentation set reference')
            return (self.arena[slot],)
        return node.children

    def retain(self, result):
        # Aliases/brackets share tuple owners. Charge new immutable storage,
        # not every reference to it, and keep identity keys alive with owners.
        identity = id(result)
        if identity not in self.range_owners:
            if self.retained_pairs + len(result) > self.limits.retained_pairs:
                raise DataError('segmentation retained range budget exceeded')
            self.range_owners[identity] = result
            self.retained_pairs += len(result)

    def evaluate(self, root):
        """Return immutable ranges; fail without publishing an incomplete set."""
        pending, active = [(root, False)], set()
        while pending:
            node, finish = pending.pop()
            self.consume()
            identity = id(node)
            if identity in self.cache:
                continue
            if not finish:
                if identity in active:
                    raise DataError('cyclic segmentation set reference')
                if len(self.cache) + len(active) >= self.limits.nodes:
                    raise DataError('segmentation set node budget exceeded')
                children = self.children(node)
                active.add(identity)
                pending.append((node, True))
                pending.extend((child, False) for child in reversed(children))
                continue
            children = [self.cache[id(child)][1] for child in self.children(node)]
            if node.kind == 'literal':
                if type(node.value) is not int or not 0 <= node.value < DOMAIN_END:
                    raise DataError('invalid segmentation set literal')
                result = ((node.value, node.value + 1),)
            elif node.kind == 'any':
                result = ((0, DOMAIN_END),)
            elif node.kind == 'property':
                if node.value not in self.properties:
                    if len(self.properties) >= self.limits.properties:
                        raise DataError('segmentation property query budget exceeded')
                    result = checked_ranges(self.resolver(node.value), self.limits.ranges)
                    self.consume(len(result))
                    self.retain(result)
                    self.properties[node.value] = result
                result = self.properties[node.value]
            elif node.kind in ('bracket', 'set_ref'):
                result = children[0]
            elif node.kind == 'union':
                result = self.union(children)
            elif node.kind == 'complement':
                result = self.difference(((0, DOMAIN_END),), children[0])
            elif node.kind == 'difference':
                result = self.difference(children[0], children[1])
            else:
                raise DataError('unsupported segmentation set expression')
            self.retain(result)
            self.cache[identity] = (node, result)
            active.remove(identity)
        return self.cache[id(root)][1]

    def operands(self, root):
        """Evaluate maximal set atoms inside a sequence without expanding it.

        Return source nodes beside their ranges, preserving ignore-lowering
        structure. Follow shared sequence slots once; assertions and epsilon
        carry no character set. Reject corrupt/cyclic injected records.
        """
        pending, active, done, result = [(root, False)], set(), set(), []
        while pending:
            node, finish = pending.pop()
            self.consume()
            identity = id(node)
            if identity in done:
                continue
            if finish:
                active.remove(identity)
                done.add(identity)
                continue
            self.shape(node)
            if identity in active:
                raise DataError('cyclic segmentation sequence reference')
            if len(active) + len(done) >= self.limits.nodes:
                raise DataError('segmentation operand node budget exceeded')
            if is_set(node):
                result.append((node, self.evaluate(node)))
                done.add(identity)
                continue
            children = node.children
            if node.kind == 'seq_ref':
                slot = node.value
                if type(slot) is not int or not 0 <= slot < len(self.arena):
                    raise DataError('invalid segmentation sequence reference')
                target = self.arena[slot]
                self.shape(target)
                if is_set(target):
                    raise DataError('set used as a sequence reference target')
                children = (target,)
            elif node.kind not in ('concat', 'alt', 'star', 'optional', 'start', 'epsilon'):
                raise DataError('unsupported segmentation sequence expression')
            active.add(identity)
            pending.append((node, True))
            pending.extend((child, False) for child in reversed(children))
        return tuple(result)
