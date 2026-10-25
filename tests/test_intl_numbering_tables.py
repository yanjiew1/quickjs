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

"""Numbering wire repertoire, preference and corruption regressions."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import bundle
import numbering
import numbering_tables as wire
from test_intl_list_patterns import graph
from test_intl_numbering import Sources


class NumberingTableTests(unittest.TestCase):
    def fixture(self):
        sources = Sources()
        sources.set('en_GB', '''<ldml><numbers><otherNumberingSystems>
          <traditional>∅∅∅</traditional>
        </otherNumberingSystems></numbers></ldml>''')
        data = numbering.collect(sources, graph())
        pool = tuple(sorted(wire.strings(data) | {'bad_tag', '0123456788',
                                                'bad rule', 'foo', 'LATN'}))
        return data, pool, {text: index for index, text in enumerate(pool)}

    def test_roundtrip_preserves_scalars_rule_references_and_optional_states(self):
        data, pool, ids = self.fixture()
        self.assertEqual(wire.decode(wire.encode(data, ids), pool),
                         {name: data[name] for name in ('systems', 'locales')})
        self.assertIn(('en-GB', 'mathbold', 'latn', '', None), data['locales'])
        self.assertIsNone(wire.decode({}, pool))

    def test_invalid_definition_fields(self):
        data, pool, ids = self.fixture()
        sections = wire.encode(data, ids)
        identity = bundle.NUMBERING_SYSTEMS
        for word, value in ((0, len(pool)), (0, ids['LATN']), (1, 2),
                            (2, ids['0123456788']), (2, ids['bad rule'])):
            changed = dict(sections)
            count, raw = sections[identity]
            raw = bytearray(raw)
            struct.pack_into('<I', raw, word * 4, value)
            changed[identity] = count, bytes(raw)
            with self.assertRaises(ValueError, msg=str((word, value))):
                wire.decode(changed, pool)
        # The final system is algorithmic, with a checked rule reference.
        changed = dict(sections)
        count, raw = sections[identity]
        raw = bytearray(raw)
        struct.pack_into('<I', raw, (count * 3 - 1) * 4, ids['bad rule'])
        changed[identity] = count, bytes(raw)
        with self.assertRaises(ValueError):
            wire.decode(changed, pool)
        altered = dict(data, systems=(('latn', 'numeric', '9876543210'),) + data['systems'][1:])
        extra_pool = tuple(sorted(set(pool) | {'9876543210'}))
        extra_ids = {text: index for index, text in enumerate(extra_pool)}
        with self.assertRaises(ValueError):
            wire.decode(wire.encode(altered, extra_ids), extra_pool)

    def test_invalid_preference_fields_and_extents(self):
        data, pool, ids = self.fixture()
        sections = wire.encode(data, ids)
        identity = bundle.NUMBERING_PREFERENCES
        for word, value in ((0, len(pool)), (0, ids['bad_tag']),
                            (1, wire.ABSENT), (2, wire.BLOCKED),
                            (3, len(data['systems'])), (4, wire.BLOCKED - 1)):
            changed = dict(sections)
            count, raw = sections[identity]
            raw = bytearray(raw)
            struct.pack_into('<I', raw, word * 4, value)
            changed[identity] = count, bytes(raw)
            with self.assertRaises(ValueError, msg=str((word, value))):
                wire.decode(changed, pool)
        for identity in wire.SECTIONS:
            for change in ('missing', 'empty', 'count', 'truncated', 'order'):
                changed = dict(sections)
                count, raw = sections[identity]
                if change == 'missing':
                    del changed[identity]
                elif change == 'empty':
                    changed[identity] = 0, b''
                elif change == 'count':
                    changed[identity] = count + 1, raw
                elif change == 'truncated':
                    changed[identity] = count, raw[:-1]
                else:
                    raw = bytearray(raw)
                    width = 3 if identity == bundle.NUMBERING_SYSTEMS else 5
                    raw[width * 4:width * 4 + 4] = raw[:4]
                    changed[identity] = count, bytes(raw)
                with self.assertRaises(ValueError, msg=str((identity, change))):
                    wire.decode(changed, pool)

    def test_required_latin_and_root_records(self):
        data, pool, ids = self.fixture()
        sections = wire.encode(data, ids)
        count, raw = sections[bundle.NUMBERING_SYSTEMS]
        modified = bytearray(raw)
        struct.pack_into('<I', modified, 0, ids['foo'])
        changed = dict(sections)
        changed[bundle.NUMBERING_SYSTEMS] = count, bytes(modified)
        with self.assertRaisesRegex(ValueError, 'Latin'):
            wire.decode(changed, pool)
        changed = dict(sections)
        count, raw = sections[bundle.NUMBERING_PREFERENCES]
        changed[bundle.NUMBERING_PREFERENCES] = count - 1, raw[:-20]
        with self.assertRaisesRegex(ValueError, 'root'):
            wire.decode(changed, pool)


if __name__ == '__main__':
    unittest.main()
