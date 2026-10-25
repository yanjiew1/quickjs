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

"""Canonical raw era mappings in optional QJIN section 35."""
import struct

import bundle
import calendar_eras as source


def strings(data):
    return {''} | {value for row in source.records(data) for value in row}


def encode(data, ids):
    rows = source.records(data)
    if ids.get('') != 0 or len(rows) > (bundle.MAX_SIZE // 4 - 1) // 3:
        raise ValueError('invalid calendar-era pool/extent')
    words = [1]
    words.extend(ids[value] for row in rows for value in row)
    return len(rows), b''.join(struct.pack('<I', bundle.uint32(value)) for value in words)


def decode(section, pool):
    if section is None:
        return None
    count, raw = section
    if (type(count) is not int or not 0 <= count <= 0xffffffff or
            not isinstance(raw, bytes) or not 4 <= len(raw) <= bundle.MAX_SIZE or
            len(raw) != 4 + count * 12 or struct.unpack_from('<I', raw)[0] != 1 or
            not pool or pool[0] != ''):
        raise ValueError('invalid calendar-era version/extent')
    rows = []
    for row in struct.iter_unpack('<3I', raw[4:]):
        if any(index >= len(pool) for index in row):
            raise ValueError('invalid calendar-era string reference')
        rows.append(tuple(pool[index] for index in row))
    result = {'rows': tuple(rows)}
    if encode(result, {value: index for index, value in enumerate(pool)}) != section:
        raise ValueError('noncanonical calendar-era tables')
    return result
