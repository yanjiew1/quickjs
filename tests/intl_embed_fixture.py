#!/usr/bin/env python3
"""Compile every byte value, chunk boundaries and trailing zero through C."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import acquire
import bundle
import embed
import tables


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    # Also cross C99's minimum guaranteed concatenated-string capacity, as the
    # supported compilers must handle much larger real prepared-data objects.
    payload = bytes(range(256)) * 17 + b'\0'
    sections = {bundle.STRINGS: tables.encode_strings(('', 'root')),
                bundle.LOCALES: (1, tables.LOCALE.pack(1, tables.ROOT)),
                123: (1, payload)}
    source = embed.render(bundle.pack(sections, bytes(range(32)), 'minimal'))
    source += '\nconst size_t qjs_test_embedding_storage_length = sizeof(qjs_intl_native_bytes);\n'
    acquire.atomic_write(args.output, source.encode('ascii'))


if __name__ == '__main__':
    main()
