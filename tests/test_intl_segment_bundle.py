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

"""Archive pairing, complete coverage and data-input identities."""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import acquire
import copy
import segment_dictionary_sources
from cldr_xml import DataError
import segment_bundle
import segment_catalog
import segment_dictionary
import segment_dictionary_tables
import segment_dictionary_trie
import segment_program_tables
import segment_suppression_tables
from source_inputs import Inputs
from segment_prepare import Properties
import tables
from test_intl_segment_catalog import sources, graph


class SegmentBundleTests(unittest.TestCase):
    def setUp(self):
        self.tags = ('en', 'root')
        self.parents = (1, 0xffffffff)
        self.identity = bytes(range(32))
        self.catalog = segment_catalog.prepare(sources(), graph(), self.tags, {}.__getitem__)
        strings = ('',) + self.tags
        dictionaries = {}
        for family in segment_dictionary_tables.FAMILIES:
            valued = family == 'cjk'
            data = segment_dictionary.parse(b'a\t0\n' if valued else b'a\n', valued)
            dictionaries[family] = segment_dictionary_trie.prepare(data)
        self.dictionaries = dictionaries
        self.parts = {
            bundle.STRINGS: tables.encode_strings(strings),
            bundle.LOCALES: tables.encode_locales(self.tags, graph(), {text: i for i, text in enumerate(strings)}),
            bundle.SEGMENT_DICTIONARIES: segment_dictionary_tables.encode(dictionaries),
            bundle.SEGMENT_PROGRAMS: segment_program_tables.encode(
                self.catalog.profiles, self.catalog.program_bindings, self.identity, 2),
            bundle.SEGMENT_SUPPRESSIONS: segment_suppression_tables.encode(
                self.catalog.lists, self.catalog.suppression_bindings, self.identity, 2),
        }

    def pack(self, parts=None, identity=None, profile='native'):
        return bundle.pack(self.parts if parts is None else parts,
                           self.identity if identity is None else identity, profile)

    def test_transfer_only_native_sections(self):
        result = segment_bundle.validate(self.pack(), self.tags, self.parents, self.identity)
        self.assertEqual(set(result), set(segment_bundle.SECTIONS))
        self.assertEqual(result, {key: self.parts[key] for key in segment_bundle.SECTIONS})

    def test_identity_profile_inventory_parent_and_section_mismatches(self):
        for data, tags, parents, identity in (
                (self.pack(profile='minimal'), self.tags, self.parents, self.identity),
                (self.pack(identity=bytes(32)), self.tags, self.parents, self.identity),
                (self.pack(), ('fr', 'root'), self.parents, self.identity),
                (self.pack(), self.tags, (0xffffffff, 0), self.identity),
                (self.pack(), self.tags, self.parents, bytes(32))):
            with self.subTest(tags=tags, parents=parents), self.assertRaises(DataError):
                segment_bundle.validate(data, tags, parents, identity)
        extra = dict(self.parts)
        extra[99] = (0, b'')
        with self.assertRaises(DataError):
            segment_bundle.validate(self.pack(extra), self.tags, self.parents, self.identity)
        for key in segment_bundle.SECTIONS:
            missing = dict(self.parts)
            del missing[key]
            with self.subTest(section=key), self.assertRaises(DataError):
                segment_bundle.validate(self.pack(missing), self.tags, self.parents, self.identity)

    def test_all_families_and_routes_are_required(self):
        parts = dict(self.parts)
        parts[bundle.SEGMENT_DICTIONARIES] = segment_dictionary_tables.encode({'cjk': self.dictionaries['cjk']})
        with self.assertRaisesRegex(DataError, 'dictionary families'):
            segment_bundle.validate(self.pack(parts), self.tags, self.parents, self.identity)
        parts = dict(self.parts)
        parts[bundle.SEGMENT_PROGRAMS] = segment_program_tables.encode(
            self.catalog.profiles, self.catalog.program_bindings[:-1], self.identity, 2)
        with self.assertRaisesRegex(DataError, 'locale/granularity routes'):
            segment_bundle.validate(self.pack(parts), self.tags, self.parents, self.identity)
        parts = dict(self.parts)
        parts[bundle.SEGMENT_SUPPRESSIONS] = segment_suppression_tables.encode(
            (None,), ((0, 0),), self.identity, 2)
        with self.assertRaisesRegex(DataError, 'whole suppression routes'):
            segment_bundle.validate(self.pack(parts), self.tags, self.parents, self.identity)

    def test_inner_fingerprints_must_match(self):
        for key, replacement in (
                (bundle.SEGMENT_PROGRAMS, segment_program_tables.encode(
                    self.catalog.profiles, self.catalog.program_bindings, bytes(32), 2)),
                (bundle.SEGMENT_SUPPRESSIONS, segment_suppression_tables.encode(
                    self.catalog.lists, self.catalog.suppression_bindings, bytes(32), 2))):
            parts = dict(self.parts)
            parts[key] = replacement
            with self.subTest(section=key), self.assertRaisesRegex(DataError, 'recipe mismatch'):
                segment_bundle.validate(self.pack(parts), self.tags, self.parents, self.identity)

    def test_identity_and_coverage_precede_dictionary_graph_copies(self):
        parts = dict(self.parts)
        parts[bundle.SEGMENT_PROGRAMS] = segment_program_tables.encode(
            self.catalog.profiles, self.catalog.program_bindings[:-1], self.identity, 2)
        incomplete = self.pack(parts)
        with patch.object(segment_bundle.segment_dictionary_tables, 'decode') as decode:
            with self.assertRaises(DataError):
                segment_bundle.validate(self.pack(profile='minimal'), self.tags, self.parents, self.identity)
            decode.assert_not_called()
            with self.assertRaisesRegex(DataError, 'locale/granularity'):
                segment_bundle.validate(incomplete, self.tags, self.parents, self.identity)
            decode.assert_not_called()

    def test_expected_parent_types_and_shared_framing_precede_decoding(self):
        valid = self.pack()
        bad = []
        for key, section in ((bundle.LOCALES, (3, self.parts[bundle.LOCALES][1])),
                              (bundle.LOCALES, (2, bytes(15))),
                              (bundle.STRINGS, (100, bytes(1024)))):
            parts = dict(self.parts)
            parts[key] = section
            bad.append(self.pack(parts))
        with patch.object(segment_bundle.tables, 'decode_locales') as locales, \
                patch.object(segment_bundle.segment_program_tables, 'decode') as programs, \
                patch.object(segment_bundle.segment_suppression_tables, 'decode') as lists:
            for parents in ((True, 0xffffffff), (1.0, 0xffffffff), (-1, 0xffffffff), (2, 0xffffffff)):
                with self.subTest(parents=parents), self.assertRaises(DataError):
                    segment_bundle.validate(valid, self.tags, parents, self.identity)
            for data in bad:
                with self.assertRaises(DataError):
                    segment_bundle.validate(data, self.tags, self.parents, self.identity)
            locales.assert_not_called()
            programs.assert_not_called()
            lists.assert_not_called()

    def test_inner_identity_precedes_typed_graph_decoding(self):
        parts = dict(self.parts)
        parts[bundle.SEGMENT_SUPPRESSIONS] = segment_suppression_tables.encode(
            self.catalog.lists, self.catalog.suppression_bindings, bytes(32), 2)
        data = self.pack(parts)
        with patch.object(segment_bundle.segment_program_tables, 'decode') as programs, \
                patch.object(segment_bundle.segment_suppression_tables, 'decode') as lists, \
                patch.object(segment_bundle.segment_dictionary_tables, 'decode') as dictionaries:
            with self.assertRaisesRegex(DataError, 'recipe mismatch'):
                segment_bundle.validate(data, self.tags, self.parents, self.identity)
            programs.assert_not_called()
            lists.assert_not_called()
            dictionaries.assert_not_called()

    @staticmethod
    def dictionary_receipt():
        return {name: 'a' * 64 for name in segment_dictionary_sources.FAMILIES}

    @staticmethod
    def receipt(data, inputs):
        return {'inputs': inputs, 'bytes': len(data), 'sha256': acquire.digest(data),
                'fingerprint': segment_bundle.fingerprint(inputs).hex()}

    def test_pair_records_reads_and_uses_final_registry(self):
        class Reader:
            def __init__(self):
                self.reads = []

            def paths(self, prefix):
                return ('common/segments/root.xml',)

            def read(self, name):
                self.reads.append(name)
                return b'fixture'

        reader = Reader()
        files = {'common/segments/root.xml': acquire.digest(b'fixture')}
        inputs = segment_bundle.recipe(files, self.dictionary_receipt(), self.tags)
        identity = segment_bundle.fingerprint(inputs)
        parts = dict(self.parts)
        parts[bundle.SEGMENT_PROGRAMS] = segment_program_tables.encode(
            self.catalog.profiles, self.catalog.program_bindings, identity, 2)
        parts[bundle.SEGMENT_SUPPRESSIONS] = segment_suppression_tables.encode(
            self.catalog.lists, self.catalog.suppression_bindings, identity, 2)
        data = self.pack(parts, identity)
        manifest = self.receipt(data, inputs)
        sections, paired = segment_bundle.pair(reader, files, graph(), self.tags, data, manifest)
        self.assertEqual(paired, inputs)
        self.assertEqual(set(sections), set(segment_bundle.SECTIONS))
        self.assertEqual(reader.reads, ['common/segments/root.xml', 'common/dtd/ldml.dtd'])

        # A sidecar cannot authorize different bytes, data sources or registries.
        for key, value in [('sha256', '0' * 64), ('bytes', len(data) + 1),
                           ('fingerprint', '0' * 64)]:
            with self.subTest(key=key), self.assertRaises(DataError):
                segment_bundle.pair(reader, files, graph(), self.tags, data,
                                    dict(manifest, **{key: value}))
        for key, value in [('cldr_inventory', '0' * 64), ('locales', ['fr', 'root']),
                           ('dictionaries', dict(self.dictionary_receipt(), LICENSE='b' * 64))]:
            changed = copy.deepcopy(manifest)
            changed['inputs'][key] = value
            with self.subTest(key=key), self.assertRaises(DataError):
                segment_bundle.pair(reader, files, graph(), self.tags, data, changed)
        with self.assertRaises(DataError):
            segment_bundle.pair(reader, dict(files, extra='0' * 64), graph(),
                                self.tags, data, manifest)
        for bad in (None, [], {}, dict(manifest, inputs=None), dict(manifest, inputs=[])):
            with self.subTest(manifest=bad), self.assertRaises(DataError):
                segment_bundle.pair(reader, files, graph(), self.tags, data, bad)
        for bad in ({}, dict(self.dictionary_receipt(), extra='0' * 64),
                    dict(self.dictionary_receipt(), LICENSE='z' * 64)):
            changed = copy.deepcopy(manifest)
            changed['inputs']['dictionaries'] = bad
            with self.subTest(dictionaries=bad), self.assertRaises(DataError):
                segment_bundle.pair(reader, files, graph(), self.tags, data, changed)
        # A real sidecar is read from JSON, where tuples become lists.
        import json
        segment_bundle.pair(reader, files, graph(), self.tags, data,
                            json.loads(json.dumps(manifest)))

    def test_input_identity_ignores_producer_source_edits(self):
        files = {'common/main/en.xml': 'b' * 64}
        # Producer source files are not data inputs; comment/checkout changes
        # need not churn otherwise identical compiled output.
        with patch.object(Path, 'read_bytes', side_effect=AssertionError('producer read')):
            before = segment_bundle.recipe(files, self.dictionary_receipt(), self.tags)
        changed = segment_bundle.recipe(files, dict(self.dictionary_receipt(), LICENSE='c' * 64), self.tags)
        self.assertNotEqual(segment_bundle.fingerprint(before), segment_bundle.fingerprint(changed))

    def test_final_pack_transfers_paired_sections_without_second_inventory(self):
        import availability
        import embed
        import generate
        import parents
        from test_intl_tables import VerifiedFixture
        from test_intl_segment_program_tables import profile

        source = VerifiedFixture()
        final_graph, _ = availability.complete(parents.load(source))
        tags = availability.profile_locales(final_graph, 'native')
        inputs = segment_bundle.recipe(source.manifest['files'], self.dictionary_receipt(), tags)
        identity = segment_bundle.fingerprint(inputs)
        programs = tuple(profile(kind, ()) for kind in
                         ('GraphemeClusterBreak', 'WordBreak', 'SentenceBreak'))
        strings = ('',) + tags
        parts = {
            bundle.STRINGS: tables.encode_strings(strings),
            bundle.LOCALES: tables.encode_locales(tags, final_graph, {text: i for i, text in enumerate(strings)}),
            bundle.SEGMENT_DICTIONARIES: self.parts[bundle.SEGMENT_DICTIONARIES],
            bundle.SEGMENT_PROGRAMS: segment_program_tables.encode(
                programs, tuple((locale, kind, kind) for locale in range(len(tags)) for kind in range(3)),
                identity, len(tags)),
            bundle.SEGMENT_SUPPRESSIONS: segment_suppression_tables.encode(
                (None,), tuple((locale, 0) for locale in range(len(tags))), identity, len(tags)),
        }
        archive = bundle.pack(parts, identity, 'native')
        binary, manifest = generate.generate(source, 'native', compiled_segmentation=archive,
                                             segmentation_manifest=self.receipt(archive, inputs))
        envelope = bundle.unpack(binary)
        for key in segment_bundle.SECTIONS:
            self.assertEqual(envelope['sections'][key], parts[key])
        # Locale IDs are shared, but the formatter string pool has its own IDs.
        decoded = tables.validate(binary)
        self.assertEqual(decoded['locales'], tags)
        self.assertNotEqual(envelope['sections'][bundle.STRINGS], parts[bundle.STRINGS])
        self.assertEqual(manifest['inputs']['segmentation'], inputs)
        self.assertIn('NOTICE-SEGMENT-DICTIONARIES', embed.render(binary))
        self.assertEqual(embed.render(binary, 'native'), embed.render(binary))
        with self.assertRaisesRegex(ValueError, 'selected build profile'):
            embed.render(binary, 'minimal')
        with self.assertRaisesRegex(ValueError, 'native profile'):
            generate.generate(source, 'minimal', compiled_segmentation=archive)
        for arguments in ({'segmentation_manifest': self.receipt(archive, inputs)},
                          {'compiled_segmentation': archive}):
            with self.assertRaisesRegex(ValueError, 'required together'):
                generate.generate(source, 'native', **arguments)


class SourceInputsTests(unittest.TestCase):
    def test_reads_are_independent_of_caller_cache_history(self):
        class Reader:
            pin = {'fixture': 1}
            consumed = {'known'}

            def read(self, name):
                if name != 'known':
                    raise ValueError('unauthenticated')
                return b'known bytes'

        reader = Reader()
        first, second = Inputs(reader), Inputs(reader)
        self.assertEqual(first.pin, reader.pin)
        self.assertEqual(first.read('known'), b'known bytes')
        self.assertEqual(first.consumed, {'known'})
        self.assertEqual(second.consumed, set())
        with self.assertRaises(ValueError):
            first.read('bad')
        self.assertEqual(first.consumed, {'known'})

    def test_property_facts_exclude_unrelated_resolver_history(self):
        class Resolver:
            def __init__(self):
                self.cache = {'Earlier': ((99, 100),), 'Current': [[65, 91]]}
                self.calls = []

            def __call__(self, query):
                self.calls.append(query)
                return self.cache[query]

        resolver = Resolver()
        properties = Properties(resolver)
        self.assertEqual(properties('Current'), ((65, 91),))
        resolver.cache['Current'][0][0] = 66
        self.assertEqual(properties('Current'), ((65, 91),))
        self.assertEqual(properties.used, {'Current': ((65, 91),)})
        self.assertEqual(resolver.calls, ['Current'])


if __name__ == '__main__':
    unittest.main()
