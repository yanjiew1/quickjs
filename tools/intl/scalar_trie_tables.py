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

"""Checked array blocks and language metadata for literal scalar DAGs.

Containers own family/list identities and membership-versus-value policy. This
module owns the common aligned arrays and graph safety/language invariants.
"""
import struct

import bundle
from cldr_xml import DataError
from scalar_trie import Graph

MAX_ALPHABET, MAX_VALUES = 65536, 65535
MAX_STATES, MAX_EDGES = 1000000, 2000000


def layout(alphabet, values, states, edges):
    """Array starts are relative to one block, independent of host alignment."""
    if (any(type(value) is not int for value in (alphabet, values, states, edges)) or
            not 1 <= alphabet <= MAX_ALPHABET or not 1 <= values <= MAX_VALUES or
            not 2 <= states <= MAX_STATES or not 1 <= edges <= MAX_EDGES):
        raise DataError('invalid scalar graph array counts')
    counts = (alphabet, values, states + 1, states, edges, edges)
    starts, offset = [], 0
    for count, width in zip(counts, (4, 4, 4, 2, 2, 4)):
        offset = (offset + 3) & ~3
        starts.append(offset)
        offset += count * width
        if offset > bundle.MAX_SIZE:
            raise DataError('scalar graph block exceeds size budget')
    return tuple(starts), offset


def validate(data):
    """Prove the DAG's language metadata without expanding its accepted words.

    Count paths per edge, not unique terminal states: shared suffixes may be
    reached by several prefixes. Postorder permits bounded iterative validation.
    """
    if not isinstance(data, Graph):
        raise DataError('invalid scalar graph array owner')
    if any(type(array) is not tuple for array in data[:6]):
        raise DataError('invalid scalar graph array owner')
    alphabet, values, starts, terminals, labels, targets = data[:6]
    states, edges = len(terminals), len(labels)
    layout(len(alphabet), len(values), states, edges)
    if len(starts) != states + 1 or len(targets) != edges:
        raise DataError('invalid scalar graph array lengths')
    metadata = data.root, data.words, data.max_scalars, data.max_utf16
    if any(type(value) is not int or not 0 <= value <= 0xffffffff for value in metadata):
        raise DataError('invalid scalar graph metadata integer')
    for array, maximum in zip(data[:6], (0xffffffff, 0xffffffff, 0xffffffff, 0xffff, 0xffff, 0xffffffff)):
        if any(type(value) is not int or not 0 <= value <= maximum for value in array):
            raise DataError('invalid scalar graph array integer')
    if (starts[0] or starts[-1] != edges or data.root != states - 1 or terminals[data.root] or
            not data.words or not data.max_scalars or not data.max_utf16):
        raise DataError('invalid scalar graph root or metadata')
    if (any(not 0 <= scalar <= 0x10ffff or 0xd800 <= scalar <= 0xdfff for scalar in alphabet) or
            tuple(sorted(set(alphabet))) != alphabet or
            any(not 0 <= value <= 0xffffffff for value in values) or
            tuple(sorted(set(values))) != values):
        raise DataError('invalid scalar graph alphabet or value pool')

    if set(labels) != set(range(len(alphabet))) or set(terminals) - {0} != set(range(1, len(values) + 1)):
        raise DataError('unused scalar graph alphabet or value')
    paths, scalars, units = [0] * states, [0] * states, [0] * states
    for state, terminal in enumerate(terminals):
        first, last = starts[state:state + 2]
        if not 0 <= first <= last <= edges or not 0 <= terminal <= len(values):
            raise DataError('invalid scalar graph state')
        count, scalar_max, unit_max, previous = int(terminal != 0), 0, 0, -1
        for edge in range(first, last):
            label, child = labels[edge], targets[edge]
            if not previous < label < len(alphabet) or not 0 <= child < state:
                raise DataError('invalid scalar graph edge')
            previous = label
            count += paths[child]
            if count > data.words:
                raise DataError('scalar graph accepted-path count exceeds metadata')
            scalar_max = max(scalar_max, 1 + scalars[child])
            unit_max = max(unit_max, (2 if alphabet[label] > 0xffff else 1) + units[child])
        if not count:
            raise DataError('unproductive scalar graph state')
        paths[state], scalars[state], units[state] = count, scalar_max, unit_max
    if (paths[data.root], scalars[data.root], units[data.root]) != (
            data.words, data.max_scalars, data.max_utf16):
        raise DataError('inconsistent scalar graph language metadata')

    reachable = bytearray(states)
    reachable[data.root] = 1
    for state in reversed(range(states)):
        if not reachable[state]:
            raise DataError('unreachable scalar graph state')
        for edge in range(starts[state], starts[state + 1]):
            reachable[targets[edge]] = 1


def encode_block(data):
    """Encode one validated graph; container metadata is stored separately."""
    validate(data)
    offsets, size = layout(len(data.alphabet), len(data.values), len(data.terminals), len(data.labels))
    payload = bytearray(size)
    for offset, code, values in zip(offsets, ('I', 'I', 'I', 'H', 'H', 'I'), data[:6]):
        raw = struct.pack('<{}{}'.format(len(values), code), *values)
        payload[offset:offset + len(raw)] = raw
    return bytes(payload)


def decode_block(payload, start, size, counts, metadata):
    """Decode a borrowed extent after bounding dimensions, before tuple copies."""
    if (type(payload) is not bytes or type(start) is not int or type(size) is not int or
            not 0 <= start <= len(payload) or not 0 <= size <= len(payload) - start or
            type(counts) is not tuple or len(counts) != 4 or
            type(metadata) is not tuple or len(metadata) != 4):
        raise DataError('invalid scalar graph block owner/extent')
    alphabet, values, states, edges = counts
    offsets, length = layout(*counts)
    if size != length:
        raise DataError('invalid scalar graph block length')
    if (any(type(value) is not int or not 0 <= value <= 0xffffffff for value in metadata) or
            metadata[0] != states - 1 or not all(metadata[1:])):
        raise DataError('invalid scalar graph block metadata')
    arrays, end = [], 0
    for offset, code, items in zip(offsets, ('I', 'I', 'I', 'H', 'H', 'I'),
                                    (alphabet, values, states + 1, states, edges, edges)):
        if any(payload[start + end:start + offset]):
            raise DataError('nonzero scalar graph array padding')
        arrays.append(struct.unpack_from('<{}{}'.format(items, code), payload, start + offset))
        end = offset + items * (2 if code == 'H' else 4)
    data = Graph(*arrays, *metadata)
    validate(data)
    return data
