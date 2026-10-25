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

"""Canonical native segmentation tables, separate from construction proofs.

This optional section contains prepared routes, not executable capabilities.
The recipe fingerprint pairs tables with source/correction receipts; it does
not authenticate a pack. Runtime refinement requirements remain unresolved.
"""
from collections import namedtuple
import struct

from cldr_xml import DataError
from segment_alphabet import Alphabet
from segment_dfa import DFA
from segment_profile import Profile
from segment_program import Machine as HostMachine, RuleProgram
from segment_sets import DOMAIN_END
from segment_sources import KINDS

MAGIC = b'QJSSEGM\0'
VERSION, UNICODE_VERSION, SEMANTIC_PROFILE = 1, 18 << 16, 1
HEADER = struct.Struct('<8s8I32s')
DESCRIPTOR = struct.Struct('<10I')
BINDING = struct.Struct('<3I')
RULE = struct.Struct('<HHBBH')
MACHINE = struct.Struct('<4I')
LEFT_RESTART, RIGHT_REVERSED_RESTART, LEFT_BOS_ONLY = range(3)
REQUIREMENTS = (0, 13, 10)
MAX_SIZE = 32 * 1024 * 1024
MAX_PROGRAMS, MAX_BINDINGS = 64, 65536
MAX_CLASSES, MAX_SPANS, MAX_RULES, MAX_MACHINES = 16384, 262144, 1024, 2048
MAX_STATES, MAX_CELLS = 4096, 1048576
MAX_TOTAL_STATES, MAX_TOTAL_CELLS = 262144, 8388608

Machine = namedtuple('Machine', 'rows accepted mode')
Rule = namedtuple('Rule', 'left right breaks role')
Program = namedtuple('Program', 'kind requirements ranges flags rules machines')
Tables = namedtuple('Tables', 'fingerprint programs bindings')


def _integer(value, minimum, maximum):
    return type(value) is int and minimum <= value <= maximum


def _align(value):
    return (value + 3) & ~3


def layout(classes, spans, rules, state_counts):
    """Canonical array offsets relative to a program block, before allocation."""
    if (not _integer(classes, 1, MAX_CLASSES) or not _integer(spans, 1, MAX_SPANS) or
            not _integer(rules, 0, MAX_RULES) or not isinstance(state_counts, tuple) or
            len(state_counts) > min(MAX_MACHINES, 2 * rules) or bool(rules) != bool(state_counts)):
        raise DataError('invalid segmentation program array counts')
    offsets, at = [], 0
    for count, width in ((spans, 4), (spans, 2), (classes, 1), (rules, RULE.size),
                         (len(state_counts), MACHINE.size)):
        at = _align(at)
        offsets.append(at)
        at += count * width
    bodies = []
    for states in state_counts:
        if not _integer(states, 1, MAX_STATES) or states * classes > MAX_CELLS:
            raise DataError('segmentation machine state/cell budget exceeded')
        transitions = _align(at)
        accepts = _align(transitions + states * classes * 2)
        at = _align(accepts + (states + 7) // 8)
        if at > MAX_SIZE:
            raise DataError('segmentation program block size budget exceeded')
        bodies.append((transitions, accepts))
    return tuple(offsets), tuple(bodies), _align(at)


def _profile_counts(profile):
    """Preflight raw owners and dimensions before constructing projected lists."""
    if (not isinstance(profile, Profile) or profile.kind not in KINDS or
            not isinstance(profile.program, RuleProgram) or
            not isinstance(profile.program.alphabet, Alphabet)):
        raise DataError('invalid segmentation profile owner')
    program = profile.program
    if any(type(array) is not tuple for array in
           (profile.roles, profile.class_flags, program.rules, program.machines, program.alphabet.ranges)):
        raise DataError('invalid segmentation profile array owner')
    if (len(program.machines) > MAX_MACHINES or len(program.rules) > MAX_RULES or
            len(program.alphabet.ranges) > MAX_SPANS or len(profile.class_flags) > MAX_CLASSES):
        raise DataError('segmentation profile dimension budget exceeded')
    if len(profile.roles) != len(program.rules):
        raise DataError('segmentation rule/role length mismatch')
    for machine in program.machines:
        if (not isinstance(machine, HostMachine) or not isinstance(machine.dfa, DFA) or
                type(machine.dfa.rows) is not tuple or type(machine.dfa.accepted) is not tuple):
            raise DataError('invalid segmentation profile machine owner')
    counts = tuple(len(machine.dfa.rows) for machine in program.machines)
    layout(len(profile.class_flags), len(program.alphabet.ranges), len(program.rules), counts)
    return len(program.alphabet.ranges), sum(counts), sum(counts) * len(profile.class_flags)


def project(profile):
    """Discard host-only AST/NFA/subset statistics, retaining execution policy."""
    _profile_counts(profile)
    program = profile.program
    machines = []
    for machine in program.machines:
        if (any(type(value) is not bool for value in
                (machine.reverse, machine.anchored, machine.dfa.restart)) or
                machine.reverse and machine.anchored or
                machine.dfa.restart == machine.anchored):
            raise DataError('invalid segmentation machine direction/restart mode')
        mode = RIGHT_REVERSED_RESTART if machine.reverse else (
            LEFT_BOS_ONLY if machine.anchored else LEFT_RESTART)
        machines.append(Machine(machine.dfa.rows, machine.dfa.accepted, mode))
    rules = tuple(Rule(row.left, row.right, row.breaks, role)
                  for row, role in zip(program.rules, profile.roles))
    return Program(KINDS.index(profile.kind), profile.requirements, program.alphabet.ranges,
                   profile.class_flags, rules, tuple(machines))


def validate(program):
    """Check graph structure and policy, not equivalence with source languages."""
    if (not isinstance(program, Program) or not _integer(program.kind, 0, 2) or
            type(program.requirements) is not int or program.requirements != REQUIREMENTS[program.kind] or
            any(type(array) is not tuple for array in program[2:])):
        raise DataError('invalid segmentation wire program owner')
    classes = len(program.flags)
    if (len(program.machines) > MAX_MACHINES or len(program.rules) > MAX_RULES or
            len(program.ranges) > MAX_SPANS or classes > MAX_CLASSES):
        raise DataError('segmentation wire dimension budget exceeded')
    if any(not isinstance(item, Machine) or type(item.rows) is not tuple or
           type(item.accepted) is not tuple for item in program.machines):
        raise DataError('invalid segmentation wire machine owner')
    counts = tuple(len(row.rows) for row in program.machines)
    layout(classes, len(program.ranges), len(program.rules), counts)
    _aggregate_counts(((len(program.ranges), sum(counts), sum(counts) * classes),))
    previous, prior_class, used = 0, None, 0
    for span in program.ranges:
        if (type(span) is not tuple or len(span) != 3 or
                any(type(value) is not int for value in span)):
            raise DataError('invalid segmentation class range')
        start, end, identifier = span
        if start != previous or not start < end <= DOMAIN_END or not 0 <= identifier < classes or identifier == prior_class:
            raise DataError('invalid segmentation class coverage')
        if identifier > used:
            raise DataError('noncanonical segmentation class order')
        if identifier == used:
            used += 1
        previous, prior_class = end, identifier
    if previous != DOMAIN_END or used != classes:
        raise DataError('incomplete segmentation class coverage')
    allowed = (0, 1, 2, 4, 8, 16) if program.kind == 2 else (0,)
    if any(type(flag) is not int or flag not in allowed for flag in program.flags):
        raise DataError('invalid segmentation context flags')
    used = 0
    for rule in program.rules:
        if (not isinstance(rule, Rule) or type(rule.breaks) is not bool or
                not _integer(rule.role, 0, 2) or
                rule.role == 1 and (program.kind != 1 or rule.breaks) or
                rule.role == 2 and (program.kind != 2 or not rule.breaks) or
                any(not _integer(index, 0, len(program.machines) - 1) for index in (rule.left, rule.right))):
            raise DataError('invalid segmentation rule role/reference')
        if program.machines[rule.left].mode not in (LEFT_RESTART, LEFT_BOS_ONLY) or program.machines[rule.right].mode != RIGHT_REVERSED_RESTART:
            raise DataError('invalid segmentation rule direction')
        for index in (rule.left, rule.right):
            if index > used:
                raise DataError('noncanonical segmentation machine order')
            if index == used:
                used += 1
    if used != len(program.machines):
        raise DataError('unused segmentation machine')
    for machine in program.machines:
        states = len(machine.rows)
        if not _integer(machine.mode, 0, 2) or len(machine.accepted) != states:
            raise DataError('invalid segmentation machine mode/acceptance')
        if any(type(value) is not bool for value in machine.accepted):
            raise DataError('invalid segmentation acceptance value')
        reached = 1
        for state, row in enumerate(machine.rows):
            if type(row) is not tuple or len(row) != classes or any(not _integer(value, 0, states - 1) for value in row):
                raise DataError('invalid segmentation transition target')
            # The compiler numbers states by BFS discovery in class order.
            # Enforce that stronger invariant so the C reader can also prove
            # reachability without a queue, visited array or allocation.
            if state >= reached:
                raise DataError('unreachable segmentation machine state')
            for target in row:
                if target > reached:
                    raise DataError('noncanonical segmentation state order')
                if target == reached:
                    reached += 1
        if reached != states:
            raise DataError('unreachable segmentation machine state')


def _bindings(bindings, programs, locale_count):
    if not _integer(locale_count, 1, 0xffffffff) or type(bindings) is not tuple or not 1 <= len(bindings) <= MAX_BINDINGS:
        raise DataError('invalid segmentation binding inventory')
    previous, used = None, 0
    for row in bindings:
        if (type(row) is not tuple or len(row) != 3 or
                not _integer(row[0], 0, locale_count - 1) or not _integer(row[1], 0, 2) or
                not _integer(row[2], 0, len(programs) - 1) or programs[row[2]].kind != row[1] or
                previous is not None and row[:2] <= previous):
            raise DataError('invalid segmentation locale/program binding')
        previous = row[:2]
        if row[2] > used:
            raise DataError('noncanonical segmentation program order')
        if row[2] == used:
            used += 1
    if used != len(programs):
        raise DataError('unbound segmentation program')


def _aggregate_counts(counts):
    spans = states = cells = 0
    for span_count, state_count, cell_count in counts:
        spans += span_count
        states += state_count
        cells += cell_count
        if spans > MAX_SPANS or states > MAX_TOTAL_STATES or cells > MAX_TOTAL_CELLS:
            raise DataError('segmentation aggregate wire budget exceeded')


def encode(profiles, bindings, fingerprint, locale_count):
    """Encode explicitly deduplicated profiles and resolved locale routes."""
    if type(profiles) is not tuple or not 1 <= len(profiles) <= MAX_PROGRAMS or type(fingerprint) is not bytes or len(fingerprint) != 32:
        raise DataError('invalid segmentation program inventory/fingerprint')
    _aggregate_counts(_profile_counts(profile) for profile in profiles)
    programs = tuple(project(profile) for profile in profiles)
    for program in programs:
        validate(program)
    _bindings(bindings, programs, locale_count)
    payload = bytearray(HEADER.size + DESCRIPTOR.size * len(programs) + BINDING.size * len(bindings))
    for index, row in enumerate(bindings):
        BINDING.pack_into(payload, HEADER.size + DESCRIPTOR.size * len(programs) + index * BINDING.size, *row)
    for index, program in enumerate(programs):
        offsets, bodies, size = layout(len(program.flags), len(program.ranges), len(program.rules),
                                       tuple(len(machine.rows) for machine in program.machines))
        start = len(payload)
        if size > MAX_SIZE - start:
            raise DataError('segmentation program section size budget exceeded')
        payload.extend(bytes(size))
        DESCRIPTOR.pack_into(payload, HEADER.size + index * DESCRIPTOR.size,
                             program.kind, program.requirements, len(program.flags), len(program.ranges),
                             len(program.rules), len(program.machines), start, size, 0, 0)
        for span, (low, high, identifier) in enumerate(program.ranges):
            struct.pack_into('<I', payload, start + offsets[0] + span * 4, high)
            struct.pack_into('<H', payload, start + offsets[1] + span * 2, identifier)
        payload[start + offsets[2]:start + offsets[2] + len(program.flags)] = bytes(program.flags)
        for rule_index, rule in enumerate(program.rules):
            RULE.pack_into(payload, start + offsets[3] + rule_index * RULE.size,
                           rule.left, rule.right, int(rule.breaks), rule.role, 0)
        for machine_index, (machine, (transitions, accepts)) in enumerate(zip(program.machines, bodies)):
            MACHINE.pack_into(payload, start + offsets[4] + machine_index * MACHINE.size,
                              len(machine.rows), machine.mode, transitions, accepts)
            for state, row in enumerate(machine.rows):
                struct.pack_into('<{}H'.format(len(row)), payload,
                                 start + transitions + state * len(row) * 2, *row)
                if machine.accepted[state]:
                    payload[start + accepts + state // 8] |= 1 << (state & 7)
    HEADER.pack_into(payload, 0, MAGIC, VERSION, UNICODE_VERSION, SEMANTIC_PROFILE,
                     len(programs), len(bindings), len(payload), 0, 0, fingerprint)
    result = len(programs), bytes(payload)
    decode(result, locale_count)
    return result


def decode(section, locale_count):
    """Validate every record before returning complete immutable host views."""
    if section is None:
        return None
    if not isinstance(section, tuple) or len(section) != 2:
        raise DataError('invalid segmentation program section pair')
    count, payload = section
    if not _integer(count, 1, MAX_PROGRAMS) or type(payload) is not bytes or not HEADER.size <= len(payload) <= MAX_SIZE:
        raise DataError('invalid segmentation program section')
    magic, version, unicode_version, profile, records, bindings_count, size, reserved1, reserved2, fingerprint = HEADER.unpack_from(payload)
    if ((magic, version, unicode_version, profile) != (MAGIC, VERSION, UNICODE_VERSION, SEMANTIC_PROFILE) or
            records != count or size != len(payload) or reserved1 or reserved2 or not 1 <= bindings_count <= MAX_BINDINGS):
        raise DataError('invalid segmentation program header')
    binding_start = HEADER.size + count * DESCRIPTOR.size
    expected = binding_start + bindings_count * BINDING.size
    if expected > size:
        raise DataError('truncated segmentation program directory')
    programs = []
    total_spans = total_states = total_cells = 0
    for index in range(count):
        kind, requirements, classes, spans, rules_count, machines_count, start, length, reserved1, reserved2 = DESCRIPTOR.unpack_from(payload, HEADER.size + index * DESCRIPTOR.size)
        if (not 0 <= kind <= 2 or requirements != REQUIREMENTS[kind] or reserved1 or reserved2 or
                start != expected or start > size or length > size - start or not 0 <= machines_count <= MAX_MACHINES):
            raise DataError('invalid segmentation program descriptor')
        # Establish the fixed arrays before reading machine state counts, then
        # recompute the complete canonical layout before unpacking table cells.
        offsets, _, _ = layout(classes, spans, rules_count, (1,) * machines_count)
        fixed_end = offsets[4] + machines_count * MACHINE.size
        if fixed_end > length:
            raise DataError('truncated segmentation program arrays')
        descriptors = tuple(MACHINE.unpack_from(payload, start + offsets[4] + item * MACHINE.size) for item in range(machines_count))
        if any(mode > LEFT_BOS_ONLY for states, mode, transitions, accepts in descriptors):
            raise DataError('invalid segmentation machine mode')
        offsets, bodies, expected_length = layout(classes, spans, rules_count, tuple(row[0] for row in descriptors))
        if expected_length != length:
            raise DataError('invalid segmentation program block length')
        total_spans += spans
        total_states += sum(row[0] for row in descriptors)
        total_cells += sum(row[0] * classes for row in descriptors)
        if total_spans > MAX_SPANS or total_states > MAX_TOTAL_STATES or total_cells > MAX_TOTAL_CELLS:
            raise DataError('segmentation aggregate wire budget exceeded')
        end = 0

        def padding(to):
            nonlocal end
            if any(payload[start + end:start + to]):
                raise DataError('nonzero segmentation program padding')
            end = to

        arrays = []
        for offset, number, code in zip(offsets[:3], (spans, spans, classes), ('I', 'H', 'B')):
            padding(offset)
            arrays.append(struct.unpack_from('<{}{}'.format(number, code), payload, start + offset))
            end = offset + number * struct.calcsize(code)
        ranges, previous = [], 0
        for high, identifier in zip(arrays[0], arrays[1]):
            ranges.append((previous, high, identifier))
            previous = high
        padding(offsets[3])
        rules = []
        for item in range(rules_count):
            left, right, breaks, role, reserved = RULE.unpack_from(payload, start + offsets[3] + item * RULE.size)
            if breaks > 1 or reserved:
                raise DataError('invalid segmentation rule action/reserved field')
            rules.append(Rule(left, right, bool(breaks), role))
        end = offsets[3] + rules_count * RULE.size
        padding(offsets[4])
        end = fixed_end
        machines = []
        for (states, mode, transitions, accepts), (expected_transitions, expected_accepts) in zip(descriptors, bodies):
            if (transitions, accepts) != (expected_transitions, expected_accepts):
                raise DataError('noncanonical segmentation machine offsets')
            padding(transitions)
            rows = tuple(struct.unpack_from('<{}H'.format(classes), payload, start + transitions + state * classes * 2) for state in range(states))
            end = transitions + states * classes * 2
            padding(accepts)
            accepted = tuple(bool(payload[start + accepts + state // 8] & (1 << (state & 7))) for state in range(states))
            end = accepts + (states + 7) // 8
            if states & 7 and payload[start + end - 1] >> (states & 7):
                raise DataError('nonzero segmentation acceptance high bits')
            machines.append(Machine(rows, accepted, mode))
        padding(length)
        program = Program(kind, requirements, tuple(ranges), arrays[2], tuple(rules), tuple(machines))
        validate(program)
        programs.append(program)
        expected = start + length
    if expected != size:
        raise DataError('unclaimed segmentation program bytes')
    bindings = tuple(BINDING.unpack_from(payload, binding_start + index * BINDING.size) for index in range(bindings_count))
    _bindings(bindings, programs, locale_count)
    return Tables(fingerprint, tuple(programs), bindings)
