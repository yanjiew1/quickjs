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

"""Canonical layered calendar names in optional QJIN section 29."""
import struct

import bundle
import calendar_names as source
from calendar_sources import CALENDAR
from parents import locale_id
from resolve import BLOCK, INHERIT, select_overlays

VERSION, HEADER, GROUP_WIDTH, NODE_WIDTH, ENTRY_WIDTH = 1, 7, 5, 4, 4
BLOCKED = 0xfffffffe
# Stable wire selectors, independent of XML order and formatter preferences.
FAMILIES = ('cyclicNameSets', 'dayPeriods', 'days', 'eras', 'monthPatterns', 'months', 'quarters')
CYCLES = ('', 'years', 'months', 'days', 'dayParts', 'zodiacs', 'solarTerms')
CONTEXTS = ('', 'format', 'stand-alone', 'numeric')
WIDTHS = ('abbreviated', 'narrow', 'wide', 'short', 'all')
YEARS = ('', 'standard', 'leap')
SUFFIXES = frozenset(key for key, path, tag in source.group_paths())


def select(data, tags):
    return select_overlays(data, tags, 'calendar-name')


def strings(data):
    values = {''} | set(data['calendars'])
    values.update(node[0] for node in data['nodes'])
    for fields in data['maps']:
        for code, year, alt, text in fields:
            values.update((code, alt))
            if text != BLOCK:
                values.add(text)
    return values


def valid_tags(tags):
    if (not isinstance(tags, tuple) or not tags or
            any(not isinstance(tag, str) or locale_id(tag) != tag for tag in tags) or
            tags != tuple(sorted(set(tags)))):
        raise ValueError('invalid calendar-name locale inventory')


def valid_inventories(calendars, groups):
    if (not isinstance(calendars, tuple) or not calendars or
            any(not isinstance(code, str) or not CALENDAR.fullmatch(code) for code in calendars) or
            calendars != tuple(sorted(set(calendars)))):
        raise ValueError('invalid calendar-name calendar inventory')
    if (not isinstance(groups, tuple) or not groups or
            any(not isinstance(key, tuple) or len(key) != GROUP_WIDTH or
                not all(isinstance(value, str) for value in key) or
                key[0] not in calendars or key[1:] not in SUFFIXES for key in groups) or
            groups != tuple(sorted(set(groups)))):
        raise ValueError('invalid calendar-name group inventory')


def field_valid(family, code, year, alt, text):
    if not all(isinstance(value, str) for value in (code, year, alt, text)):
        raise ValueError('invalid calendar-name source field')
    if (family not in FAMILIES or year not in YEARS or year and family != 'months' or
            text == INHERIT or any('\0' in value for value in (code, alt, text))):
        raise ValueError('invalid calendar-name source selector/state')
    if (source.ERA.fullmatch(code) is None if family == 'eras' else code not in source.LEAF_CODES[family]):
        raise ValueError('invalid calendar-name source code')
    for value in (code, alt, text):
        value.encode('utf-8')


def encode(data, tags, ids):
    calendars, groups = data['calendars'], data['groups']
    valid_tags(tags)
    valid_inventories(calendars, groups)
    if ids.get('') != 0:
        raise ValueError('calendar-name string zero must be empty')
    compiled_groups = tuple(sorted((calendars.index(cal), FAMILIES.index(family),
                                    CYCLES.index(cycle), CONTEXTS.index(context), WIDTHS.index(width))
                                   for cal, family, cycle, context, width in groups))
    wire_groups = tuple((calendars[cal], FAMILIES[family], CYCLES[cycle], CONTEXTS[context], WIDTHS[width])
                        for cal, family, cycle, context, width in compiled_groups)
    group_ids = {key: index for index, key in enumerate(wire_groups)}
    compiled, checked, used_maps, depths, by_depth = {}, set(), set(), [0], {}
    for old, (locale, group, index, parent) in enumerate(data['nodes'], 1):
        if (not isinstance(locale, str) or locale_id(locale) != locale or group not in group_ids or
                type(index) is not int or not 0 <= index < len(data['maps']) or
                type(parent) is not int or not 0 <= parent < old or
                parent and data['nodes'][parent - 1][1][1] != group[1]):
            raise ValueError('invalid calendar-name node/source identity')
        depths.append(depths[parent] + 1)
        by_depth.setdefault(depths[-1], []).append(old)
        used_maps.add(index)
        if (index, group[1]) in checked:
            continue
        entries, seen = [], set()
        for field in data['maps'][index]:
            field_valid(group[1], *field)
            code, year, alt, text = field
            key = ids[code], YEARS.index(year), ids[alt]
            if key in seen:
                raise ValueError('repeated calendar-name source field')
            seen.add(key)
            entries.append(key + (BLOCKED if text == BLOCK else ids[text],))
        if not entries:
            raise ValueError('empty calendar-name source layer')
        compiled[index] = tuple(sorted(entries))
        checked.add((index, group[1]))
    if used_maps != set(range(len(data['maps']))):
        raise ValueError('unused calendar-name source map')
    maps = tuple(sorted(set(compiled.values())))
    map_ids = {fields: index for index, fields in enumerate(maps)}
    node_ids, nodes = {0: 0}, []
    # Number parents before children, then sort/deduplicate each depth so IDs
    # do not depend on collection order. The reader traverses IDs in reverse.
    for depth in sorted(by_depth):
        candidates = []
        for old in by_depth[depth]:
            locale, group, index, parent = data['nodes'][old - 1]
            record = ids[locale], group_ids[group], map_ids[compiled[index]], node_ids[parent]
            candidates.append((record, old))
        previous = None
        for record, old in sorted(candidates):
            if record != previous:
                nodes.append(record)
            node_ids[old] = len(nodes)
            previous = record
    rows = {tag: [0] * len(groups) for tag in tags}
    seen = set()
    for tag, group, head in data['rows']:
        if (tag not in rows or group not in group_ids or (tag, group) in seen or
                type(head) is not int or head <= 0 or head not in node_ids or
                data['nodes'][head - 1][1][1] != group[1]):
            raise ValueError('invalid calendar-name locale head')
        seen.add((tag, group))
        rows[tag][group_ids[group]] = node_ids[head]
    locale_rows = tuple(tuple(rows[tag]) for tag in tags)
    reachable = {head for row in locale_rows for head in row if head}
    for index in range(len(nodes), 0, -1):
        if index in reachable and nodes[index - 1][3]:
            reachable.add(nodes[index - 1][3])
    if reachable != set(range(1, len(nodes) + 1)):
        raise ValueError('unused calendar-name source node')
    shared = tuple(sorted(set(locale_rows)))
    row_ids = {row: index for index, row in enumerate(shared)}
    words = [VERSION, len(calendars), len(groups), len(shared), len(maps), len(nodes), sum(map(len, maps))]
    words.extend(ids[cal] for cal in calendars)
    words.extend(value for group in compiled_groups for value in group)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in shared for value in row)
    offset = 0
    for fields in maps:
        words.extend((offset, len(fields)))
        offset += len(fields)
    words.extend(value for node in nodes for value in node)
    words.extend(value for fields in maps for field in fields for value in field)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    valid_tags(tags)
    count, raw = section
    if count != len(tags) or not HEADER * 4 <= len(raw) <= bundle.MAX_SIZE or len(raw) % 4:
        raise ValueError('invalid calendar-name locale count/extent')
    version, cal_count, group_count, row_count, map_count, node_count, entry_count = struct.unpack_from('<7I', raw)
    expected = (HEADER + cal_count + group_count * GROUP_WIDTH + count + row_count * group_count +
                map_count * 2 + node_count * NODE_WIDTH + entry_count * ENTRY_WIDTH)
    if (version != VERSION or not cal_count or not group_count or not 1 <= row_count <= count or
            map_count > node_count or entry_count < map_count or len(raw) != expected * 4 or
            not pool or pool[0] != ''):
        raise ValueError('invalid calendar-name version/dimensions')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    at = HEADER
    refs = words[at:at + cal_count]
    if any(index >= len(pool) for index in refs):
        raise ValueError('invalid calendar-name calendar reference')
    calendars = tuple(pool[index] for index in refs)
    at += cal_count
    encoded_groups = tuple(words[i:i + GROUP_WIDTH] for i in range(at, at + group_count * GROUP_WIDTH, GROUP_WIDTH))
    if (encoded_groups != tuple(sorted(set(encoded_groups))) or
            any(cal >= cal_count or family >= len(FAMILIES) or cycle >= len(CYCLES) or
                context >= len(CONTEXTS) or width >= len(WIDTHS)
                for cal, family, cycle, context, width in encoded_groups)):
        raise ValueError('invalid/noncanonical calendar-name groups')
    groups = tuple((calendars[cal], FAMILIES[family], CYCLES[cycle], CONTEXTS[context], WIDTHS[width])
                   for cal, family, cycle, context, width in encoded_groups)
    valid_inventories(calendars, tuple(sorted(groups)))
    at += group_count * GROUP_WIDTH
    locale_rows = words[at:at + count]
    if set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid/unused calendar-name locale row')
    at += count
    rows = tuple(words[i:i + group_count] for i in range(at, at + row_count * group_count, group_count))
    if rows != tuple(sorted(set(rows))) or any(head > node_count for row in rows for head in row):
        raise ValueError('invalid/noncanonical calendar-name rows')
    at += row_count * group_count
    descriptors = tuple(words[i:i + 2] for i in range(at, at + map_count * 2, 2))
    at += map_count * 2
    nodes = tuple(words[i:i + NODE_WIDTH] for i in range(at, at + node_count * NODE_WIDTH, NODE_WIDTH))
    at += node_count * NODE_WIDTH
    entries = tuple(words[i:i + ENTRY_WIDTH] for i in range(at, len(words), ENTRY_WIDTH))
    maps, offset = [], 0
    for start, length in descriptors:
        if start != offset or not length or length > entry_count - offset:
            raise ValueError('invalid calendar-name map span')
        values = entries[start:start + length]
        if any(a[:3] >= b[:3] for a, b in zip(values, values[1:])):
            raise ValueError('noncanonical calendar-name field ordering')
        for code, year, alt, text in values:
            if (code >= len(pool) or year >= len(YEARS) or alt >= len(pool) or
                    text != BLOCKED and (text >= len(pool) or pool[text] in (BLOCK, INHERIT))):
                raise ValueError('invalid calendar-name field reference/state')
        maps.append(values)
        offset += length
    if offset != entry_count or tuple(maps) != tuple(sorted(set(maps))):
        raise ValueError('noncanonical calendar-name maps/entries')
    depths, previous, used_maps, checked, decoded_nodes = [0], None, set(), set(), []
    for index, (origin, group, fields, parent) in enumerate(nodes, 1):
        if (origin >= len(pool) or locale_id(pool[origin]) != pool[origin] or group >= group_count or
                fields >= map_count or parent >= index):
            raise ValueError('invalid calendar-name node/reference/source identity')
        family = groups[group][1]
        if parent and groups[nodes[parent - 1][1]][1] != family:
            raise ValueError('calendar-name parent has a different family')
        depth = depths[parent] + 1
        order = depth, origin, group, fields, parent
        if previous is not None and order <= previous:
            raise ValueError('noncanonical calendar-name node order')
        depths.append(depth)
        previous = order
        used_maps.add(fields)
        if (fields, family) not in checked:
            for code, year, alt, text in maps[fields]:
                field_valid(family, pool[code], YEARS[year], pool[alt], BLOCK if text == BLOCKED else pool[text])
            checked.add((fields, family))
        decoded_nodes.append((pool[origin], groups[group], fields, parent))
    if any(head and groups[nodes[head - 1][1]][1] != groups[group][1]
           for row in rows for group, head in enumerate(row)):
        raise ValueError('calendar-name head has a different family')
    reachable = {head for row in rows for head in row if head}
    for index in range(node_count, 0, -1):
        if index in reachable and nodes[index - 1][3]:
            reachable.add(nodes[index - 1][3])
    if reachable != set(range(1, node_count + 1)) or used_maps != set(range(map_count)):
        raise ValueError('unused calendar-name node/map')
    raw_maps = tuple(tuple(sorted((pool[code], YEARS[year], pool[alt], BLOCK if text == BLOCKED else pool[text])
                                  for code, year, alt, text in fields)) for fields in maps)
    return {'calendars': calendars, 'groups': tuple(sorted(groups)), 'maps': raw_maps,
            'nodes': tuple(decoded_nodes),
            'rows': tuple(sorted((tag, groups[group], head) for tag, row in zip(tags, locale_rows)
                                 for group, head in enumerate(rows[row]) if head))}
