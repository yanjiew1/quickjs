#!/usr/bin/env python3
"""Standalone host generator for metadata1.2 + native ListFormat.

Uses frozen metadata/preference modules without editing their main generator.
Python3.6+; source-only preparation: this entry point is executed only by root.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import sys
import metadata as m
import preferences as p
import list_patterns as lists

SCHEMA_HEADER_SHA256 = '43388a9c26d47a7a22f4a687de2f8b87691b9c33452a3bc5ff26a78f39906208'
SCHEMA_JSON_SHA256 = '186d1aff78bad2b4b11a74576697431ba0faac15f3c732cac21b3e1586fa748b'
ICU_COMMIT = '21d1eb0f306e1141c10931e914dfc038c06121da'
ICU_SOURCE_SHA256 = 'd716844d6352dd2f306cf16d60f971a6d17c9ed5f9c1272a29d12b15c89a45ca'


def encode_blob(sections):
    """Additive1.2 derivative of the metadata owner's byte assembler.

    Uses their immutable widths1..23; all new layout constants are frozen in
    the paired metadata-data.h/WIRE-SCHEMA. No serialized native C structs.
    """
    widths = dict(m.SECTION_WIDTHS)
    widths.update(m.OPTIONAL_WIDTHS)
    widths.update({lists.SECTION_ID: lists.RECORD_WIDTH,
                   lists.SCRIPT_SECTION_ID: lists.SCRIPT_RECORD_WIDTH})
    m.require(set(m.SECTION_WIDTHS) <= set(sections) <= set(widths), 'wrong schema1.2 sections')
    m.require(16 in sections and 40 in sections and 41 in sections, 'missing ListFormat sections')
    count = len(sections)
    cursor = m.HEADER_SIZE + m.DIRECTORY_SIZE * count
    directories, inventory = [], []
    payload = bytearray()
    for section_id in sorted(sections):
        data, width = sections[section_id], widths[section_id]
        m.require(isinstance(data, bytes) and len(data) % width == 0,
                  'invalid section byte type/width')
        padding = (-cursor) % 4
        payload.extend(b'\0' * padding)
        cursor += padding
        records = len(data) // width
        entry = (section_id, cursor, len(data), records, width, 0)
        directories.append(b''.join(m.u32(value) for value in entry))
        inventory.append({'id': section_id, 'offset': cursor, 'bytes': len(data),
                          'records': records, 'record_size': width})
        payload.extend(data)
        cursor += len(data)
        m.u32(cursor)
    header = b'QJSINTL\0' + struct.pack('<HH', m.SCHEMA_MAJOR, lists.SCHEMA_MINOR)
    header += b''.join(m.u32(value) for value in
                      (m.HEADER_SIZE, cursor, count, m.HEADER_SIZE, m.DIRECTORY_SIZE,
                       0, 18 << 16, 49 << 16, 0))
    header += b'\0' * 16
    m.require(len(header) == m.HEADER_SIZE, 'header layout drift')
    blob = header + b''.join(directories) + bytes(payload)
    m.require(len(blob) == cursor, 'blob layout drift')
    return blob, inventory


def generated_outputs(args):
    context = m.VerifiedInputContext()
    reference = Path(args.icu_listformatter_reference).read_bytes()
    m.require(hashlib.sha256(reference).hexdigest() == ICU_SOURCE_SHA256,
              'ICU context reference hash mismatch')
    aliases, defaults, likely, bcp, consumed, identity, unused_digest = m.load_sources(
        args.cldr_dir, args.ucd_dir, args.cldr_manifest, args.ucd_manifest, context)
    metadata = p.load_pinned(args, defaults, bcp, consumed, context)
    service = lists.load_pinned(args, metadata, consumed, context)
    identity['inputs'] = {path: item['sha256'] for path, item in sorted(consumed.items())}
    identity['list_context_reference'] = {'commit': ICU_COMMIT,
        'logical_path': 'icu/source/i18n/listformatter.cpp', 'sha256': ICU_SOURCE_SHA256}
    identity['wire_schema'] = {'version': [1, 2], 'header_sha256': SCHEMA_HEADER_SHA256,
                               'json_sha256': SCHEMA_JSON_SHA256}
    digest = hashlib.sha256(m.canonical_json(identity)).digest()

    def extensions(pool):
        sections = p.encode(metadata, pool)
        extra = lists.encode(service, pool)
        m.require(not set(sections) & set(extra), 'duplicate service section')
        sections.update(extra)
        return sections

    extra_strings = p.strings(metadata) + list(lists.strings(service))
    sections = m.encode_sections(aliases, likely, bcp, digest, extra_strings,
                                extensions, pool_type=m.UTF8StringPool)
    blob, inventory = encode_blob(sections)
    outputs = {'intl-data.bin': blob}
    if args.embed_c:
        header, source = m.embedding(blob)
        outputs['locale-metadata.h'] = header
        outputs['locale-metadata.c'] = source
    report = {'schema': [1, 2], 'identity': identity, 'input_digest_sha256': digest.hex(),
        'inputs': consumed, 'binary_bytes': len(blob), 'sections': inventory,
        'generator_dependencies': {name: hashlib.sha256(Path(module.__file__).read_bytes()).hexdigest()
                                   for name, module in (('metadata.py', m), ('preferences.py', p),
                                                        ('list_patterns.py', lists))},
        'preferences_evidence': metadata['evidence'], 'list_evidence': service['evidence'],
        'service_coverage': 0, 'provider_activation': 'pending root verification; ICU default unchanged',
        'generated_payloads': {name: {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                               for name, data in sorted(outputs.items())}}
    report['verified_input_context'] = dict(context.stats)
    outputs['generated-input-manifest.json'] = m.canonical_json(report)
    return outputs


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cldr-dir', required=True)
    parser.add_argument('--ucd-dir', required=True)
    parser.add_argument('--cldr-manifest', required=True)
    parser.add_argument('--ucd-manifest', required=True)
    parser.add_argument('--icu-listformatter-reference', required=True)
    parser.add_argument('--output-dir', required=True)
    parser.add_argument('--embed-c', action='store_true')
    args = parser.parse_args(argv)
    try:
        outputs = generated_outputs(args)
        destination = Path(args.output_dir)
        destination.mkdir(parents=True, exist_ok=True)
        for name, data in sorted(outputs.items()):
            target = destination / name
            if not target.exists() or target.read_bytes() != data:
                target.write_bytes(data)
    except (m.DataError, OSError) as exc:
        parser.exit(1, 'intl list bundle: {}\n'.format(exc))
    return 0


if __name__ == '__main__':
    sys.exit(main())
