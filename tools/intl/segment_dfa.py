"""Bounded deterministic suffix-existence machines over a shared alphabet.

Unanchored machines restart at every boundary. The reviewed leading-left
absolute-start form instead seeds once at BOS; no artificial BOS character
enters the code-point alphabet. These are host records, not runtime bytes.
"""
from collections import namedtuple

from cldr_xml import DataError
from segment_alphabet import Alphabet, classify
from segment_lower import Language
from segment_nfa import Instruction, NFA

ProductionSide = namedtuple('ProductionSide', 'language anchored reverse')
DFA = namedtuple('DFA', 'rows accepted subsets restart construction_work')
DFALimits = namedtuple('DFALimits', 'states cells members work')
DEFAULT_LIMITS = DFALimits(4096, 1048576, 262144, 16000000)
MAX_ANCHOR_VISITS = 262144


def production_side(language, side):
    """Verify the pinned leading-left anchor form and derive an anchor-free body.

    Retain the caller's original language for NFA equivalence/provenance. New
    assertion placements require review rather than unconditional epsilon
    substitution or silently restarting anchored contexts.
    """
    if (side not in ('left', 'right') or not isinstance(language, Language) or
            not isinstance(language.children, tuple) or
            any(not isinstance(child, Language) for child in language.children)):
        raise DataError('invalid segmentation production side')
    anchored = (side == 'left' and language.kind == 'concat' and language.children and
                language.children[0].kind == 'start')
    body = language
    if anchored:
        anchor = language.children[0]
        if (anchor.children != () or anchor.character is not None or
                type(anchor.height) is not int or anchor.height != 1):
            raise DataError('invalid leading segmentation anchor leaf')
        children = language.children[1:]
        if not children:
            raise DataError('empty anchored segmentation body')
        body = children[0] if len(children) == 1 else Language(
            'concat', children, None, 1 + max(child.height for child in children))
    pending, seen, visits = [body], set(), 0
    while pending:
        node = pending.pop()
        visits += 1
        if visits > MAX_ANCHOR_VISITS:
            raise DataError('segmentation anchor verification budget exceeded')
        if id(node) in seen:
            continue
        if len(seen) >= MAX_ANCHOR_VISITS:
            raise DataError('segmentation anchor verification budget exceeded')
        if not isinstance(node, Language) or not isinstance(node.children, tuple):
            raise DataError('invalid segmentation production language')
        if node.kind == 'start':
            raise DataError('unreviewed segmentation anchor placement')
        if len(pending) + len(node.children) > MAX_ANCHOR_VISITS:
            raise DataError('segmentation anchor pending budget exceeded')
        seen.add(id(node))
        pending.extend(node.children)
    return ProductionSide(body, bool(anchored), side == 'right')


class _Compiler:
    def __init__(self, program, alphabet, restart, limits):
        if (not isinstance(limits, DFALimits) or
                any(type(value) is not int or value < 1 for value in limits)):
            raise DataError('invalid segmentation DFA limits')
        if (not isinstance(program, NFA) or not program.instructions or
                len(program.instructions) > 8192 or type(program.entry) is not int or
                not 0 <= program.entry < len(program.instructions) or
                not isinstance(alphabet, Alphabet) or not alphabet.masks or type(restart) is not bool):
            raise DataError('invalid segmentation DFA inputs')
        self.program, self.alphabet, self.restart, self.limits = program, alphabet, restart, limits
        self.work = 0
        self.consume(sum(len(value) for value in alphabet.predicates))
        pool = {value: index for index, value in enumerate(alphabet.predicates)}
        self.bits = []
        for values in program.predicates:
            self.consume(len(values) + 1)
            if values not in pool:
                raise DataError('segmentation predicate absent from alphabet owner')
            self.bits.append(1 << pool[values])
        count = len(program.instructions)
        for instruction in program.instructions:
            self.consume()
            if not isinstance(instruction, Instruction):
                raise DataError('invalid segmentation NFA instruction')
            kind = instruction.kind
            if kind == 'start':
                raise DataError('unverified segmentation start assertion')
            if kind not in ('match', 'split', 'jump', 'accept'):
                raise DataError('unknown segmentation NFA instruction')
            targets = (instruction.first, instruction.second) if kind == 'split' else (
                () if kind == 'accept' else (instruction.first,))
            if any(type(target) is not int or not 0 <= target < count for target in targets):
                raise DataError('invalid segmentation NFA target')
            if kind == 'match':
                if type(instruction.predicate) is not int or not 0 <= instruction.predicate < len(self.bits):
                    raise DataError('invalid segmentation NFA predicate')
            elif instruction.predicate is not None:
                raise DataError('unexpected segmentation NFA predicate')
        self.subsets, self.indices, self.accepted = [], {}, []
        self.members = 0

    def consume(self, count=1):
        self.work += count
        if self.work > self.limits.work:
            raise DataError('segmentation DFA work budget exceeded')

    def closure(self, seeds):
        pending, visited, result = list(seeds), set(), set()
        while pending:
            self.consume()
            index = pending.pop()
            if index in visited:
                continue
            visited.add(index)
            instruction = self.program.instructions[index]
            if instruction.kind == 'split':
                pending.extend((instruction.first, instruction.second))
            elif instruction.kind == 'jump':
                pending.append(instruction.first)
            else:
                result.add(index)
        self.consume(len(result) * max(1, len(result).bit_length()))
        return tuple(sorted(result))

    def intern(self, subset):
        # Hashing a tuple, even on a cache hit, visits its members. Check all
        # retained owners before adding a new state or its eventual table row.
        self.consume(len(subset) + 1)
        index = self.indices.get(subset)
        if index is None:
            count = len(self.subsets) + 1
            if count > self.limits.states:
                raise DataError('segmentation DFA state budget exceeded')
            if count * len(self.alphabet.masks) > self.limits.cells:
                raise DataError('segmentation DFA transition budget exceeded')
            if self.members + len(subset) > self.limits.members:
                raise DataError('segmentation DFA subset-member budget exceeded')
            self.consume(len(subset))
            index = len(self.subsets)
            self.indices[subset] = index
            self.subsets.append(subset)
            self.members += len(subset)
            self.accepted.append(any(self.program.instructions[item].kind == 'accept' for item in subset))
        return index

    def compile(self):
        self.intern(self.closure((self.program.entry,)))
        rows, index = [], 0
        while index < len(self.subsets):
            subset, row = self.subsets[index], []
            for mask in self.alphabet.masks:
                self.consume()
                following = [self.program.entry] if self.restart else []
                for item in subset:
                    self.consume()
                    instruction = self.program.instructions[item]
                    if instruction.kind == 'match' and mask & self.bits[instruction.predicate]:
                        following.append(instruction.first)
                row.append(self.intern(self.closure(following)))
            rows.append(tuple(row))
            index += 1
        return DFA(tuple(rows), tuple(self.accepted), tuple(self.subsets), self.restart, self.work)


def compile_machine(program, alphabet, restart=True, limits=DEFAULT_LIMITS):
    """Determinize an assertion-free NFA; keep rule machines independent.

    program is an unchanged compile_language() result after production-side
    assertion verification. alphabet is the unchanged partition() result for
    the whole rule program. Complete rows use ascending class IDs; BFS state
    numbering is independent of Python set iteration. No minimization is performed here.
    """
    return _Compiler(program, alphabet, restart, limits).compile()


def acceptance(machine, alphabet, text, reverse=False):
    """Host proof acceptance at every original code-point boundary in one pass."""
    if not isinstance(text, str) or len(text) > 1000000 or type(reverse) is not bool:
        raise DataError('invalid segmentation DFA reference input')
    position, state = (len(text) if reverse else 0), 0
    result = [False] * (len(text) + 1)
    result[position] = machine.accepted[state]
    while position != (0 if reverse else len(text)):
        following = position - 1 if reverse else position + 1
        code = ord(text[following if reverse else position])
        state = machine.rows[state][classify(alphabet, code)]
        position = following
        result[position] = machine.accepted[state]
    return tuple(result)
