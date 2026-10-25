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

"""Layered source names and normalized lookup indexes in QJIN section 26."""
import struct

import bundle
import display_names as source
from parents import locale_id
from resolve import BLOCK, INHERIT

VERSION, HEADER, ENTRY_WIDTH, NODE_WIDTH = 1, 6, 6, 4
BLOCKED = 0xfffffffe
IDENTIFIERS = frozenset(source.GROUPS[:5])


def normalized(group, code):
    return code.lower().replace('_', '-') if group in IDENTIFIERS else code


def field_valid(group, code, alt, menu, scope, text):
    if not all(isinstance(value, str) for value in (code, alt, menu, scope, text)):
        raise ValueError('invalid display-name source field')
    if (not code or '\0' in code or any('\0' in value for value in (alt, menu, scope, text)) or
            text == INHERIT or menu and group != 'language' or scope and group != 'calendar'):
        raise ValueError('invalid display-name source selectors/state')
    if group in IDENTIFIERS and source.CODE.fullmatch(code) is None:
        raise ValueError('invalid display-name source code')
    if group == 'pattern' and code not in source.PATTERNS:
        raise ValueError('invalid display-name pattern key')


def select(data, tags):
    selected = set(tags)
    rows = tuple(row for row in data['rows'] if row[0] in selected)
    used = set()
    for tag, group, head in rows:
        if type(head) is not int or head < 0:
            raise ValueError('invalid display-name chain reference')
        while head:
            if type(head) is not int or not 1 <= head <= len(data['nodes']):
                raise ValueError('invalid display-name chain reference')
            if head in used:
                break
            used.add(head)
            parent = data['nodes'][head - 1][3]
            if type(parent) is not int or not 0 <= parent < head:
                raise ValueError('invalid display-name parent reference')
            head = parent
    nodes = tuple(sorted(used))
    used_maps = sorted({data['nodes'][index - 1][2] for index in nodes})
    if any(type(index) is not int or not 0 <= index < len(data['maps']) for index in used_maps):
        raise ValueError('invalid display-name source map reference')
    node_ids = {index: i + 1 for i, index in enumerate(nodes)}
    map_ids = {index: i for i, index in enumerate(used_maps)}
    return dict(data, maps=tuple(data['maps'][index] for index in used_maps),
                nodes=tuple((locale, group, map_ids[index], node_ids.get(parent, 0))
                            for old in nodes for locale, group, index, parent in (data['nodes'][old - 1],)),
                rows=tuple((tag, group, node_ids.get(head, 0)) for tag, group, head in rows))


def strings(data):
    values = {''}
    for locale, group, index, parent in data['nodes']:
        values.add(locale)
        for code, alt, menu, scope, text in data['maps'][index]:
            values.update((code, normalized(group, code), alt, menu, scope))
            if text != BLOCK:
                values.add(text)
    return values


def encode(data, tags, ids):
    groups = source.GROUPS
    rows = {(tag, group): head for tag, group, head in data['rows']}
    if (data['groups'] != groups or not tags or len(set(tags)) != len(tags) or ids.get('') != 0 or
            len(rows) != len(data['rows']) or
            set(rows) != {(tag, group) for tag in tags for group in groups}):
        raise ValueError('invalid display-name locale/group coverage')
    compiled, depths, used_maps = {}, [0], set()
    for i, (locale, group, index, parent) in enumerate(data['nodes'], 1):
        if (not isinstance(locale, str) or locale_id(locale) != locale or group not in groups or
                type(index) is not int or not 0 <= index < len(data['maps']) or
                type(parent) is not int or not 0 <= parent < i or
                parent and data['nodes'][parent - 1][1] != group):
            raise ValueError('invalid display-name node/source identity')
        depths.append(depths[parent] + 1)
        used_maps.add(index)
        if (index, group) in compiled:
            continue
        entries, seen = [], set()
        for entry in data['maps'][index]:
            field_valid(group, *entry)
            code, alt, menu, scope, text = entry
            key = normalized(group, code), alt, menu, scope
            if key in seen:
                raise ValueError('display-name normalization collision within a source layer')
            seen.add(key)
            entries.append(tuple(ids[value] for value in key + (code,)) +
                           (BLOCKED if text == BLOCK else ids[text],))
        if not entries:
            raise ValueError('empty display-name source layer')
        compiled[index, group] = tuple(sorted(entries))
    if used_maps != set(range(len(data['maps']))):
        raise ValueError('unused display-name source map')
    maps = tuple(sorted(set(compiled.values())))
    map_ids = {entries: index for index, entries in enumerate(maps)}
    # Canonicalize siblings only after their lower-depth parent IDs are known.
    # This gives the same wire order for equivalent source topological orders.
    node_ids, nodes = {0: 0}, []
    by_depth = {}
    for index, node in enumerate(data['nodes'], 1):
        by_depth.setdefault(depths[index], []).append((index, node))
    for depth in sorted(by_depth):
        candidates = []
        for old, (locale, group, index, parent) in by_depth[depth]:
            record = ids[locale], groups.index(group), map_ids[compiled[index, group]], node_ids[parent]
            candidates.append((record, old))
        previous = None
        for record, old in sorted(candidates):
            if record != previous:
                nodes.append(record)
            node_ids[old] = len(nodes)
            previous = record
    locale_rows = []
    for tag in tags:
        row = []
        for group in groups:
            head = rows[tag, group]
            if (type(head) is not int or head not in node_ids or
                    head and data['nodes'][head - 1][1] != group):
                raise ValueError('invalid display-name locale head')
            row.append(node_ids[head])
        locale_rows.append(tuple(row))
    reachable = {head for row in locale_rows for head in row if head}
    for index in range(len(nodes), 0, -1):
        if index in reachable and nodes[index - 1][3]:
            reachable.add(nodes[index - 1][3])
    if reachable != set(range(1, len(nodes) + 1)):
        raise ValueError('unused display-name source node')
    shared = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(shared)}
    keys = tuple(sorted({(group, entry[0]) for locale, group, index, parent in nodes
                         for entry in maps[index]}))
    words = [VERSION, len(shared), len(maps), len(nodes), sum(map(len, maps)), len(keys)]
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in shared for value in row)
    offset = 0
    for entries in maps:
        words.extend((offset, len(entries)))
        offset += len(entries)
    words.extend(value for node in nodes for value in node)
    words.extend(value for entries in maps for entry in entries for value in entry)
    words.extend(value for key in keys for value in key)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid display-name locale count/extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    version, row_count, map_count, node_count, entry_count, key_count = words[:HEADER]
    width = len(source.GROUPS)
    expected = HEADER + count + row_count * width + map_count * 2 + node_count * NODE_WIDTH + entry_count * ENTRY_WIDTH + key_count * 2
    if (version != VERSION or not 1 <= row_count <= count or map_count > node_count or
            entry_count < map_count or key_count > entry_count * width or
            len(words) != expected or not pool or pool[0] != ''):
        raise ValueError('invalid display-name version/dimensions')
    at = HEADER
    locale_rows = words[at:at + count]
    if set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid/unused display-name locale row')
    at += count
    rows = tuple(words[i:i + width] for i in range(at, at + row_count * width, width))
    if rows != tuple(sorted(set(rows))) or any(head > node_count for row in rows for head in row):
        raise ValueError('invalid/noncanonical display-name rows')
    at += row_count * width
    descriptors = tuple(words[i:i + 2] for i in range(at, at + map_count * 2, 2))
    at += map_count * 2
    nodes = tuple(words[i:i + NODE_WIDTH] for i in range(at, at + node_count * NODE_WIDTH, NODE_WIDTH))
    at += node_count * NODE_WIDTH
    if any(head and nodes[head - 1][1] != group
           for row in rows for group, head in enumerate(row)):
        raise ValueError('display-name row head has the wrong group')
    entries = tuple(words[i:i + ENTRY_WIDTH] for i in range(at, at + entry_count * ENTRY_WIDTH, ENTRY_WIDTH))
    at += entry_count * ENTRY_WIDTH
    keys = tuple(words[i:i + 2] for i in range(at, len(words), 2))
    maps, offset = [], 0
    for start, length in descriptors:
        if start != offset or not length or length > entry_count - offset:
            raise ValueError('invalid display-name map span')
        values = entries[start:start + length]
        if any(a[:4] >= b[:4] for a, b in zip(values, values[1:])):
            raise ValueError('noncanonical display-name lookup ordering/collision')
        for norm, alt, menu, scope, code, text in values:
            if (any(index >= len(pool) for index in (norm, alt, menu, scope, code)) or
                    text != BLOCKED and (text >= len(pool) or pool[text] in (BLOCK, INHERIT))):
                raise ValueError('invalid display-name string reference')
        maps.append(values)
        offset += length
    if offset != entry_count or tuple(maps) != tuple(sorted(set(maps))):
        raise ValueError('noncanonical display-name maps/entries')
    depths, previous, used_maps, checked, required_keys = [0], None, set(), set(), set()
    decoded_nodes = []
    for i, (origin, group, index, parent) in enumerate(nodes, 1):
        if (origin >= len(pool) or locale_id(pool[origin]) != pool[origin] or group >= width or
                index >= map_count or parent >= i or parent and nodes[parent - 1][1] != group):
            raise ValueError('invalid display-name node/reference/source identity')
        depth = depths[parent] + 1
        order = depth, origin, group, index, parent
        if previous is not None and order <= previous:
            raise ValueError('noncanonical display-name node order')
        depths.append(depth)
        previous = order
        used_maps.add(index)
        if (index, group) not in checked:
            for norm, alt, menu, scope, code, text in maps[index]:
                field_valid(source.GROUPS[group], pool[code], pool[alt], pool[menu], pool[scope],
                            BLOCK if text == BLOCKED else pool[text])
                if normalized(source.GROUPS[group], pool[code]) != pool[norm]:
                    raise ValueError('inconsistent display-name normalized key')
                required_keys.add((group, norm))
            checked.add((index, group))
        decoded_nodes.append((pool[origin], source.GROUPS[group], index, parent))
    reachable = {head for row in rows for head in row if head}
    for index in range(node_count, 0, -1):
        if index in reachable and nodes[index - 1][3]:
            reachable.add(nodes[index - 1][3])
    if (reachable != set(range(1, node_count + 1)) or used_maps != set(range(map_count)) or
            keys != tuple(sorted(required_keys))):
        raise ValueError('unused display-name node/map or invalid key inventory')
    # Reconstruct the collector's raw facts, not the normalized lookup columns.
    # Normalization may create distinct compiled maps for one raw map/group pair.
    raw_maps = tuple(tuple(sorted((pool[code], pool[alt], pool[menu], pool[scope],
                                   BLOCK if text == BLOCKED else pool[text])
                                  for norm, alt, menu, scope, code, text in values)) for values in maps)
    return {'groups': source.GROUPS, 'maps': raw_maps, 'nodes': tuple(decoded_nodes),
            'rows': tuple((tag, group, rows[index][i]) for tag, index in zip(tags, locale_rows)
                          for i, group in enumerate(source.GROUPS))}
