#!/usr/bin/env python3
"""Prepare deterministic minimal/native Intl sections from authenticated CLDR."""
import argparse
import copy
from pathlib import Path

import acquire
import bundle
import list_patterns
import parents
import tables


class Inputs:
    """Track this generation's reads independently of a caller's cache history."""
    def __init__(self, sources):
        self.sources = sources
        self.consumed = set()

    def paths(self, prefix):
        return self.sources.paths(prefix)

    def read(self, name):
        result = self.sources.read(name)
        self.consumed.add(name)
        return result


def generate(sources, profile):
    if profile not in bundle.PROFILES:
        raise ValueError('unknown native Intl data profile')
    inputs_read = Inputs(sources)
    inputs_read.read('LICENSE')
    graph = parents.load(inputs_read)
    lists = list_patterns.collect(inputs_read, graph)
    tags = graph.tags if profile == 'native' else tuple(sorted(graph.chain('en')))
    selected_tags = set(tags)
    selected = [row for row in lists['rows'] if row[0] in selected_tags]
    strings = tuple(sorted(set(('',) + tags + tuple(
        text for locale, variant, values in selected for text in values)),
        key=lambda text: text.encode('utf-8')))
    string_ids = {text: index for index, text in enumerate(strings)}
    sections = {bundle.STRINGS: tables.encode_strings(strings),
                bundle.LOCALES: tables.encode_locales(tags, graph, string_ids),
                bundle.LISTS: tables.encode_lists(selected, tags, string_ids)}
    inputs = {'schema': list(bundle.VERSION), 'profile': profile,
              'cldr': copy.deepcopy(sources.pin),
              'sources': {name: sources.manifest['files'][name] for name in sorted(inputs_read.consumed)},
              'policy': lists['policy'],
              'sections': {str(identity): {'count': count, 'sha256': acquire.digest(data)}
                           for identity, (count, data) in sorted(sections.items())}}
    generation = bytes.fromhex(acquire.digest(acquire.json_bytes(inputs)))
    binary = bundle.pack(sections, generation, profile)
    tables.validate(binary)
    manifest = {'format': 1, 'inputs': inputs, 'generation': generation.hex(),
                'sha256': acquire.digest(binary), 'bytes': len(binary),
                'locales': len(tags), 'strings': len(strings),
                'complete_services': [],
                'note': 'Prepared list templates; not formatter capability or full Intl coverage.'}
    return binary, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(bundle.PROFILES), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    binary, manifest = generate(acquire.Sources(args.sources), args.profile)
    acquire.atomic_write(args.output, binary)
    acquire.atomic_write(args.manifest, acquire.json_bytes(manifest))


if __name__ == '__main__':
    main()
