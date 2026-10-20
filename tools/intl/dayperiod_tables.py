"""Canonical explicit day-period rule lists in optional QJIN section 34."""
import struct

import bundle
import dayperiod_rules as source
from dayperiod_codes import CODES
from parents import locale_id

VERSION, HEADER, ROW_WIDTH, MAP_WIDTH, RULE_WIDTH = 1, 3, 3, 2, 4
KINDS, FORMS = source.KINDS, ('at', 'range')


def records(data):
    if not isinstance(data, dict) or set(data) != {'rows'} or not isinstance(data['rows'], tuple):
        raise ValueError('invalid day-period source records')
    rows, seen = [], set()
    for row in data['rows']:
        if not isinstance(row, tuple) or len(row) != 3:
            raise ValueError('invalid day-period row shape')
        kind, tag, rules = row
        if (not isinstance(kind, str) or kind not in KINDS or not isinstance(tag, str) or
                locale_id(tag) != tag or not isinstance(rules, tuple) or len(rules) > len(CODES) or
                any(not isinstance(rule, tuple) or len(rule) != RULE_WIDTH or
                    not isinstance(rule[0], str) or not isinstance(rule[1], str) for rule in rules)):
            raise ValueError('invalid day-period row identity/rules')
        if (kind, tag) in seen:
            raise ValueError('repeated day-period kind/locale')
        seen.add((kind, tag))
        # A shared list must pass each referencing kind's semantics. Selection
        # admission does not waive the stricter format hour-bucket constraint.
        rows.append((kind, tag, source.validate_rules(kind, rules)))
    return tuple(sorted(rows))


def strings(data):
    return {''} | {tag for kind, tag, rules in data['rows']}


def encode(data, ids):
    rows = records(data)
    if ids.get('') != 0:
        raise ValueError('day-period string zero must be empty')
    lists = {tuple(sorted((CODES.index(code), FORMS.index(form), start, end)
                          for code, form, start, end in rules)) for kind, tag, rules in rows}
    maps = tuple(sorted(lists))
    map_ids = {rules: index for index, rules in enumerate(maps)}
    row_records = tuple(sorted((KINDS.index(kind), ids[tag],
                               map_ids[tuple(sorted((CODES.index(code), FORMS.index(form), start, end)
                                                    for code, form, start, end in rules))])
                              for kind, tag, rules in rows))
    descriptors, rule_records = [], []
    for rules in maps:
        descriptors.append((len(rule_records), len(rules)))
        rule_records.extend(rules)
    words = [VERSION, len(maps), len(rule_records)]
    for values in (row_records, descriptors, rule_records):
        words.extend(value for row in values for value in row)
    if len(words) > bundle.MAX_SIZE // 4:
        raise ValueError('day-period section exceeds size limit')
    return len(rows), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool):
    if section is None:
        return None
    count, raw = section
    if (not isinstance(raw, bytes) or not HEADER * 4 <= len(raw) <= bundle.MAX_SIZE or len(raw) % 4 or
            not pool or pool[0] != '' or type(count) is not int or not 0 <= count <= 0xffffffff):
        raise ValueError('invalid day-period extent/count')
    version, map_count, rule_count = struct.unpack_from('<3I', raw)
    if (version != VERSION or map_count > count or rule_count > map_count * len(CODES) or
            len(raw) // 4 != HEADER + count * ROW_WIDTH + map_count * MAP_WIDTH + rule_count * RULE_WIDTH):
        raise ValueError('invalid day-period version/dimensions')
    at = HEADER * 4
    rows = tuple(struct.iter_unpack('<3I', raw[at:at + count * ROW_WIDTH * 4]))
    at += count * ROW_WIDTH * 4
    maps = tuple(struct.iter_unpack('<2I', raw[at:at + map_count * MAP_WIDTH * 4]))
    at += map_count * MAP_WIDTH * 4
    rules = tuple(struct.iter_unpack('<4I', raw[at:]))
    lists, offset = [], 0
    for start, length in maps:
        if start != offset or length > len(CODES) or length > rule_count - offset:
            raise ValueError('invalid day-period rule span')
        values = []
        for code, form, begin, end in rules[start:start + length]:
            if code >= len(CODES) or form >= len(FORMS):
                raise ValueError('invalid day-period rule enum')
            values.append((CODES[code], FORMS[form], begin, end))
        lists.append(tuple(sorted(values)))
        offset += length
    if offset != rule_count:
        raise ValueError('unused day-period rule')
    values = []
    for kind, ref, index in rows:
        if kind >= len(KINDS) or ref >= len(pool) or index >= len(lists):
            raise ValueError('invalid day-period row reference')
        values.append((KINDS[kind], pool[ref], lists[index]))
    result = {'rows': tuple(sorted(values))}
    # Each list has at most twelve rules. Canonical reconstruction checks map
    # interning, ordering, reachability and all per-kind semantic constraints.
    if encode(result, {text: index for index, text in enumerate(pool)}) != section:
        raise ValueError('noncanonical day-period tables')
    return result
