"""Canonical interval source layers in optional QJIN section 31.

Calendar blocks share dense suffix-head rows. Decoding keeps the rectangular
inventory factored, rather than materializing every calendar/suffix pair.
"""
import struct

import bundle
from calendar_sources import CALENDAR
import date_intervals as source
from parents import locale_id
from resolve import BLOCK, INHERIT

VERSION, HEADER, SUFFIX_WIDTH, NODE_WIDTH, ENTRY_WIDTH = 1, 7, 4, 4, 3
BLOCKED = 0xfffffffe
FAMILIES = source.FAMILIES


def select(data, tags):
    selected = set(tags)
    rows = tuple(row for row in data['rows'] if row[0] in selected)
    used = set()
    for tag, group, head in rows:
        if type(head) is not int or not 1 <= head <= len(data['nodes']):
            raise ValueError('invalid interval chain reference')
        while head:
            if type(head) is not int or not 1 <= head <= len(data['nodes']):
                raise ValueError('invalid interval chain reference')
            if head in used:
                break
            used.add(head)
            parent = data['nodes'][head - 1][3]
            if type(parent) is not int or not 0 <= parent < head:
                raise ValueError('invalid interval parent reference')
            head = parent
    nodes = tuple(sorted(used))
    maps = sorted({data['nodes'][index - 1][2] for index in nodes})
    if any(type(index) is not int or not 0 <= index < len(data['maps']) for index in maps):
        raise ValueError('invalid interval map reference')
    node_ids = {old: index + 1 for index, old in enumerate(nodes)}
    map_ids = {old: index for index, old in enumerate(maps)}
    return dict(data, maps=tuple(data['maps'][index] for index in maps),
                nodes=tuple((locale, group, map_ids[index], node_ids.get(parent, 0))
                            for old in nodes for locale, group, index, parent in (data['nodes'][old - 1],)),
                rows=tuple((tag, group, node_ids[head]) for tag, group, head in rows))


def valid_tags(tags):
    if (not isinstance(tags, tuple) or not tags or
            any(not isinstance(tag, str) or locale_id(tag) != tag for tag in tags) or
            tags != tuple(sorted(set(tags)))):
        raise ValueError('invalid interval locale inventory')


def valid_inventories(calendars, suffixes):
    if (not isinstance(calendars, tuple) or not calendars or
            any(not isinstance(code, str) or not CALENDAR.fullmatch(code) for code in calendars) or
            calendars != tuple(sorted(set(calendars)))):
        raise ValueError('invalid interval calendar inventory')
    if (not isinstance(suffixes, tuple) or not suffixes or
            any(not isinstance(key, tuple) or len(key) != SUFFIX_WIDTH or
                not all(isinstance(value, str) for value in key) for key in suffixes)):
        raise ValueError('invalid interval suffix inventory')
    if suffixes != tuple(sorted(set(suffixes))):
        raise ValueError('noncanonical interval suffix inventory')
    for alt, family, skeleton, item_alt in suffixes:
        if (family not in FAMILIES or any('\0' in text for text in (alt, skeleton, item_alt)) or
                (source.SKELETON.fullmatch(skeleton) is None if family == 'item'
                 else skeleton != '' or item_alt != '')):
            raise ValueError('invalid interval suffix selectors')
        for text in (alt, skeleton, item_alt):
            text.encode('utf-8')
    # Global source-group IDs remain uint32 even though the inventory is factored.
    bundle.uint32(len(calendars) * len(suffixes))


def inventory(data):
    calendars = data['calendars']
    if 'groups' in data:
        groups = data['groups']
        if (not isinstance(groups, tuple) or not groups or
                any(not isinstance(key, tuple) or len(key) != SUFFIX_WIDTH + 1 or
                    not all(isinstance(value, str) for value in key) for key in groups) or
                groups != tuple(sorted(set(groups)))):
            raise ValueError('invalid interval group inventory')
        suffixes = tuple(sorted({key[1:] for key in groups}))
        valid_inventories(calendars, suffixes)
        if (len(groups) != len(calendars) * len(suffixes) or
                any(key[0] not in calendars for key in groups)):
            raise ValueError('interval group inventory must be rectangular')
    else:
        suffixes = data['suffixes']
        valid_inventories(calendars, suffixes)
    return calendars, suffixes


def strings(data):
    calendars, suffixes = inventory(data)
    values = {''} | set(calendars)
    values.update(value for alt, family, skeleton, item_alt in suffixes for value in (alt, skeleton, item_alt))
    values.update(node[0] for node in data['nodes'])
    for fields in data['maps']:
        for code, alt, text in fields:
            values.update((code, alt))
            if text != BLOCK:
                values.add(text)
    return values


def field_valid(family, code, alt, text):
    if (not all(isinstance(value, str) for value in (code, alt, text)) or
            family not in FAMILIES or text == INHERIT or
            any('\0' in value for value in (code, alt, text)) or
            (code not in source.DIFFERENCES if family == 'item' else
             code not in source.RANGES if family == 'range' else code != '')):
        raise ValueError('invalid interval source field/state')
    for value in (code, alt, text):
        value.encode('utf-8')


def encode(data, tags, ids):
    valid_tags(tags)
    calendars, suffixes = inventory(data)
    if ids.get('') != 0:
        raise ValueError('interval string zero must be empty')
    registry = tuple(sorted((ids[alt], FAMILIES.index(family), ids[skeleton], ids[item_alt], key)
                            for key in suffixes for alt, family, skeleton, item_alt in (key,)))
    wire_suffixes = tuple(row[SUFFIX_WIDTH] for row in registry)
    suffix_ids = {key: index for index, key in enumerate(wire_suffixes)}
    calendar_ids = {key: index for index, key in enumerate(calendars)}

    def group_id(key):
        if (not isinstance(key, tuple) or len(key) != SUFFIX_WIDTH + 1 or
                key[0] not in calendar_ids or key[1:] not in suffix_ids):
            raise ValueError('invalid interval source group')
        return calendar_ids[key[0]] * len(suffixes) + suffix_ids[key[1:]]

    compiled, checked, used_maps, depths, by_depth = {}, set(), set(), [0], {}
    for old, (locale, group, index, parent) in enumerate(data['nodes'], 1):
        group_id(group)
        if (not isinstance(locale, str) or locale_id(locale) != locale or
                type(index) is not int or not 0 <= index < len(data['maps']) or
                type(parent) is not int or not 0 <= parent < old or
                parent and data['nodes'][parent - 1][1][2] != group[2]):
            raise ValueError('invalid interval node/source identity')
        depths.append(depths[parent] + 1)
        by_depth.setdefault(depths[-1], []).append(old)
        used_maps.add(index)
        if (index, group[2]) in checked:
            continue
        entries, seen = [], set()
        for field in data['maps'][index]:
            field_valid(group[2], *field)
            code, alt, text = field
            key = ids[code], ids[alt]
            if key in seen:
                raise ValueError('repeated interval source field')
            seen.add(key)
            entries.append(key + (BLOCKED if text == BLOCK else ids[text],))
        if not entries:
            raise ValueError('empty interval source layer')
        compiled[index] = tuple(sorted(entries))
        checked.add((index, group[2]))
    if used_maps != set(range(len(data['maps']))):
        raise ValueError('unused interval source map')
    maps = tuple(sorted(set(compiled.values())))
    map_ids = {fields: index for index, fields in enumerate(maps)}
    node_ids, nodes = {0: 0}, []
    for depth in sorted(by_depth):
        candidates = []
        for old in by_depth[depth]:
            locale, group, index, parent = data['nodes'][old - 1]
            record = ids[locale], group_id(group), map_ids[compiled[index]], node_ids[parent]
            candidates.append((record, old))
        previous = None
        for record, old in sorted(candidates):
            if record != previous:
                nodes.append(record)
            node_ids[old] = len(nodes)
            previous = record
    rows = {(tag, cal): [0] * len(suffixes) for tag in tags for cal in calendars}
    seen = set()
    for tag, group, head in data['rows']:
        encoded_group = group_id(group)
        if (tag not in tags or (tag, group) in seen or
                type(head) is not int or head <= 0 or head not in node_ids or
                data['nodes'][head - 1][1][2] != group[2]):
            raise ValueError('invalid interval locale head')
        seen.add((tag, group))
        rows[tag, group[0]][encoded_group % len(suffixes)] = node_ids[head]
    locale_rows = tuple(tuple(rows[tag, cal]) for tag in tags for cal in calendars)
    shared = tuple(sorted(set(locale_rows)))
    reachable = {head for row in shared for head in row if head}
    for index in range(len(nodes), 0, -1):
        if index in reachable and nodes[index - 1][3]:
            reachable.add(nodes[index - 1][3])
    if reachable != set(range(1, len(nodes) + 1)):
        raise ValueError('unused interval source node')
    row_ids = {row: index for index, row in enumerate(shared)}
    words = [VERSION, len(calendars), len(suffixes), len(shared), len(maps), len(nodes), sum(map(len, maps))]
    words.extend(ids[cal] for cal in calendars)
    words.extend(value for row in registry for value in row[:SUFFIX_WIDTH])
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in shared for value in row)
    offset = 0
    for fields in maps:
        words.extend((offset, len(fields)))
        offset += len(fields)
    words.extend(value for node in nodes for value in node)
    words.extend(value for fields in maps for field in fields for value in field)
    if len(words) > bundle.MAX_SIZE // 4:
        raise ValueError('interval section exceeds size limit')
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    valid_tags(tags)
    count, raw = section
    if count != len(tags) or not HEADER * 4 <= len(raw) <= bundle.MAX_SIZE or len(raw) % 4:
        raise ValueError('invalid interval locale count/extent')
    version, cal_count, suffix_count, row_count, map_count, node_count, entry_count = struct.unpack_from('<7I', raw)
    group_count = bundle.uint32(cal_count * suffix_count)
    expected = (HEADER + cal_count + suffix_count * SUFFIX_WIDTH + count * cal_count +
                row_count * suffix_count + map_count * 2 + node_count * NODE_WIDTH + entry_count * ENTRY_WIDTH)
    if (version != VERSION or not cal_count or not suffix_count or not 1 <= row_count <= count * cal_count or
            map_count > node_count or entry_count < map_count or len(raw) != expected * 4 or
            not pool or pool[0] != ''):
        raise ValueError('invalid interval version/dimensions')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    at = HEADER
    refs = words[at:at + cal_count]
    if any(index >= len(pool) for index in refs):
        raise ValueError('invalid interval calendar reference')
    calendars = tuple(pool[index] for index in refs)
    at += cal_count
    encoded_suffixes = tuple(words[i:i + SUFFIX_WIDTH] for i in range(at, at + suffix_count * SUFFIX_WIDTH, SUFFIX_WIDTH))
    if (encoded_suffixes != tuple(sorted(set(encoded_suffixes))) or
            any(family >= len(FAMILIES) or max(alt, skeleton, item_alt) >= len(pool)
                for alt, family, skeleton, item_alt in encoded_suffixes)):
        raise ValueError('invalid/noncanonical interval suffixes')
    suffixes = tuple((pool[alt], FAMILIES[family], pool[skeleton], pool[item_alt])
                     for alt, family, skeleton, item_alt in encoded_suffixes)
    valid_inventories(calendars, tuple(sorted(suffixes)))

    def group(index):
        return (calendars[index // suffix_count],) + suffixes[index % suffix_count]

    at += suffix_count * SUFFIX_WIDTH
    locale_rows = words[at:at + count * cal_count]
    if set(locale_rows) != set(range(row_count)):
        raise ValueError('invalid/unused interval calendar block')
    at += count * cal_count
    rows = tuple(words[i:i + suffix_count] for i in range(at, at + row_count * suffix_count, suffix_count))
    if rows != tuple(sorted(set(rows))) or any(head > node_count for row in rows for head in row):
        raise ValueError('invalid/noncanonical interval blocks')
    at += row_count * suffix_count
    descriptors = tuple(words[i:i + 2] for i in range(at, at + map_count * 2, 2))
    at += map_count * 2
    nodes = tuple(words[i:i + NODE_WIDTH] for i in range(at, at + node_count * NODE_WIDTH, NODE_WIDTH))
    at += node_count * NODE_WIDTH
    entries = tuple(words[i:i + ENTRY_WIDTH] for i in range(at, len(words), ENTRY_WIDTH))
    maps, offset = [], 0
    for start, length in descriptors:
        if start != offset or not length or length > entry_count - offset:
            raise ValueError('invalid interval map span')
        values = entries[start:start + length]
        if any(a[:2] >= b[:2] for a, b in zip(values, values[1:])):
            raise ValueError('noncanonical interval field ordering')
        for code, alt, text in values:
            if (code >= len(pool) or alt >= len(pool) or
                    text != BLOCKED and (text >= len(pool) or pool[text] in (BLOCK, INHERIT))):
                raise ValueError('invalid interval field reference/state')
        maps.append(values)
        offset += length
    if offset != entry_count or tuple(maps) != tuple(sorted(set(maps))):
        raise ValueError('noncanonical interval maps/entries')
    depths, previous, used_maps, checked, decoded_nodes = [0], None, set(), set(), []
    for index, (origin, source_group, fields, parent) in enumerate(nodes, 1):
        if (origin >= len(pool) or locale_id(pool[origin]) != pool[origin] or source_group >= group_count or
                fields >= map_count or parent >= index):
            raise ValueError('invalid interval node/reference/source identity')
        family = suffixes[source_group % suffix_count][1]
        if parent and suffixes[nodes[parent - 1][1] % suffix_count][1] != family:
            raise ValueError('interval parent has a different family')
        depth = depths[parent] + 1
        order = depth, origin, source_group, fields, parent
        if previous is not None and order <= previous:
            raise ValueError('noncanonical interval node order')
        depths.append(depth)
        previous = order
        used_maps.add(fields)
        if (fields, family) not in checked:
            for code, alt, text in maps[fields]:
                field_valid(family, pool[code], pool[alt], BLOCK if text == BLOCKED else pool[text])
            checked.add((fields, family))
        decoded_nodes.append((pool[origin], group(source_group), fields, parent))
    nonzero = []
    for row in rows:
        heads = []
        for suffix, head in enumerate(row):
            if head:
                if suffixes[nodes[head - 1][1] % suffix_count][1] != suffixes[suffix][1]:
                    raise ValueError('interval head has a different family')
                heads.append((suffix, head))
        nonzero.append(heads)
    reachable = {head for row in nonzero for suffix, head in row}
    for index in range(node_count, 0, -1):
        if index in reachable and nodes[index - 1][3]:
            reachable.add(nodes[index - 1][3])
    if reachable != set(range(1, node_count + 1)) or used_maps != set(range(map_count)):
        raise ValueError('unused interval node/map')
    raw_maps = tuple(tuple(sorted((pool[code], pool[alt], BLOCK if text == BLOCKED else pool[text])
                                  for code, alt, text in fields)) for fields in maps)
    # Output-sensitive expansion only: never scan the implicit locale/calendar/
    # suffix cube, including absent heads. The inventory itself stays factored.
    return {'calendars': calendars, 'suffixes': tuple(sorted(suffixes)), 'maps': raw_maps,
            'nodes': tuple(decoded_nodes),
            'rows': tuple(sorted((tags[index // cal_count], group(index % cal_count * suffix_count + suffix), head)
                                 for index, row in enumerate(locale_rows) for suffix, head in nonzero[row]))}
