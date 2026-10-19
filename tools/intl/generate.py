#!/usr/bin/env python3
"""Prepare deterministic minimal/native Intl sections from authenticated CLDR."""
import argparse
import copy
from pathlib import Path

import acquire
import availability
import bundle
import list_patterns
import locale_metadata
import locale_tables
import numbering
import numbering_tables
import number_symbols
import number_symbol_tables
import parents
import preferences
import preference_tables
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
    graph, fallback_aliases = availability.complete(parents.load(inputs_read))
    locale_data = locale_metadata.collect(inputs_read, graph)
    preference_data = preferences.collect(inputs_read)
    numbering_data = numbering.collect(inputs_read, graph)
    symbol_data = number_symbols.collect(inputs_read, graph)
    lists = list_patterns.collect(inputs_read, graph)
    if profile == 'native':
        tags = graph.tags
    else:
        # Retain the explicit US identifier as well as generic English. Sparse
        # authored source fixtures need not contain every production seed.
        seeds = (locale for locale in ('en', 'en-US') if locale in graph.tags)
        tags = tuple(sorted({tag for locale in seeds for tag in graph.chain(locale)}))
    selected_tags = set(tags)
    symbol_data['rows'] = tuple(row for row in symbol_data['rows'] if row[0] in selected_tags)
    selected = [row for row in lists['rows'] if row[0] in selected_tags]
    contexts = [row for row in lists['contexts'] if row[0] in selected_tags]
    pool_strings = {''}
    pool_strings.update(tags)
    pool_strings.update(locale_tables.strings(locale_data))
    pool_strings.update(preference_tables.strings(preference_data))
    pool_strings.update(numbering_tables.strings(numbering_data))
    pool_strings.update(number_symbol_tables.strings(symbol_data))
    pool_strings.update(text for locale, variant, values in selected for text in values)
    pool_strings.update(text for locale, variant, kind, pair, end in contexts
                        for text in (pair, end))
    strings = tuple(sorted(pool_strings, key=lambda text: text.encode('utf-8')))
    string_ids = {text: index for index, text in enumerate(strings)}
    sections = {bundle.STRINGS: tables.encode_strings(strings),
                bundle.LOCALES: tables.encode_locales(tags, graph, string_ids),
                bundle.LISTS: tables.encode_lists(selected, tags, string_ids),
                bundle.LIST_CONTEXTS: tables.encode_contexts(contexts, tags, string_ids)}
    sections.update(locale_tables.encode(locale_data, string_ids))
    sections.update(preference_tables.encode(preference_data, string_ids))
    sections.update(numbering_tables.encode(numbering_data, string_ids))
    sections[bundle.NUMBER_SYMBOLS] = number_symbol_tables.encode(symbol_data, tags,
                                                               numbering_data, string_ids)
    inputs = {'schema': list(bundle.VERSION), 'profile': profile,
              'cldr': copy.deepcopy(sources.pin),
              'sources': {name: sources.manifest['files'][name] for name in sorted(inputs_read.consumed)},
              'policy': lists['policy'] + '; formatter fallback registry policy 1',
              'formatter_fallbacks': tuple(row for row in fallback_aliases if row[0] in selected_tags),
              'sections': {str(identity): {'count': count, 'sha256': acquire.digest(data)}
                           for identity, (count, data) in sorted(sections.items())}}
    generation = bytes.fromhex(acquire.digest(acquire.json_bytes(inputs)))
    binary = bundle.pack(sections, generation, profile)
    tables.validate(binary)
    manifest = {'format': 1, 'inputs': inputs, 'generation': generation.hex(),
                'sha256': acquire.digest(binary), 'bytes': len(binary),
                'locales': len(tags), 'strings': len(strings),
                'complete_services': [],
                'note': 'Prepared locale metadata, preferences, list templates and numeric symbols; not formatter capability or full Intl coverage.'}
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
