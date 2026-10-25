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

"""Sparse unit forms and shared locale/width maps in QJIN section 21."""
import struct

import bundle
from parents import locale_id
import unit_patterns as source
from resolve import BLOCK, INHERIT

VERSION, HEADER = 2, 8
BLOCKED = 0xfffffffe
KINDS = ('compound', 'unit')


def overlay_sources(data):
    groups, seen = set(data['groups']), set()
    for i, node in enumerate(data['overlay_nodes']):
        locale, width, kind, name, index, tail = node
        if (locale_id(locale) != locale or width not in source.WIDTHS or
                (kind, name) not in groups or type(index) is not int or
                not 0 <= index < len(data['overlay_maps']) or
                not data['overlay_maps'][index] or type(tail) is not int or
                not 0 <= tail <= i or node in seen):
            raise ValueError('invalid unit source overlay node')
        seen.add(node)
        if tail and data['overlay_nodes'][tail - 1][2] != kind:
            raise ValueError('unit overlay tail has wrong group kind')


def select(data, tags):
    selected = set(tags)
    rows = tuple(row for row in data['rows'] if row[0] in selected)
    used = sorted({row[-1] for row in rows})
    if any(type(index) is not int or not 0 <= index < len(data['maps']) for index in used):
        raise ValueError('invalid unit source map reference')
    indices = {old: new for new, old in enumerate(used)}
    overlay_sources(data)
    overlay_rows = tuple(row for row in data['overlay_rows'] if row[0] in selected)
    reachable = set()
    for row in overlay_rows:
        head = row[-1]
        if type(head) is not int or not 0 <= head <= len(data['overlay_nodes']):
            raise ValueError('invalid unit source overlay head')
        while head and head not in reachable:
            reachable.add(head)
            head = data['overlay_nodes'][head - 1][-1]
    node_ids = {old: new for new, old in enumerate(sorted(reachable), 1)}
    node_ids[0] = 0
    used_overlays = sorted({data['overlay_nodes'][head - 1][-2] for head in reachable})
    overlay_ids = {old: new for new, old in enumerate(used_overlays)}
    return dict(data, maps=tuple(data['maps'][index] for index in used),
                rows=tuple(row[:-1] + (indices[row[-1]],) for row in rows),
                overlay_maps=tuple(data['overlay_maps'][index] for index in used_overlays),
                overlay_nodes=tuple(data['overlay_nodes'][head - 1][:-2] +
                                    (overlay_ids[data['overlay_nodes'][head - 1][-2]],
                                     node_ids[data['overlay_nodes'][head - 1][-1]])
                                    for head in sorted(reachable)),
                overlay_rows=tuple(row[:-1] + (node_ids[row[-1]],) for row in overlay_rows))


def strings(data):
    return {name for kind, name in data['groups']} | {''} | {
        node[0] for node in data['overlay_nodes']} | {
        value for entries in data['maps'] + data['overlay_maps'] for field, count, case, text in entries
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
    overlay_sources(data)
    heads = {row[:-1]: row[-1] for row in data['overlay_rows']}
    if (len(heads) != len(data['overlay_rows']) or set(heads) != set(rows) or
            any(type(head) is not int or not 0 <= head <= len(data['overlay_nodes'])
                for head in heads.values()) or
            {node[-2] for node in data['overlay_nodes']} != set(range(len(data['overlay_maps'])))):
        raise ValueError('invalid unit overlay inventory/coverage')
    reachable = set(head for head in heads.values() if head)
    for head in range(len(data['overlay_nodes']), 0, -1):
        if head in reachable and data['overlay_nodes'][head - 1][-1]:
            reachable.add(data['overlay_nodes'][head - 1][-1])
    if reachable != set(range(1, len(data['overlay_nodes']) + 1)):
        raise ValueError('unreachable unit overlay node')
    all_maps = data['maps'] + data['overlay_maps']
    case_names, families = {''}, []
    for entries in all_maps:
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
    for entries in all_maps:
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
    # Breadth-first numbering by the final parent ID makes duplicate detection
    # a linear adjacent-key check, without storing chain depths in the wire.
    children = {}
    group_ids = {group: i for i, group in enumerate(groups)}
    raw_map_count = len(data['maps'])
    for head, (locale, width, kind, name, index, tail) in enumerate(data['overlay_nodes'], 1):
        if families[raw_map_count + index] != (2 if kind == 'compound' else 1):
            raise ValueError('unit overlay map has wrong group kind')
        column = source.WIDTHS.index(width) * len(groups) + group_ids[(kind, name)]
        children.setdefault(tail, []).append((ids[locale], column,
                                              map_ids[compiled[raw_map_count + index]], head))
    node_ids, nodes, old_order = {0: 0}, [], [0]
    for tail in old_order:
        for locale, column, index, head in sorted(children.get(tail, ())):
            if nodes and nodes[-1] == (locale, column, index, node_ids[tail]):
                raise ValueError('duplicate compiled unit overlay node')
            node_ids[head] = len(nodes) + 1
            nodes.append((locale, column, index, node_ids[tail]))
            old_order.append(head)
    if len(nodes) != len(data['overlay_nodes']):
        raise ValueError('incomplete unit overlay topology')
    overlay_locale_rows = tuple(tuple(node_ids[heads[(tag, width, kind, name)]]
                                     for width in source.WIDTHS for kind, name in groups) for tag in tags)
    overlay_rows = tuple(sorted(set(overlay_locale_rows)))
    overlay_row_ids = {value: index for index, value in enumerate(overlay_rows)}
    for key, head in heads.items():
        if head and data['overlay_nodes'][head - 1][2] != key[2]:
            raise ValueError('unit overlay head has wrong group kind')
    entry_count = sum(len(value) for value in maps)
    words = [VERSION, len(groups), len(cases), len(shared_rows), len(maps), entry_count,
             len(overlay_rows), len(nodes)]
    words.extend(value for kind, name in groups for value in (KINDS.index(kind), ids[name]))
    words.extend(ids[case] for case in cases)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in shared_rows for value in row)
    words.extend(overlay_row_ids[row] for row in overlay_locale_rows)
    words.extend(value for row in overlay_rows for value in row)
    words.extend(value for node in nodes for value in node)
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
    version, group_count, case_count, row_count, map_count, entry_count, overlay_count, node_count = words[:HEADER]
    width = len(source.WIDTHS) * group_count
    expected = (HEADER + group_count * 2 + case_count + count * 2 +
                (row_count + overlay_count) * width + node_count * 4 + map_count * 2 + entry_count * 2)
    if (version != VERSION or group_count < 2 or not 1 <= case_count <= 65536 or
            not 1 <= row_count <= count or not 1 <= overlay_count <= count or
            not 1 <= map_count <= row_count * width + node_count or
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
    used_maps = {value for row in rows for value in row}
    if (any(a >= b for a, b in zip(rows, rows[1:])) or any(index >= map_count for index in used_maps)):
        raise ValueError('invalid/unused unit map or row order')
    overlay_indices = words[at:at + count]
    at += count
    if set(overlay_indices) != set(range(overlay_count)):
        raise ValueError('invalid/unused unit overlay locale row')
    end = at + overlay_count * width
    overlay_rows = tuple(words[i:i + width] for i in range(at, end, width))
    at = end
    if (any(a >= b for a, b in zip(overlay_rows, overlay_rows[1:])) or
            any(head > node_count for row in overlay_rows for head in row)):
        raise ValueError('invalid unit overlay row order/head')
    nodes, reachable, previous = [], {head for row in overlay_rows for head in row if head}, None
    checked_origins = set()
    for i in range(node_count):
        origin, column, index, tail = words[at:at + 4]
        at += 4
        key = tail, origin, column, index
        if (origin >= len(pool) or column >= width or
                index >= map_count or tail > i or previous is not None and key <= previous):
            raise ValueError('invalid/noncanonical unit overlay node')
        if origin not in checked_origins:
            if locale_id(pool[origin]) != pool[origin]:
                raise ValueError('invalid unit overlay origin locale')
            checked_origins.add(origin)
        previous = key
        used_maps.add(index)
        nodes.append((pool[origin], source.WIDTHS[column // group_count],
                      *groups[column % group_count], index, tail))
    for head in range(node_count, 0, -1):
        if head in reachable and nodes[head - 1][-1]:
            reachable.add(nodes[head - 1][-1])
    if reachable != set(range(1, node_count + 1)) or used_maps != set(range(map_count)):
        raise ValueError('unreachable unit overlay node/map')
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
    for locale, width_name, kind, name, index, tail in nodes:
        if (not maps[index] or families[index] != (2 if kind == 'compound' else 1) or
                tail and nodes[tail - 1][2] != kind):
            raise ValueError('invalid unit overlay map/tail family')
    for row in overlay_rows:
        for column, head in enumerate(row):
            if head and nodes[head - 1][2] != groups[column % group_count][0]:
                raise ValueError('unit overlay head has wrong group kind')
    semantic_maps = tuple(sorted({decoded[index] for row in rows for index in row}))
    remap = {value: index for index, value in enumerate(semantic_maps)}
    overlay_maps = tuple(sorted({decoded[node[-2]] for node in nodes}))
    overlay_remap = {value: index for index, value in enumerate(overlay_maps)}
    return {'widths': source.WIDTHS, 'groups': tuple(groups), 'fields': source.FIELDS, 'counts': source.COUNTS,
            'maps': semantic_maps,
            'overlay_maps': overlay_maps,
            'overlay_nodes': tuple(node[:-2] + (overlay_remap[decoded[node[-2]]], node[-1]) for node in nodes),
            'overlay_rows': tuple((tag, width_name, kind, name,
                                   overlay_rows[overlay_indices[i]][w * group_count + g])
                                  for i, tag in enumerate(tags) for w, width_name in enumerate(source.WIDTHS)
                                  for g, (kind, name) in enumerate(groups)),
            'rows': tuple((tag, width_name, kind, name,
                           remap[decoded[rows[indices[i]][w * group_count + g]]])
                          for i, tag in enumerate(tags) for w, width_name in enumerate(source.WIDTHS)
                          for g, (kind, name) in enumerate(groups))}
