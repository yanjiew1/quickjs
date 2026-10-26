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

"""Check the narrow display supplement's source policy and real compiled data."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/intl'))
import display_names
import icu4x_display as display
from cldr_xml import DataError
from test_intl_display_names import Sources
from test_intl_list_patterns import graph
from resolve import BLOCK

COMPILER = sys.argv[1:] or shlex.split(os.environ.get('CC', 'cc'))


def source_fixture():
    source = Sources()
    source.set('root', '<scripts><script type="Hans">Simplified</script>'
                      '<script type="Hans" alt="stand-alone">Simplified Han</script></scripts>'
                      '<types><type key="calendar" type="gregorian">Gregorian</type></types>')
    source.set('en', '<scripts><script type="Hans">Child ordinary</script></scripts>'
                    '<types><type key="calendar" type="gregorian" alt="short">Grego</type></types>')
    source.set('en_GB', '<scripts><script type="Hans" alt="stand-alone"></script></scripts>')
    return source


class DisplaySupplementTests(unittest.TestCase):
    def test_standalone_priority_aliases_and_empty(self):
        data = display_names.collect(source_fixture(), graph(), groups=('script', 'calendar'))
        metadata = {'types': [('u', 'ca', 'gregory', ('gregorian',), '', False, False, '', '')]}
        projection = display.calendar_sources(metadata, data)
        self.assertEqual(projection, {'gregory': 'gregorian'})
        maps = display.name_maps(data, projection)
        self.assertEqual(maps['root', 'script'],
                         tuple(('Hans', style, 'Simplified Han') for style in range(3)))
        self.assertEqual(maps['en', 'script'], ())
        self.assertEqual(maps['en-GB', 'script'], tuple(('Hans', style, '') for style in range(3)))
        self.assertEqual(maps['en', 'calendar'],
                         (('gregory', 0, 'Gregorian'), ('gregory', 1, 'Grego'), ('gregory', 2, 'Grego')))

    def test_calendar_public_aliases_and_preferred_chains(self):
        source = Sources()
        source.set('root', '<types><type key="calendar" type="islamic-civil">Civil</type>'
                          '<type key="calendar" type="ethiopic-amete-alem">Alem</type></types>')
        names = display_names.collect(source, graph(), groups=('script', 'calendar'))
        metadata = {'types': [
            ('u', 'ca', 'islamicc', (), 'islamic-civil', True, False, '', ''),
            ('u', 'ca', 'islamic-civil', (), '', False, False, '', ''),
            ('u', 'ca', 'ethioaa', ('ethiopic-amete-alem',), '', False, False, '', '')]}
        projection = display.calendar_sources(metadata, names)
        self.assertEqual(projection, {'islamicc': 'islamic-civil',
                                     'islamic-civil': 'islamic-civil',
                                     'ethioaa': 'ethiopic-amete-alem',
                                     'ethiopic-amete-alem': 'ethiopic-amete-alem'})
        maps = display.name_maps(names, projection)
        self.assertIn(('islamicc', 0, 'Civil'), maps['en', 'calendar'])
        self.assertIn(('ethiopic-amete-alem', 0, 'Alem'), maps['en', 'calendar'])
        metadata['types'][0] = ('u', 'ca', 'islamicc', (), 'islamicc', True, False, '', '')
        with self.assertRaises(DataError):
            display.calendar_sources(metadata, names)

    def test_blocked_alternative_does_not_revive_parent(self):
        source = source_fixture()
        source.set('en', '<scripts><script type="Hans" alt="stand-alone">'+BLOCK+'</script></scripts>')
        data = display_names.collect(source, graph(), groups=('script', 'calendar'))
        maps = display.name_maps(data, {})
        self.assertEqual(maps['en', 'script'], ())

    def test_normalization_collision_is_rejected(self):
        source = source_fixture()
        source.set('en', '<scripts><script type="Hans">One</script><script type="hans">Two</script></scripts>')
        data = display_names.collect(source, graph(), groups=('script', 'calendar'))
        with self.assertRaises(DataError):
            display.name_maps(data, {})

    def test_context_override(self):
        source = Sources()
        source.files['common/dtd/ldml.dtd'] += b"""
<!ELEMENT contextTransforms ANY><!ELEMENT contextTransformUsage ANY>
<!ATTLIST contextTransformUsage type NMTOKEN #REQUIRED>
<!ELEMENT contextTransform (#PCDATA)><!ATTLIST contextTransform type NMTOKEN #REQUIRED>"""
        for locale, value in [('root', 'titlecase-firstword'), ('en', 'no-change')]:
            path = 'common/main/'+locale+'.xml'
            extra = ('<contextTransforms><contextTransformUsage type="languages">'
                     '<contextTransform type="stand-alone">'+value+'</contextTransform>'
                     '</contextTransformUsage></contextTransforms></ldml>')
            source.files[path] = source.files[path].replace(b'</ldml>', extra.encode())
        flags = display.context_flags(source, graph())
        self.assertEqual(flags['root'], 1)
        self.assertEqual(flags['de'], 1)
        self.assertEqual(flags['en'], 0)
        self.assertEqual(flags['en-GB'], 0)

    def test_compiled_utf8_offsets_width_masks_and_map_interning(self):
        labels = (('Hans', 0, 'Han \" \\ 漢 ??/'), ('Hans', 1, 'Han \" \\ 漢 ??/'),
                  ('Hans', 2, ''))
        data = {'version': '49.0.0', 'locales': (
            ('en', labels, (), (), 0), ('en-US', labels, (), (), 0))}
        rendered = display.render(data)
        probe = r'''#include "src/intl/display/icu4x-data.h"
#include <assert.h>
#include <string.h>
int main(void) {
 const char *s = qjs_icu4x_display_strings;
 const QJSICU4XDisplayLocale *a = &qjs_icu4x_display_locales[0];
 const QJSICU4XDisplayLocale *b = &qjs_icu4x_display_locales[1];
 const QJSICU4XDisplayMap *map = &qjs_icu4x_display_maps[a->maps[0]];
 unsigned i, ordinary = 0, empty = 0;
 assert(qjs_icu4x_display_locale_count == 2);
 assert(!strcmp(s + qjs_icu4x_display_version, "49.0.0"));
 assert(!strcmp(s + a->tag, "en") && !strcmp(s + b->tag, "en-US"));
 assert(a->maps[0] == b->maps[0] && map->count == 2);
 for (i=0; i<map->count; i++) {
   const QJSICU4XDisplayLabel *label = &qjs_icu4x_display_labels[map->begin+i];
   assert(!strcmp(s+label->code, "Hans"));
   if (label->styles == 3) {
     assert(!strcmp(s+label->text, "Han \" \\ \346\274\242 \077\077/"));
     ordinary++;
   } else {
     assert(label->styles == 4 && label->text == 0);
     empty++;
   }
 }
 assert(ordinary == 1 && empty == 1);
 assert(qjs_icu4x_display_maps[a->maps[1]].count == 0);
 return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='icu4x-display-') as temporary:
            directory = Path(temporary)
            generated, test = directory / 'data.c', directory / 'test.c'
            generated.write_text(rendered)
            test.write_text(probe)
            executable = directory / 'test.exe'
            subprocess.check_call(COMPILER+['-std=c11', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', '-I'+str(ROOT),
                                  str(generated), str(test), '-o', str(executable)], timeout=60)
            wrapper = shlex.split(os.environ.get('MESON_EXE_WRAPPER', ''))
            subprocess.check_call(wrapper+[str(executable)], timeout=30)

    def test_invalid_tables(self):
        with self.assertRaises(DataError):
            display.render({'version': '49.0.0', 'locales': ()})
        with self.assertRaises(ValueError):
            display.c_string('bad\0label')


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]])
