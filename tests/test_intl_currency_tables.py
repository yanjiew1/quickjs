#!/usr/bin/env python3
"""Currency field-vector sharing, projection, optional states and corrupt data."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import currencies
import currency_tables as wire
from resolve import BLOCK, INHERIT
from test_intl_currencies import Sources
from test_intl_list_patterns import graph


class CurrencyTableTests(unittest.TestCase):
    def fixture(self):
        source = Sources()
        source.set('en', '<currency type="USD"><displayName count="one">dollar</displayName>'
                        '<decimal>;</decimal><pattern>¤0.00</pattern></currency>')
        source.set('en_GB', '<currency type="USD"><symbol/><displayName count="other">' + BLOCK +
                           '</displayName></currency>')
        data = currencies.collect(source, graph())
        pool = tuple(sorted(wire.strings(data) | {''}, key=lambda value: value.encode('utf-8')))
        ids = {text: index for index, text in enumerate(pool)}
        return data, pool, ids

    def semantics(self, data):
        return {locale: data['maps'][index] for locale, index in data['rows']}

    def test_roundtrip_and_projection_preserve_source_facts(self):
        data, pool, ids = self.fixture()
        section = wire.encode(data, graph().tags, ids)
        result = wire.decode(section, pool, graph().tags)
        self.assertEqual(self.semantics(data), self.semantics(result))
        self.assertEqual(result['fractions'], data['fractions'])
        self.assertEqual(result['codes'], ('CHF', 'JPY', 'USD'))
        self.assertIsNone(wire.decode(None, pool, graph().tags))
        tags = ('en-GB', 'root')
        selected = wire.select(data, tags)
        result = wire.decode(wire.encode(selected, tags, ids), pool, tags)
        self.assertEqual(self.semantics(result), {tag: value for tag, value in self.semantics(data).items() if tag in tags})

    def test_supplemental_only_code_has_no_invented_name(self):
        data, pool, ids = self.fixture()
        data = dict(data, maps=((),), rows=tuple((tag, 0) for tag in graph().tags))
        section = wire.encode(data, graph().tags, ids)
        self.assertEqual(struct.unpack_from('<I', section[1], 12)[0], 0) # No field vectors.
        result = wire.decode(section, pool, graph().tags)
        self.assertTrue(all(not fields for fields in result['maps']))
        self.assertEqual(result['fractions'], data['fractions'])

    def test_invalid_source_inventories_and_fields(self):
        data, pool, ids = self.fixture()
        for changed in (dict(data, fields=()), dict(data, counts=()),
                        dict(data, codes=data['codes'] + data['codes'][:1]),
                        dict(data, rows=data['rows'][:-1]),
                        dict(data, rows=data['rows'] + data['rows'][:1]),
                        dict(data, maps=data['maps'] + ((),)),
                        dict(data, fractions=()),
                        dict(data, fractions=data['fractions'] + data['fractions'][:1])):
            with self.assertRaises(ValueError):
                wire.encode(changed, graph().tags, ids)
        for entry in (('USD', 'unknown', '', '$'), ('USD', 'symbol', 'one', '$'),
                      ('USD', 'name', 'unknown', '$'), ('USD', 'symbol', '', INHERIT),
                      ('USD', 'symbol', '', None)):
            changed = dict(data, maps=((entry,),), rows=tuple((tag, 0) for tag in graph().tags))
            with self.assertRaises(ValueError):
                wire.encode(changed, graph().tags, ids)
        with self.assertRaises(ValueError):
            wire.select(dict(data, rows=(('en', len(data['maps'])),)), ('en',))

    def test_malformed_extents_references_defaults_and_canonicality(self):
        data, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, ids)
        version, codes, fractions, vectors, maps = struct.unpack_from('<5I', raw)
        fraction_start = wire.HEADER + codes
        locale_start = fraction_start + fractions * 5
        map_start = locale_start + count
        vector_start = map_start + maps * codes
        changes = [(0, version + 1), (1, 0), (2, codes + 1), (3, vectors + 1), (4, 0),
                   (5, 101), (7, 101), (wire.HEADER, len(pool)),
                   (fraction_start, codes), (fraction_start + 1, 101),
                   (fraction_start + 3, 101), (locale_start, maps),
                   (map_start, vectors), (vector_start, 0), (vector_start, len(pool))]
        for index, value in changes:
            changed = bytearray(raw)
            struct.pack_into('<I', changed, index * 4, value)
            with self.assertRaises(ValueError, msg=str((index, value))):
                wire.decode((count, bytes(changed)), pool, graph().tags)
        for start, width, rows in ((map_start, codes, maps), (vector_start, wire.FIELD_COUNT, vectors)):
            self.assertGreater(rows, 1)
            changed = bytearray(raw)
            changed[(start + width) * 4:(start + width * 2) * 4] = changed[start * 4:(start + width) * 4]
            with self.assertRaises(ValueError):
                wire.decode((count, bytes(changed)), pool, graph().tags)
        for section in ((0, raw), (count, raw[:-1]), (count, raw + bytes(4)),
                        (count, bytes(wire.HEADER * 4 - 4))):
            with self.assertRaises(ValueError):
                wire.decode(section, pool, graph().tags)

    def test_markers_cannot_be_encoded_as_literal_pool_references(self):
        data, pool, ids = self.fixture()
        count, raw = wire.encode(data, graph().tags, ids)
        version, codes, fractions, vectors, maps = struct.unpack_from('<5I', raw)
        at = wire.HEADER + codes + fractions * 5 + count + maps * codes
        # Increasing the final literal reference preserves canonical ordering.
        field = max(i for i in range(at, at + vectors * wire.FIELD_COUNT)
                    if 0 < struct.unpack_from('<I', raw, i * 4)[0] < len(pool))
        for marker in (BLOCK, INHERIT):
            changed = bytearray(raw)
            struct.pack_into('<I', changed, field * 4, len(pool))
            with self.assertRaisesRegex(ValueError, 'invalid currency text reference'):
                wire.decode((count, bytes(changed)), pool + (marker,), graph().tags)


if __name__ == '__main__':
    unittest.main()
