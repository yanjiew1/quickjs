#!/usr/bin/env python3
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

"""Counted tailoring IR for the host C compiler, not a runtime pack section."""
import argparse
import struct
from pathlib import Path

import acquire
from cldr_xml import DataError
import collation_programs
import collation_bindings
import collation_rules as rules

MAGIC = b'QJCI'
VERSION = 2
UNICODE_VERSION = 18 << 16
MAX_SIZE = 64 * 1024 * 1024
HEADER = struct.Struct('<4s4I32s')
PROGRAM = struct.Struct('<5I')
NODE = struct.Struct('<6I')
U32 = struct.Struct('<I')
KINDS = {rules.Reset: 1, rules.Relation: 2, rules.Starred: 3,
         rules.Setting: 4, rules.SuppressContractions: 5, rules.Optimize: 6}


def integer(value):
    if type(value) is not int or not 0 <= value <= 0xffffffff:
        raise DataError('collation IR integer outside uint32')
    return value


def text(value):
    if (not isinstance(value, str) or not value or
            any(ord(c) < 0x20 or ord(c) == 0x7f for c in value)):
        raise DataError('invalid collation IR label')
    return value.encode('utf-8')


def sequence(values):
    values = tuple(values)
    if len(values) > (MAX_SIZE - U32.size) // U32.size or any(
           type(c) is not int or not 0 <= c <= 0x10ffff or 0xd800 <= c <= 0xdfff
           for c in values):
        raise DataError('collation IR sequence contains a non-scalar')
    return U32.pack(integer(len(values))) + b''.join(U32.pack(c) for c in values)


def encode_node(node):
    kind = KINDS.get(type(node))
    if kind is None:
        raise DataError('unexpanded or unknown collation IR instruction')
    auxiliary, fields = 0, [(), (), ()]
    if isinstance(node, rules.Reset):
        auxiliary = node.before
        fields = [node.text, tuple(map(ord, node.special)), ()]
        if auxiliary not in range(4) or bool(node.text) == bool(node.special):
            raise DataError('invalid collation IR reset')
    elif isinstance(node, rules.Relation):
        auxiliary, fields = node.strength, [node.prefix, node.text, node.extension]
        if auxiliary not in range(5) or not node.text:
            raise DataError('invalid collation IR relation')
    elif isinstance(node, (rules.Starred, rules.SuppressContractions, rules.Optimize)):
        if isinstance(node, rules.Starred):
            auxiliary = node.strength
            if auxiliary not in range(5):
                raise DataError('invalid collation IR starred strength')
        for first, last in node.ranges:
            if not first <= last or first <= 0xdfff and last >= 0xd800:
                raise DataError('invalid collation IR ranges')
        fields[0] = tuple(c for pair in node.ranges for c in pair)
        if isinstance(node, rules.Starred) and not fields[0]:
            raise DataError('empty collation IR ranges')
    elif isinstance(node, rules.Setting):
        fields = [tuple(map(ord, node.name)),
                  tuple(map(ord, '\0'.join(node.values) + '\0')), ()]
        if not node.values:
            raise DataError('empty collation IR setting')
    source = text(node.position.source)
    if not integer(node.position.line) or not integer(node.position.column):
        raise DataError('invalid collation IR source position')
    body = source + b''.join(sequence(field) for field in fields)
    size = NODE.size + len(body)
    return NODE.pack(integer(size), kind, integer(auxiliary),
                     integer(node.position.line), integer(node.position.column),
                     integer(len(source))) + body


def encode(programs, fingerprint):
    """Programs are already import-expanded; order and provenance stay intact.

    Records carry three scalar vectors. Reset uses text/special/empty;
    relation uses prefix/text/extension; range operations use inclusive pairs;
    setting uses name/NUL-terminated values/empty. Only source labels are UTF-8.
    All fields are counted, so text NUL is preserved without C string inference.
    """
    if not isinstance(fingerprint, bytes) or len(fingerprint) != 32:
        raise DataError('collation IR needs a 32-byte source manifest fingerprint')
    output, previous, count = bytearray(HEADER.size), None, 0
    for program in programs:
        key = collation_programs.identity(program)
        if previous is not None and key <= previous:
            raise DataError('collation IR programs must be unique and sorted')
        previous = key
        locale, collation = text(program.locale), text(program.collation)
        alternate = program.alternate.encode('utf-8')
        if any(ord(c) < 0x20 or ord(c) == 0x7f for c in program.alternate):
            raise DataError('invalid collation IR alternate')
        body = bytearray(locale + collation + alternate)
        # This is a transient build artifact. Counted records favor a simple
        # checked C reader over a second general-purpose XML/JSON parser.
        for node in program.nodes:
            encoded = encode_node(node)
            if PROGRAM.size + len(output) + len(body) + len(encoded) > MAX_SIZE:
                raise DataError('collation IR exceeds size budget')
            body.extend(encoded)
        size = PROGRAM.size + len(body)
        output.extend(PROGRAM.pack(integer(size), len(locale), len(collation), len(alternate),
                                   integer(len(program.nodes))))
        output.extend(body)
        if len(output) > MAX_SIZE:
            raise DataError('collation IR exceeds size budget')
        count += 1
    HEADER.pack_into(output, 0, MAGIC, VERSION, UNICODE_VERSION, len(output), integer(count), fingerprint)
    return bytes(output)


def encode_catalog(catalog, fingerprint, keys):
    """Expand selected independent programs in their original identity order."""
    return encode((catalog.programs[key]._replace(nodes=catalog.expand(catalog.programs[key]))
                   for key in keys), fingerprint)


def prepare(sources, profile=None):
    catalog = collation_programs.load(sources)
    keys = sorted(catalog.programs)
    if profile is not None:
        # Programs execute independently after ordered import expansion. Keep
        # every usage target, but no standalone private/alternate/unused record
        # solely because its rules were imported into a selected public program.
        bindings = collation_bindings.from_catalog(catalog, profile)
        keys = collation_bindings.program_targets(bindings)
    return encode_catalog(catalog, bytes.fromhex(sources.pin['manifest_sha256']), keys)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', choices=('minimal', 'native'))
    args = parser.parse_args()
    acquire.atomic_write(args.output, prepare(acquire.Sources(args.sources), args.profile))


if __name__ == '__main__':
    main()
