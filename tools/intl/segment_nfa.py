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

"""Bounded Thompson compilation and a host code-point reference executor.

This is preparation/proof machinery, not the native UTF-16 runtime. Repeated
language-DAG occurrences receive fresh continuations; immutable source sharing
does not permit sharing a mutable Thompson fragment between those occurrences.
"""
from bisect import bisect_right
from collections import namedtuple

from cldr_xml import DataError
from segment_lower import Character, Language
from segment_sets import DOMAIN_END, checked_ranges

Instruction = namedtuple('Instruction', 'kind first second predicate')
Fragment = namedtuple('Fragment', 'entry patches')
NFA = namedtuple('NFA', 'instructions predicates entry construction_work')
NFALimits = namedtuple('NFALimits', 'states work predicates retained_pairs ranges')
DEFAULT_LIMITS = NFALimits(8192, 65536, 256, 262144, 16384)


class _Builder:
    def __init__(self, limits):
        if (not isinstance(limits, NFALimits) or
                any(type(value) is not int or value < 1 for value in limits)):
            raise DataError('invalid segmentation NFA limits')
        self.limits, self.work = limits, 0
        self.instructions, self.predicates = [], []
        self.predicate_ids, self.range_owners = {}, {}
        self.retained_pairs = 0

    def consume(self, count=1):
        self.work += count
        if self.work > self.limits.work:
            raise DataError('segmentation NFA work budget exceeded')

    def emit(self, kind, first=None, second=None, predicate=None):
        if len(self.instructions) >= self.limits.states:
            raise DataError('segmentation NFA state budget exceeded')
        result = len(self.instructions)
        self.instructions.append(Instruction(kind, first, second, predicate))
        return result

    def predicate(self, ranges):
        identity = id(ranges)
        if identity in self.range_owners:
            return self.range_owners[identity][1]
        result = checked_ranges(ranges, self.limits.ranges)
        self.consume(len(result))
        index = self.predicate_ids.get(result)
        if index is None:
            if len(self.predicates) >= self.limits.predicates:
                raise DataError('segmentation NFA predicate budget exceeded')
            if self.retained_pairs + len(result) > self.limits.retained_pairs:
                raise DataError('segmentation NFA retained range budget exceeded')
            index = len(self.predicates)
            self.predicate_ids[result] = index
            self.predicates.append(result)
            self.retained_pairs += len(result)
        self.range_owners[identity] = ranges, index
        return index

    def patch(self, patches, target):
        self.consume(len(patches))
        for index, field in patches:
            instruction = self.instructions[index]
            if getattr(instruction, field) is not None:
                raise DataError('reused segmentation NFA continuation')
            self.instructions[index] = instruction._replace(**{field: target})

    def compile(self, root, reverse):
        pending, fragments, active = [(root, False)], [], set()
        while pending:
            node, finish = pending.pop()
            self.consume()
            if not isinstance(node, Language) or not isinstance(node.children, tuple):
                raise DataError('invalid segmentation language node')
            if not finish:
                if id(node) in active:
                    raise DataError('cyclic segmentation language')
                if node.kind in ('char', 'epsilon', 'start'):
                    if node.children:
                        raise DataError('invalid segmentation NFA leaf')
                    if node.kind == 'char':
                        if not isinstance(node.character, Character):
                            raise DataError('invalid segmentation character atom')
                        index = self.emit('match', predicate=self.predicate(node.character.ranges))
                    else:
                        index = self.emit('jump' if node.kind == 'epsilon' else 'start')
                    fragments.append(Fragment(index, ((index, 'first'),)))
                    continue
                count = len(node.children)
                if (node.kind in ('concat', 'alt') and count < 2 or
                        node.kind in ('star', 'optional') and count != 1 or
                        node.kind not in ('concat', 'alt', 'star', 'optional')):
                    raise DataError('invalid segmentation NFA language shape')
                if len(pending) + count + 1 > self.limits.work:
                    raise DataError('segmentation NFA pending budget exceeded')
                children = node.children[::-1] if reverse and node.kind == 'concat' else node.children
                active.add(id(node))
                pending.append((node, True))
                pending.extend((child, False) for child in reversed(children))
                continue
            active.remove(id(node))
            count = len(node.children)
            children = fragments[-count:]
            del fragments[-count:]
            if node.kind == 'concat':
                for left, right in zip(children, children[1:]):
                    self.patch(left.patches, right.entry)
                result = Fragment(children[0].entry, children[-1].patches)
            elif node.kind == 'alt':
                entry = children[0].entry
                self.consume(len(children[0].patches))
                patches = list(children[0].patches)
                for child in children[1:]:
                    entry = self.emit('split', entry, child.entry)
                    self.consume(len(child.patches))
                    patches.extend(child.patches)
                result = Fragment(entry, tuple(patches))
            else:
                child = children[0]
                entry = self.emit('split', child.entry)
                exit_patch = ((entry, 'second'),)
                if node.kind == 'star':
                    self.patch(child.patches, entry)
                    result = Fragment(entry, exit_patch)
                else:
                    self.consume(len(child.patches))
                    result = Fragment(entry, child.patches + exit_patch)
            fragments.append(result)
        if len(fragments) != 1:
            raise DataError('incomplete segmentation NFA fragments')
        accept = self.emit('accept')
        self.patch(fragments[0].patches, accept)
        return NFA(tuple(self.instructions), tuple(self.predicates), fragments[0].entry, self.work)


def compile_language(language, reverse=False, limits=DEFAULT_LIMITS):
    """Compile one language, optionally reversing concatenation recursively.

    Absolute start retains its original-position meaning under reversal. Each
    instruction has at most two edges, so the state cap also bounds edge count.
    This host representation is not a declared runtime binary format.
    """
    if type(reverse) is not bool:
        raise DataError('invalid segmentation NFA direction')
    return _Builder(limits).compile(language, reverse)


def endpoints(program, text, start, reverse=False, work_limit=1000000):
    """Reference accepted endpoints from one original code-point position.

    This accepts a code-point string, not a UTF-16 buffer or segment-relative
    anchor. Native decoding/index mapping belongs to the later C input owner.
    Do not use per-candidate calls for production boundary traversal.
    """
    if (not isinstance(text, str) or type(reverse) is not bool or
            type(start) is not int or not 0 <= start <= len(text) or
            type(work_limit) is not int or work_limit < 1):
        raise DataError('invalid segmentation NFA reference input')
    work = 0

    def consume():
        nonlocal work
        work += 1
        if work > work_limit:
            raise DataError('segmentation NFA reference work budget exceeded')

    def closure(seeds, position):
        pending, visited, result = list(seeds), set(), set()
        while pending:
            consume()
            index = pending.pop()
            if index in visited:
                continue
            visited.add(index)
            instruction = program.instructions[index]
            if instruction.kind == 'split':
                pending.extend((instruction.first, instruction.second))
            elif instruction.kind == 'jump' or instruction.kind == 'start' and position == 0:
                pending.append(instruction.first)
            elif instruction.kind != 'start':
                result.add(index)
        return result

    position, result = start, set()
    active = closure((program.entry,), position)
    while active:
        consume()
        if any(program.instructions[index].kind == 'accept' for index in active):
            result.add(position)
        if position == (0 if reverse else len(text)):
            break
        following = position - 1 if reverse else position + 1
        code = ord(text[following if reverse else position])
        pending = []
        for index in active:
            consume()
            instruction = program.instructions[index]
            if instruction.kind == 'match':
                ranges = program.predicates[instruction.predicate]
                at = bisect_right(ranges, (code, DOMAIN_END)) - 1
                if at >= 0 and code < ranges[at][1]:
                    pending.append(instruction.first)
        position = following
        active = closure(pending, position)
    return frozenset(result)
