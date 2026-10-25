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

"""Deduplicated relative-time source maps in QJIN section 23."""
import struct

import bundle
from parents import locale_id
from resolve import BLOCK, INHERIT
import relative_patterns as source

VERSION, HEADER, ROW_WIDTH, ENTRY_WIDTH = 1, 5, 24, 4
BLOCKED = 0xfffffffe


def select(data, tags):
    selected = set(tags)
    rows = tuple(row for row in data['rows'] if row[0] in selected)
    used = sorted({row[-1] for row in rows})
    if any(type(index) is not int or not 0 <= index < len(data['maps']) for index in used):
        raise ValueError('invalid relative source map reference')
    indices = {old: new for new, old in enumerate(used)}
    maps = tuple(data['maps'][index] for index in used)
    keys = {selector for entries in maps for kind, selector, *rest in entries if kind == 'literal'}
    return dict(data, maps=maps, literal_keys=tuple(sorted(keys)),
                rows=tuple(row[:-1] + (indices[row[-1]],) for row in rows))


def strings(data):
    return {''} | set(data['literal_keys']) | {
        value for entries in data['maps'] for kind, selector, blocked, text, origin, unit, width in entries
        for value in (text, origin)}


def entry_valid(kind, selector, blocked, text, origin, unit, width):
    if (type(blocked) is not bool or not isinstance(text, str) or
            text in (BLOCK, INHERIT) or blocked and text or
            not isinstance(origin, str) or locale_id(origin) != origin or
            unit not in source.UNITS or width not in source.WIDTHS):
        raise ValueError('invalid relative source form/state/origin')
    if kind == 'literal':
        if not isinstance(selector, str):
            raise ValueError('invalid relative literal key type')
        source.literal_key(selector)
    elif kind not in source.TENSES or selector not in source.COUNTS:
        raise ValueError('invalid relative source selector')
    elif not blocked:
        source.numeric_pattern(text)


def required(entries):
    values = {(kind, selector): blocked for kind, selector, blocked, *rest in entries}
    if any(values.get((tense, 'other'), True) for tense in source.TENSES):
        raise ValueError('missing/blocked required relative long form')


def encode(data, tags, ids):
    rows = {row[:-1]: row[-1] for row in data['rows']}
    keys = data['literal_keys']
    if (data['units'] != source.UNITS or data['widths'] != source.WIDTHS or
            data['counts'] != source.COUNTS or keys != tuple(sorted(set(keys))) or
            not tags or len(set(tags)) != len(tags) or len(rows) != len(data['rows']) or
            set(rows) != {(tag, unit, width) for tag in tags for unit in source.UNITS
                         for width in source.WIDTHS} or
            any(type(index) is not int or not 0 <= index < len(data['maps']) for index in rows.values()) or
            set(rows.values()) != set(range(len(data['maps'])))):
        raise ValueError('invalid relative source inventory/coverage')
    for key in keys:
        source.literal_key(key)
    key_ids = {key: i + 12 for i, key in enumerate(keys)}
    compiled, used_keys = [], set()
    for entries in data['maps']:
        values, seen = [], set()
        for entry in entries:
            kind, selector, blocked, text, origin, unit, width = entry
            entry_valid(*entry)
            if kind == 'literal':
                if selector not in key_ids:
                    raise ValueError('unknown relative literal key')
                code = key_ids[selector]
                used_keys.add(selector)
            else:
                code = source.TENSES.index(kind) * 6 + source.COUNTS.index(selector)
            if code in seen:
                raise ValueError('repeated relative source selector')
            seen.add(code)
            values.append((code, BLOCKED if blocked else ids[text], ids[origin],
                           source.UNITS.index(unit) * 3 + source.WIDTHS.index(width)))
        compiled.append(tuple(sorted(values)))
    if used_keys != set(keys):
        raise ValueError('unused relative literal key')
    for (tag, unit, width), index in rows.items():
        if width == 'long':
            required(data['maps'][index])
    maps = tuple(sorted(set(compiled)))
    map_ids = {value: index for index, value in enumerate(maps)}
    locale_rows = tuple(tuple(map_ids[compiled[rows[tag, unit, width]]]
                              for unit in source.UNITS for width in source.WIDTHS) for tag in tags)
    shared = tuple(sorted(set(locale_rows)))
    row_ids = {row: i for i, row in enumerate(shared)}
    words = [VERSION, len(keys), len(shared), len(maps), sum(map(len, maps))]
    words.extend(ids[key] for key in keys)
    words.extend(row_ids[row] for row in locale_rows)
    words.extend(value for row in shared for value in row)
    offset = 0
    for entries in maps:
        words.extend((offset, len(entries)))
        offset += len(entries)
    words.extend(value for entries in maps for entry in entries for value in entry)
    return len(tags), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool, tags):
    if section is None:
        return None
    count, raw = section
    if count != len(tags) or not count or len(raw) < HEADER * 4 or len(raw) % 4:
        raise ValueError('invalid relative locale count/extent')
    words = tuple(item[0] for item in struct.iter_unpack('<I', raw))
    version, key_count, row_count, map_count, entry_count = words[:HEADER]
    expected = HEADER + key_count + count + row_count * ROW_WIDTH + map_count * 2 + entry_count * ENTRY_WIDTH
    if (version != VERSION or not 1 <= row_count <= count or
            not 1 <= map_count <= row_count * ROW_WIDTH or
            len(words) != expected or not pool or pool[0] != ''):
        raise ValueError('invalid relative version/dimensions')
    at = HEADER
    key_refs = words[at:at + key_count]
    if (any(index >= len(pool) for index in key_refs) or
            key_refs != tuple(sorted(set(key_refs)))):
        raise ValueError('invalid/noncanonical relative literal keys')
    keys = tuple(pool[index] for index in key_refs)
    for key in keys:
        source.literal_key(key)
    at += key_count
    locales = words[at:at + count]
    if set(locales) != set(range(row_count)):
        raise ValueError('invalid/unused relative locale row')
    at += count
    rows = tuple(words[start:start + ROW_WIDTH] for start in range(at, at + row_count * ROW_WIDTH, ROW_WIDTH))
    if (rows != tuple(sorted(set(rows))) or
            {index for row in rows for index in row} != set(range(map_count))):
        raise ValueError('invalid/noncanonical relative locale rows')
    at += row_count * ROW_WIDTH
    descriptors = words[at:at + map_count * 2]
    at += map_count * 2
    maps, previous, end, used_keys = [], None, 0, set()
    for i in range(map_count):
        start, length = descriptors[i * 2:i * 2 + 2]
        if start != end or length > entry_count - start or length > 12 + key_count:
            raise ValueError('invalid relative map extent')
        end += length
        entries, compiled, last = [], [], -1
        for position in range(at + start * ENTRY_WIDTH, at + end * ENTRY_WIDTH, ENTRY_WIDTH):
            code, text, origin, group = words[position:position + ENTRY_WIDTH]
            if (not last < code < 12 + key_count or group >= ROW_WIDTH or origin >= len(pool) or
                    text != BLOCKED and text >= len(pool)):
                raise ValueError('invalid relative form reference/selector')
            kind = source.TENSES[code // 6] if code < 12 else 'literal'
            selector = source.COUNTS[code % 6] if code < 12 else keys[code - 12]
            if code >= 12:
                used_keys.add(code - 12)
            entry = (kind, selector, text == BLOCKED, '' if text == BLOCKED else pool[text],
                     pool[origin], source.UNITS[group // 3], source.WIDTHS[group % 3])
            entry_valid(*entry)
            entries.append(entry)
            compiled.append((code, text, origin, group))
            last = code
        vector = tuple(compiled)
        if previous is not None and vector <= previous:
            raise ValueError('noncanonical relative map order')
        previous = vector
        maps.append(tuple(sorted(entries)))
    if end != entry_count or used_keys != set(range(key_count)):
        raise ValueError('unused relative entries/literal keys')
    # Return source-order maps so authored roundtrips retain their readable form.
    ordered = tuple(sorted(maps))
    indices = {value: i for i, value in enumerate(ordered)}
    remap = {i: indices[value] for i, value in enumerate(maps)}
    result_rows = tuple((tag, unit, width, remap[rows[row][u * 3 + w]])
                        for tag, row in zip(tags, locales)
                        for u, unit in enumerate(source.UNITS) for w, width in enumerate(source.WIDTHS))
    for tag, unit, width, index in result_rows:
        if width == 'long':
            required(ordered[index])
    return {'units': source.UNITS, 'widths': source.WIDTHS, 'counts': source.COUNTS,
            'literal_keys': keys, 'maps': ordered, 'rows': result_rows}
