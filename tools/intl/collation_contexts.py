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

"""Preserve CLDR root prefix contexts without duplicating its CE/key pools."""
import re
import struct

import bundle
import collation_root
from cldr_xml import DataError

SOURCE = 'common/uca/FractionalUCA.txt'
HEADER = struct.Struct('<2I')
ROW = struct.Struct('<3I')
FRACTIONAL_WEIGHT = r'[ \t]*(?:[0-9A-Fa-f]{2}(?:[ \t]+[0-9A-Fa-f]{2})*)?[ \t]*'
FRACTIONAL = re.compile(r'\[' + ','.join([FRACTIONAL_WEIGHT] * 3) + r'\]')


def parse(raw, root):
    """Supplement allkeys with prefix identity, not fractional weight encoding.

    allkeys expresses these mappings as the CEs for prefix+suffix together.
    FractionalUCA identifies which are actually prefix rules. Verify the
    interchangeable allkeys form begins with the prefix's own complete CEs;
    the remainder is the contextual suffix mapping. Never zip CE vectors
    between fractional and UCA encodings: their expansion lengths can differ.
    """
    try:
        text = raw.decode('utf-8')
    except (AttributeError, UnicodeDecodeError) as error:
        raise DataError('invalid fractional collation text: ' + str(error))
    rows = dict(root['rows'])
    indices = {key: index for index, (key, elements) in enumerate(root['rows'])}
    result, seen, used_sources = [], set(), set()
    version = None
    for number, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.split('#', 1)[0].strip(' \t')

        def fail(message):
            raise DataError('{}:{}: {}'.format(SOURCE, number, message))

        if line.startswith('[UCA version'):
            if version is not None or line != '[UCA version = {}]'.format(root['version']):
                fail('repeated or incompatible fractional UCA version')
            version = root['version']
            continue
        if '|' not in line:
            continue
        if version is None or line.count('|') != 1 or line.count(';') != 1:
            fail('invalid root prefix record')
        key, weights = line.split(';')
        before, suffix = (field.strip(' \t') for field in key.split('|'))
        if not collation_root.KEY.fullmatch(before) or not collation_root.KEY.fullmatch(suffix):
            fail('invalid prefix/suffix scalar sequence')
        before, suffix = (tuple(int(c, 16) for c in field.split()) for field in (before, suffix))
        if any(c > 0x10ffff or 0xd800 <= c <= 0xdfff for c in before + suffix):
            fail('prefix is not a scalar sequence')
        if (before, suffix) in seen:
            fail('repeated prefix mapping')
        seen.add((before, suffix))
        # Validate the fractional CE syntax, but use allkeys for UCA weights.
        at, count = 0, 0
        while at < len(weights):
            if weights[at] in ' \t':
                at += 1
                continue
            match = FRACTIONAL.match(weights, at)
            if match is None:
                fail('invalid fractional prefix elements')
            at = match.end()
            count += 1
        if not count:
            fail('empty fractional prefix mapping')
        combined = rows.get(before + suffix)
        prefix = rows.get(before)
        if (combined is None or prefix is None or len(combined) <= len(prefix) or
                combined[:len(prefix)] != prefix):
            fail('prefix mapping has no equivalent allkeys decomposition')
        source_index = indices[before + suffix]
        if source_index in used_sources:
            fail('multiple context splits for one allkeys mapping')
        used_sources.add(source_index)
        result.append((source_index, len(before), len(prefix)))
    if version is None:
        raise DataError('fractional source lacks UCA version')
    return tuple(sorted(result))


def collect(sources, root):
    return parse(sources.read(SOURCE), root)


def validate(rows, root):
    """Check metadata against a parsed/decoded root; preserve its row identity.

    The root codec sorts rows when encoding, so reject noncanonical input here
    instead of creating cross-references which would change meaning on write.
    """
    if (not isinstance(rows, (tuple, list)) or not isinstance(root, dict) or
            root.get('version') != collation_root.VERSION):
        raise ValueError('invalid collation contexts/root')
    mappings = root['rows']
    if any(a[0] >= b[0] for a, b in zip(mappings, mappings[1:])):
        raise ValueError('context root mappings are not in canonical key order')
    by_key = dict(mappings)
    previous = -1
    for row in rows:
        if (not isinstance(row, (tuple, list)) or len(row) != 3 or
                any(type(value) is not int or not 0 <= value <= 0xffffffff for value in row)):
            raise ValueError('invalid collation context record')
        index, prefix_length, skip = row
        if not previous < index < len(mappings):
            raise ValueError('unordered, repeated or missing context source')
        key, elements = mappings[index]
        if not 0 < prefix_length < len(key) or not 0 < skip < len(elements):
            raise ValueError('invalid context split')
        if by_key.get(key[:prefix_length]) != elements[:skip]:
            raise ValueError('context does not preserve prefix elements')
        previous = index
    return tuple(tuple(row) for row in rows)


def encode(rows, root):
    rows = validate(rows, root)
    if len(rows) > (bundle.MAX_SIZE - HEADER.size) // ROW.size:
        raise ValueError('oversized collation contexts')
    return len(rows), HEADER.pack(1, 18 << 16) + b''.join(ROW.pack(*row) for row in rows)


def decode(section, root):
    if section is None:
        return None
    count, raw = section
    if (type(count) is not int or not 0 <= count <= 0xffffffff or
            not isinstance(raw, bytes) or not HEADER.size <= len(raw) <= bundle.MAX_SIZE or
            HEADER.unpack_from(raw) != (1, 18 << 16) or
            len(raw) != HEADER.size + count * ROW.size):
        raise ValueError('invalid collation contexts extent/version')
    return validate(tuple(ROW.iter_unpack(raw[HEADER.size:])), root)
