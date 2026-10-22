"""Partition code points by realized overlapping segmentation predicates.

Sweep interval endpoints rather than enumerating Unicode or all 2**P possible
signatures. Predicate bit positions retain the caller's explicit input order.
"""
from bisect import bisect_right
from collections import namedtuple

from cldr_xml import DataError
from segment_sets import DOMAIN_END, checked_ranges

Alphabet = namedtuple('Alphabet', 'predicates ranges masks construction_work')
AlphabetLimits = namedtuple('AlphabetLimits', 'predicates pairs endpoints classes ranges work')
DEFAULT_LIMITS = AlphabetLimits(256, 262144, 524290, 16384, 524289, 8000000)


def partition(predicates, limits=DEFAULT_LIMITS):
    """Return a complete immutable [0,0x110000) class map, including surrogates.

    Classes are numbered by first original-code-point occurrence. Equal
    signatures share an ID even when their intervals are far apart. All
    simultaneous endpoint changes are applied together, avoiding empty classes.
    """
    if (not isinstance(limits, AlphabetLimits) or
            any(type(value) is not int or value < 1 for value in limits)):
        raise DataError('invalid segmentation alphabet limits')
    if not isinstance(predicates, (tuple, list)) or len(predicates) > limits.predicates:
        raise DataError('segmentation alphabet predicate budget exceeded')
    events, prepared, pairs, work = {}, [], 0, 0

    def consume(count=1):
        nonlocal work
        work += count
        if work > limits.work:
            raise DataError('segmentation alphabet work budget exceeded')

    def event(point, bits):
        if point not in events and len(events) >= limits.endpoints:
            raise DataError('segmentation alphabet endpoint budget exceeded')
        events[point] = events.get(point, 0) ^ bits

    event(0, 0)
    event(DOMAIN_END, 0)
    for index, values in enumerate(predicates):
        consume()
        if not isinstance(values, (tuple, list)):
            raise DataError('invalid segmentation alphabet predicate')
        count = len(values)
        if count > limits.pairs - pairs:
            raise DataError('segmentation alphabet pair budget exceeded')
        # Bound the validation/copy before visiting its elements, including
        # when earlier predicates have nearly exhausted the aggregate budget.
        consume(count)
        values = checked_ranges(values, count)
        pairs += count
        prepared.append(values)
        bit = 1 << index
        for start, end in values:
            consume()
            event(start, bit)
            event(end, bit)
    # Charge a conservative sorting allowance before materializing the sorted
    # endpoint list, in addition to the independent endpoint-count cap.
    consume(len(events) * max(1, len(events).bit_length()))
    ranges, masks, identifiers = [], [], {}
    previous = mask = 0
    for point in sorted(events):
        consume()
        if point > previous:
            identifier = identifiers.get(mask)
            if identifier is None:
                if len(masks) >= limits.classes:
                    raise DataError('segmentation alphabet class budget exceeded')
                identifier = len(masks)
                identifiers[mask] = identifier
                masks.append(mask)
            if ranges and ranges[-1][2] == identifier:
                ranges[-1] = ranges[-1][0], point, identifier
            else:
                if len(ranges) >= limits.ranges:
                    raise DataError('segmentation alphabet range budget exceeded')
                ranges.append((previous, point, identifier))
        mask ^= events[point]
        previous = point
    if previous != DOMAIN_END or mask:
        raise DataError('incomplete segmentation alphabet sweep')
    return Alphabet(tuple(prepared), tuple(ranges), tuple(masks), work)


def classify(alphabet, code):
    """Look up a code point in an unchanged result of partition()."""
    if type(code) is not int or not 0 <= code < DOMAIN_END:
        raise DataError('invalid segmentation alphabet code point')
    index = bisect_right(alphabet.ranges, (code, DOMAIN_END, len(alphabet.masks))) - 1
    return alphabet.ranges[index][2]
