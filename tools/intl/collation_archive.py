#!/usr/bin/env python3
"""Frame checked C compiler output; do not execute rules or normalize text here."""
import argparse
from pathlib import Path
import struct
import subprocess

import acquire
from cldr_xml import DataError

HEADER = struct.Struct('<4s7I32s')
DIRECTORY = struct.Struct('<4I')
PROGRAM = struct.Struct('<8I')
BLOCK = struct.Struct('<16I')
MAX_SIZE = 256 * 1024 * 1024
VERSION = 18 << 16
STRIDES = (80, 16, 4, 12, 24, 12, 4, 8, 32, 0, 0, 24)
BLOCK_STRIDES = (16, 4, 12, 24, 4, 4, 8, 8, 8, 8, 16, 8)


def unpack(data, fingerprint=None):
    """Check the canonical envelope/counts; C owns CE/key/Unicode semantics.

    Return borrowed byte spans as memoryviews to avoid duplicating large pools.
    This compiler-output phase has no resolved public locale/type bindings yet.
    A valid frame alone must not advertise native Collator availability.
    """
    if not isinstance(data, bytes) or not HEADER.size + 12 * DIRECTORY.size <= len(data) <= MAX_SIZE:
        raise DataError('invalid compiled collation size/type')
    magic, version, unicode, uca, size, count, programs, reserved, source = HEADER.unpack_from(data)
    if (magic != b'QJCL' or version != 1 or unicode != VERSION or uca != VERSION or
            size != len(data) or count != 12 or reserved or
            (fingerprint is not None and source != fingerprint)):
        raise DataError('invalid compiled collation header/source identity')
    sections, counts, at = [], [], HEADER.size + 12 * DIRECTORY.size
    view = memoryview(data)
    for i, stride in enumerate(STRIDES):
        kind, start, length, records = DIRECTORY.unpack_from(data, HEADER.size + i * DIRECTORY.size)
        if (kind != i + 1 or start != at or length % 4 or length > len(data) - at or
                (stride and records * stride != length)):
            raise DataError('invalid compiled collation directory')
        sections.append(view[start:start + length])
        counts.append(records)
        at += length
    if (at != len(data) or counts[0] != 1 or counts[8] != programs or counts[10] != programs or
            counts[11] or sections[11] or counts[9] > len(sections[9]) or
            len(sections[9]) - counts[9] > 3 or any(sections[9][counts[9]:])):
        raise DataError('invalid compiled collation pools/counts/padding')
    labels, blocks, label_at, block_at, previous = sections[9], sections[10], 0, 0, None
    for i in range(programs):
        row = PROGRAM.unpack_from(sections[8], i * PROGRAM.size)
        identity = []
        for j in range(3):
            start, length = row[j * 2:j * 2 + 2]
            if start != label_at or length > counts[9] - label_at or (j < 2 and not length):
                raise DataError('invalid compiled collation label span')
            label = bytes(labels[start:start + length])
            try:
                text = label.decode('utf-8')
            except UnicodeDecodeError as error:
                raise DataError('invalid compiled collation label UTF-8') from error
            if any(ord(c) < 0x20 or ord(c) == 0x7f for c in text):
                raise DataError('invalid compiled collation label control')
            identity.append(label)
            label_at += length
        identity = tuple(identity)
        if previous is not None and identity <= previous:
            raise DataError('unordered compiled collation program identities')
        previous = identity
        start, length = row[6:]
        if start != block_at or length < BLOCK.size or length > len(blocks) - block_at:
            raise DataError('invalid compiled collation program block')
        words = BLOCK.unpack_from(blocks, start)
        if BLOCK.size + sum(n * stride for n, stride in zip(words[4:], BLOCK_STRIDES)) != length:
            raise DataError('invalid compiled collation block counts')
        block_at += length
    if label_at != counts[9] or block_at != len(blocks):
        raise DataError('unclaimed compiled collation labels/blocks')
    return {'fingerprint': source, 'program_count': programs, 'sections': sections, 'counts': counts}


def compile_archive(compiler, root, programs, output):
    """Publish only a complete paired image. Failed/truncated runs preserve output."""
    with root.open('rb') as file:
        header = file.read(64)
    if len(header) != 64 or header[:4] != b'QJCR':
        raise DataError('invalid root input header')
    try:
        result = subprocess.run([str(compiler.resolve()), '--compile-pair', str(root), str(programs)],
                                timeout=180, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise DataError('collation compiler did not complete') from error
    if result.returncode:
        raise DataError('collation compiler failed: ' + result.stderr.decode('utf-8', errors='replace'))
    unpack(result.stdout, header[32:64])
    acquire.atomic_write(output, result.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--root-input', type=Path, required=True)
    parser.add_argument('--program-input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compile_archive(args.compiler, args.root_input, args.program_input, args.output)


if __name__ == '__main__':
    main()
