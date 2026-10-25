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

"""Compile authenticated CLDR/Unicode/ICU inputs into one native data archive.

This is an explicit host preparation step. Normal engine builds consume the
prepared final QJIN pack offline and do not parse XML, dictionaries or JSON.
"""
import argparse
from pathlib import Path

import acquire
import availability
import bundle
from cldr_xml import DataError
import parents
import segment_bundle
import segment_catalog
from segment_compat import japanese_word_fe
import segment_dictionary
import segment_dictionary_sources
import segment_dictionary_tables
import segment_dictionary_trie
import segment_program_tables
from segment_property import PropertyResolver, DEFAULT_LIMITS as PROPERTY_LIMITS
from segment_sets import checked_ranges
import segment_sources
import segment_suppression_tables
from source_inputs import Inputs
import tables


class Properties:
    """Snapshot this invocation's facts while sharing an underlying resolver."""
    def __init__(self, resolver):
        self.resolver = resolver
        self.used = {}
        self.pairs = 0

    def __call__(self, query):
        if query not in self.used:
            if len(self.used) >= PROPERTY_LIMITS.queries:
                raise DataError('segmentation property invocation budget exceeded')
            values = checked_ranges(self.resolver(query), PROPERTY_LIMITS.ranges)
            if self.pairs + len(values) > PROPERTY_LIMITS.retained_pairs:
                raise DataError('segmentation property invocation range budget exceeded')
            self.pairs += len(values)
            self.used[query] = values
        return self.used[query]


def prepare(sources, dictionaries, resolver):
    """Return native archive, reproducible receipt and preserved source notices."""
    cldr = Inputs(sources)
    dictionary_inputs = Inputs(dictionaries)
    cldr.read('LICENSE')
    license_bytes = dictionary_inputs.read('LICENSE')
    corrected = japanese_word_fe(cldr, segment_sources.collect(cldr)['records'])
    graph, aliases = availability.complete(parents.load(cldr))
    tags = availability.profile_locales(graph, 'native')
    properties = Properties(resolver)
    catalog = segment_catalog.prepare(corrected.records, graph, tags, properties)
    parsed_dictionaries = segment_dictionary.collect(dictionary_inputs)
    graphs = {family: segment_dictionary_trie.prepare(parsed_dictionaries[family])
              for family in segment_dictionary_tables.FAMILIES}
    inputs = segment_bundle.recipe(sources.manifest['files'],
                                   dictionaries.identity()['files'], tags)
    identity = segment_bundle.fingerprint(inputs)
    strings = ('',) + tags
    sections = {
        bundle.STRINGS: tables.encode_strings(strings),
        bundle.LOCALES: tables.encode_locales(tags, graph, {text: index for index, text in enumerate(strings)}),
        bundle.SEGMENT_DICTIONARIES: segment_dictionary_tables.encode(graphs),
        bundle.SEGMENT_PROGRAMS: segment_program_tables.encode(
            catalog.profiles, catalog.program_bindings, identity, len(tags)),
        bundle.SEGMENT_SUPPRESSIONS: segment_suppression_tables.encode(
            catalog.lists, catalog.suppression_bindings, identity, len(tags)),
    }
    binary = bundle.pack(sections, identity, 'native')
    ids = {tag: index for index, tag in enumerate(tags)}
    parent_ids = tuple(0xffffffff if graph.parent(tag) is None else ids[graph.parent(tag)] for tag in tags)
    segment_bundle.validate(binary, tags, parent_ids, identity)

    # Retain the complete release license separately and every original comment
    # line with its source file. Individual headers include additional notices
    # (for example Apple/Google) that a generic Unicode notice cannot replace.
    lines = ['Segmentation dictionary source notices',
             'ICU ' + dictionaries.pin['release'] + ' (' + dictionaries.pin['commit'] + ')',
             'See LICENSE-SEGMENT-DICTIONARIES for the full release license.', '']
    for row in dictionaries.pin['files']:
        if row['family'] == 'license':
            continue
        lines.extend(('Source: ' + row['source'], ''))
        lines.extend(parsed_dictionaries[row['family']].comments)
        lines.append('')
    notices = ('\n'.join(lines) + '\n').encode('utf-8')
    facts = properties.used
    manifest = {
        'format': 1, 'inputs': inputs, 'fingerprint': identity.hex(),
        'bytes': len(binary), 'sha256': acquire.digest(binary),
        'sources': {name: sources.manifest['files'][name] for name in sorted(cldr.consumed)},
        'corrections': tuple(row._asdict() for row in corrected.corrections),
        'formatter_aliases': aliases,
        'streams': tuple({'kind': row.kind, 'program': row.program,
                          'variables': row.variables, 'rules': row.rules} for row in catalog.streams),
        'stream_routes': catalog.stream_routes,
        'suppression_sources': tuple(row._asdict() for row in catalog.suppression_sources),
        'suppression_origins': catalog.suppression_origins,
        'property_facts': tuple({'query': query, 'ranges': len(values),
                                 'sha256': acquire.digest(acquire.json_bytes(values))}
                                for query, values in sorted(facts.items())),
        'sections': {str(key): {'count': count, 'bytes': len(data), 'sha256': acquire.digest(data)}
                     for key, (count, data) in sorted(sections.items())},
        'license_sha256': acquire.digest(license_bytes), 'notices_sha256': acquire.digest(notices),
        'note': 'Prepared segmentation inputs; runtime readers and native provider admission remain separate.',
    }
    return binary, manifest, license_bytes, notices


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--source-pin', type=Path,
                        help='explicit alternate source pin for release/Git comparison')
    parser.add_argument('--dictionaries', type=Path, required=True)
    parser.add_argument('--dictionary-pin', type=Path, help='optional declared dictionary metadata')
    parser.add_argument('--property-tool', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--license', type=Path, required=True)
    parser.add_argument('--notices', type=Path, required=True)
    args = parser.parse_args()
    binary, manifest, license_bytes, notices = prepare(
        acquire.Sources(args.sources, acquire.load_pin(args.source_pin)),
        segment_dictionary_sources.Sources(
            args.dictionaries, segment_dictionary_sources.load_pin(args.dictionary_pin)),
        PropertyResolver(args.property_tool))
    for path, contents in ((args.output, binary), (args.manifest, acquire.json_bytes(manifest)),
                            (args.license, license_bytes), (args.notices, notices)):
        acquire.atomic_write(path, contents)
    print('{} bytes; {} locales; {} streams'.format(len(binary), len(manifest['inputs']['locales']),
                                                  len(manifest['streams'])))


if __name__ == '__main__':
    main()
