"""Prepared literal suppression-list container, separate from dictionary mode.

Shared locale IDs bind complete lists; explicit empty policy is a present list
without a graph. The optional section does not establish runtime capability.
"""
from collections import namedtuple
import struct

from cldr_xml import DataError
from scalar_trie import Graph
import scalar_trie_tables as graphs

MAGIC = b'QJSSUPP\0'
VERSION, UNICODE_VERSION, SEMANTIC_PROFILE = 1, 18 << 16, 1
HEADER = struct.Struct('<8s8I32s')
DESCRIPTOR = struct.Struct('<12I')
BINDING = struct.Struct('<2I')
MAX_SIZE, MAX_LISTS, MAX_BINDINGS = 32 * 1024 * 1024, 256, 65536
MAX_STATES, MAX_EDGES, MAX_WORDS, MAX_SCALARS = 262144, 524288, 65536, 256
MAX_TOTAL_STATES, MAX_TOTAL_EDGES, MAX_TOTAL_WORDS = 1000000, 2000000, 1000000
Tables = namedtuple('Tables', 'fingerprint lists bindings')


def _integer(value, low, high):
    return type(value) is int and low <= value <= high


def _shape(graph):
    if graph is None:
        return (0,) * 8
    if not isinstance(graph, Graph) or any(type(array) is not tuple for array in graph[:6]):
        raise DataError('invalid suppression graph owner')
    metadata = graph.root, graph.words, graph.max_scalars, graph.max_utf16
    counts = len(graph.alphabet), len(graph.values), len(graph.terminals), len(graph.labels)
    fields = metadata + counts
    _limits(fields)
    if graph.values != (0,):
        raise DataError('nonmembership suppression payload')
    return fields


def _limits(fields):
    root, words, scalars, units, alphabet, values, states, edges = fields
    if (any(type(value) is not int for value in fields) or
            not 1 <= words <= MAX_WORDS or not 1 <= scalars <= MAX_SCALARS or
            not scalars <= units <= MAX_SCALARS * 2 or
            not 1 <= alphabet <= 65536 or values != 1 or
            not 2 <= states <= MAX_STATES or root != states - 1 or not 1 <= edges <= MAX_EDGES):
        raise DataError('invalid suppression graph metadata/budget')


def _routes(bindings, lists, locale_count):
    if (not _integer(locale_count, 1, 0xffffffff) or type(bindings) is not tuple or
            not 1 <= len(bindings) <= MAX_BINDINGS):
        raise DataError('invalid suppression route inventory')
    previous, used = -1, 0
    for row in bindings:
        if (type(row) is not tuple or len(row) != 2 or not _integer(row[0], 0, locale_count - 1) or
                row[0] <= previous or not _integer(row[1], 0, len(lists) - 1) or row[1] > used):
            raise DataError('invalid suppression locale/list route')
        previous = row[0]
        if row[1] == used:
            used += 1
    if used != len(lists):
        raise DataError('unbound suppression list')


def _totals(fields):
    states = sum(row[6] for row in fields)
    edges = sum(row[7] for row in fields)
    words = sum(row[1] for row in fields)
    if states > MAX_TOTAL_STATES or edges > MAX_TOTAL_EDGES or words > MAX_TOTAL_WORDS:
        raise DataError('suppression aggregate graph budget exceeded')


def encode(lists, bindings, fingerprint, locale_count):
    if (type(lists) is not tuple or not 1 <= len(lists) <= MAX_LISTS or
            type(fingerprint) is not bytes or len(fingerprint) != 32):
        raise DataError('invalid suppression list inventory/fingerprint')
    fields = tuple(_shape(graph) for graph in lists)
    _totals(fields)
    _routes(bindings, lists, locale_count)
    size = HEADER.size + len(lists) * DESCRIPTOR.size + len(bindings) * BINDING.size
    # Preflight the complete extent before materializing any graph block.
    for graph, row in zip(lists, fields):
        length = graphs.layout(*row[4:])[1] if graph is not None else 0
        if length > MAX_SIZE - size:
            raise DataError('suppression section size budget exceeded')
        size += length
    payload = bytearray(HEADER.size + len(lists) * DESCRIPTOR.size + len(bindings) * BINDING.size)
    for index, binding in enumerate(bindings):
        BINDING.pack_into(payload, HEADER.size + len(lists) * DESCRIPTOR.size + index * BINDING.size, *binding)
    for index, (graph, row) in enumerate(zip(lists, fields)):
        block = graphs.encode_block(graph) if graph is not None else b''
        DESCRIPTOR.pack_into(payload, HEADER.size + index * DESCRIPTOR.size,
                             *row, len(payload), len(block), 0, 0)
        payload.extend(block)
    HEADER.pack_into(payload, 0, MAGIC, VERSION, UNICODE_VERSION, SEMANTIC_PROFILE,
                     len(lists), len(bindings), len(payload), 0, 0, fingerprint)
    result = len(lists), bytes(payload)
    decode(result, locale_count)
    return result


def decode(section, locale_count):
    if section is None:
        return None
    if not isinstance(section, tuple) or len(section) != 2:
        raise DataError('invalid suppression section pair')
    count, payload = section
    if not _integer(count, 1, MAX_LISTS) or type(payload) is not bytes or not HEADER.size <= len(payload) <= MAX_SIZE:
        raise DataError('invalid suppression section')
    magic, version, unicode_version, profile, records, route_count, length, reserved1, reserved2, fingerprint = HEADER.unpack_from(payload)
    if ((magic, version, unicode_version, profile) != (MAGIC, VERSION, UNICODE_VERSION, SEMANTIC_PROFILE) or
            count != records or length != len(payload) or reserved1 or reserved2 or not 1 <= route_count <= MAX_BINDINGS):
        raise DataError('invalid suppression section header')
    route_start = HEADER.size + count * DESCRIPTOR.size
    expected = route_start + route_count * BINDING.size
    if expected > length:
        raise DataError('truncated suppression directory')
    descriptors = tuple(DESCRIPTOR.unpack_from(payload, HEADER.size + index * DESCRIPTOR.size) for index in range(count))
    for row in descriptors:
        if row[1]:
            _limits(row[:8])
        elif any(row[:8]):
            raise DataError('invalid empty suppression list')
    _totals(descriptors)
    lists = []
    for row in descriptors:
        start, size, reserved1, reserved2 = row[8:]
        if start != expected or start > length or size > length - start or reserved1 or reserved2:
            raise DataError('invalid suppression list extent/reserved field')
        graph = None
        if row[1]:
            graph = graphs.decode_block(payload, start, size, row[4:8], row[:4])
            if graph.values != (0,):
                raise DataError('nonmembership suppression payload')
        elif size:
            raise DataError('nonempty empty-policy suppression block')
        lists.append(graph)
        expected = start + size
    if expected != length:
        raise DataError('unclaimed suppression section bytes')
    bindings = tuple(BINDING.unpack_from(payload, route_start + index * BINDING.size) for index in range(route_count))
    _routes(bindings, lists, locale_count)
    return Tables(fingerprint, tuple(lists), bindings)
