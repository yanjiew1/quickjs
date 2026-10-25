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

"""Parse authenticated segmentation word lists, retaining CJK path costs.

This is the pinned text profile, not ICU's general gendict language. Binary
compilation and word-boundary policy are separate consumers of these records.
"""
import argparse
from collections import namedtuple
import json
from pathlib import Path
import re

from cldr_xml import DataError
import segment_dictionary_sources as sources

Entry = namedtuple('Entry', 'word cost')
Dictionary = namedtuple('Dictionary', 'entries valued max_scalars max_utf16 comments')
Limits = namedtuple('Limits', 'bytes records line_bytes key_scalars total_scalars')
DEFAULT_LIMITS = Limits(sources.MAX_FILE, 1000000, 16384, 128, 16000000)
DECIMAL = re.compile(r'[0-9]+')


def parse(data, valued, label='dictionary', limits=DEFAULT_LIMITS):
    """Return immutable scalar-sorted records without folding or normalization.

    SEA records contain one literal key; CJK records require one tab and an
    unsigned decimal cost. None denotes membership, distinct from cost zero.
    Only LF/CRLF delimit lines; Unicode separators remain literal key scalars.
    Preserve source comment lines so derivative data can retain attribution.
    """
    if (not isinstance(data, bytes) or type(valued) is not bool or
            not isinstance(limits, Limits) or
            any(type(value) is not int or value <= 0 for value in limits)):
        raise DataError('invalid dictionary parser arguments')
    if len(data) > limits.bytes:
        raise DataError(label + ': source byte budget exceeded')
    try:
        text = data.decode('utf-8-sig')
    except UnicodeError:
        raise DataError(label + ': invalid UTF-8')
    if '\0' in text or '\ufeff' in text:
        raise DataError(label + ': NUL or interior BOM')

    entries, comments = {}, []
    total, max_scalars, max_utf16 = 0, 0, 0
    lines = text.split('\n')
    for index, raw in enumerate(lines):
        where = '{}:{}'.format(label, index + 1)
        if len(raw.encode('utf-8')) > limits.line_bytes:
            raise DataError(where + ': line byte budget exceeded')
        if raw.endswith('\r') and index + 1 < len(lines):
            raw = raw[:-1]
        if '\r' in raw:
            raise DataError(where + ': bare CR')
        visible = raw.lstrip(' \t')
        if not visible:
            continue
        if visible.startswith('#'):
            comments.append(raw)
            continue

        fields = raw.split('\t')
        if len(fields) != (2 if valued else 1):
            raise DataError(where + ': unexpected dictionary fields')
        word = fields[0]
        # Whitespace is syntax only in blank/comment lines. Never strip a
        # malformed key or reinterpret punctuation as a regex/set expression.
        if not word or any(ord(char) <= 0x20 or ord(char) == 0x7f for char in word):
            raise DataError(where + ': empty, padded or control-containing key')
        if len(word) > limits.key_scalars:
            raise DataError(where + ': key scalar budget exceeded')
        cost = None
        if valued:
            if not DECIMAL.fullmatch(fields[1]):
                raise DataError(where + ': invalid unsigned decimal cost')
            digits = fields[1].lstrip('0') or '0'
            if len(digits) > 10 or int(digits) > 0xffffffff:
                raise DataError(where + ': cost exceeds uint32')
            cost = int(digits)
        if word in entries:
            raise DataError(where + ': duplicate dictionary key')
        if len(entries) == limits.records:
            raise DataError(where + ': record budget exceeded')
        total += len(word)
        if total > limits.total_scalars:
            raise DataError(where + ': total scalar budget exceeded')
        entries[word] = cost
        max_scalars = max(max_scalars, len(word))
        max_utf16 = max(max_utf16, len(word) + sum(ord(char) > 0xffff for char in word))

    if not entries:
        raise DataError(label + ': empty dictionary')
    return Dictionary(tuple(Entry(word, entries[word]) for word in sorted(entries)),
                      valued, max_scalars, max_utf16, tuple(comments))


def collect(authenticated):
    """Consume checked release bytes through their acquisition owner."""
    result = {}
    for name, family in sorted(sources.FAMILIES.items()):
        if family != 'license':
            result[family] = parse(authenticated.read(name), family == 'cjk', name)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    authenticated = sources.Sources(args.sources)
    dictionaries = collect(authenticated)
    print(json.dumps({'parser_version': 1, 'identity': authenticated.identity(),
                      'dictionaries': {
                          family: {'entries': len(data.entries), 'valued': data.valued,
                                   'max_scalars': data.max_scalars, 'max_utf16': data.max_utf16,
                                   'comment_lines': len(data.comments)}
                          for family, data in sorted(dictionaries.items())}},
                     indent=2, sort_keys=True))


if __name__ == '__main__':
    main()
