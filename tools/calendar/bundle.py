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

"""Endian-independent QJCA year and era records; no compression dependency."""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import re
import struct

HEADER = struct.Struct('>4s7I')
INDEX = struct.Struct('>IiIi4I')
YEAR = struct.Struct('>iIBBH')
ERA = struct.Struct('>ii32s')
KINDS = {'persian': 10, 'islamic-umalqura': 20, 'chinese': 30, 'dangi': 40,
         'japanese': 50}
NAMES = {v: k for k, v in KINDS.items()}


def checked_eras(rows):
    if not 1 <= len(rows) <= 1024:
        raise ValueError('invalid Japanese era count')
    previous, names = None, {'ce', 'bce', 'ad', 'bc'}
    for day, offset, name in rows:
        if (type(day) is not int or not -2147483648 <= day <= 2147483647 or
                type(offset) is not int or not -1000000 <= offset <= 1000000 or
                previous is not None and day <= previous or
                not isinstance(name, str) or
                not re.fullmatch(r'[a-z][a-z0-9-]{0,30}', name) or name in names):
            raise ValueError('invalid Japanese era record')
        previous = day
        names.add(name)


def checked_year(kind, months, leap):
    count = len(months)
    if kind == 10:
        valid = count == 12 and not leap and months[:11] == [31] * 6 + [30] * 5 and months[11] in (29, 30)
    else:
        valid = count in (12, 13) and all(m in (29, 30) for m in months)
        if kind == 20:
            valid = valid and count == 12 and not leap and sum(months) in (354, 355)
        else:
            valid = valid and ((count == 12 and not leap and 353 <= sum(months) <= 355) or
                              (count == 13 and 1 <= leap <= 12 and 383 <= sum(months) <= 385))
    if not valid:
        raise ValueError('invalid calendar year/month shape')


def pack(datasets):
    if not 1 <= len(datasets) <= len(KINDS):
        raise ValueError('invalid calendar dataset count')
    datasets = sorted(datasets, key=lambda d: KINDS[d['calendar']])
    names = [d['calendar'] for d in datasets]
    if len(set(names)) != len(names):
        raise ValueError('duplicate calendar dataset')
    string_start = HEADER.size + len(datasets) * INDEX.size
    strings = bytearray()
    for data in datasets:
        version = data['version'].encode('ascii')
        if not 1 <= len(version) <= 95 or any(c < 33 or c > 126 for c in version):
            raise ValueError('invalid calendar source version')
        strings.extend(version + b'\0')
    payload_start = (string_start + len(strings) + 3) & ~3
    strings.extend(bytes(payload_start - string_start - len(strings)))
    index, payload = bytearray(), bytearray()
    version_at = string_start
    for data in datasets:
        kind = KINDS[data['calendar']]
        if kind == 50:
            rows = data['eras']
            checked_eras(rows)
            offset = payload_start + len(payload)
            for day, year_offset, name in rows:
                payload.extend(ERA.pack(day, year_offset, name.encode('ascii')))
            index.extend(INDEX.pack(kind, 0, len(rows), 0, offset,
                                    len(rows) * ERA.size, version_at, 0))
            version_at += len(data['version'].encode('ascii')) + 1
            continue
        rows = data['years']
        if not rows or len(rows) > 10000 or not -1000000 <= data['first_year'] <= 1000000 - len(rows) + 1:
            raise ValueError('invalid calendar year range')
        offset = payload_start + len(payload)
        for day, months, leap in rows:
            checked_year(kind, months, leap)
            bits = sum((m - 29) << (2 * i) for i, m in enumerate(months))
            payload.extend(YEAR.pack(day, bits, len(months), leap, 0))
        index.extend(INDEX.pack(kind, data['first_year'], len(rows), data['end_epoch_day'],
                                offset, len(rows) * YEAR.size, version_at, 0))
        version_at += len(data['version'].encode('ascii')) + 1
    blob = HEADER.pack(b'QJCA', 2, payload_start + len(payload), len(datasets),
                       HEADER.size, string_start, payload_start, 1) + index + strings + payload
    unpack(blob)
    return blob


def unpack(blob):
    if len(blob) < HEADER.size or len(blob) > 1024 * 1024:
        raise ValueError('invalid calendar pack size')
    magic, version, size, count, index_at, strings_at, payload_at, algorithm = HEADER.unpack_from(blob)
    if (magic, version, algorithm) != (b'QJCA', 2, 1) or size != len(blob):
        raise ValueError('invalid calendar pack header')
    if not 1 <= count <= len(KINDS) or index_at != HEADER.size or strings_at != index_at + count * INDEX.size:
        raise ValueError('invalid calendar pack index')
    if not strings_at <= payload_at <= size or payload_at % 4:
        raise ValueError('invalid calendar pack sections')
    next_string, next_payload, previous = strings_at, payload_at, 0
    datasets = []
    for i in range(count):
        kind, first, years, end_day, offset, length, source, reserved = INDEX.unpack_from(blob, index_at + i * INDEX.size)
        if kind not in NAMES or kind <= previous or reserved:
            raise ValueError('invalid calendar kind or reserved field')
        if (not 1 <= years <= (1024 if kind == 50 else 10000) or
                (kind == 50 and (first or end_day)) or
                (kind != 50 and not -1000000 <= first <= 1000000 - years + 1)):
            raise ValueError('invalid calendar year range')
        record_size = ERA.size if kind == 50 else YEAR.size
        if length != years * record_size or offset != next_payload or length > size - offset:
            raise ValueError('invalid calendar payload extent')
        if source != next_string or source >= payload_at:
            raise ValueError('invalid calendar source string offset')
        end = blob.find(b'\0', source, payload_at)
        if end < source + 1 or end > source + 95 or any(c < 33 or c > 126 for c in blob[source:end]):
            raise ValueError('invalid calendar source string')
        version_text = blob[source:end].decode('ascii')
        next_string = end + 1
        if kind == 50:
            rows = []
            for n in range(years):
                day, year_offset, raw_name = ERA.unpack_from(blob, offset + n * ERA.size)
                zero = raw_name.find(b'\0')
                if zero < 0 or any(raw_name[zero:]):
                    raise ValueError('invalid Japanese era name storage')
                try:
                    name = raw_name[:zero].decode('ascii')
                except UnicodeDecodeError:
                    raise ValueError('invalid Japanese era name')
                rows.append((day, year_offset, name))
            checked_eras(rows)
            datasets.append({'calendar': 'japanese', 'version': version_text,
                             'eras': rows})
            next_payload += length
            previous = kind
            continue
        rows = []
        expected = None
        for n in range(years):
            day, bits, months, leap, padding = YEAR.unpack_from(blob, offset + n * YEAR.size)
            if months not in (12, 13) or bits >> (2 * months) or padding:
                raise ValueError('invalid calendar year encoding')
            lengths = [29 + ((bits >> (2 * m)) & 3) for m in range(months)]
            checked_year(kind, lengths, leap)
            if expected is not None and day != expected:
                raise ValueError('noncontiguous calendar years')
            expected = day + sum(lengths)
            rows.append((day, lengths, leap))
        if expected != end_day:
            raise ValueError('calendar terminal boundary mismatch')
        datasets.append({'calendar': NAMES[kind], 'first_year': first,
                         'end_epoch_day': end_day, 'version': version_text, 'years': rows})
        next_payload += length
        previous = kind
    if next_payload != size or payload_at - next_string > 3 or any(blob[next_string:payload_at]):
        raise ValueError('unclaimed calendar bytes')
    return datasets
