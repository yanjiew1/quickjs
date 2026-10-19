"""Sparse unit forms and shared locale/width maps in QJIN section 21."""
import struct

import bundle
import unit_patterns as source
from resolve import BLOCK, INHERIT

VERSION, HEADER = 1, 6
BLOCKED = 0xfffffffe
KINDS = ('compound', 'unit')


def select(data, tags):
    selected = set(tags)
    rows = tuple(row for row in data['rows'] if row[0] in selected)
    used = sorted({row[-1] for row in rows})
    if any(type(index) is not int or not 0 <= index < len(data['maps']) for index in used):
        raise ValueError('invalid unit source map reference')
    indices = {old: new for new, old in enumerate(used)}
    return dict(data, maps=tuple(data['maps'][index] for index in used),
                rows=tuple(row[:-1] + (indices[row[-1]],) for row in rows))


def strings(data):
    return {name for kind, name in data['groups']} | {''} | {
        value for entries in data['maps'] for field, count, case, text in entries
        for value in (case, text) if value != BLOCK}


def field_family(field, count, case, text):
    if (field not in source.FIELDS or not isinstance(text, str) or not isinstance(count, str) or
            not isinstance(case, str) or text == INHERIT or
            case and source.CASE.fullmatch(case) is None or
            (field == 'unitPattern' and count not in source.COUNTS) or
            (field != 'unitPattern' and count) or
            (field not in ('unitPattern', 'compoundUnitPattern') and case) or
            (field == 'gender' and text not in ('', BLOCK) and source.IDENTIFIER.fullmatch(text) is None)):
        raise ValueError('invalid unit form field/selector/text')
    return 2 if field == 'compoundUnitPattern' else 1


def group_valid(kind, name):
    return (kind == 'compound' and name == 'per' or
            kind == 'unit' and isinstance(name, str) and source.IDENTIFIER.fullmatch(name) is not None)


def encode(data, tags, ids):
    groups = data['groups']
    rows = {row[:-1]: row[-1] for row in data['rows']}
    if (data['fields'] != source.FIELDS or data['counts'] != source.COUNTS or
            data['widths'] != source.WIDTHS or not tags or len(set(tags)) != len(tags) or
            groups != tuple(sorted(set(groups))) or ('compound', 'per') not in groups or
            not any(kind == 'unit' for kind, name in groups) or
            any(not group_valid(kind, name) for kind, name in groups) or
            len(rows) != len(data['rows']) or
            set(rows) != {(tag, width, kind, name) for tag in tags for width in source.WIDTHS
                         for kind, name in groups} or
            any(type(index) is not int or not 0 <= index < len(data['maps']) for index in rows.values()) or
            set(rows.values()) != set(range(len(data['maps'])))):
        raise ValueError('invalid unit source inventory/coverage')
    case_names, families = {''}, []
    for entries in data['maps']:
        family = 0
        for field, count, case, text in entries:
            family |= field_family(field, count, case, text)
            case_names.add(case)
        if family == 3:
            raise ValueError('mixed unit/compound form map')
        families.append(family)
    cases = tuple(sorted(case_names))
    if len(cases) > 65536:
        raise ValueError('unit case inventory exceeds selector width')
    case_ids = {name: index for index, name in enumerate(cases)}
    compiled = []
    for entries in data['maps']:
        values, seen = [], set()
        for field, count, case, text in entries:
            selector = (source.FIELDS.index(field) << 24 | case_ids[case] |
                        ((source.COUNTS.index(count) + 1) << 16 if count else 0))
            if selector in seen:
                raise ValueError('repeated unit form selector')
            seen.add(selector)
            values.append((selector, BLOCKED if text == BLOCK else ids[text]))
        compiled.append(tuple(sorted(values)))
    for (tag, width, kind, name), index in rows.items():
        if families[index] not in (0, 2 if kind == 'compound' else 1):
            raise ValueError('unit map assigned to wrong group kind')
    maps = tuple(sorted(set(compiled)))
    map_ids = {value: index for index, value in enumerate(maps)}
    locale_rows = tuple(tuple(map_ids[compiled[rows[(tag, width, kind, name)]]]
                              for width in source.WIDTHS for kind, name in groups) for tag in tags)
    shared_rows = tuple(sorted(set(locale_rows)))
    row_ids = {value: index for index, value in enumerate(shared_rows)}
    entry_count = sum(len(value) for value in maps)
    words = [VERSION, len(groups), len(cases), len(shared_rows), len(maps), entry_count]
    words.extend(value for kind, name in groups for value in (KINDS.index(kind), ids[name]))
    words.extend(ids[case] for case in cases)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in shared_rows for value in row)
    at = 0
    for entries in maps:
        words.extend((at, len(entries)))
        at += len(entries)
    words.extend(value for entries in maps for entry in entries for value in entry)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid unit locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, group_count, case_count, row_count, map_count, entry_count = words[:HEADER]
    width = len(source.WIDTHS) * group_count
    expected = HEADER + group_count * 2 + case_count + count + row_count * width + map_count * 2 + entry_count * 2
    if (version != VERSION or group_count < 2 or not 1 <= case_count <= 65536 or
            not 1 <= row_count <= count or not 1 <= map_count <= row_count * width or
            len(words) != expected or not pool or pool[0] != ''):
        raise ValueError('invalid unit dimensions/version')
    at = HEADER
    groups = []
    for i in range(group_count):
        kind, name = words[at:at + 2]
        at += 2
        if kind >= len(KINDS) or not 0 < name < len(pool) or not group_valid(KINDS[kind], pool[name]):
            raise ValueError('invalid unit group reference')
        groups.append((KINDS[kind], pool[name]))
    if groups != sorted(set(groups)) or groups[0] != ('compound', 'per') or groups[1][0] != 'unit':
        raise ValueError('noncanonical unit group inventory')
    cases = []
    for reference in words[at:at + case_count]:
        if reference >= len(pool) or pool[reference] and source.CASE.fullmatch(pool[reference]) is None:
            raise ValueError('invalid unit case reference')
        cases.append(pool[reference])
    at += case_count
    if cases[0] != '' or cases != sorted(set(cases)):
        raise ValueError('noncanonical unit case inventory')
    indices = words[at:at + count]
    if set(indices) != set(range(row_count)):
        raise ValueError('invalid/unused unit locale row')
    at += count
    end = at + row_count * width
    rows = tuple(words[i:i + width] for i in range(at, end, width))
    at = end
    if (any(a >= b for a, b in zip(rows, rows[1:])) or
            {value for row in rows for value in row} != set(range(map_count))):
        raise ValueError('invalid/unused unit map or row order')
    descriptors = tuple(words[i:i + 2] for i in range(at, at + map_count * 2, 2))
    at += map_count * 2
    entries = tuple(words[i:i + 2] for i in range(at, len(words), 2))
    maps, decoded, families, used_cases, expected = [], [], [], {0}, 0
    for offset, length in descriptors:
        if offset != expected or length > entry_count - offset:
            raise ValueError('invalid unit map span')
        expected += length
        values = entries[offset:offset + length]
        if any(a[0] >= b[0] for a, b in zip(values, values[1:])) or maps and values <= maps[-1]:
            raise ValueError('noncanonical unit map/selector order')
        maps.append(values)
        result, family = [], 0
        for selector, reference in values:
            field, plural, case = selector >> 24, (selector >> 16) & 255, selector & 65535
            if field >= len(source.FIELDS) or plural > len(source.COUNTS) or case >= case_count:
                raise ValueError('invalid unit form selector')
            if reference == BLOCKED:
                text = BLOCK
            elif reference >= len(pool) or pool[reference] in (BLOCK, INHERIT):
                raise ValueError('invalid unit text reference')
            else:
                text = pool[reference]
            count_value = source.COUNTS[plural - 1] if plural else ''
            family |= field_family(source.FIELDS[field], count_value, cases[case], text)
            used_cases.add(case)
            result.append((source.FIELDS[field], count_value, cases[case], text))
        if family == 3:
            raise ValueError('mixed unit/compound form map')
        families.append(family)
        decoded.append(tuple(sorted(result)))
    if expected != entry_count or used_cases != set(range(case_count)):
        raise ValueError('unowned unit entries or unused case')
    for row in rows:
        for column, index in enumerate(row):
            kind = groups[column % group_count][0]
            if families[index] not in (0, 2 if kind == 'compound' else 1):
                raise ValueError('unit map assigned to wrong group kind')
    semantic_maps = tuple(sorted(decoded))
    remap = {value: index for index, value in enumerate(semantic_maps)}
    return {'widths': source.WIDTHS, 'groups': tuple(groups), 'fields': source.FIELDS, 'counts': source.COUNTS,
            'maps': semantic_maps,
            'rows': tuple((tag, width_name, kind, name,
                           remap[decoded[rows[indices[i]][w * group_count + g]]])
                          for i, tag in enumerate(tags) for w, width_name in enumerate(source.WIDTHS)
                          for g, (kind, name) in enumerate(groups))}
