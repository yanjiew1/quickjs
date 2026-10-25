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

"""Authored group anchors and aliases, independent of downloaded collation data."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import collation_groups as groups
from cldr_xml import DataError

VERSION = '[UCA version = 18.0.0]\n'
# Deliberately retain comments that are not script identities and first real
# keys different from their synthetic representatives. Neither can drive policy.
BODY = '''FDD1 00A0; [03 02 02, 05, 05] # not a space label
0009; [03 04, 05, 05]
FDD1 201C; [05 06 02, 05, 05]
203E; [05 06 04, 05, 05]
FDD1 263A; [0C 02 02, 05, 05]
0060; [0C 04, 05, 05]
FDD1 20AC; [0D A9 02, 05, 05]
00A4; [0D A9 04, 05, 05]
FDD1 0034; [0F 02 02, 05, 05]
FDD0 0034; [10, 05, 05]
0030; [12, 05, 05]
FDD0 FF21; [28 02 02, 05, 05]
FDD1 004C; [2A 02 02, 05, 05]
0061; [2A 02 04, 05, 05]
FDD0 FF3A; [5F 02 02, 05, 05]
FDD1 03A9; [61 04 02, 05, 05]
03B1; [61 04 04, 05, 05]
FDD1 03E2; [61 60 02, 05, 05]
2C81; [61 60 04, 05, 05]
FDD1 304B; [7D 04 02, 05, 05]
FDD1 30AB; [7D 04 02, 05, 05]
3041; [7D 04 04, 05, 05]
FDD1 109A0; [80 3B 02, 05, 05]
FDD1 10980; [80 3B 02, 05, 05]
10980; [80 3B 04, 05, 05]
FDD1 18229; [81 04 02, 05, 05]
17000; [81 04 04, 05, 05]
FDD1 5B57; [82 02 02, 05, 05]
2F00; [U+4E00, 10]
FDD1 FDD0; [E4, 05, 05]
FFFE; [02, 05, 05]
FFFF; [EF FF, 05, 05]
'''
TOKENS = '''SPACE 03=12
PUNCTUATION 05=34
SYMBOL 0C=56
CURRENCY 0D=1 0E=1
DIGIT 0F=1 10=1 12=1
Latn 2A=50
Grek 61=647
Copt 61=122
Hira 7D=1
Kana 7D=696
Hrkt 7D=1
Merc 80=27
Mero 80=32
Tang 81=1
Hani 82=1 83=1
Hans 82=1 83=1
Hant 82=1 83=1
FIELD-SEPARATOR 02=1
IMPLICIT E4=5
TRAILING EF=11
'''
FOOTER = ''.join('[reorderingTokens ' + line + ' ]\n' for line in TOKENS.splitlines())
RAW = (VERSION + BODY + FOOTER).encode()
METADATA = b'''# Extra composite samples do not establish reorder aliases.
Latn; 2; 004C; ignored extra field
Grek; 1; 03A9
Copt; 1; 03E2
Hira; 1; 304B
Kana; 1; 30AB
Jpan; 1; 304B
Merc; 1; 109A0
Mero; 1; 10980
Tang; 1; 18229
Hani; 1; 5B57
Hans; 1; 5B57
Hant; 1; 5B57
Hanb; 1; 5B57
'''


def parse(raw=RAW, metadata=METADATA):
    return groups.parse(raw, metadata)


class GroupTests(unittest.TestCase):
    def test_first_real_keys_and_special_groups(self):
        data = parse()
        by_token = {token: group for group in data for token in group.tokens}
        for token, key in (('space', 9), ('punct', 0x203e), ('symbol', 0x60),
                           ('currency', 0xa4), ('digit', 0x30), ('Latn', 0x61)):
            self.assertEqual(by_token[token].first_key, (key,))
        self.assertEqual(by_token['digit'].samples, (0x34,))
        self.assertEqual(data[-1].kind, 'unassigned')
        self.assertIsNone(data[-1].first_key)
        self.assertEqual(data[-1].tokens, ())  # Not a separately selectable script.
        self.assertNotIn('FIELD-SEPARATOR', by_token)
        self.assertNotIn('TRAILING', by_token)
        self.assertEqual([g.line for g in data], sorted(g.line for g in data))

    def test_shared_scripts_and_coarse_lead_byte_collision(self):
        by_token = {token: group for group in parse() for token in group.tokens}
        self.assertIs(by_token['Hira'], by_token['Kana'])
        self.assertIs(by_token['Hrkt'], by_token['Kana'])
        self.assertIs(by_token['Merc'], by_token['Mero'])
        self.assertIsNot(by_token['Grek'], by_token['Copt'])
        self.assertEqual(by_token['Grek'].fractional[0], by_token['Copt'].fractional[0])
        self.assertIs(by_token['Hani'], by_token['Hans'])
        self.assertIs(by_token['Hani'], by_token['Hant'])
        self.assertNotIn('Hanb', by_token)
        self.assertNotIn('Jpan', by_token)

    def test_anchor_keys_do_not_require_stored_root_rows(self):
        # Tangut uses algorithmic implicit weights, resolved later by the host
        # semantic compiler. Do not parse a comment or copy fractional weights.
        tangut = next(g for g in parse() if 'Tang' in g.tokens)
        self.assertEqual(tangut.first_key, (0x17000,))
        self.assertEqual(tangut.fractional, (0x81, 4, 2))

    def test_missing_or_changed_versions(self):
        for raw in (b'', RAW.replace(VERSION.encode(), b''),
                    RAW.replace(b'18.0.0', b'17.0.0'), VERSION.encode() + RAW, b'\xff'):
            with self.subTest(raw=raw[:50]), self.assertRaises(DataError):
                parse(raw)

    def test_boundary_integrity(self):
        changes = [(b'0061; [2A 02 04, 05, 05]\n', b''),
                   (b'FDD1 03E2;', b'FDD1 03A9;'),
                   (b'[61 60 02, 05, 05]', b'[60 60 02, 05, 05]'),
                   (b'FDD1 004C;', b'FDD1 D800;'),
                   (b'FDD1 004C;', b'FDD1 110000;'),
                   (b'FDD1 004C;', b'FDD1 004C 0061;'),
                   (b'[2A 02 02, 05, 05]', b'[2A 00 02, 05, 05]'),
                   (b'[2A 02 02, 05, 05]', b'[2A02, 05, 05]'),
                   (b'FDD1 FDD0; [E4, 05, 05]\n', b''),
                   (b'3041; [7D 04 04, 05, 05]', b'304B | 3041; [7D 04, 05, 05]')]
        for before, after in changes:
            with self.subTest(before=before), self.assertRaises(DataError):
                parse(RAW.replace(before, after))
        with self.assertRaises(DataError):
            parse(RAW + b'FDD1 004C; [EF, 05, 05]\n')

    def test_every_core_group_is_required(self):
        for sample in groups.SPECIAL:
            prefix = 'FDD1 {:04X};'.format(sample)
            line = next(line for line in BODY.splitlines() if line.startswith(prefix))
            with self.subTest(sample=sample), self.assertRaises(DataError):
                parse(RAW.replace((line + '\n').encode(), b''))

    def test_footer_identity_and_alias_checks(self):
        for before, after in ((b'Latn 2A=50', b'Latn 2A=0'),
                              (b'Latn 2A=50', b'Latn 2A=1 2A=2'),
                              (b'Latn 2A=50', b'Latn 2B=1 2A=2'),
                              (b'Latn 2A=50', b'Latn 2A=bad'),
                              (b'Hans 82=1 83=1', b'Hans 82=2'),
                              (b'SPACE 03=12', b'MYSTERY 03=12'),
                              (b'FDD1 30AB; [7D 04 02, 05, 05]',
                               b'FDD1 30AB; [7D 05 02, 05, 05]')):
            with self.subTest(before=before), self.assertRaises(DataError):
                parse(RAW.replace(before, after))
        with self.assertRaises(DataError):
            parse(RAW + b'[reorderingTokens Latn 2A=50 ]\n')
        # A sample alone is insufficient, but an explicit future footer token
        # with the same sample/ranges can establish a new shared identity.
        extended = parse(RAW + b'[reorderingTokens Jpan 7D=1 ]\n')
        self.assertTrue(any({'Jpan', 'Hira'}.issubset(g.tokens) for g in extended))

    def test_script_metadata_failures(self):
        for metadata in (b'', b'\xff', METADATA + b'Latn; 1; 004C\n',
                         METADATA.replace(b'004C', b'D800'),
                         METADATA.replace(b'004C', b'110000'),
                         METADATA.replace(b'Latn;', b'latin;')):
            with self.subTest(metadata=metadata[:30]), self.assertRaises(DataError):
                parse(metadata=metadata)

    def test_authenticated_source_boundary(self):
        class Sources:
            def __init__(self):
                self.consumed = []

            def read(self, name):
                self.consumed.append(name)
                return {groups.SOURCE: RAW, groups.SCRIPTS: METADATA}[name]
        sources = Sources()
        self.assertEqual(groups.collect(sources), parse())
        self.assertEqual(sources.consumed, [groups.SOURCE, groups.SCRIPTS])


if __name__ == '__main__':
    unittest.main()
