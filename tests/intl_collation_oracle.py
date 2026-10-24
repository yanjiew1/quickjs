#!/usr/bin/env python3
"""Prepare independent allkeys/context test data, never executable ranked data."""
import argparse
import copy
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import acquire
import bundle
import collation_contexts
import collation_root
import collation_tables
from source_inputs import Inputs


def generate(sources):
    inputs_read = Inputs(sources)
    inputs_read.read('LICENSE')
    root = collation_root.collect(inputs_read)
    contexts = collation_contexts.collect(inputs_read, root)
    sections = {bundle.COLLATION_ROOT: collation_tables.encode(root),
                bundle.COLLATION_CONTEXTS: collation_contexts.encode(contexts, root)}
    # This oracle comes directly from raw source mappings. Deriving it from the
    # runtime's ranked section would make compiler/comparator checks circular.
    inputs = {'cldr': copy.deepcopy(sources.pin),
              'sources': {name: sources.manifest['files'][name]
                          for name in sorted(inputs_read.consumed)}}
    generation = acquire.digest(acquire.json_bytes(inputs))
    # The profile-independent oracle reuses the minimal envelope tag; it has
    # no locale registry and retains every raw scalar mapping.
    binary = bundle.pack(sections, bytes.fromhex(generation), 'minimal')
    return binary, {'inputs': inputs, 'generation': generation,
                    'sha256': acquire.digest(binary), 'bytes': len(binary)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--source-pin', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources, acquire.load_pin(args.source_pin))
    binary, manifest = generate(sources)
    acquire.atomic_write(args.output, binary)
    acquire.atomic_write(args.manifest, acquire.json_bytes(manifest))


if __name__ == '__main__':
    main()
