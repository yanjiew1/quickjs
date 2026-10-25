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

"""Collect root group identities and anchor keys for semantic compilation."""
import argparse
from collections import namedtuple
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError
import collation_root

SOURCE = 'common/uca/FractionalUCA.txt'
SCRIPTS = 'common/properties/scriptMetadata.txt'
Group = namedtuple('Group', 'fractional tokens samples first_key kind line')
SCRIPT = re.compile(r'[A-Z][a-z]{3}\Z')
TOKEN = re.compile(r'\[reorderingTokens[ \t]+([A-Za-z-]+)[ \t]+(.+)\]\Z')
BYTE_COUNT = re.compile(r'([0-9A-Fa-f]{2})=([1-9][0-9]*)\Z')
WEIGHT = r'[ \t]*([0-9A-Fa-f]{2}(?:[ \t]+[0-9A-Fa-f]{2})*)[ \t]*'
BOUNDARY = re.compile(r'\[' + WEIGHT + r',[ \t]*05[ \t]*,[ \t]*05[ \t]*\]\Z')
# These synthetic representative keys identify the five non-script groups in
# FractionalUCA. Their following REAL mappings supply allkeys anchor keys.
SPECIAL = {0x00a0: 'space', 0x201c: 'punct', 0x263a: 'symbol',
           0x20ac: 'currency', 0x0034: 'digit'}
SPECIAL_TOKENS = {'space': 'SPACE', 'punct': 'PUNCTUATION', 'symbol': 'SYMBOL',
                  'currency': 'CURRENCY', 'digit': 'DIGIT'}
FIXED_TOKENS = {'FIELD-SEPARATOR', 'IMPLICIT', 'LEVEL-SEPARATOR', 'SPECIAL',
                'TERMINATOR', 'TRAILING'}


def scalar(text):
    if not re.fullmatch(r'[0-9A-Fa-f]{4,6}', text):
        raise DataError('invalid collation group scalar')
    code = int(text, 16)
    if code > 0x10ffff or 0xd800 <= code <= 0xdfff:
        raise DataError('collation group key is not a scalar')
    return code


def script_samples(raw):
    """Use pinned CLDR samples, never comments or the host Unicode version."""
    result = {}
    for number, line in enumerate(raw.decode('utf-8').splitlines(), 1):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        fields = [field.strip() for field in line.split(';')]
        if len(fields) < 3 or not SCRIPT.fullmatch(fields[0]) or fields[0] in result:
            raise DataError('{}:{}: invalid/repeated script metadata'.format(SCRIPTS, number))
        result[fields[0]] = scalar(fields[2])
    return result


def parse(raw, metadata, expected_version=collation_root.VERSION):
    """Preserve ordered boundaries, shared scripts and the first real key.

    This is a source catalog, not a fractional-to-UCA CE conversion. The host
    semantic compiler resolves keys with the same root/implicit weight engine
    as execution. In particular, implicit continuation primaries must not be
    classified independently or zipped with fractional expansion elements.
    """
    try:
        lines = raw.decode('utf-8').splitlines()
        samples = script_samples(metadata)
    except (AttributeError, UnicodeDecodeError) as error:
        raise DataError('invalid collation group text: ' + str(error))
    boundaries, token_data, pending = [], {}, None
    version = None
    seen_samples, previous = set(), ()
    unassigned = False
    for number, raw_line in enumerate(lines, 1):
        line = raw_line.split('#', 1)[0].strip(' \t')

        def fail(message):
            raise DataError('{}:{}: {}'.format(SOURCE, number, message))

        if line.startswith('[UCA version'):
            if version is not None or line != '[UCA version = {}]'.format(expected_version):
                fail('repeated or incompatible fractional UCA version')
            version = expected_version
        elif line.startswith('[reorderingTokens'):
            match = TOKEN.fullmatch(line)
            if version is None or not match or match[1] in token_data:
                fail('invalid/repeated reordering token')
            if not SCRIPT.fullmatch(match[1]) and match[1] not in (
                    set(SPECIAL_TOKENS.values()) | FIXED_TOKENS):
                fail('unknown non-script reordering token')
            counts = []
            for field in match[2].split():
                pair = BYTE_COUNT.fullmatch(field)
                if not pair:
                    fail('invalid reordering token byte count')
                counts.append((int(pair[1], 16), int(pair[2])))
            if not counts or any(a[0] >= b[0] for a, b in zip(counts, counts[1:])):
                fail('unordered/repeated reordering token bytes')
            token_data[match[1]] = tuple(counts)
        elif line.startswith('FDD1'):
            if version is None or unassigned or line.count(';') != 1:
                fail('invalid group boundary position')
            key, value = (part.strip(' \t') for part in line.split(';'))
            parts = key.split()
            match = BOUNDARY.fullmatch(value)
            if len(parts) != 2 or parts[0] != 'FDD1' or not match:
                fail('invalid group boundary mapping')
            sample = scalar(parts[1])
            weight = tuple(int(byte, 16) for byte in match[1].split())
            if sample in seen_samples or weight < previous or any(byte == 0 for byte in weight):
                fail('repeated or unordered group boundary')
            seen_samples.add(sample)
            if pending is not None:
                old = boundaries[pending]
                if old.fractional != weight:
                    fail('group boundary has no following real mapping')
                boundaries[pending] = old._replace(samples=old.samples + (sample,))
            elif sample == 0xfdd0:
                # Generic implicit weights end the "others" bucket. They
                # have no separately selectable script token or real anchor.
                boundaries.append(Group(weight, (), (sample,), None, 'unassigned', number))
                unassigned = True
            else:
                if weight == previous:
                    fail('shared group boundaries must precede their first mapping')
                boundaries.append(Group(weight, (), (sample,), None, 'reorderable', number))
                pending = len(boundaries) - 1
            previous = weight
        elif pending is not None and ';' in line:
            key = line.split(';', 1)[0].strip(' \t')
            # Numeric and reserved synthetic records are not group anchors.
            if key.split()[:1] == ['FDD0']:
                continue
            if not collation_root.KEY.fullmatch(key):
                fail('invalid first real group key')
            key = tuple(scalar(code) for code in key.split())
            boundaries[pending] = boundaries[pending]._replace(first_key=key)
            pending = None
    if version is None or pending is not None or not boundaries or not unassigned:
        raise DataError('incomplete fractional group catalog')

    scripts = {token for token in token_data if SCRIPT.fullmatch(token)}
    if not scripts or scripts - set(samples) - {'Hrkt'}:
        raise DataError('reordering scripts lack pinned sample metadata')
    by_sample = {}
    for token in sorted(scripts):
        # Filtering by executable reordering tokens excludes composites like
        # Jpan/Kore/Hanb whose metadata samples alone are not group aliases.
        if token != 'Hrkt':
            by_sample.setdefault(samples[token], []).append(token)
    used, result = set(), []
    for group in boundaries:
        tokens = []
        for sample in group.samples:
            if sample == 0xfdd0:
                continue
            if sample in SPECIAL:
                token = SPECIAL[sample]
                if SPECIAL_TOKENS[token] not in token_data:
                    raise DataError('special group lacks reordering token: ' + token)
                tokens.append(token)
            else:
                aliases = by_sample.get(sample, ())
                if not aliases:
                    raise DataError('group representative has no script identity')
                if any(token_data[token] != token_data[aliases[0]] for token in aliases):
                    raise DataError('ambiguous script sample has different reordering ranges')
                tokens.extend(aliases)
        # Hrkt is the Unicode alias for Katakana_Or_Hiragana. CLDR omits a
        # sample for this composite; require both component boundaries to be
        # in this same group instead of inferring equality from a lead byte.
        if 'Hrkt' in scripts and {'Hira', 'Kana'}.issubset(tokens):
            tokens.append('Hrkt')
        if len(set(tokens)) != len(tokens) or used.intersection(tokens):
            raise DataError('script assigned to multiple reordering groups')
        used.update(tokens)
        result.append(group._replace(tokens=tuple(sorted(tokens))))
    if scripts - used:
        raise DataError('reordering scripts lack group boundaries')
    if set(SPECIAL.values()) - used:
        raise DataError('core reordering groups lack boundaries')
    return tuple(result)


def collect(sources):
    return parse(sources.read(SOURCE), sources.read(SCRIPTS))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources)
    groups = collect(sources)
    root = dict(collation_root.collect(sources)['rows'])
    missing = [group.first_key for group in groups if group.first_key not in root and
               group.kind == 'reorderable']
    print(json.dumps({'groups': len(groups), 'tokens': sum(len(g.tokens) for g in groups),
                      'implicit_anchor_keys': missing}, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
