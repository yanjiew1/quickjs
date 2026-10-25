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

"""Collect CLDR's root UCA elements without applying locale tailorings."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError

SOURCE = 'common/uca/allkeys_CLDR.txt'
VERSION = '18.0.0'
KEY = re.compile(r'[0-9A-Fa-f]{4,6}(?:[ \t]+[0-9A-Fa-f]{4,6})*\Z')
ELEMENT = re.compile(r'\[([.*])([0-9A-Fa-f]{4})\.([0-9A-Fa-f]{4})\.([0-9A-Fa-f]{4})\]')


def parse(raw, expected_version=VERSION):
    """Return canonical key order and complete, ordered collation elements.

    A key can be a contraction and its value can be an expansion. Neither is
    limited to the maxima seen in one release. Keep the variable flag separate
    from all three weight levels; applying alternate handling belongs to the
    comparator, not collection. Source comments and source row order do not
    define lookup identity or an additional comparison level.
    """
    try:
        text = raw.decode('utf-8')
    except (AttributeError, UnicodeDecodeError) as error:
        raise DataError('invalid root collation text: ' + str(error))
    version = None
    rows = {}
    for number, line in enumerate(text.splitlines(), 1):
        line = line.split('#', 1)[0].strip(' \t')
        if not line:
            continue

        def fail(message):
            raise DataError('{}:{}: {}'.format(SOURCE, number, message))

        if line.startswith('@'):
            # The pinned CLDR root file has only @version. Do not silently
            # discard a future implicit-weight or other executable directive.
            fields = line.split()
            if (len(fields) != 2 or fields[0] != '@version' or version is not None or
                    rows or fields[1] != expected_version):
                fail('unexpected, repeated or mismatched root directive')
            version = fields[1]
            continue
        if version is None:
            fail('mapping precedes version')
        if line.count(';') != 1:
            fail('mapping must contain one separator')
        key, values = (part.strip(' \t') for part in line.split(';'))
        if not KEY.fullmatch(key):
            fail('invalid root code point sequence')
        key = tuple(int(code, 16) for code in key.split())
        if any(code > 0x10ffff or 0xd800 <= code <= 0xdfff for code in key):
            fail('root key is not a Unicode scalar sequence')
        if key in rows:
            fail('repeated root key')
        elements, at = [], 0
        while at < len(values):
            while at < len(values) and values[at] in ' \t':
                at += 1
            if at == len(values):
                break
            match = ELEMENT.match(values, at)
            if match is None:
                fail('invalid root collation element')
            element = (match[1] == '*',) + tuple(int(match[i], 16) for i in (2, 3, 4))
            if element[0] and not element[1]:
                fail('variable element has an ignorable primary weight')
            elements.append(element)
            at = match.end()
        if not elements:
            fail('empty root mapping')
        rows[key] = tuple(elements)
    if version is None or not rows:
        raise DataError('empty root collation source')
    return {'version': version, 'rows': tuple(sorted(rows.items()))}


def collect(sources):
    # Sources verifies this file against the same pinned release archive as
    # the locale XML. No host Unicode implementation participates in parsing.
    return parse(sources.read(SOURCE))


def summary(data):
    rows = data['rows']
    return {
        'version': data['version'],
        'mappings': len(rows),
        'contractions': sum(len(key) > 1 for key, elements in rows),
        'expansions': sum(len(elements) > 1 for key, elements in rows),
        'elements': sum(len(elements) for key, elements in rows),
        'maximum_key_length': max(len(key) for key, elements in rows),
        'maximum_expansion_length': max(len(elements) for key, elements in rows),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(summary(collect(acquire.Sources(args.sources))), sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
