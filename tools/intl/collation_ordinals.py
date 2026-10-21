#!/usr/bin/env python3
"""Assign compact logical root coordinates without encoding implicit bytes."""
import argparse
from collections import namedtuple
import json
from pathlib import Path

import acquire
from cldr_xml import DataError
import collation_fractional as fractional

Coordinate = namedtuple('Coordinate', 'primary secondary tertiary case variable virtual')
LOWER, MIXED, UPPER = range(3)
GENERIC_COUNT = 0x110000


def ordinary_tertiary(raw):
    if not raw:
        return ()
    result = (raw[0] & 0x3f,) + raw[1:]
    return result if any(result) else ()


def _fixed(data, name):
    value = data['fixed'].get(name)
    if value is None or value == 0:
        raise DataError('missing fixed root byte: ' + name)
    return (value,)


class Catalog:
    """Stable root IDs; inserted symbolic IDs and final ranks are later domains.

    Generic implicit positions are arithmetic, not a million-entry array. Only
    Han's source-defined radical order needs a code point -> position map. Original
    keys stay original: synthesizing NFD root contractions can hide later component
    tailoring. The C matcher normalizes lookup input instead.
    """
    def __init__(self, data):
        self.data = data
        rows = data['rows'] + data['synthetic']
        ces = [ce for row in rows for ce in row.elements]
        boundary = [row for row in data['rows'] if not row.prefix and row.text == (0xfdd1, 0x5b57)]
        if (len(boundary) != 1 or len(boundary[0].elements) != 1 or
                boundary[0].elements[0].primary.kind != 'weight'):
            raise DataError('missing or ambiguous first-Hani boundary')
        self.han_weight = boundary[0].elements[0].primary.value
        first = _fixed(data, 'first implicit')
        last = _fixed(data, 'last implicit')
        trail = _fixed(data, 'first trail')
        if not self.han_weight or not self.han_weight < first < last < trail:
            raise DataError('unordered Han/implicit/trailing domains')

        pkeys = {ce.primary.value for ce in ces if ce.primary.kind == 'weight' and ce.primary.value}
        skeys = {ce.secondary for ce in ces if ce.secondary}
        tkeys = {ordinary_tertiary(ce.tertiary) for ce in ces if ordinary_tertiary(ce.tertiary)}
        for weights in data['limits'].values():
            if weights[0]:
                pkeys.add(weights[0])
            if weights[1]:
                skeys.add(weights[1])
            if weights[2]:
                tkeys.add(ordinary_tertiary(weights[2]))
        pkeys.update((first, last, trail, data['variable_top']))
        sb = _fixed(data, 'first ignorable secondary')
        tb = _fixed(data, 'first ignorable tertiary')
        skeys.add(sb)
        tkeys.add(tb)
        if any(self.han_weight < key < first or first < key < last or
               last < key < trail for key in pkeys):
            raise DataError('literal primary overlaps a virtual root domain')

        self.primaries = {(): 0}
        rank = 0
        for key in sorted(pkeys):
            rank += 1
            self.primaries[key] = rank
            if key == self.han_weight:
                self.han_base = rank
                rank += len(data['han_order'])
            elif key == last:
                # E4 is the source unassigned-group boundary. Keep its literal
                # coordinate outside the arithmetic generic interval.
                self.generic_base = rank
                rank += GENERIC_COUNT
        self.primary_max = rank
        self.han = {code: self.han_base + i + 1 for i, code in enumerate(data['han_order'])}
        self.generic_limit = self.generic_base + GENERIC_COUNT + 1
        # These are source marker facts, not the extrema of the current map.
        self.implicit_first_marker = self.primaries[first]
        self.implicit_last_marker = self.primaries[last]
        self.variable_top = self.primaries[data['variable_top']]

        # Preserve the engine's common aliases while retaining below-common T
        # and positive unused S roots for before-common insertion.
        self.secondaries = {(): 0}
        self.secondaries.update((key, i + 32) for i, key in enumerate(sorted(skeys)))
        self.tertiaries = {(): 0}
        self.tertiaries.update((key, i + 1) for i, key in enumerate(sorted(tkeys)))
        self.secondary_common = self.secondaries.get((5,))
        self.tertiary_common = self.tertiaries.get((5,))
        self.secondary_boundary = self.secondaries[sb]
        self.tertiary_boundary = self.tertiaries[tb]
        self.secondary_max = max(self.secondaries.values())
        self.tertiary_max = max(self.tertiaries.values())
        if (self.secondary_common != 32 or self.tertiary_common != 2 or
                not self.secondary_common < self.secondary_boundary < self.secondary_max or
                not self.tertiary_common < self.tertiary_boundary < self.tertiary_max):
            raise DataError('unsupported common/ignorable root weight layout')
        if max(self.primary_max, self.secondary_max, self.tertiary_max) >= 0x80000000:
            raise DataError('root coordinates exceed symbolic ID domain')
        numeric = [row for row in data['synthetic'] if not row.prefix and row.text == (0xfdd0, 0x34)]
        if (len(numeric) != 1 or len(numeric[0].elements) != 1 or
                numeric[0].elements[0].primary.kind != 'weight' or
                not numeric[0].elements[0].primary.value):
            raise DataError('missing numeric primary reservation')
        # Only P defines this reservation. Real source records can include
        # common S/T; intentionally incomplete forms are valid synthetic data too.
        numeric_weight = numeric[0].elements[0].primary.value
        if any(ce.primary == fractional.Primary('weight', numeric_weight)
               for row in data['rows'] for ce in row.elements):
            raise DataError('numeric marker collides with an executable primary')
        self.numeric = self.primaries[numeric_weight]
        separator = [row for row in data['rows'] if not row.prefix and row.text == (0xfffe,)]
        if (len(separator) != 1 or len(separator[0].elements) != 1 or
                separator[0].elements[0].primary.kind != 'weight'):
            raise DataError('missing field-separator primary')
        self.field_separator = self.primaries[separator[0].elements[0].primary.value]
        # Do not use the footer's last-regular CE: TR35 selects first Hani.
        self.last_regular = self.han_base
        for row in data['rows']:
            for ce in row.elements:
                self.coordinate(ce)

    def coordinate(self, ce):
        if ce.primary.kind == 'han':
            p = self.han.get(ce.primary.value)
        elif ce.primary.kind == 'weight':
            p = self.primaries.get(ce.primary.value)
        else:
            p = None
        s = self.secondaries.get(ce.secondary)
        t = self.tertiaries.get(ordinary_tertiary(ce.tertiary))
        if p is None or s is None or t is None:
            raise DataError('CE is outside the collected root coordinate domain')
        case = ce.tertiary[0] >> 6 if ce.tertiary else LOWER
        if case > UPPER:
            raise DataError('reserved root case value')
        if p:
            valid = 0 < s < self.secondary_boundary and 0 < t < self.tertiary_boundary
        elif s:
            valid = s > self.secondary_boundary and 0 < t < self.tertiary_boundary
        elif t:
            valid = t > self.tertiary_boundary
            case = UPPER
        else:
            valid = case == LOWER
        if not valid:
            raise DataError('logical root CE violates WF1/WF2 bands')
        return Coordinate(p, s, t, case, bool(self.field_separator < p <= self.variable_top),
                          ce.primary.kind == 'han')

    def implicit(self, code):
        # Ill-formed UTF-16 units also need deterministic fallback coordinates.
        if not isinstance(code, int) or not 0 <= code <= 0x10ffff:
            raise DataError('invalid implicit code point')
        p = self.han.get(code, self.generic_base + code + 1)
        return Coordinate(p, self.secondary_common, self.tertiary_common, LOWER, False, True)

    def summary(self):
        return {name: getattr(self, name) for name in (
            'primary_max', 'secondary_max', 'tertiary_max', 'secondary_common',
            'tertiary_common', 'secondary_boundary', 'tertiary_boundary',
            'han_base', 'generic_base', 'generic_limit',
            'implicit_first_marker', 'implicit_last_marker',
            'variable_top', 'numeric', 'last_regular', 'field_separator')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    data = fractional.collect(acquire.Sources(args.sources))
    print(json.dumps(Catalog(data).summary(), sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
