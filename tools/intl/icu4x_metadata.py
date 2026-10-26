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

"""Emit the narrow CLDR supplement for gaps in ICU4X public metadata.

Reuse the source-schema collectors. Generation accepts compatible supplied
files; authentication belongs to acquisition, not to this compiler stage.
Language aliases, likely subtags and real formatting data stay in ICU4X.
"""
import argparse
import json
from pathlib import Path
import re
import sys

import acquire
import availability
import locale_metadata
import parents
import preferences
from embed import WRAPPER_LICENSE


def cldr_version(sources):
    release = str(sources.pin['release'])
    if not re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,2}', release):
        raise ValueError('a numeric CLDR release is required for ICU4X metadata')
    version = '.'.join((release.split('.') + ['0', '0'])[:3])
    return version


def collect(sources):
    graph = parents.load(sources)
    metadata = locale_metadata.collect(sources, graph)
    facts = preferences.collect(sources)
    registry, _ = availability.complete(graph)
    version = cldr_version(sources)
    return dict(version=version, keys=metadata['keys'], types=metadata['types'],
                weeks=facts['weeks'], hours=facts['hours'], calendars=facts['calendars'],
                unknown_scripts=tuple(key for key, direction in facts['scripts'] if not direction),
                registry=tuple(tag for tag in registry.tags if tag != 'root'))


def render(data):
    # Whole-string interning keeps the tiny metadata tables pointer-free. The
    # C compiler supplies target endianness/alignment; there is no wire decoder.
    strings = {'', data['version']}
    for extension, key, kind, aliases, preferred, deprecated in data['keys']:
        strings.update((key, ' '.join(aliases), preferred))
    for extension, key, name, aliases, preferred, deprecated, variable, iana, region in data['types']:
        strings.update((name, ' '.join(aliases), preferred, iana, region))
    for family in ('hours', 'calendars'):
        for name, values in data[family]:
            strings.update((name, ' '.join(values)))
    strings.update(data['unknown_scripts'])
    strings.update(data['registry'])
    pool, length = {}, 0
    for value in sorted(strings):
        if '\0' in value or any(ord(char) > 127 for char in value):
            raise ValueError('ICU4X metadata strings must be NUL-free ASCII')
        pool[value] = length
        length += len(value) + 1
    if length > 0xffffffff or len(data['keys']) > 0xffff:
        raise ValueError('ICU4X metadata exceeds compiled field bounds')
    lines = [WRAPPER_LICENSE, '/* CLDR '+data['version']+'; data license: data/intl/LICENSE-CLDR. */',
             '#include "src/intl/icu4x/locale-data.h"', '',
             'const char qjs_icu4x_locale_strings[] =']
    lines.extend('    ' + json.dumps(value)[:-1] + r'\0"' for value in sorted(pool))
    lines.append('    ;\n')

    def array(ctype, name, rows, public=True):
        prefix = '' if public else 'static '
        lines.append(prefix+'const '+ctype+' '+name+'[] = {')
        lines.extend('    {'+', '.join(str(value) for value in row)+'},' for row in rows)
        if not rows:
            lines.append('    {0},')
        lines.append('};\n')

    key_ids = {tuple(row[:2]): index for index, row in enumerate(data['keys'])}
    kinds = locale_metadata.VALUE_TYPES
    array('QJSICU4XLocaleKey', 'locale_keys', [
        (pool[key], pool[' '.join(aliases)], pool[preferred], ord(extension), kinds.index(kind),
         int(deprecated), 0) for extension, key, kind, aliases, preferred, deprecated in data['keys']], False)
    array('QJSICU4XLocaleType', 'locale_types', [
        (key_ids[(extension,key)], int(deprecated), int(variable), pool[name], pool[' '.join(aliases)],
         pool[preferred], pool[iana], pool[region])
        for extension,key,name,aliases,preferred,deprecated,variable,iana,region in data['types']], False)
    lines += ['const QJSICU4XLocaleAliases qjs_icu4x_locale_aliases = {',
              '    qjs_icu4x_locale_strings, '+str(length)+', '+str(pool[data['version']])+',',
              '    locale_keys, '+str(len(data['keys']))+', locale_types, '+str(len(data['types'])), '};\n']
    array('QJSICU4XWeek', 'qjs_icu4x_locale_weeks', [
        (json.dumps(region), first, minimum, sum(1 << (day-1) for day in weekend), 0)
        for region,first,minimum,weekend in data['weeks']])
    for family in ('hours','calendars'):
        array('QJSICU4XPreference', 'qjs_icu4x_locale_'+family,
              [(pool[name],pool[' '.join(values)]) for name,values in data[family]])
    for family, symbol in [('unknown_scripts','qjs_icu4x_locale_unknown_scripts'),
                           ('registry','qjs_icu4x_locale_registry')]:
        lines.append('const uint32_t '+symbol+'[] = {')
        values = [str(pool[name]) for name in data[family]] or ['0']
        lines.extend('    '+', '.join(values[i:i+12])+',' for i in range(0,len(values),12))
        lines.append('};\n')
    for symbol,count in [('week',len(data['weeks'])),('hour',len(data['hours'])),
                         ('calendar',len(data['calendars'])),('unknown_script',len(data['unknown_scripts']))]:
        lines.append('const size_t qjs_icu4x_locale_'+symbol+'_count = '+str(count)+';')
    lines.append('const size_t qjs_icu4x_locale_registry_count = '+str(len(data['registry']))+';')
    return '\n'.join(lines)+'\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--component', choices=('locale', 'display', 'number'), default='locale')
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--source-pin', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources, acquire.load_pin(args.source_pin))
    if args.component in ('display', 'number'):
        if args.component == 'display':
            import icu4x_display as component
        else:
            import icu4x_number as component
        data = component.collect(sources)
        data['version'] = cldr_version(sources)
        output = component.render(data)
    else:
        data = collect(sources)
        output = render(data)
    acquire.atomic_write(args.output, output.encode('ascii'))
    print('Generated ICU4X CLDR '+data['version']+' supplement: '+str(args.output))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as error:
        print('icu4x metadata: ' + str(error), file=sys.stderr)
        sys.exit(1)
