#!/usr/bin/env python3
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

"""Validate a QJTZ pack and emit its sole C owner and immutable rule views."""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

from acquire import atomic_write
from bundle import HEADER, INDEX, unpack


WRAPPER_LICENSE = """\
/*
 * Generated C wrapper; embedded data retains its documented source licenses.
 *
 * Copyright (c) 2026 Yan-Jie Wang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

"""


def render(binary, records, spans, descriptors):
    """Render records already checked by unpack() against these binary bytes."""
    if len(spans) != len(descriptors):
        raise ValueError('incomplete C reader result')
    states = dict(zip(spans, descriptors))
    if len(states) != len(spans):
        raise ValueError('duplicate descriptor extent')
    utc = [row for row in records if row['identifier'].lower() == 'utc']
    if len(utc) != 1 or utc[0]['identifier'] != 'UTC' or utc[0]['primary'] != 'UTC':
        raise ValueError('core requires canonical UTC resolving to itself')
    # Every payload, including aliases of core UTC, has been checked by the
    # same C reader that serves native queries. Production zic input must
    # retain its footer rather than silently extending a truncated ending.
    if any(not state['future'] for state in descriptors):
        raise ValueError('production timezone payload lacks a complete footer')
    for row in records:
        if row['primary'] != 'UTC':
            continue
        state = states[(row['offset'], row['length'])]
        types = state['types']
        if state['future'] != 1 or state['standard_offset'] or any(
                binary[types + i * 6:types + i * 6 + 5] != bytes(5)
                for i in range(state['type_count'])):
            raise ValueError('UTC payload must have zero offsets and no DST')
    named = [row for row in records if row['identifier'] != 'UTC']
    used = sorted({(r['offset'], r['length']) for r in named if r['primary'] != 'UTC'})
    indices = {span: index for index, span in enumerate(used)}
    lines = [
        '/* Generated from the complete QJTZ pack; do not edit. */',
        '#include "src/timezone/timezone-native.h"',
        '', 'const unsigned char qjs_native_timezone_data[] = {',
    ]
    for start in range(0, len(binary), 16):
        lines.append('    ' + ', '.join('0x%02x' % b for b in binary[start:start + 16]) + ',')
    lines += ['};', 'const size_t qjs_native_timezone_data_size =',
              '    sizeof(qjs_native_timezone_data);', '']
    if used:
        lines.append('static const QJSTimeZoneRules rules[] = {')
        for span in used:
            state = states[span]
            lines.append('    { .tzif = {')
            for field in ('times', 'indices', 'types', 'characters'):
                lines.append('        .%s = qjs_native_timezone_data + %d,' % (field, state[field]))
            for field in ('time_count', 'type_count', 'character_count', 'future',
                          'standard_offset', 'daylight_offset'):
                lines.append('        .%s = %d,' % (field, state[field]))
            for field in ('start', 'end'):
                lines.append('        .%s = { %s },' %
                             (field, ', '.join(str(v) for v in state[field])))
            lines.append('    } },')
        lines += ['};', '']
    lines.append('const QJSTimeZone qjs_native_timezone_zones[] = {')
    for row in named:
        rule = 'NULL' if row['primary'] == 'UTC' else '&rules[%d]' % indices[(row['offset'], row['length'])]
        lines += ['    {',
            '        (const char *)qjs_native_timezone_data + %d,' % row['identifier_offset'],
            '        (const char *)qjs_native_timezone_data + %d,' % row['primary_offset'],
            '        %s,' % rule, '    },']
    if not named:
        lines.append('    { NULL, NULL, NULL }, /* Portable empty-backend storage. */')
    lines += ['};', 'const size_t qjs_native_timezone_zone_count = %d;' % len(named), '',
              'const uint16_t qjs_native_timezone_lookup_order[] = {']
    folded = sorted(range(len(named)), key=lambda i: named[i]['identifier'].lower())
    for start in range(0, len(folded), 16):
        lines.append('    ' + ', '.join(str(i) for i in folded[start:start + 16]) + ',')
    if not folded:
        lines.append('    0,')
    version_at = HEADER.size + len(records) * INDEX.size
    lines += ['};', 'const char *const qjs_native_timezone_data_version =',
              '    (const char *)qjs_native_timezone_data + %d;' % version_at, '']
    return WRAPPER_LICENSE + '\n'.join(lines)


def generate(input_path, output_path, describe_path):
    binary = input_path.read_bytes()
    contents = unpack(binary)
    spans = sorted({(r['offset'], r['length']) for r in contents['records']})
    request = ''.join('%d:%d\n' % row for row in spans).encode('ascii')
    # Descriptors and embedded bytes must come from the same input snapshot,
    # even if the caller replaces its source pack while generation is running.
    with tempfile.TemporaryDirectory(prefix='timezone-embed-') as directory:
        snapshot = Path(directory) / 'input.bin'
        snapshot.write_bytes(binary)
        result = subprocess.run([str(describe_path.resolve()), str(snapshot)],
                                input=request, stdout=subprocess.PIPE, check=True, timeout=60)
    source = render(binary, contents['records'], spans, json.loads(result.stdout))
    atomic_write(output_path, source.encode('ascii'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--describe', type=Path, required=True)
    args = parser.parse_args()
    generate(args.input, args.output, args.describe)


if __name__ == '__main__':
    main()
