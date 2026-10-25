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

"""Resolve public locale/type targets, separately from source program identity."""
import argparse
from collections import namedtuple
import json
from pathlib import Path
import re

import acquire
import availability
from cldr_xml import DataError
import collation_programs
import parents

Binding = namedtuple('Binding', 'locale collation sort search')
PUBLIC_TYPE = re.compile(r'[a-z0-9]{3,8}(?:-[a-z0-9]{3,8})*')


def eligible(program):
    return (program is not None and not program.alternate and
            not program.collation.startswith('private-'))


def resolve(catalog, locales, aliases=()):
    """Bind one prepared profile without expanding its public locale inventory.

    Empty co means the semantic null/default, never a public keyword named
    'default'. Search initially exposes only null; named type rows are sort-only.
    Targets retain original compiler identities for later checked ID binding.
    """
    locales = tuple(locales)
    if (not locales or 'root' in locales or tuple(sorted(set(locales))) != locales or
            any(parents.locale_id(tag) != tag for tag in locales)):
        raise DataError('collation binding locales must be sorted source identities')

    aliases = dict(aliases)
    for tag, target in aliases.items():
        if (parents.locale_id(tag) != tag or parents.locale_id(target) != target or
                target not in catalog.graph.tags or target in aliases):
            raise DataError('invalid synthetic collation locale target')

    # The BCP47 alias/preferred graph supplies public spellings. Program labels
    # can be legacy LDML names, private helpers or non-normal diagnostic variants.
    types = tuple(sorted(name for name in catalog.spellings
                         if PUBLIC_TYPE.fullmatch(name) and
                         name not in ('default', 'standard', 'search') and
                         not name.startswith('private-')))
    rows = []
    for locale in locales:
        source = aliases.get(locale, locale)
        default = catalog.select(source)
        search = catalog.find(source, 'search')
        if not eligible(default) or not eligible(search):
            raise DataError('missing executable collation default/search: ' + locale)

        rows.append(Binding(locale, '', collation_programs.identity(default),
                            collation_programs.identity(search)))
        for kind in types:
            # Exact inherited type lookup is support. select() can deliberately
            # fall back to another type and therefore cannot establish support.
            program = catalog.find(source, kind)
            if eligible(program):
                rows.append(Binding(locale, kind, collation_programs.identity(program), None))

    return tuple(rows)


def from_catalog(catalog, profile):
    """Reuse one parsed catalog for target selection and binding resolution."""
    graph, aliases = availability.complete(catalog.graph)
    # root owns source fallback, not a public BCP47 service locale. Keep it in
    # prepared formatter tables, but never in executable service bindings.
    locales = tuple(tag for tag in availability.profile_locales(graph, profile) if tag != 'root')
    return resolve(catalog, locales, aliases)


def collect(sources, profile):
    return from_catalog(collation_programs.load(sources), profile)


def program_targets(rows):
    """Sorted standalone targets; ordered imports are compiled into each one."""
    return tuple(sorted({target for row in rows for target in (row.sort, row.search)
                         if target is not None}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--profile', choices=('minimal', 'native'), default='native')
    args = parser.parse_args()
    rows = collect(acquire.Sources(args.sources), args.profile)
    print(json.dumps({'locales': sum(not row.collation for row in rows),
                      'public_types': sorted({row.collation for row in rows if row.collation}),
                      'binding_rows': len(rows),
                      'program_targets': len({target for row in rows for target in
                                             (row.sort, row.search) if target is not None})},
                     sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
