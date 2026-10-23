"""Prepare literal sentence suppression lists without changing their spelling."""
from collections import namedtuple

from cldr_xml import DataError
from scalar_trie import DEFAULT_LIMITS, prepare as prepare_graph
from segment_inheritance import SuppressionEnvironment

PreparedSuppressions = namedtuple('PreparedSuppressions', 'entries graph origin')
LIMITS = DEFAULT_LIMITS._replace(entries=65536, scalars=1048576, key_length=256,
                                states=262144, edges=524288, values=1)


def prepare(environment):
    """Keep ordered source text beside a canonical scalar lookup graph.

    Empty policy has no epsilon terminal. Spaces and punctuation are literal,
    including inside multi-part abbreviations; dictionary source restrictions
    deliberately do not apply here. Matching and soft-break policy are later.
    """
    if (not isinstance(environment, SuppressionEnvironment) or type(environment.entries) is not tuple or
            len(environment.entries) > LIMITS.entries):
        raise DataError('invalid suppression list owner or entry budget')
    if environment.locale is None:
        if environment.source is not None or environment.entries:
            raise DataError('suppression entries without declaration origin')
        origin = None
    else:
        if (not isinstance(environment.locale, str) or not environment.locale or
                not isinstance(environment.source, str) or not environment.source):
            raise DataError('invalid suppression declaration origin')
        origin = environment.locale, environment.source
    total = 0
    for text in environment.entries:
        if not isinstance(text, str) or not text:
            raise DataError('empty or invalid literal suppression key')
        total += len(text)
        if len(text) > LIMITS.key_length or total > LIMITS.scalars:
            raise DataError('suppression key/scalar budget exceeded')
    if len(set(environment.entries)) != len(environment.entries):
        raise DataError('duplicate literal suppression key')
    graph = None
    if environment.entries:
        graph = prepare_graph(tuple((text, None) for text in sorted(environment.entries)), LIMITS)
    return PreparedSuppressions(environment.entries, graph, origin)
