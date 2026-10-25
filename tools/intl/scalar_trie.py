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

"""Bounded scalar-key DAG construction shared by literal data consumers.

Keys are sorted, nonempty Unicode scalar strings; terminal payloads are u32
values or None (membership value zero). Consumer-specific key/cost policies
belong to their source owners, not to the suffix-sharing mechanics here.
"""
from collections import namedtuple

from cldr_xml import DataError

Graph = namedtuple('Graph', 'alphabet values edge_starts terminals labels targets root '
                  'words max_scalars max_utf16')
GraphLimits = namedtuple('GraphLimits', 'entries scalars key_length states edges alphabet values')
DEFAULT_LIMITS = GraphLimits(1000000, 16777216, 4096, 1000000, 2000000, 65536, 65535)


class _Node:
    __slots__ = ('terminal', 'edges')

    def __init__(self):
        self.terminal = 0
        self.edges = []


def prepare(entries, limits=DEFAULT_LIMITS):
    """Finalize only the active prefix frontier, interning equivalent suffixes.

    Children precede parents, so every edge target is below its source state.
    Terminal payload identity participates in sharing. No recursion, source
    normalization or runtime matching policy is involved.
    """
    if (not isinstance(limits, GraphLimits) or
            any(type(value) is not int or not 1 <= value <= maximum
                for value, maximum in zip(limits, DEFAULT_LIMITS))):
        raise DataError('invalid scalar graph limits')
    if type(entries) is not tuple or not 1 <= len(entries) <= limits.entries:
        raise DataError('scalar graph entry budget exceeded')
    alphabet, values = set(), set()
    previous = ''
    scalar_count = max_scalars = max_utf16 = 0
    for entry in entries:
        if not isinstance(entry, tuple) or len(entry) != 2:
            raise DataError('invalid scalar graph entry')
        key, value = entry
        if not isinstance(key, str) or not key or key <= previous:
            raise DataError('invalid, duplicate or unordered scalar graph key')
        scalar_count += len(key)
        if len(key) > limits.key_length or scalar_count > limits.scalars:
            raise DataError('scalar graph key/scalar budget exceeded')
        if value is not None and (type(value) is not int or not 0 <= value <= 0xffffffff):
            raise DataError('invalid scalar graph terminal payload')
        values.add(0 if value is None else value)
        if len(values) > limits.values:
            raise DataError('scalar graph value budget exceeded')
        units = len(key)
        for character in key:
            code = ord(character)
            if 0xd800 <= code <= 0xdfff:
                raise DataError('surrogate in scalar graph key')
            if code not in alphabet:
                if len(alphabet) >= limits.alphabet:
                    raise DataError('scalar graph alphabet budget exceeded')
                alphabet.add(code)
            units += code > 0xffff
        max_scalars = max(max_scalars, len(key))
        max_utf16 = max(max_utf16, units)
        previous = key
    alphabet, values = tuple(sorted(alphabet)), tuple(sorted(values))
    alphabet_ids = {scalar: index for index, scalar in enumerate(alphabet)}
    value_ids = {value: index + 1 for index, value in enumerate(values)}
    states, interned = [], {}
    frontier = [_Node()]
    previous = ''
    edge_count = pending_edges = 0

    def intern(node):
        nonlocal edge_count, pending_edges
        pending_edges -= len(node.edges)
        key = node.terminal, tuple(node.edges)
        found = interned.get(key)
        if found is not None:
            return found
        if len(states) >= limits.states or len(node.edges) > limits.edges - edge_count - pending_edges:
            raise DataError('scalar graph state/edge budget exceeded')
        found = len(states)
        states.append(key)
        interned[key] = found
        edge_count += len(node.edges)
        return found

    def finish_suffix(common):
        nonlocal pending_edges
        while len(frontier) > common + 1:
            depth = len(frontier) - 1
            child = intern(frontier.pop())
            label = alphabet_ids[ord(previous[depth - 1])]
            if pending_edges + edge_count >= limits.edges:
                raise DataError('scalar graph active-edge budget exceeded')
            frontier[-1].edges.append((label, child))
            pending_edges += 1

    for word, value in entries:
        common = 0
        while common < min(len(previous), len(word)) and previous[common] == word[common]:
            common += 1
        finish_suffix(common)
        frontier.extend(_Node() for _ in range(len(word) - common))
        frontier[-1].terminal = value_ids[0 if value is None else value]
        previous = word
    finish_suffix(0)
    root = intern(frontier.pop())

    starts, terminals, labels, targets = [0], [], [], []
    for terminal, edges in states:
        terminals.append(terminal)
        labels.extend(label for label, target in edges)
        targets.extend(target for label, target in edges)
        starts.append(len(labels))
    return Graph(alphabet, values, tuple(starts), tuple(terminals), tuple(labels), tuple(targets),
                 root, len(entries), max_scalars, max_utf16)
