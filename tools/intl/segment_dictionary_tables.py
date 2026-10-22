"""Checked little-endian wire format for owned segmentation dictionary DAGs."""
import struct

import bundle
from cldr_xml import DataError
from segment_dictionary_trie import Trie, MAX_ALPHABET, MAX_VALUES

HEADER = struct.Struct('<8sII')
RECORD = struct.Struct('<12I')
MAGIC = b'QJSDICT\0'
VERSION = 1
FAMILIES = ('cjk', 'khmer', 'lao', 'myanmar', 'thai')
# Bound host tuple/graph-validation amplification as well as serialized bytes.
MAX_STATES = 1000000
MAX_EDGES = 2000000


def layout(alphabet, values, states, edges):
    """Array starts are relative to one block, independent of host alignment."""
    if (any(type(value) is not int for value in (alphabet, values, states, edges)) or
            not 1 <= alphabet <= MAX_ALPHABET or not 1 <= values <= MAX_VALUES or
            not 2 <= states <= MAX_STATES or not 1 <= edges <= MAX_EDGES):
        raise DataError('invalid segmentation dictionary array counts')
    counts = (alphabet, values, states + 1, states, edges, edges)
    starts, offset = [], 0
    for count, width in zip(counts, (4, 4, 4, 2, 2, 4)):
        offset = (offset + 3) & ~3
        starts.append(offset)
        offset += count * width
        if offset > bundle.MAX_SIZE:
            raise DataError('segmentation dictionary block exceeds size budget')
    return tuple(starts), offset


def validate(data):
    """Prove the DAG's language metadata without expanding its accepted words.

    Count paths per edge, not unique terminal states: shared suffixes may be
    reached by several prefixes. Postorder permits bounded iterative validation.
    """
    if not isinstance(data, Trie) or type(data.valued) is not bool:
        raise DataError('invalid segmentation dictionary array owner')
    if any(type(array) is not tuple for array in data[:6]):
        raise DataError('invalid segmentation dictionary array owner')
    alphabet, values, starts, terminals, labels, targets = data[:6]
    states, edges = len(terminals), len(labels)
    layout(len(alphabet), len(values), states, edges)
    if len(starts) != states + 1 or len(targets) != edges:
        raise DataError('invalid segmentation dictionary array lengths')
    metadata = data.root, data.words, data.max_scalars, data.max_utf16
    if any(type(value) is not int or not 0 <= value <= 0xffffffff for value in metadata):
        raise DataError('invalid segmentation dictionary metadata integer')
    for array, maximum in zip(data[:6], (0xffffffff, 0xffffffff, 0xffffffff, 0xffff, 0xffff, 0xffffffff)):
        if any(type(value) is not int or not 0 <= value <= maximum for value in array):
            raise DataError('invalid segmentation dictionary array integer')
    if (starts[0] or starts[-1] != edges or data.root != states - 1 or terminals[data.root] or
            not data.words or not data.max_scalars or not data.max_utf16):
        raise DataError('invalid segmentation dictionary root or metadata')
    if (any(not 0 <= scalar <= 0x10ffff or 0xd800 <= scalar <= 0xdfff for scalar in alphabet) or
            tuple(sorted(set(alphabet))) != alphabet or
            any(not 0 <= value <= 0xffffffff for value in values) or
            tuple(sorted(set(values))) != values or (not data.valued and values != (0,))):
        raise DataError('invalid segmentation dictionary alphabet or value pool')

    if set(labels) != set(range(len(alphabet))) or set(terminals) - {0} != set(range(1, len(values) + 1)):
        raise DataError('unused segmentation dictionary alphabet or value')
    paths, scalars, units = [0] * states, [0] * states, [0] * states
    for state, terminal in enumerate(terminals):
        first, last = starts[state:state + 2]
        if not 0 <= first <= last <= edges or not 0 <= terminal <= len(values):
            raise DataError('invalid segmentation dictionary state')
        count, scalar_max, unit_max, previous = int(terminal != 0), 0, 0, -1
        for edge in range(first, last):
            label, child = labels[edge], targets[edge]
            if not previous < label < len(alphabet) or not 0 <= child < state:
                raise DataError('invalid segmentation dictionary edge')
            previous = label
            count += paths[child]
            if count > data.words:
                raise DataError('segmentation dictionary accepted-path count exceeds metadata')
            scalar_max = max(scalar_max, 1 + scalars[child])
            unit_max = max(unit_max, (2 if alphabet[label] > 0xffff else 1) + units[child])
        if not count:
            raise DataError('unproductive segmentation dictionary state')
        paths[state], scalars[state], units[state] = count, scalar_max, unit_max
    if (paths[data.root], scalars[data.root], units[data.root]) != (
            data.words, data.max_scalars, data.max_utf16):
        raise DataError('inconsistent segmentation dictionary language metadata')

    reachable = bytearray(states)
    reachable[data.root] = 1
    for state in reversed(range(states)):
        if not reachable[state]:
            raise DataError('unreachable segmentation dictionary state')
        for edge in range(starts[state], starts[state + 1]):
            reachable[targets[edge]] = 1


def encode(dictionaries):
    """Return an optional Intl section; source identity belongs to its envelope."""
    if not isinstance(dictionaries, dict) or not dictionaries or set(dictionaries) - set(FAMILIES):
        raise DataError('invalid segmentation dictionary family inventory')
    payload = bytearray(HEADER.pack(MAGIC, VERSION, len(dictionaries)))
    payload.extend(bytes(RECORD.size * len(dictionaries)))
    total_states = total_edges = 0
    for index, family in enumerate(sorted(dictionaries, key=FAMILIES.index)):
        data = dictionaries[family]
        if not isinstance(data, Trie) or type(data.valued) is not bool or data.valued != (family == 'cjk'):
            raise DataError('invalid segmentation dictionary family/value mode')
        validate(data)
        counts = len(data.alphabet), len(data.values), len(data.terminals), len(data.labels)
        total_states += counts[2]
        total_edges += counts[3]
        if total_states > MAX_STATES or total_edges > MAX_EDGES:
            raise DataError('segmentation dictionary aggregate graph budget exceeded')
        offsets, size = layout(*counts)
        start = len(payload)
        if size > bundle.MAX_SIZE - start:
            raise DataError('segmentation dictionary section exceeds size budget')
        payload.extend(bytes(size))
        arrays = data.alphabet, data.values, data.edge_starts, data.terminals, data.labels, data.targets
        for offset, code, values in zip(offsets, ('I', 'I', 'I', 'H', 'H', 'I'), arrays):
            raw = struct.pack('<{}{}'.format(len(values), code), *values)
            payload[start + offset:start + offset + len(raw)] = raw
        RECORD.pack_into(payload, HEADER.size + index * RECORD.size,
                         FAMILIES.index(family), int(data.valued), data.root, data.words,
                         data.max_scalars, data.max_utf16, *counts, start, size)
    result = len(dictionaries), bytes(payload)
    decode(result)
    return result


def decode(section):
    if section is None:
        return {}
    if not isinstance(section, tuple) or len(section) != 2:
        raise DataError('invalid segmentation dictionary section pair')
    count, payload = section
    if (type(count) is not int or not isinstance(payload, bytes) or not HEADER.size <= len(payload) <= bundle.MAX_SIZE or
            not 1 <= count <= len(FAMILIES)):
        raise DataError('invalid segmentation dictionary section')
    magic, version, records = HEADER.unpack_from(payload)
    if magic != MAGIC or version != VERSION or records != count:
        raise DataError('invalid segmentation dictionary header')
    expected = HEADER.size + RECORD.size * count
    if expected > len(payload):
        raise DataError('truncated segmentation dictionary directory')
    result, previous = {}, -1
    total_states = total_edges = 0
    for index in range(count):
        (family, valued, root, words, max_scalars, max_utf16,
         alphabet, values, states, edges, start, size) = RECORD.unpack_from(payload, HEADER.size + index * RECORD.size)
        if (not previous < family < len(FAMILIES) or valued != int(family == 0) or
                start != expected or start > len(payload) or size > len(payload) - start):
            raise DataError('invalid segmentation dictionary descriptor')
        offsets, length = layout(alphabet, values, states, edges)
        total_states += states
        total_edges += edges
        if total_states > MAX_STATES or total_edges > MAX_EDGES:
            raise DataError('segmentation dictionary aggregate graph budget exceeded')
        if size != length:
            raise DataError('invalid segmentation dictionary block length')
        arrays = []
        end = 0
        counts = alphabet, values, states + 1, states, edges, edges
        for offset, code, items in zip(offsets, ('I', 'I', 'I', 'H', 'H', 'I'), counts):
            if any(payload[start + end:start + offset]):
                raise DataError('nonzero segmentation dictionary array padding')
            row = struct.unpack_from('<{}{}'.format(items, code), payload, start + offset)
            arrays.append(row)
            end = offset + items * (2 if code == 'H' else 4)
        data = Trie(*arrays, root, bool(valued), words, max_scalars, max_utf16)
        validate(data)
        result[FAMILIES[family]] = data
        previous, expected = family, start + size
    if expected != len(payload):
        raise DataError('unclaimed segmentation dictionary section bytes')
    return result
