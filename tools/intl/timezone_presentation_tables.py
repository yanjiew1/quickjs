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

"""Global supplemental presentation records in optional QJIN section 33.

Finite wire timestamps use the full signed 64-bit seconds domain. The source
collector separately limits textual timestamps to its reviewed Gregorian grammar.
Release metadata is provenance, not part of runtime presentation records.
"""
import re
import struct

import bundle
from timezone_syntax import ZONE, METAZONE

VERSION, HEADER, ID_WIDTH, ZONE_WIDTH, PERIOD_WIDTH = 1, 5, 2, 3, 8
START, END, STANDARD, DAYLIGHT = 1, 2, 4, 8
REGION = re.compile(r'[A-Z]{2}|[0-9]{3}')


def runtime_records(data):
    """Project typed source columns, retaining release facts in provenance."""
    result = {key: data[key] for key in ('zones', 'periods', 'preferred', 'primary')}
    result['ids'] = tuple((short, long) for short, long, since in data['ids'])
    return result


def strings(data):
    values = {''} | set(data['zones'])
    values.update(text for record in data['ids'] for text in record)
    values.update(territory for metazone, territory, zone in data['preferred'])
    values.update(text for record in data['primary'] for text in record)
    return values


def records(data):
    """Validate semantic records; canonical ordering is the encoder's job."""
    for key, width in (('periods', 6), ('preferred', 3), ('primary', 2), ('ids', 2)):
        if (not isinstance(data[key], tuple) or
                any(not isinstance(row, tuple) or len(row) != width for row in data[key])):
            raise ValueError('invalid timezone-presentation record shape')
    zones = data['zones']
    if (not isinstance(zones, tuple) or any(not isinstance(zone, str) or not ZONE.fullmatch(zone) for zone in zones) or
            len(set(zones)) != len(zones)):
        raise ValueError('invalid timezone-presentation zone inventory')
    ids, long_ids = {}, set()
    for short, long in data['ids']:
        if (not isinstance(short, str) or not re.fullmatch('[a-z]{4}', short) or not isinstance(long, str) or
                not METAZONE.fullmatch(long) or short in ids or long in long_ids):
            raise ValueError('invalid/repeated timezone-presentation identity')
        ids[short] = long
        long_ids.add(long)
    periods = dict((zone, []) for zone in zones)
    membership = set()
    for zone, start, end, metazone, standard, daylight in data['periods']:
        if not isinstance(zone, str) or not isinstance(metazone, str) or zone not in periods or metazone not in long_ids:
            raise ValueError('invalid timezone-presentation period reference')
        for value in (start, end):
            if value is not None and (type(value) is not int or not -(1 << 63) <= value < 1 << 63):
                raise ValueError('invalid timezone-presentation timestamp')
        for value in (standard, daylight):
            if value is not None and (type(value) is not int or not -86340 <= value <= 86340 or value % 60):
                raise ValueError('invalid timezone-presentation offset')
        if start is not None and end is not None and start >= end:
            raise ValueError('empty timezone-presentation period')
        periods[zone].append((start, end, metazone, standard, daylight))
        membership.add((zone, metazone))
    for values in periods.values():
        values.sort(key=lambda row: (row[0] is not None, row[0] or 0))
        for previous, current in zip(values, values[1:]):
            if previous[1] is None or current[0] is None or previous[1] > current[0]:
                raise ValueError('overlapping timezone-presentation periods')
    preferred = {}
    for metazone, territory, zone in data['preferred']:
        if (not isinstance(metazone, str) or not isinstance(zone, str) or
                metazone not in long_ids or not isinstance(territory, str) or not REGION.fullmatch(territory) or
                (zone, metazone) not in membership or (metazone, territory) in preferred):
            raise ValueError('invalid/repeated timezone-presentation preferred mapping')
        preferred[metazone, territory] = zone
    primary = {}
    for territory, zone in data['primary']:
        if (not isinstance(territory, str) or not REGION.fullmatch(territory) or not isinstance(zone, str) or
                not ZONE.fullmatch(zone) or territory in primary):
            raise ValueError('invalid/repeated timezone-presentation dominant mapping')
        primary[territory] = zone
    return periods, ids, preferred, primary


def signed_words(value):
    bits = value & ((1 << 64) - 1)
    return bits & 0xffffffff, bits >> 32


def encode(data, ids):
    periods, identities, preferred, primary = records(data)
    if ids.get('') != 0:
        raise ValueError('timezone-presentation string zero must be empty')
    id_rows = tuple(sorted((ids[long], ids[short], long) for short, long in identities.items()))
    mzone_ids = dict((long, index) for index, (_, _, long) in enumerate(id_rows))
    zone_rows = tuple(sorted((ids[zone], zone) for zone in periods))
    zone_ids = dict((zone, index) for index, (_, zone) in enumerate(zone_rows))
    descriptors, encoded_periods = [], []
    for ref, zone in zone_rows:
        values = periods[zone]
        descriptors.append((ref, len(encoded_periods), len(values)))
        for start, end, metazone, standard, daylight in values:
            flags = (START if start is not None else 0) | (END if end is not None else 0)
            flags |= (STANDARD if standard is not None else 0) | (DAYLIGHT if daylight is not None else 0)
            encoded_periods.append(signed_words(start or 0) + signed_words(end or 0) +
                                   (mzone_ids[metazone], flags, (standard or 0) & 0xffffffff,
                                    (daylight or 0) & 0xffffffff))
    preferred_rows = tuple(sorted((mzone_ids[metazone], ids[territory], zone_ids[zone])
                                 for (metazone, territory), zone in preferred.items()))
    primary_rows = tuple(sorted((ids[territory], ids[zone]) for territory, zone in primary.items()))
    words = [VERSION, len(encoded_periods), len(preferred_rows), len(primary_rows), len(id_rows)]
    for rows in (tuple(row[:2] for row in id_rows), descriptors, encoded_periods, preferred_rows, primary_rows):
        words.extend(value for row in rows for value in row)
    if len(words) > bundle.MAX_SIZE // 4:
        raise ValueError('timezone-presentation section exceeds size limit')
    return len(zone_rows), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def signed(value, bits):
    return value if value < 1 << (bits - 1) else value - (1 << bits)


def decode(section, pool):
    if section is None:
        return None
    count, raw = section
    if (not HEADER * 4 <= len(raw) <= bundle.MAX_SIZE or len(raw) % 4 or not pool or pool[0] != '' or
            type(count) is not int or not 0 <= count <= 0xffffffff):
        raise ValueError('invalid timezone-presentation extent/count')
    version, period_count, preferred_count, primary_count, id_count = struct.unpack_from('<5I', raw)
    if (version != VERSION or len(raw) // 4 != HEADER + id_count * ID_WIDTH + count * ZONE_WIDTH +
            period_count * PERIOD_WIDTH + preferred_count * 3 + primary_count * 2):
        raise ValueError('invalid timezone-presentation version/dimensions')
    words = tuple(value[0] for value in struct.iter_unpack('<I', raw))
    at = HEADER

    def take(length, width):
        nonlocal at
        result = tuple(words[i:i + width] for i in range(at, at + length * width, width))
        at += length * width
        return result

    identities = take(id_count, ID_WIDTH)
    zones = take(count, ZONE_WIDTH)
    periods = take(period_count, PERIOD_WIDTH)
    preferred = take(preferred_count, 3)
    primary = take(primary_count, 2)

    def text(index):
        if index >= len(pool):
            raise ValueError('invalid timezone-presentation string reference')
        return pool[index]

    id_names = tuple((text(short), text(long)) for long, short in identities)
    zone_names = tuple(text(ref) for ref, start, length in zones)
    values, offset = [], 0
    for zone, (ref, start, length) in zip(zone_names, zones):
        if start != offset or length > period_count - offset:
            raise ValueError('invalid timezone-presentation period span')
        for lo, hi, end_lo, end_hi, metazone, flags, standard, daylight in periods[start:start + length]:
            if (metazone >= id_count or flags & ~15 or not flags & START and (lo or hi) or
                    not flags & END and (end_lo or end_hi) or not flags & STANDARD and standard or
                    not flags & DAYLIGHT and daylight):
                raise ValueError('invalid timezone-presentation flags/reference/state')
            values.append((zone, signed(lo | hi << 32, 64) if flags & START else None,
                           signed(end_lo | end_hi << 32, 64) if flags & END else None, id_names[metazone][1],
                           signed(standard, 32) if flags & STANDARD else None,
                           signed(daylight, 32) if flags & DAYLIGHT else None))
        offset += length
    if offset != period_count:
        raise ValueError('unused timezone-presentation period')
    preferred_names = []
    for metazone, territory, zone in preferred:
        if metazone >= id_count or zone >= count:
            raise ValueError('invalid timezone-presentation preferred reference')
        preferred_names.append((id_names[metazone][1], text(territory), zone_names[zone]))
    data = {'zones': tuple(sorted(zone_names)), 'ids': tuple(sorted(id_names)), 'periods': tuple(values),
            'preferred': tuple(sorted(preferred_names)),
            'primary': tuple(sorted((text(territory), text(zone)) for territory, zone in primary))}
    # The small global tables have no implicit locale cube. Re-encoding checks
    # exact canonical physical order and all semantic relations in one place.
    if encode(data, dict((text, index) for index, text in enumerate(pool))) != section:
        raise ValueError('noncanonical timezone-presentation tables')
    return data
