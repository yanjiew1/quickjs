"""Checked little-endian wire format for owned segmentation dictionary DAGs."""
import struct

import bundle
from cldr_xml import DataError
from segment_dictionary_trie import Trie
from scalar_trie import Graph
import scalar_trie_tables as graph_tables
from scalar_trie_tables import layout, MAX_STATES, MAX_EDGES

HEADER = struct.Struct('<8sII')
RECORD = struct.Struct('<12I')
MAGIC = b'QJSDICT\0'
VERSION = 1
FAMILIES = ('cjk', 'khmer', 'lao', 'myanmar', 'thai')

def _graph(data):
    if not isinstance(data, Trie) or type(data.valued) is not bool:
        raise DataError('invalid segmentation dictionary array owner')
    if any(type(array) is not tuple for array in data[:6]):
        raise DataError('invalid segmentation dictionary array owner')
    if not data.valued and data.values != (0,):
        raise DataError('payload on a membership dictionary')
    return Graph(*data[:6], data.root, data.words, data.max_scalars, data.max_utf16)


def validate(data):
    """Validate graph mechanics and the dictionary container's payload mode."""
    graph_tables.validate(_graph(data))


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
        graph = _graph(data)
        counts = len(data.alphabet), len(data.values), len(data.terminals), len(data.labels)
        total_states += counts[2]
        total_edges += counts[3]
        if total_states > MAX_STATES or total_edges > MAX_EDGES:
            raise DataError('segmentation dictionary aggregate graph budget exceeded')
        _, size = layout(*counts)
        start = len(payload)
        if size > bundle.MAX_SIZE - start:
            raise DataError('segmentation dictionary section exceeds size budget')
        payload.extend(graph_tables.encode_block(graph))
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
        _, length = layout(alphabet, values, states, edges)
        total_states += states
        total_edges += edges
        if total_states > MAX_STATES or total_edges > MAX_EDGES:
            raise DataError('segmentation dictionary aggregate graph budget exceeded')
        if size != length:
            raise DataError('invalid segmentation dictionary block length')
        graph = graph_tables.decode_block(payload, start, size, (alphabet, values, states, edges),
                                          (root, words, max_scalars, max_utf16))
        data = Trie(*graph[:6], graph.root, bool(valued), graph.words, graph.max_scalars, graph.max_utf16)
        _graph(data)  # Membership payload policy is owned by this container.
        result[FAMILIES[family]] = data
        previous, expected = family, start + size
    if expected != len(payload):
        raise DataError('unclaimed segmentation dictionary section bytes')
    return result
