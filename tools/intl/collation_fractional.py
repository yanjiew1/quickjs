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

"""Collect logical root CEs directly from authenticated FractionalUCA text."""
from collections import namedtuple
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError
import collation_root

SOURCE = 'common/uca/FractionalUCA.txt'
Primary = namedtuple('Primary', 'kind value')
Element = namedtuple('Element', 'primary secondary tertiary')
Reference = namedtuple('Reference', 'scalar overrides')
Mapping = namedtuple('Mapping', 'prefix text elements line')
SCALAR = re.compile(r'[0-9A-Fa-f]{4,6}\Z')
BYTE = re.compile(r'[0-9A-Fa-f]{2}\Z')


def scalar(text):
    if not SCALAR.fullmatch(text):
        raise DataError('invalid scalar spelling')
    code = int(text, 16)
    if code > 0x10ffff or 0xd800 <= code <= 0xdfff:
        raise DataError('non-scalar code point')
    return code


def weight(text):
    words = text.split()
    if any(not BYTE.fullmatch(word) or int(word, 16) == 0 for word in words):
        raise DataError('weights must use nonzero bytes or be empty')
    return tuple(int(word, 16) for word in words)


def ranges(text):
    """Retain declared order while rejecting overlap, even across ranges."""
    result = []
    for word in text.split():
        parts = word.split('..')
        if len(parts) not in (1, 2):
            raise DataError('invalid ideograph range')
        first, last = scalar(parts[0]), scalar(parts[-1])
        if first > last or first <= 0xdfff and last >= 0xd800:
            raise DataError('invalid ideograph extent')
        result.append((first, last))
    ordered = sorted(result)
    if not result or any(a[1] >= b[0] for a, b in zip(ordered, ordered[1:])):
        raise DataError('empty or overlapping ideograph ranges')
    return tuple(result)


def elements(text):
    result, at = [], 0
    for match in re.finditer(r'\[([^\[\]]*)\]', text):
        if text[at:match.start()].strip():
            raise DataError('unexpected text between CEs')
        fields = [field.strip() for field in match[1].split(',')]
        if fields[0].startswith('U+'):
            if not 1 <= len(fields) <= 3:
                raise DataError('invalid reference field count')
            result.append(Reference(scalar(fields[0][2:]), tuple(weight(x) for x in fields[1:])))
        else:
            if len(fields) != 3:
                raise DataError('literal CE needs P/S/T fields')
            result.append(Element(Primary('weight', weight(fields[0])),
                                  weight(fields[1]), weight(fields[2])))
        at = match.end()
    if not result or text[at:].strip():
        raise DataError('missing or malformed CE sequence')
    return tuple(result)


def radical_order(lines, ideographs):
    """Expand authored scalar ranges, not comments or host Unicode properties.

    Labels/representatives are informational. The ordered text after ':' alone
    defines rank. Exact membership catches omission, overlap and foreign scalars.
    """
    expected = {code for first, last in ideographs for code in range(first, last + 1)}
    result, seen = [], set()
    for body in lines:
        at = 0
        while at < len(body):
            first = last = ord(body[at])
            at += 1
            if at < len(body) and body[at] == '-':
                if at + 1 >= len(body):
                    raise DataError('unfinished radical range')
                last = ord(body[at + 1])
                at += 2
            if first > last or last > 0x10ffff:
                raise DataError('reversed radical range')
            for code in range(first, last + 1):
                if code not in expected or code in seen:
                    raise DataError('foreign or repeated radical-order scalar')
                seen.add(code)
                result.append(code)
    if seen != expected:
        raise DataError('radical order does not cover declared ideographs')
    return tuple(result)


def _resolve(rows, ideographs, common_s, common_t):
    """References copy one logical CE, never zip fractional and allkeys CEs."""
    earlier, result = {}, []
    references = 0
    for row in rows:
        resolved = []
        for ce in row.elements:
            if isinstance(ce, Reference):
                references += 1
                base = earlier.get((ce.scalar,))
                if base is not None:
                    if len(base) != 1:
                        raise DataError('{}:{}: reference needs a single earlier CE'.format(SOURCE, row.line))
                    base = base[0]
                elif any(first <= ce.scalar <= last for first, last in ideographs):
                    base = Element(Primary('han', ce.scalar), common_s, common_t)
                else:
                    raise DataError('{}:{}: reference needs an earlier or implicit Han CE'.format(
                        SOURCE, row.line))
                if len(ce.overrides) == 0:
                    ce = base
                elif len(ce.overrides) == 1:
                    ce = base._replace(tertiary=ce.overrides[0])
                else:
                    ce = base._replace(secondary=ce.overrides[0], tertiary=ce.overrides[1])
            # Synthetic FDD0 records can include incomplete numeric anchors.
            # Preserve them separately; never admit them as mappings.
            if row.text[0] != 0xfdd0:
                p = ce.primary.kind == 'han' or bool(ce.primary.value)
                if (p and not ce.secondary) or ((p or ce.secondary) and not ce.tertiary):
                    raise DataError('{}:{}: logical CE violates UCA WF1'.format(SOURCE, row.line))
            resolved.append(ce)
        row = row._replace(elements=tuple(resolved))
        if not row.prefix:
            earlier[row.text] = row.elements
        result.append(row)
    return tuple(result), references


def parse(raw, expected_version=collation_root.VERSION):
    """Parse source semantics; keep original root keys and counted source identity.

    Keep fractional byte vectors and Han references independent of any chosen
    ordinal/implicit encoding. Footer limits are source facts, not a replacement
    for TR35's logical last-regular (first-Hani) reset rule.
    """
    try:
        lines = raw.decode('utf-8').splitlines()
    except (AttributeError, UnicodeDecodeError) as error:
        raise DataError('invalid fractional UTF-8: ' + str(error))
    version, ideographs, top = None, None, None
    rows, radical_lines, metadata = [], [], []
    keys, limits, fixed = set(), {}, {}
    radical_end, mappings_started = False, False
    for number, raw_line in enumerate(lines, 1):
        line = raw_line.split('#', 1)[0].strip()
        if not line:
            continue
        try:
            if line.startswith('[UCA version'):
                if version is not None or rows or line != '[UCA version = {}]'.format(expected_version):
                    raise DataError('repeated or incompatible UCA version')
                version = expected_version
            elif version is None:
                raise DataError('content precedes version')
            elif line.startswith('[Unified_Ideograph '):
                if ideographs is not None or mappings_started or not line.endswith(']'):
                    raise DataError('repeated or misplaced ideograph declaration')
                ideographs = ranges(line[len('[Unified_Ideograph '):-1])
            elif line.startswith('[radical '):
                if ideographs is None or radical_end or mappings_started or not line.endswith(']'):
                    raise DataError('misplaced radical data')
                if line == '[radical end]':
                    radical_end = True
                else:
                    match = re.fullmatch(r'\[radical [1-9][0-9]*=[^:]+:(.*)\]', line)
                    if not match:
                        raise DataError('invalid radical line')
                    radical_lines.append(match[1])
            elif line.startswith(('[first ', '[last ')):
                match = re.fullmatch(r'\[((?:first|last) [a-z -]+) \[([^\[\]]*)\]\]', line)
                if not match or match[1] in limits:
                    raise DataError('invalid/repeated limit')
                fields = [field.strip() for field in match[2].split(',')]
                if len(fields) != 3:
                    raise DataError('limit needs three fields')
                limits[match[1]] = tuple(None if field == 'X' else weight(field) for field in fields)
            elif line.startswith('[variable top'):
                match = re.fullmatch(r'\[variable top = (.+)\]', line)
                if top is not None or not match:
                    raise DataError('invalid/repeated variable top')
                top = weight(match[1])
                if not top:
                    raise DataError('empty variable top')
            elif line.startswith('[fixed '):
                match = re.fullmatch(r'\[fixed ([a-z -]+) byte ([0-9A-Fa-f]{2})\]', line)
                if not match or match[1] in fixed:
                    raise DataError('invalid/repeated fixed byte')
                fixed[match[1]] = int(match[2], 16)
            elif line.startswith(('[top_byte ', '[top_byte\t', '[reorderingTokens ',
                                  '[reorderingTokens\t', '[categories ', '[categories\t')):
                # This collector retains auxiliary source metadata verbatim.
                # Group/token interpretation has its own authenticated owner.
                if not line.endswith(']') or '[' in line[1:] or ']' in line[:-1]:
                    raise DataError('malformed auxiliary metadata')
                metadata.append(line)
            elif line.startswith('['):
                raise DataError('unknown fractional directive')
            else:
                if not radical_end or line.count(';') != 1:
                    raise DataError('mapping before radical end or missing separator')
                mappings_started = True
                key, values = (part.strip() for part in line.split(';'))
                parts = key.split('|')
                if len(parts) not in (1, 2):
                    raise DataError('invalid prefix separator')
                text = tuple(scalar(word) for word in parts[-1].split())
                prefix = tuple(scalar(word) for word in parts[0].split()) if len(parts) == 2 else ()
                if not text or len(parts) == 2 and not prefix or (prefix, text) in keys:
                    raise DataError('empty or repeated mapping key')
                keys.add((prefix, text))
                rows.append(Mapping(prefix, text, elements(values), number))
        except DataError as error:
            raise DataError('{}:{}: {}'.format(SOURCE, number, error))
    if version is None or ideographs is None or not radical_end or not rows or top is None:
        raise DataError('incomplete fractional source')
    if fixed.get('secondary common') != 5 or fixed.get('tertiary common') != 5:
        raise DataError('unsupported or missing implicit common bytes')
    han_order = radical_order(radical_lines, ideographs)
    rows, references = _resolve(rows, ideographs, (5,), (5,))
    return {'version': version, 'ideographs': ideographs, 'han_order': han_order,
            'rows': tuple(row for row in rows if row.text[0] != 0xfdd0),
            'synthetic': tuple(row for row in rows if row.text[0] == 0xfdd0),
            'limits': limits, 'fixed': fixed, 'variable_top': top,
            'metadata': tuple(metadata), 'references': references}


def collect(sources):
    return parse(sources.read(SOURCE))


def summary(data):
    rows = data['rows'] + data['synthetic']
    ces = [ce for row in rows for ce in row.elements]
    return {'mappings': len(data['rows']), 'synthetic': len(data['synthetic']),
            'prefixes': sum(bool(row.prefix) for row in data['rows']),
            'references': data['references'], 'han': len(data['han_order']),
            # Zero has a permanent shared ID; report the nonzero source keys.
            'literal_primaries': len({ce.primary.value for ce in ces
                                      if ce.primary.kind == 'weight' and ce.primary.value}),
            'secondaries': len({ce.secondary for ce in ces if ce.secondary}),
            'tertiaries': len({ce.tertiary for ce in ces if ce.tertiary})}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(summary(collect(acquire.Sources(args.sources))), sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
