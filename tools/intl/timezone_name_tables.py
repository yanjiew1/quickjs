"""Canonical localized timezone presentation layers in optional QJIN section 32."""
import struct

import bundle
from parents import locale_id
from resolve import BLOCK, INHERIT, select_overlays
import timezone_names as source

VERSION, HEADER, GROUP_WIDTH, NODE_WIDTH, ENTRY_WIDTH = 1, 9, 5, 4, 4
BLOCKED = 0xfffffffe
FAMILIES = ('format', 'city', 'zone-name', 'metazone-name')
WIDTHS = ('', 'long', 'short')
TYPES = ('', 'standard', 'daylight')


def select(data, tags):
    return select_overlays(data, tags, 'timezone-name')


def semantic(family):
    # Zone overrides and metazone names have identical leaf semantics.
    return min(FAMILIES.index(family), 2)


def valid_tags(tags):
    if (not isinstance(tags, tuple) or not tags or
            any(not isinstance(tag, str) or locale_id(tag) != tag for tag in tags) or
            tags != tuple(sorted(set(tags)))):
        raise ValueError('invalid timezone-name locale inventory')


def valid_groups(groups):
    if (not isinstance(groups, tuple) or not groups or
            any(not isinstance(key, tuple) or len(key) != GROUP_WIDTH or
                not all(isinstance(value, str) for value in key) for key in groups) or
            groups != tuple(sorted(set(groups)))):
        raise ValueError('invalid timezone-name group inventory')
    for family, entity, entity_alt, width, width_alt in groups:
        if family not in FAMILIES or any('\0' in text for text in (entity, entity_alt, width_alt)):
            raise ValueError('invalid timezone-name group selectors')
        if family == 'format':
            valid = not any((entity, entity_alt, width, width_alt))
        elif family == 'city':
            valid = source.ZONE.fullmatch(entity) is not None and not width and not width_alt
        else:
            identity = source.ZONE if family == 'zone-name' else source.METAZONE
            valid = identity.fullmatch(entity) is not None and width in source.WIDTHS
        if not valid:
            raise ValueError('invalid timezone-name group selectors')
        for text in (entity, entity_alt, width_alt):
            text.encode('utf-8')


def strings(data):
    values = {''}
    for family, entity, entity_alt, width, width_alt in data['groups']:
        values.update((entity, entity_alt, width_alt))
    values.update(node[0] for node in data['nodes'])
    for fields in data['maps']:
        for code, subtype, alt, text in fields:
            values.update((code, alt))
            if text != BLOCK:
                values.add(text)
    return values


def field_valid(family, code, subtype, alt, text):
    if (not all(isinstance(value, str) for value in (code, subtype, alt, text)) or
            family not in FAMILIES or subtype not in TYPES or text == INHERIT or
            any('\0' in value for value in (code, alt, text))):
        raise ValueError('invalid timezone-name source field/state')
    if (code not in source.FORMATS if family == 'format' else
            code != 'exemplarCity' if family == 'city' else code not in source.NAMES):
        raise ValueError('invalid timezone-name source code')
    if subtype and (family != 'format' or code != 'regionFormat'):
        raise ValueError('invalid timezone-name leaf type')
    for value in (code, alt, text):
        value.encode('utf-8')


def encode(data, tags, ids):
    groups = data['groups']
    valid_tags(tags)
    valid_groups(groups)
    if ids.get('') != 0:
        raise ValueError('timezone-name string zero must be empty')
    registry = tuple(sorted((FAMILIES.index(family), ids[entity], ids[entity_alt], WIDTHS.index(width), ids[width_alt], key)
                            for key in groups for family, entity, entity_alt, width, width_alt in (key,)))
    wire_groups = tuple(row[GROUP_WIDTH] for row in registry)
    group_ids = {key: index for index, key in enumerate(wire_groups)}
    spans = tuple(tuple(index for index, key in enumerate(wire_groups) if key[0] == family) for family in FAMILIES)
    compiled, checked, used_maps, depths, by_depth = {}, set(), set(), [0], {}
    for old, (locale, group, index, parent) in enumerate(data['nodes'], 1):
        if (not isinstance(locale, str) or locale_id(locale) != locale or group not in group_ids or
                type(index) is not int or not 0 <= index < len(data['maps']) or
                type(parent) is not int or not 0 <= parent < old or
                parent and semantic(data['nodes'][parent - 1][1][0]) != semantic(group[0])):
            raise ValueError('invalid timezone-name node/source identity')
        depths.append(depths[parent] + 1)
        by_depth.setdefault(depths[-1], []).append(old)
        used_maps.add(index)
        if (index, semantic(group[0])) in checked:
            continue
        entries, seen = [], set()
        for field in data['maps'][index]:
            field_valid(group[0], *field)
            code, subtype, alt, text = field
            key = ids[code], TYPES.index(subtype), ids[alt]
            if key in seen:
                raise ValueError('repeated timezone-name source field')
            seen.add(key)
            entries.append(key + (BLOCKED if text == BLOCK else ids[text],))
        if not entries:
            raise ValueError('empty timezone-name source layer')
        compiled[index] = tuple(sorted(entries))
        checked.add((index, semantic(group[0])))
    if used_maps != set(range(len(data['maps']))):
        raise ValueError('unused timezone-name source map')
    maps = tuple(sorted(set(compiled.values())))
    map_ids = {fields: index for index, fields in enumerate(maps)}
    node_ids, nodes = {0: 0}, []
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
                semantic(data['nodes'][head - 1][1][0]) != semantic(group[0])):
            raise ValueError('invalid timezone-name locale head')
        seen.add((tag, group))
        rows[tag][group_ids[group]] = node_ids[head]
    families, reachable = [], set()
    for span in spans:
        if not span:
            families.append(((), ()))
            continue
        locale_rows = tuple(tuple(rows[tag][index] for index in span) for tag in tags)
        shared = tuple(sorted(set(locale_rows)))
        row_ids = {row: index for index, row in enumerate(shared)}
        families.append((tuple(row_ids[row] for row in locale_rows), shared))
        reachable.update(head for row in shared for head in row if head)
    for index in range(len(nodes), 0, -1):
        if index in reachable and nodes[index - 1][3]:
            reachable.add(nodes[index - 1][3])
    if reachable != set(range(1, len(nodes) + 1)):
        raise ValueError('unused timezone-name source node')
    words = [VERSION, len(groups), len(maps), len(nodes), sum(map(len, maps))]
    words.extend(len(shared) for refs, shared in families)
    words.extend(value for row in registry for value in row[:GROUP_WIDTH])
    for refs, shared in families:
        words.extend(refs)
        words.extend(value for row in shared for value in row)
    offset = 0
    for fields in maps:
        words.extend((offset, len(fields)))
        offset += len(fields)
    words.extend(value for node in nodes for value in node)
    words.extend(value for fields in maps for field in fields for value in field)
    if len(words) > bundle.MAX_SIZE // 4:
        raise ValueError('timezone-name section exceeds size limit')
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    valid_tags(tags)
    count, raw = section
    if count != len(tags) or not HEADER * 4 <= len(raw) <= bundle.MAX_SIZE or len(raw) % 4:
        raise ValueError('invalid timezone-name locale count/extent')
    header = struct.unpack_from('<9I', raw)
    version, group_count, map_count, node_count, entry_count = header[:5]
    row_counts = header[5:]
    if (version != VERSION or not group_count or map_count > node_count or entry_count < map_count or
            HEADER + group_count * GROUP_WIDTH > len(raw) // 4 or not pool or pool[0] != ''):
        raise ValueError('invalid timezone-name version/dimensions')
    encoded_groups = tuple(struct.unpack_from('<5I', raw, (HEADER + index * GROUP_WIDTH) * 4)
                           for index in range(group_count))
    if (encoded_groups != tuple(sorted(set(encoded_groups))) or
            any(family >= len(FAMILIES) or width >= len(WIDTHS) or max(entity, entity_alt, width_alt) >= len(pool)
                for family, entity, entity_alt, width, width_alt in encoded_groups)):
        raise ValueError('invalid/noncanonical timezone-name groups')
    groups = tuple((FAMILIES[family], pool[entity], pool[entity_alt], WIDTHS[width], pool[width_alt])
                   for family, entity, entity_alt, width, width_alt in encoded_groups)
    valid_groups(tuple(sorted(groups)))
    spans = tuple(tuple(index for index, key in enumerate(groups) if key[0] == family) for family in FAMILIES)
    expected = HEADER + group_count * GROUP_WIDTH + map_count * 2 + node_count * NODE_WIDTH + entry_count * ENTRY_WIDTH
    for span, row_count in zip(spans, row_counts):
        if (span and not 1 <= row_count <= count) or (not span and row_count):
            raise ValueError('invalid timezone-name family dimensions')
        if span:
            expected += count + row_count * len(span)
    if len(raw) != expected * 4:
        raise ValueError('invalid timezone-name table extent')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    at, families = HEADER + group_count * GROUP_WIDTH, []
    for span, row_count in zip(spans, row_counts):
        if not span:
            families.append(((), ()))
            continue
        locale_rows = words[at:at + count]
        if set(locale_rows) != set(range(row_count)):
            raise ValueError('invalid/unused timezone-name family row')
        at += count
        width = len(span)
        rows = tuple(words[i:i + width] for i in range(at, at + row_count * width, width))
        if rows != tuple(sorted(set(rows))) or any(head > node_count for row in rows for head in row):
            raise ValueError('invalid/noncanonical timezone-name family rows')
        at += row_count * width
        families.append((locale_rows, rows))
    descriptors = tuple(words[i:i + 2] for i in range(at, at + map_count * 2, 2))
    at += map_count * 2
    nodes = tuple(words[i:i + NODE_WIDTH] for i in range(at, at + node_count * NODE_WIDTH, NODE_WIDTH))
    at += node_count * NODE_WIDTH
    entries = tuple(words[i:i + ENTRY_WIDTH] for i in range(at, len(words), ENTRY_WIDTH))
    maps, offset = [], 0
    for start, length in descriptors:
        if start != offset or not length or length > entry_count - offset:
            raise ValueError('invalid timezone-name map span')
        values = entries[start:start + length]
        if any(a[:3] >= b[:3] for a, b in zip(values, values[1:])):
            raise ValueError('noncanonical timezone-name field ordering')
        for code, subtype, alt, text in values:
            if (code >= len(pool) or subtype >= len(TYPES) or alt >= len(pool) or
                    text != BLOCKED and (text >= len(pool) or pool[text] in (BLOCK, INHERIT))):
                raise ValueError('invalid timezone-name field reference/state')
        maps.append(values)
        offset += length
    if offset != entry_count or tuple(maps) != tuple(sorted(set(maps))):
        raise ValueError('noncanonical timezone-name maps/entries')
    depths, previous, used_maps, checked, decoded_nodes = [0], None, set(), set(), []
    for index, (origin, group, fields, parent) in enumerate(nodes, 1):
        if (origin >= len(pool) or locale_id(pool[origin]) != pool[origin] or group >= group_count or
                fields >= map_count or parent >= index):
            raise ValueError('invalid timezone-name node/reference/source identity')
        family = groups[group][0]
        if parent and semantic(groups[nodes[parent - 1][1]][0]) != semantic(family):
            raise ValueError('timezone-name parent has a different semantic class')
        depth = depths[parent] + 1
        order = depth, origin, group, fields, parent
        if previous is not None and order <= previous:
            raise ValueError('noncanonical timezone-name node order')
        depths.append(depth)
        previous = order
        used_maps.add(fields)
        if (fields, semantic(family)) not in checked:
            for code, subtype, alt, text in maps[fields]:
                field_valid(family, pool[code], TYPES[subtype], pool[alt], BLOCK if text == BLOCKED else pool[text])
            checked.add((fields, semantic(family)))
        decoded_nodes.append((pool[origin], groups[group], fields, parent))
    reachable, decoded_rows = set(), []
    for family, span, (locale_rows, rows) in zip(FAMILIES, spans, families):
        nonzero = []
        for row in rows:
            heads = []
            for group, head in zip(span, row):
                if head:
                    if semantic(groups[nodes[head - 1][1]][0]) != semantic(family):
                        raise ValueError('timezone-name head has a different semantic class')
                    heads.append((group, head))
                    reachable.add(head)
            nonzero.append(heads)
        # Expand only present rows for the host source view, not the implicit
        # locale/group cube. C opening/queries retain the physical tables.
        decoded_rows.extend((tag, groups[group], head) for tag, row in zip(tags, locale_rows)
                            for group, head in nonzero[row])
    for index in range(node_count, 0, -1):
        if index in reachable and nodes[index - 1][3]:
            reachable.add(nodes[index - 1][3])
    if reachable != set(range(1, node_count + 1)) or used_maps != set(range(map_count)):
        raise ValueError('unused timezone-name node/map')
    raw_maps = tuple(tuple(sorted((pool[code], TYPES[subtype], pool[alt], BLOCK if text == BLOCKED else pool[text])
                                  for code, subtype, alt, text in fields)) for fields in maps)
    return {'groups': tuple(sorted(groups)), 'maps': raw_maps, 'nodes': tuple(decoded_nodes),
            'rows': tuple(sorted(decoded_rows))}
