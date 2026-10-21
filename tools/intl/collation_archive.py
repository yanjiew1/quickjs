#!/usr/bin/env python3
"""Frame checked C compiler output; do not execute rules or normalize text here."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

import acquire
from cldr_xml import DataError
import collation_bindings
import parents

HEADER = struct.Struct('<4s7I32s')
DIRECTORY = struct.Struct('<4I')
PROGRAM = struct.Struct('<8I')
BLOCK = struct.Struct('<16I')
BINDING = struct.Struct('<6I')
MAX_SIZE = 256 * 1024 * 1024
FORMAT_VERSION = 2
VERSION = 18 << 16
STRIDES = (80, 16, 4, 12, 24, 12, 4, 8, 32, 0, 0, 24)
BLOCK_STRIDES = (16, 4, 12, 24, 4, 4, 8, 8, 8, 8, 16, 8)


def unpack(data, fingerprint=None):
    """Check the canonical envelope/counts; C owns CE/key/Unicode semantics.

    Return borrowed byte spans as memoryviews to avoid duplicating large pools.
    Optional binding metadata does not establish comparator capability.
    A valid frame alone must not advertise native Collator availability.
    """
    if not isinstance(data, bytes) or not HEADER.size + 12 * DIRECTORY.size <= len(data) <= MAX_SIZE:
        raise DataError('invalid compiled collation size/type')
    magic, version, unicode, uca, size, count, programs, reserved, source = HEADER.unpack_from(data)
    if (magic != b'QJCL' or version != FORMAT_VERSION or unicode != VERSION or uca != VERSION or
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
            counts[9] > len(sections[9]) or
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
    previous = None
    for row in BINDING.iter_unpack(sections[11]):
        identity = []
        for start, length in ((row[0], row[1]), (row[2], row[3])):
            if start != label_at or length > counts[9] - label_at:
                raise DataError('invalid collation binding label span')
            identity.append(bytes(labels[start:start + length]))
            label_at += length
        identity = tuple(identity)
        if (not identity[0] or (previous is not None and identity <= previous) or
                ((previous is None or identity[0] != previous[0]) and identity[1])):
            raise DataError('invalid collation binding identity/default order')
        if ((not identity[1] and any(index >= programs for index in row[4:])) or
                (identity[1] and (row[4] == row[5] == 0xffffffff or
                                 any(index != 0xffffffff and index >= programs for index in row[4:])))):
            raise DataError('invalid collation binding target')
        previous = identity
    if label_at != counts[9] or block_at != len(blocks):
        raise DataError('unclaimed compiled collation labels/blocks')
    return {'fingerprint': source, 'program_count': programs, 'sections': sections, 'counts': counts}


def bind(data, rows, fingerprint):
    """Append metadata, proving target IDs against the same compiled generation.

    The collector owns source resolution. This stage leaves all program bytes
    unchanged, and rejects targets absent from normal/public compiler output.
    """
    view = unpack(data, fingerprint)
    if view['counts'][11]:
        raise DataError('collation archive already has bindings')

    # Only labels and bindings change; borrow the checked program sections until
    # the final join rather than copying the complete archive into temporaries.
    sections = list(view['sections'])
    counts = list(view['counts'])
    labels = bytearray(sections[9][:counts[9]])
    identities = {}
    for index, row in enumerate(PROGRAM.iter_unpack(sections[8])):
        names = tuple(bytes(labels[row[i]:row[i] + row[i + 1]]).decode('utf-8') for i in (0, 2, 4))
        identities[names] = index

    records, previous, count = bytearray(), None, 0
    for binding in rows:
        locale, kind = binding.locale, binding.collation
        key = locale, kind
        if (locale == 'root' or parents.locale_id(locale) != locale or
                (kind and (not collation_bindings.PUBLIC_TYPE.fullmatch(kind) or
                           kind in ('default', 'standard', 'search') or kind.startswith('private-'))) or
                (previous is not None and key <= previous) or
                ((previous is None or locale != previous[0]) and kind)):
            raise DataError('invalid collation binding identity/default order')

        ids = []
        for target in (binding.sort, binding.search):
            if target is None:
                ids.append(0xffffffff)
            elif (target not in identities or target[2] or target[1].startswith('private-')):
                raise DataError('collation binding target is not a compiled public program')
            else:
                ids.append(identities[target])
        if ((not kind and 0xffffffff in ids) or ids == [0xffffffff, 0xffffffff]):
            raise DataError('missing collation binding usage/default target')

        names = []
        for value in key:
            raw = value.encode('ascii')
            names.extend((len(labels), len(raw)))
            labels.extend(raw)
        records.extend(BINDING.pack(*(names + ids)))
        if len(labels) + len(records) > MAX_SIZE:
            raise DataError('collation binding metadata exceeds size budget')
        previous, count = key, count + 1

    counts[9], counts[11] = len(labels), count
    sections[9] = bytes(labels) + bytes(-len(labels) % 4)
    sections[11] = bytes(records)
    at, directory = HEADER.size + 12 * DIRECTORY.size, bytearray()
    for i, section in enumerate(sections):
        if len(section) > MAX_SIZE - at:
            raise DataError('bound collation archive exceeds size budget')
        directory.extend(DIRECTORY.pack(i + 1, at, len(section), counts[i]))
        at += len(section)

    header = HEADER.pack(b'QJCL', FORMAT_VERSION, VERSION, VERSION, at, 12,
                         view['program_count'], 0, fingerprint)
    result = b''.join([header, directory] + sections)
    unpack(result, fingerprint)
    return result


def compile_archive(compiler, root, programs, output, bindings=None):
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
    data = result.stdout
    unpack(data, header[32:64])
    if bindings is not None:
        fingerprint, rows = bindings
        data = bind(data, rows, fingerprint)

        # C owns semantic acceptance of the final bound image too. Stage beside
        # the destination so large temporary files use the chosen data volume.
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=str(output.parent)) as temporary:
            candidate = Path(temporary) / 'bound.qjcl'
            candidate.write_bytes(data)
            try:
                audit = subprocess.run([str(compiler.resolve()), '--audit-compiled', str(candidate)],
                                       timeout=60, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            except (OSError, subprocess.TimeoutExpired) as error:
                raise DataError('bound collation audit did not complete') from error
            if audit.returncode:
                raise DataError('bound collation audit failed: ' + audit.stderr.decode('utf-8', errors='replace'))

    acquire.atomic_write(output, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--root-input', type=Path, required=True)
    parser.add_argument('--program-input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sources', type=Path)
    parser.add_argument('--profile', choices=('minimal', 'native'), default='native')
    args = parser.parse_args()
    bindings = None
    if args.sources is not None:
        sources = acquire.Sources(args.sources)
        bindings = (bytes.fromhex(sources.pin['manifest_sha256']),
                    collation_bindings.collect(sources, args.profile))
    compile_archive(args.compiler, args.root_input, args.program_input, args.output, bindings)


if __name__ == '__main__':
    main()
