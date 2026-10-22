"""Assemble bounded rule programs and share complete side-machine owners.

Source/proof records remain attached, while semantic keys exclude construction
statistics and NFA subset IDs. This stage still declares no runtime encoding.
"""
from collections import namedtuple

from cldr_xml import DataError
from segment_alphabet import DEFAULT_LIMITS as ALPHABET_LIMITS, partition
from segment_dfa import (DEFAULT_LIMITS as DFA_LIMITS, MAX_ANCHOR_VISITS,
                         compile_machine, production_side)
from segment_lower import LoweredProgram
from segment_nfa import DEFAULT_LIMITS as NFA_LIMITS, compile_language
from segment_sets import checked_ranges

Machine = namedtuple('Machine', 'dfa reverse anchored')
Rule = namedtuple('Rule', 'source left right breaks')
ProgramTotals = namedtuple('ProgramTotals', 'nfa_states dfa_states cells members work predicate_pairs')
RuleProgram = namedtuple('RuleProgram', 'alphabet machines rules lowered totals')
ProgramLimits = namedtuple('ProgramLimits', 'rules nfa_states dfa_states cells members work predicate_pairs')
DEFAULT_LIMITS = ProgramLimits(1024, 131072, 65536, 8388608, 4194304, 128000000, 262144)


def _machine_key(machine):
    # Subsets and work describe compilation, not the transition language.
    # Direction and BOS/restart policy remain part of complete machine identity.
    return (machine.dfa.rows, machine.dfa.accepted, machine.dfa.restart,
            machine.reverse, machine.anchored)


def semantic_key(program):
    """Pure base-boundary table identity; provenance stays with each owner.

    The wider profile key also needs its granularity and any later refinement
    or protected-boundary roles. Those policies are not encoded in this stage.
    """
    return (program.alphabet.ranges, tuple(_machine_key(item) for item in program.machines),
            tuple((rule.left, rule.right, rule.breaks) for rule in program.rules))


def assemble(lowered, limits=DEFAULT_LIMITS, extra_predicates=()):
    """Compile ordered side machines under aggregate storage and work gates.

    lowerer and per-machine limits still apply. Charge work for every attempted
    compilation, even when its completed machine can share a retained owner.
    Publish only a complete immutable program with matching source context.
    """
    if (not isinstance(limits, ProgramLimits) or
            any(type(value) is not int or value < 1 for value in limits)):
        raise DataError('invalid segmentation program limits')
    if not isinstance(lowered, LoweredProgram) or len(lowered.rules) > limits.rules:
        raise DataError('segmentation program rule budget exceeded')
    if not isinstance(extra_predicates, (tuple, list)) or len(extra_predicates) > 256:
        raise DataError('segmentation auxiliary predicate budget exceeded')
    predicates, predicate_ids, sides = [], {}, []
    nfa_states = work = predicate_pairs = 0

    def charge(count):
        nonlocal work
        work += count
        if work > limits.work:
            raise DataError('segmentation aggregate program work budget exceeded')

    def remaining_work(maximum):
        remaining = limits.work - work
        if remaining <= 0:
            raise DataError('segmentation aggregate program work budget exceeded')
        return min(remaining, maximum)

    for rule in lowered.rules:
        pair = []
        for language, side in ((rule.left, 'left'), (rule.right, 'right')):
            production = production_side(language, side, work_limit=remaining_work(MAX_ANCHOR_VISITS))
            charge(production.construction_work)
            available_states = limits.nfa_states - nfa_states
            if available_states <= 0:
                raise DataError('segmentation aggregate NFA state budget exceeded')
            nfa_limits = NFA_LIMITS._replace(states=min(available_states, NFA_LIMITS.states),
                                            work=remaining_work(NFA_LIMITS.work))
            nfa = compile_language(production.language, reverse=production.reverse, limits=nfa_limits)
            charge(nfa.construction_work)
            if nfa_states + len(nfa.instructions) > limits.nfa_states:
                raise DataError('segmentation aggregate NFA state budget exceeded')
            nfa_states += len(nfa.instructions)
            canonical = []
            for values in nfa.predicates:
                charge(len(values) + 1)
                predicate = predicate_ids.get(values)
                if predicate is None:
                    if len(predicates) >= 256:
                        raise DataError('segmentation program predicate budget exceeded')
                    if predicate_pairs + len(values) > limits.predicate_pairs:
                        raise DataError('segmentation aggregate predicate-pair budget exceeded')
                    charge(len(values) + 1)
                    predicate = len(predicates)
                    predicate_ids[values] = predicate
                    predicates.append(values)
                    predicate_pairs += len(values)
                canonical.append(predicates[predicate])
            # Each NFA compiler normalizes/copies its own range tuples. Replace
            # them before retention so repeated side bodies cannot multiply a
            # large identical predicate beyond the program's unique-pair cap.
            nfa = nfa._replace(predicates=tuple(canonical))
            pair.append((production, nfa))
        sides.append(tuple(pair))
    # Refinement contexts can require predicates not used by a base language.
    # Partition them together so every emitted class has homogeneous context
    # flags; a later policy owner must not infer flags from one sample point.
    for values in extra_predicates:
        if not isinstance(values, (tuple, list)) or len(values) > limits.predicate_pairs:
            raise DataError('invalid segmentation auxiliary predicate')
        charge(len(values) + 1)
        values = checked_ranges(values, limits.predicate_pairs)
        charge(len(values) + 1)
        if values not in predicate_ids:
            if len(predicates) >= 256:
                raise DataError('segmentation program predicate budget exceeded')
            if predicate_pairs + len(values) > limits.predicate_pairs:
                raise DataError('segmentation aggregate predicate-pair budget exceeded')
            charge(len(values) + 1)
            predicate_ids[values] = len(predicates)
            predicates.append(values)
            predicate_pairs += len(values)
    alphabet = partition(tuple(predicates), ALPHABET_LIMITS._replace(work=remaining_work(ALPHABET_LIMITS.work)))
    charge(alphabet.construction_work)
    machines, machine_ids, rules = [], {}, []
    states = cells = members = 0
    for source, pair in zip(lowered.rules, sides):
        indexes = []
        for production, nfa in pair:
            dfa = compile_machine(nfa, alphabet, restart=not production.anchored,
                                  limits=DFA_LIMITS._replace(work=remaining_work(DFA_LIMITS.work)))
            charge(dfa.construction_work)
            item = Machine(dfa, production.reverse, production.anchored)
            # Charge table-key hashing even if this body already has an owner.
            item_cells = sum(len(row) for row in dfa.rows)
            charge(item_cells + len(dfa.accepted) + 3)
            key = _machine_key(item)
            index = machine_ids.get(key)
            if index is None:
                item_states = len(dfa.rows)
                item_members = sum(len(subset) for subset in dfa.subsets)
                if states + item_states > limits.dfa_states:
                    raise DataError('segmentation aggregate DFA state budget exceeded')
                if cells + item_cells > limits.cells:
                    raise DataError('segmentation aggregate transition budget exceeded')
                if members + item_members > limits.members:
                    raise DataError('segmentation aggregate subset-member budget exceeded')
                states += item_states
                cells += item_cells
                members += item_members
                index = len(machines)
                charge(item_cells + len(dfa.accepted) + 3)
                machine_ids[key] = index
                machines.append(item)
            indexes.append(index)
        rules.append(Rule(source, indexes[0], indexes[1], source.breaks))
    return RuleProgram(alphabet, tuple(machines), tuple(rules), lowered,
                       ProgramTotals(nfa_states, states, cells, members, work, predicate_pairs))
