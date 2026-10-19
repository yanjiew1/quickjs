"""Canonical raw digital-duration source rows in QJIN section 24."""
import struct

import bundle
from parents import locale_id
from resolve import BLOCK, INHERIT
from duration_patterns import SKELETONS

VERSION, HEADER, ROW_WIDTH, RECORD_WIDTH = 1, 3, 3, 4
STATES = ('missing', 'value', 'blocked')


def select(data, tags):
    selected = set(tags)
    return dict(data, rows=tuple(row for row in data['rows'] if row[0] in selected))


def strings(data):
    return {''} | {value for tag, name, state, text, origin, key in data['rows']
                   for value in (text, origin)}


def valid(state, text, origin, key):
    if state not in STATES or not all(isinstance(value, str) for value in (text, origin, key)):
        raise ValueError('invalid digital-duration source record')
    if state == 'missing':
        if text or origin or key:
            raise ValueError('noncanonical missing digital-duration record')
    elif (not origin or locale_id(origin) != origin or key not in SKELETONS or
          text in (BLOCK, INHERIT) or state == 'blocked' and text):
        raise ValueError('invalid digital-duration source state/provenance')


def encode(data, tags, ids):
    values = {(tag, name): (state, text, origin, key)
              for tag, name, state, text, origin, key in data['rows']}
    if (data['skeletons'] != SKELETONS or not tags or len(set(tags)) != len(tags) or
            ids.get('') != 0 or len(values) != len(data['rows']) or
            set(values) != {(tag, name) for tag in tags for name in SKELETONS}):
        raise ValueError('invalid digital-duration source coverage')
    compiled = {}
    for identity, record in values.items():
        state, text, origin, key = record
        valid(*record)
        compiled[identity] = (STATES.index(state), ids[text], ids[origin],
                              0 if state == 'missing' else SKELETONS.index(key))
    records = tuple(sorted(set(compiled.values())))
    record_ids = {record: index for index, record in enumerate(records)}
    locale_rows = tuple(tuple(record_ids[compiled[(tag, name)]] for name in SKELETONS) for tag in tags)
    rows = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(rows)}
    words = [VERSION, len(rows), len(records)]
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in rows for value in row)
    words.extend(value for record in records for value in record)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid digital-duration locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, row_count, record_count = words[:HEADER]
    expected = HEADER + count + row_count * ROW_WIDTH + record_count * RECORD_WIDTH
    if (version != VERSION or not 1 <= row_count <= count or
            not 1 <= record_count <= row_count * ROW_WIDTH or len(words) != expected or
            not pool or pool[0] != ''):
        raise ValueError('invalid digital-duration version/dimensions')
    locale_rows = words[HEADER:HEADER + count]
    if set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid/unused digital-duration row')
    at = HEADER + count
    rows = tuple(words[start:start + ROW_WIDTH]
                 for start in range(at, at + row_count * ROW_WIDTH, ROW_WIDTH))
    if (rows != tuple(sorted(set(rows))) or
            {index for row in rows for index in row} != set(range(record_count))):
        raise ValueError('invalid/noncanonical digital-duration rows')
    at += row_count * ROW_WIDTH
    records, previous = [], None
    for start in range(at, len(words), RECORD_WIDTH):
        record = words[start:start + RECORD_WIDTH]
        state, text, origin, key = record
        if (state >= len(STATES) or text >= len(pool) or origin >= len(pool) or
                key >= ROW_WIDTH or previous is not None and record <= previous):
            raise ValueError('invalid/noncanonical digital-duration record')
        if state == 0 and record != (0, 0, 0, 0):
            raise ValueError('noncanonical missing digital-duration record')
        decoded = STATES[state], pool[text], pool[origin], '' if state == 0 else SKELETONS[key]
        valid(*decoded)
        records.append(decoded)
        previous = record
    return {'skeletons': SKELETONS,
            'rows': tuple((tag, name) + records[rows[index][i]]
                          for tag, index in zip(tags, locale_rows)
                          for i, name in enumerate(SKELETONS))}
