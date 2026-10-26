#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
# Copyright (c) 2026 Yan-Jie Wang
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

"""Validate missing number facts through the generated target C layout."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/intl'))
import icu4x_number as number
from cldr_xml import DataError
from resolve import BLOCK

COMPILER = sys.argv[1:] or shlex.split(os.environ.get('CC', 'cc'))


def fixture():
    fields = ('%', '+', '-', '', 'اس', '‰', '∞', 'NaN ??/ \"', '{1} to {0}', '~{0}', '.', ',')
    patterns = ('#,##0.###', '0.###E+00', '0%', '¤0', '¤0;(¤0)', None, BLOCK, '', "'-'0")
    rows = ((fields, patterns, (3, 3, 1)),) * 2
    return {'version': '49.0.0', 'systems': ('arab', 'latn'),
            'digits': (tuple(range(0x660, 0x66a)), tuple(range(48, 58))),
            'locales': (('en', rows), ('en-US', rows))}


class NumberSupplementTests(unittest.TestCase):
    def test_compiled_semantics_and_shared_records(self):
        generated = number.render(fixture())
        probe = r'''#include "src/intl/number/icu4x-data.h"
#include <assert.h>
#include <string.h>
int main(void) {
 const char *s = qjs_icu4x_number_strings;
 const QJSICU4XNumberLocale *a = &qjs_icu4x_number_locales[0];
 const QJSICU4XNumberLocale *b = &qjs_icu4x_number_locales[1];
 const QJSICU4XNumberRecord *r = &qjs_icu4x_number_records[qjs_icu4x_number_rows[a->row]];
 const QJSICU4XNumberPattern *p;
 unsigned i;
 const QJSICU4XNumberToken *token;
 assert(qjs_icu4x_number_system_count == 2 && qjs_icu4x_number_locale_count == 2);
 assert(!strcmp(s + qjs_icu4x_number_version, "49.0.0"));
 assert(!strcmp(s + qjs_icu4x_number_systems[0], "arab"));
 assert(!strcmp(s + qjs_icu4x_number_systems[1], "latn"));
 assert(!strcmp(s + a->tag, "en") && !strcmp(s + b->tag, "en-US"));
 assert(a->row == b->row);
 assert(qjs_icu4x_number_rows[a->row] == qjs_icu4x_number_rows[a->row+1]);
 assert(r->fields[3] == 0);
 assert(!strcmp(s+r->fields[10], ".") && !strcmp(s+r->fields[11], ","));
 assert(r->grouping[0] == 3 && r->grouping[1] == 3 && r->grouping[2] == 1);
 for (i = 0; i < 10; i++) {
   assert(qjs_icu4x_number_digits[0][i] == 0x660+i);
   assert(qjs_icu4x_number_digits[1][i] == 48+i);
 }
 assert(!strcmp(s+r->fields[4], "\330\247\330\263"));
 assert(!strcmp(s+r->fields[7], "NaN \077\077/ \""));
 assert(!strcmp(s+r->fields[8], "{1} to {0}"));
 assert(r->patterns[5] == QJS_ICU4X_NUMBER_ABSENT);
 assert(r->patterns[6] == QJS_ICU4X_NUMBER_BLOCKED);
 assert(r->patterns[7] == QJS_ICU4X_NUMBER_EMPTY);
 /* Numeric skeleton defaults, including E+00, do not alter ECMA's
    exponent semantics; identical outer affixes share a pattern. */
 assert(r->patterns[0] == r->patterns[1]);
 p = &qjs_icu4x_number_patterns[r->patterns[8]];
 token = &qjs_icu4x_number_tokens[p->forms[0].begin];
 /* A quoted hyphen remains literal; it is not a minusSign placeholder. */
 assert(p->forms[0].count == 2 && token[0].kind == 0 && !strcmp(s+token[0].text, "-"));
 assert(token[1].kind == 1);
 p = &qjs_icu4x_number_patterns[r->patterns[0]];
 token = &qjs_icu4x_number_tokens[p->forms[1].begin];
 assert(token[0].kind == 2); /* Default negative affix uses semantic minus. */
 token = &qjs_icu4x_number_tokens[p->forms[2].begin];
 assert(token[0].kind == 3); /* Explicit plus substitutes only semantic minus. */
 return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='icu4x-number-') as temporary:
            directory = Path(temporary)
            data, source = directory / 'data.c', directory / 'test.c'
            data.write_text(generated)
            source.write_text(probe)
            executable = directory / 'test.exe'
            subprocess.check_call(COMPILER + ['-std=c11', '-Wall', '-Wextra', '-Werror', '-UNDEBUG',
                                  '-I'+str(ROOT), str(data), str(source), '-o', str(executable)], timeout=60)
            wrapper = shlex.split(os.environ.get('MESON_EXE_WRAPPER', ''))
            subprocess.check_call(wrapper + [str(executable)], timeout=30)

    def test_invalid_identities_and_extents(self):
        for changes in ({'systems': ()}, {'systems': ('latn', 'latn')},
                        {'locales': ()}, {'locales': (('en', ()),)}):
            data = fixture()
            data.update(changes)
            with self.assertRaises(DataError):
                number.render(data)

    def test_invalid_numeric_fallback(self):
        for digits in (((48,)*10, tuple(range(48, 58))),
                       (tuple(range(48, 57)), tuple(range(48, 58))),
                       ((0xd800,)+tuple(range(49, 58)), tuple(range(48, 58)))):
            data = fixture()
            data['digits'] = digits
            with self.assertRaises(DataError):
                number.render(data)
        data = fixture()
        fields, patterns, grouping = data['locales'][0][1][0]
        for grouping in ((0, 3, 1), (3, 3, 0), (3, 3, 10)):
            data['locales'] = (('en', ((fields, patterns, grouping),)*2),)
            with self.assertRaises(DataError):
                number.render(data)

    def test_invalid_pattern_and_nul(self):
        data = fixture()
        row = data['locales'][0][1][0]
        for fields, patterns in ((row[0], ('broken',)*9), (('bad\0text',)*12, row[1])):
            data['locales'] = (('en', ((fields, patterns, row[2]),)*2),)
            with self.assertRaises(ValueError):
                number.render(data)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]])
