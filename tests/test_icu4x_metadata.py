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

"""Check generated locale metadata through its real compiled C contract."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/intl'))
import icu4x_metadata

COMPILER = sys.argv[1:] or shlex.split(os.environ.get('CC', 'cc'))


def fixture():
    return dict(version='49.0.0',
                keys=[('u', 'ca', 'incremental', ('calendar',), '', False)],
                types=[('u', 'ca', 'gregory', ('gregorian', 'quote"back\\slash'), '', False, False, '', ''),
                       ('u', 'ca', 'islamic-civil', (), '', False, False, '', ''),
                       ('u', 'ca', 'islamicc', (), 'islamic-civil', True, False, '', '')],
                weeks=[('001', 1, 1, (6, 7))], hours=[('001', ('h12', 'h23'))],
                calendars=[('001', ('gregorian', 'islamic-civil'))],
                unknown_scripts=['Brai'], segment_locales=['en', 'en-US'])


class MetadataTest(unittest.TestCase):
    def test_compiled_contract(self):
        data = fixture()
        rendered = icu4x_metadata.render(data)
        # Inspect semantic fields after the C compiler resolves offsets and
        # escapes. Comparing source text alone would miss an incorrect pool.
        source = r'''#include "src/intl/icu4x/locale-data.h"
#include <assert.h>
#include <string.h>
int main(void) {
 const QJSICU4XLocaleAliases *d = &qjs_icu4x_locale_aliases;
 const char *s = d->strings;
 assert(!s[0] && !strcmp(s + d->version, "49.0.0"));
 assert(d->key_count == 1 && d->type_count == 3);
 assert(!strcmp(s + d->keys[0].name, "ca") && d->keys[0].value_type == 2);
 assert(!strcmp(s + d->types[0].name, "gregory"));
 assert(!strcmp(s + d->types[0].aliases, EXPECTED_ALIAS));
 assert(!d->types[1].deprecated && d->types[2].deprecated);
 assert(!strcmp(s + d->types[2].preferred, "islamic-civil"));
 assert(qjs_icu4x_locale_week_count == 1);
 assert(qjs_icu4x_locale_weeks[0].weekend == 96);
 assert(!strcmp(s + qjs_icu4x_locale_hours[0].value, "h12 h23"));
 assert(!strcmp(s + qjs_icu4x_locale_calendars[0].value, "gregorian islamic-civil"));
 assert(!strcmp(s + qjs_icu4x_locale_unknown_scripts[0], "Brai"));
 assert(qjs_icu4x_segment_locale_count == 2);
 assert(!strcmp(s + qjs_icu4x_segment_locales[1], "en-US"));
 return 0;
}
'''
        source = source.replace('EXPECTED_ALIAS', json.dumps(' '.join(data['types'][0][3])))
        with tempfile.TemporaryDirectory(prefix='icu4x-metadata-') as temporary:
            directory = Path(temporary)
            generated, probe = directory / 'data.c', directory / 'probe.c'
            generated.write_text(rendered)
            probe.write_text(source)
            executable = directory / 'probe.exe'
            subprocess.check_call(COMPILER + ['-std=c11', '-UNDEBUG', '-I' + str(ROOT),
                                  str(generated), str(probe), '-o', str(executable)], timeout=60)
            wrapper = shlex.split(os.environ.get('MESON_EXE_WRAPPER', ''))
            subprocess.check_call(wrapper + [str(executable)], timeout=30)

    def test_rejects_non_ascii_metadata(self):
        data = fixture()
        data['unknown_scripts'] = ['not-ASCII-\u200b']
        with self.assertRaises(ValueError):
            icu4x_metadata.render(data)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]])
