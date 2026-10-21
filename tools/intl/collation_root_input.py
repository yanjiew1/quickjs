#!/usr/bin/env python3
"""QJCR logical root input for the host compiler, not a frozen runtime pack."""
import argparse
from collections import namedtuple
import os
from pathlib import Path
import re
import struct
import tempfile

import acquire
from cldr_xml import DataError
import collation_fractional as fractional
import collation_groups
import collation_ordinals as ordinals

MAGIC, VERSION = b'QJCR', 1
UNICODE_VERSION = UCA_VERSION = 18 << 16
MAX_SIZE = 64 * 1024 * 1024
HEADER = struct.Struct('<4s7I32s')
DIRECTORY = struct.Struct('<4I')
U32 = struct.Struct('<I')
ROW = struct.Struct('<4I')
CONTEXT = struct.Struct('<3I')
CE = struct.Struct('<6I')
RESET = struct.Struct('<8I')
TOKEN = struct.Struct('<2I')
META, MAPPINGS, KEYS, CONTEXTS, CES, HAN_RANGES, HAN_RANKS = range(1, 8)
RESETS, WEAK_LIMITS, GROUPS, TOKENS, TOKEN_BYTES = range(8, 13)
SECTION_COUNT = 12
META_FIELDS = ('primary_max', 'secondary_max', 'tertiary_max',
               'secondary_common', 'tertiary_common', 'secondary_boundary', 'tertiary_boundary',
               'han_base', 'han_count', 'generic_base', 'generic_count',
               'generic_limit', 'implicit_first_marker', 'implicit_last_marker',
               'variable_top', 'numeric',
               'field_separator', 'last_regular')
CLASSES = ('tertiary ignorable', 'secondary ignorable', 'primary ignorable',
           'variable', 'regular', 'implicit', 'trailing')
RESET_NAMES = tuple(direction + ' ' + kind for kind in CLASSES for direction in ('first', 'last'))
WEAK_NAMES = ('first secondary in primary non-ignorable',
              'last secondary in primary non-ignorable',
              'first tertiary in secondary non-ignorable',
              'last tertiary in secondary non-ignorable')
SOURCE_CE, ARTIFICIAL_BOUNDARY, DERIVED_INITIAL = range(1, 4)
REORDERABLE, UNASSIGNED = 1, 2
Mapping = namedtuple('Mapping', 'prefix text ce_start ce_count')
TOKEN_NAME = re.compile(rb'(?:[A-Z][a-z]{3}|space|punct|symbol|currency|digit)\Z')


def _integer(value):
    if type(value) is not int or not 0 <= value <= 0xffffffff:
        raise DataError('QJCR integer outside uint32')
    return value


def _words(values):
    return b''.join(U32.pack(_integer(value)) for value in values)


def _ce(value):
    flags = value.case | int(value.variable) << 2 | int(value.virtual) << 3
    return (value.primary, value.secondary, value.tertiary, 0, value.primary, flags)


def _facts(catalog):
    result = []
    for index, name in enumerate(RESET_NAMES, 1):
        kind = name.split(' ', 1)[1]
        if name == 'last regular':
            ce = fractional.Element(fractional.Primary('weight', catalog.han_weight), (5,), (5,))
            role = DERIVED_INITIAL
        else:
            values = catalog.data['limits'].get(name)
            if values is None or any(value is None for value in values):
                raise DataError('missing complete reset source fact: ' + name)
            ce = fractional.Element(fractional.Primary('weight', values[0]), values[1], values[2])
            role = ARTIFICIAL_BOUNDARY if kind in (
                'tertiary ignorable', 'secondary ignorable', 'implicit') else SOURCE_CE
        result.append((index, role) + _ce(catalog.coordinate(ce)))
    return tuple(result)


def _weak(catalog):
    result = []
    for index, name in enumerate(WEAK_NAMES, 1):
        values = catalog.data['limits'].get(name)
        axis = 1 if index <= 2 else 2
        if values is None or any(value is not None for i, value in enumerate(values) if i != axis):
            raise DataError('invalid partial weak-limit source fact: ' + name)
        value = values[axis]
        if not value:
            raise DataError('missing partial weak-limit source fact: ' + name)
        ordinal = catalog.secondaries[value] if axis == 1 else catalog.tertiaries[
            ordinals.ordinary_tertiary(value)]
        result.append((index, axis, ordinal, 0))
    return tuple(result)


def encode(catalog, groups, fingerprint):
    """Source identities survive sorting; only identical CE vectors share storage.

    Keys partition their pool rather than sharing long spans, which bounds a C
    reader's adjacent-key validation work. Prefix payloads contain suffix CEs
    only; the diagnostic allkeys prefix-CE skip does not apply.
    """
    if not isinstance(fingerprint, bytes) or len(fingerprint) != 32:
        raise DataError('QJCR needs a 32-byte source manifest fingerprint')
    metadata = catalog.summary()
    metadata.update(han_count=len(catalog.han), generic_count=ordinals.GENERIC_COUNT)
    keys, rows, contexts, pool, sequences = [], [], [], [], {}
    ordered = sorted(catalog.data['rows'], key=lambda row: (row.prefix + row.text, len(row.prefix)))
    for index, row in enumerate(ordered):
        values = tuple(_ce(catalog.coordinate(ce)) for ce in row.elements)
        if values not in sequences:
            sequences[values] = len(pool)
            pool.extend(values)
        key = row.prefix + row.text
        rows.append((len(keys), len(key), sequences[values], len(values)))
        keys.extend(key)
        if row.prefix:
            contexts.append((index, len(row.prefix), 0))
    ranges, ranks = [], []
    for first, last in sorted(catalog.data['ideographs']):
        ranges.append((first, last, len(ranks)))
        ranks.extend(catalog.han[code] - catalog.han_base for code in range(first, last + 1))
    group_rows, tokens, strings = [], [], bytearray()
    for group in groups:
        if group.kind not in ('reorderable', 'unassigned'):
            raise DataError('unknown group kind')
        group_rows.append((catalog.primaries[group.fractional],
                           REORDERABLE if group.kind == 'reorderable' else UNASSIGNED,
                           len(tokens), len(group.tokens)))
        for token in group.tokens:
            raw = token.encode('ascii')
            tokens.append((len(strings), len(raw)))
            strings.extend(raw)
    byte_count = len(strings)
    strings.extend(b'\0' * (-len(strings) % 4))
    sections = {
        META: (1, _words(metadata[name] for name in META_FIELDS)),
        MAPPINGS: (len(rows), b''.join(ROW.pack(*row) for row in rows)),
        KEYS: (len(keys), _words(keys)),
        CONTEXTS: (len(contexts), b''.join(CONTEXT.pack(*row) for row in contexts)),
        CES: (len(pool), b''.join(CE.pack(*value) for value in pool)),
        HAN_RANGES: (len(ranges), b''.join(CONTEXT.pack(*row) for row in ranges)),
        HAN_RANKS: (len(ranks), _words(ranks)),
        RESETS: (len(RESET_NAMES), b''.join(RESET.pack(*row) for row in _facts(catalog))),
        WEAK_LIMITS: (len(WEAK_NAMES), b''.join(ROW.pack(*row) for row in _weak(catalog))),
        GROUPS: (len(group_rows), b''.join(ROW.pack(*row) for row in group_rows)),
        TOKENS: (len(tokens), b''.join(TOKEN.pack(*row) for row in tokens)),
        TOKEN_BYTES: (byte_count, bytes(strings)),
    }
    output = bytearray(HEADER.size + SECTION_COUNT * DIRECTORY.size)
    for kind in range(1, SECTION_COUNT + 1):
        count, raw = sections[kind]
        if len(output) + len(raw) > MAX_SIZE:
            raise DataError('QJCR exceeds size budget')
        DIRECTORY.pack_into(output, HEADER.size + (kind - 1) * DIRECTORY.size,
                            kind, len(output), len(raw), count)
        output.extend(raw)
    HEADER.pack_into(output, 0, MAGIC, VERSION, UNICODE_VERSION, UCA_VERSION,
                     len(output), SECTION_COUNT, 0, 0, fingerprint)
    result = bytes(output)
    decode(result)
    return result


def _metadata(values):
    result = dict(zip(META_FIELDS, values))
    m = result
    if (any(m[name] >= 0x80000000 for name in META_FIELDS) or
            not 0 < m['secondary_common'] < m['secondary_boundary'] <= m['secondary_max'] or
            not 0 < m['tertiary_common'] < m['tertiary_boundary'] <= m['tertiary_max'] or
            not m['han_count'] or m['han_count'] > 0x110000 or
            m['generic_count'] != ordinals.GENERIC_COUNT or
            not m['field_separator'] < m['variable_top'] < m['numeric'] < m['han_base'] or
            m['field_separator'] == 0 or
            m['han_base'] + m['han_count'] >= m['implicit_first_marker'] or
            not m['implicit_first_marker'] < m['implicit_last_marker'] == m['generic_base'] or
            m['generic_base'] + m['generic_count'] + 1 != m['generic_limit'] or
            m['generic_limit'] > m['primary_max'] or m['last_regular'] != m['han_base']):
        raise DataError('invalid QJCR coordinate metadata')
    return result


def _check_ce(value, m):
    p, s, t, q, origin, flags = value
    if (p > m['primary_max'] or s > m['secondary_max'] or t > m['tertiary_max'] or
            q or origin != p or flags & ~15 or flags & 3 == 3 or p == m['numeric']):
        raise DataError('invalid QJCR root CE')
    case = flags & 3
    virtual = (m['han_base'] < p <= m['han_base'] + m['han_count'] or
               m['generic_base'] < p < m['generic_limit'])
    if bool(flags & 8) != virtual or bool(flags & 4) != (m['field_separator'] < p <= m['variable_top']):
        raise DataError('inconsistent QJCR CE provenance')
    if p:
        valid = 0 < s < m['secondary_boundary'] and 0 < t < m['tertiary_boundary']
    elif s:
        valid = s > m['secondary_boundary'] and 0 < t < m['tertiary_boundary']
    elif t:
        valid = t > m['tertiary_boundary'] and case == ordinals.UPPER
    else:
        valid = case == ordinals.LOWER
    if not valid:
        raise DataError('QJCR CE violates WF1/WF2')


def decode(raw):
    """Strict structural/root-domain audit; C performs pinned prefix checks too."""
    if not isinstance(raw, bytes) or not HEADER.size <= len(raw) <= MAX_SIZE:
        raise DataError('invalid QJCR size')
    magic, version, unicode, uca, size, count, r1, r2, fingerprint = HEADER.unpack_from(raw)
    if ((magic, version, unicode, uca, size, count, r1, r2) !=
            (MAGIC, VERSION, UNICODE_VERSION, UCA_VERSION, len(raw), SECTION_COUNT, 0, 0)):
        raise DataError('invalid QJCR header')
    at = HEADER.size + SECTION_COUNT * DIRECTORY.size
    if at > len(raw):
        raise DataError('truncated QJCR directory')
    sections = {}
    for index in range(SECTION_COUNT):
        kind, offset, length, records = DIRECTORY.unpack_from(raw, HEADER.size + index * DIRECTORY.size)
        if kind != index + 1 or offset != at or length % 4 or length > len(raw) - at:
            raise DataError('invalid QJCR section extent')
        sections[kind] = (records, raw[at:at + length])
        at += length
    if at != len(raw):
        raise DataError('QJCR trailing bytes')

    def rows(kind, codec):
        count, data = sections[kind]
        if count != len(data) // codec.size or len(data) % codec.size:
            raise DataError('QJCR record count/stride mismatch')
        return tuple(codec.unpack_from(data, i * codec.size) for i in range(count))

    if sections[META][0] != 1 or len(sections[META][1]) != len(META_FIELDS) * 4:
        raise DataError('invalid QJCR metadata extent')
    metadata = _metadata(tuple(x[0] for x in struct.iter_unpack('<I', sections[META][1])))
    keys = tuple(x[0] for x in rows(KEYS, U32))
    if any(code > 0x10ffff or 0xd800 <= code <= 0xdfff for code in keys):
        raise DataError('non-scalar QJCR source key')
    ces = rows(CES, CE)
    for ce in ces:
        _check_ce(ce, metadata)
    mapping_rows = rows(MAPPINGS, ROW)
    contexts, previous = {}, -1
    for index, prefix, reserved in rows(CONTEXTS, CONTEXT):
        if reserved or index <= previous or index >= len(mapping_rows) or not 0 < prefix < mapping_rows[index][1]:
            raise DataError('invalid QJCR context')
        contexts[index] = prefix
        previous = index
    mappings, previous, end = [], None, 0
    for index, (start, length, ce_start, ce_count) in enumerate(mapping_rows):
        if (start != end or not length or length > len(keys) - end or not ce_count or
                ce_start > len(ces) or ce_count > len(ces) - ce_start):
            raise DataError('invalid QJCR mapping')
        end += length
        key, prefix = keys[start:end], contexts.get(index, 0)
        identity = key, prefix
        if previous is not None and previous >= identity or key[prefix] == 0xfdd0:
            raise DataError('unordered, duplicate or synthetic QJCR mapping')
        # Keep borrowed pool coordinates: expanding shared vectors per row could
        # make a small valid artifact consume quadratic diagnostic memory.
        mappings.append(Mapping(key[:prefix], key[prefix:], ce_start, ce_count))
        previous = identity
    if not mappings or end != len(keys):
        raise DataError('incomplete QJCR keys')

    ranges, ranks, end, previous = rows(HAN_RANGES, CONTEXT), rows(HAN_RANKS, U32), 0, -1
    for first, last, start in ranges:
        if (first <= previous or first > last or last > 0x10ffff or
                first <= 0xdfff and last >= 0xd800 or start != end):
            raise DataError('invalid QJCR Han range')
        end += last - first + 1
        previous = last
    ranks = tuple(value[0] for value in ranks)
    if end != len(ranks) or end != metadata['han_count'] or sorted(ranks) != list(range(1, end + 1)):
        raise DataError('QJCR Han ranks are not an exact permutation')

    resets = rows(RESETS, RESET)
    if len(resets) != len(RESET_NAMES):
        raise DataError('incomplete QJCR reset facts')
    for index, fact in enumerate(resets, 1):
        name = RESET_NAMES[index - 1]
        kind = name.split(' ', 1)[1]
        role = (DERIVED_INITIAL if name == 'last regular' else ARTIFICIAL_BOUNDARY
                if kind in ('tertiary ignorable', 'secondary ignorable', 'implicit') else SOURCE_CE)
        if fact[:2] != (index, role):
            raise DataError('invalid QJCR reset fact identity')
        _check_ce(fact[2:], metadata)
        p, s, t, q, origin, flags = fact[2:]
        if kind == 'tertiary ignorable':
            valid = (p, s, t) == (0, 0, 0)
        elif kind == 'secondary ignorable':
            valid = p == s == 0 and t > metadata['tertiary_boundary']
        elif kind == 'primary ignorable':
            valid = p == 0 and s > metadata['secondary_boundary']
        elif kind == 'variable':
            valid = metadata['field_separator'] < p <= metadata['variable_top']
        elif kind == 'regular':
            valid = metadata['variable_top'] < p <= metadata['han_base']
        elif kind == 'implicit':
            marker = 'implicit_first_marker' if name.startswith('first ') else 'implicit_last_marker'
            valid = p == metadata[marker]
        else:
            valid = metadata['generic_limit'] <= p <= metadata['primary_max']
        if name == 'last regular':
            valid = valid and p == metadata['last_regular']
        if kind in ('implicit', 'trailing') or name == 'last regular':
            valid = valid and (s, t, flags) == (metadata['secondary_common'],
                                              metadata['tertiary_common'], 0)
        if not valid:
            raise DataError('QJCR reset fact contradicts its class or metadata')
    for first, last in zip(resets[::2], resets[1::2]):
        if first[2:5] > last[2:5]:
            raise DataError('unordered QJCR reset source facts')
    weak = rows(WEAK_LIMITS, ROW)
    if len(weak) != 4:
        raise DataError('incomplete QJCR weak-limit facts')
    for index, (kind, axis, value, reserved) in enumerate(weak, 1):
        expected_axis = 1 if index <= 2 else 2
        boundary = metadata['secondary_boundary' if axis == 1 else 'tertiary_boundary']
        if kind != index or axis != expected_axis or reserved or not 0 < value < boundary:
            raise DataError('invalid QJCR weak-limit fact')

    if weak[0][2] > weak[1][2] or weak[2][2] > weak[3][2]:
        raise DataError('unordered QJCR weak-limit facts')

    token_count, strings = sections[TOKEN_BYTES]
    if (len(strings) != ((token_count + 3) & ~3) or token_count > len(strings) or any(strings[token_count:])):
        raise DataError('invalid QJCR token pool padding')
    tokens, end = [], 0
    for start, length in rows(TOKENS, TOKEN):
        if start != end or not length or length > token_count - end:
            raise DataError('invalid QJCR token extent')
        end += length
        name = strings[start:end]
        if not TOKEN_NAME.fullmatch(name):
            raise DataError('invalid QJCR token spelling')
        tokens.append(name.decode('ascii'))
    if end != token_count or len(tokens) != len(set(tokens)):
        raise DataError('incomplete or duplicate QJCR tokens')
    groups, end, previous, unassigned = [], 0, 0, 0
    for primary, kind, start, count in rows(GROUPS, ROW):
        if (not previous < primary <= metadata['generic_base'] or kind not in (REORDERABLE, UNASSIGNED) or
                start != end or count > len(tokens) - end or (kind == REORDERABLE) != bool(count)):
            raise DataError('invalid QJCR group')
        if kind == UNASSIGNED:
            if primary != metadata['generic_base']:
                raise DataError('wrong QJCR unassigned boundary')
            unassigned += 1
        groups.append((primary, kind, tuple(tokens[start:start + count])))
        end += count
        previous = primary
    if (end != len(tokens) or unassigned != 1 or
            not {'space', 'punct', 'symbol', 'currency', 'digit'}.issubset(tokens)):
        raise DataError('incomplete QJCR group inventory')
    by_token = {token: index for index, group in enumerate(groups) for token in group[2]}
    core = [by_token[token] for token in ('space', 'punct', 'symbol', 'currency', 'digit')]
    if (any(a >= b for a, b in zip(core, core[1:])) or
            'Hani' not in by_token or groups[by_token['Hani']][0] != metadata['han_base']):
        raise DataError('QJCR groups contradict root order or Han boundary')
    digit = by_token['digit']
    if not groups[digit][0] <= metadata['numeric'] < groups[digit + 1][0]:
        raise DataError('QJCR numeric reservation outside digit group')
    return {'fingerprint': fingerprint, 'metadata': metadata, 'mappings': tuple(mappings), 'ces': ces,
            'han_ranges': ranges, 'han_ranks': ranks, 'resets': resets, 'weak': weak,
            'groups': tuple(groups)}


def write(path, raw):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    name = None
    try:
        with tempfile.NamedTemporaryFile(dir=str(path.parent), prefix=path.name + '.', delete=False) as output:
            name = output.name
            output.write(raw)
        os.replace(name, str(path))
    finally:
        if name and os.path.exists(name):
            os.unlink(name)


def prepare(sources):
    catalog = ordinals.Catalog(fractional.collect(sources))
    groups = collation_groups.collect(sources)
    return encode(catalog, groups, bytes.fromhex(sources.pin['manifest_sha256']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    raw = prepare(sources)
    if args.output:
        write(args.output, raw)
    counts = [DIRECTORY.unpack_from(raw, HEADER.size + (kind - 1) * DIRECTORY.size)[3]
              for kind in (MAPPINGS, HAN_RANKS)]
    print('QJCR: {} bytes, {} mappings, {} Han ranks'.format(
        len(raw), counts[0], counts[1]))


if __name__ == '__main__':
    main()
