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

"""Check a narrow unit supplement with the actual shared target C decoders."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/intl'))
from cldr_xml import DataError
from resolve import BLOCK
import icu4x_unit as supplement
import unit_patterns
import unit_grammar
from test_intl_unit_patterns import Sources, graph
from test_intl_unit_grammar import Sources as GrammarSources, graph as grammar_graph

COMPILER = sys.argv[1:] or shlex.split(os.environ.get('CC', 'cc'))
GROUPS = (('compound', 'per'), ('unit', 'length-meter'))


def source():
    result = Sources()
    raw = result.files['common/main/root.xml'].decode('utf-8')
    raw = raw.replace('</unitLength>', '<unit type="length-unused"><unitPattern count="other">'
                      '{0} unused</unitPattern></unit><unit type="length-middle">'
                      '<alias source="locale" path="../unit[@type=\'duration-second\']"/>'
                      '</unit></unitLength>', 1)
    result.files['common/main/root.xml'] = raw.encode('utf-8')
    result.set('en', '''<unitLength type="long"><unit type="length-meter">
      <alias source="root" path="../unit[@type='length-middle']"/>
      </unit></unitLength>''')
    result.set('en_GB', '<unitLength type="long"><unit type="length-meter">'
               '<unitPattern count="one">'+BLOCK+'</unitPattern>'
               '<unitPattern count="other"/></unit></unitLength>')
    return result


class UnitSupplementTests(unittest.TestCase):
    def test_sanctioned_inventory_and_alias_closure(self):
        self.assertEqual(len(supplement.sanctioned_groups()), 51)
        data = unit_patterns.collect(source(), graph(), selected_groups=GROUPS)
        self.assertEqual(data['groups'], (('compound', 'per'), ('unit', 'duration-second'),
                                           ('unit', 'length-meter'), ('unit', 'length-middle')))
        self.assertTrue(any(node[2:4] == ('unit', 'duration-second')
                            for node in data['overlay_nodes']))
        with self.assertRaisesRegex(DataError, 'unknown selected unit group'):
            unit_patterns.collect(source(), graph(), selected_groups=(('unit', 'missing'),))

    def test_compiled_source_states_and_section_scope(self):
        inheritance = grammar_graph()
        data = dict(version='49.0.0', graph=inheritance,
                    units=unit_patterns.collect(source(), inheritance, selected_groups=GROUPS,
                                                selected_fields=supplement.FORMAT_FIELDS),
                    grammar=unit_grammar.collect(GrammarSources(), inheritance))
        rendered = supplement.render(data)
        self.assertEqual(rendered, supplement.render(data))
        probe = r'''#include "src/intl/icu4x/unit-data.h"
#include "src/intl/unit/data.h"
#include "src/intl/unit/grammar-data.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static void *allocate(void *p, size_t n) { (void)p; return malloc(n); }
static void release(void *p, void *v) { (void)p; free(v); }
int main(void) {
 QJSIntlAllocator allocator = {NULL, allocate, NULL, release};
 QJSIntlData data;
 QJSIntlUnitData units;
 QJSIntlUnitGrammarData grammar;
 QJSIntlSection section;
 QJSIntlUnitOverlay layer;
 QJSIntlUnitForm form;
 QJSIntlUnitMap map;
 QJSIntlBytes name;
 uint32_t locale, parent, british = UINT32_MAX, root = UINT32_MAX, group, head;
 assert(!strcmp(qjs_icu4x_unit_version, "49.0.0"));
 assert(!qjs_intl_data_open(qjs_icu4x_unit_bytes, qjs_icu4x_unit_length, &allocator, &data));
 assert(qjs_intl_pack_section(&data.pack, QJS_INTL_SECTION_PLURALS, &section) == QJS_INTL_UNSUPPORTED);
 assert(!qjs_intl_unit_data_open(&data, &allocator, &units));
 assert(!qjs_intl_unit_grammar_data_open(&data, &allocator, &grammar));
 assert(units.group_count == 4);
 for (locale = 0; locale < data.locales.count; locale++) {
  assert(!qjs_intl_data_locale(&data, locale, &name, &parent));
  if (name.length == 4 && !memcmp(name.data, "root", 4)) root = locale;
  if (name.length == 5 && !memcmp(name.data, "en-GB", 5)) british = locale;
 }
 assert(british != UINT32_MAX && root != UINT32_MAX);
 assert(!qjs_intl_unit_group_find(&units, QJS_INTL_UNIT_GROUP_SINGLE,
                                 (QJSIntlBytes){"length-meter", 12}, &group));
 assert(!qjs_intl_unit_map_at(&units, root, QJS_INTL_UNIT_LONG, group, &map));
 assert(qjs_intl_unit_form_find(&units, &map, QJS_INTL_UNIT_DISPLAY_NAME,
                               QJS_INTL_UNIT_COUNT_NONE, (QJSIntlBytes){0}, &form) == QJS_INTL_UNSUPPORTED);
 assert(qjs_intl_unit_form_find(&units, &map, QJS_INTL_UNIT_GENDER,
                               QJS_INTL_UNIT_COUNT_NONE, (QJSIntlBytes){0}, &form) == QJS_INTL_UNSUPPORTED);
 assert(!qjs_intl_unit_overlay_head(&units, british, QJS_INTL_UNIT_LONG, group, &head));
 assert(!qjs_intl_unit_overlay_at(&units, head, &layer));
 assert(layer.locale.length == 5 && !memcmp(layer.locale.data, "en-GB", 5));
 assert(!qjs_intl_unit_form_find(&units, &layer.map, QJS_INTL_UNIT_PATTERN,
                                QJS_INTL_UNIT_COUNT_ONE, (QJSIntlBytes){0}, &form));
 assert(form.state == QJS_INTL_UNIT_FORM_BLOCKED);
 assert(!qjs_intl_unit_form_find(&units, &layer.map, QJS_INTL_UNIT_PATTERN,
                                QJS_INTL_UNIT_COUNT_OTHER, (QJSIntlBytes){0}, &form));
 assert(form.state == QJS_INTL_UNIT_FORM_VALUE && !form.text.length);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'data.c').write_text(rendered, encoding='ascii')
            (path / 'probe.c').write_text(probe)
            sources = [str(ROOT / ('src/intl/'+name)) for name in
                       ('native/data.c', 'native/pack.c', 'common/locale-grammar.c', 'common/source-locale.c',
                        'unit/data.c', 'unit/grammar-data.c')]
            subprocess.run(COMPILER + ['-std=c11', '-Wall', '-Werror', '-I'+str(ROOT),
                           str(path/'data.c'), str(path/'probe.c')] + sources +
                           ['-o', str(path/'probe')], check=True, timeout=60)
            subprocess.run([str(path/'probe')], check=True, timeout=10)


if __name__ == '__main__':
    sys.argv[1:] = []
    unittest.main()
