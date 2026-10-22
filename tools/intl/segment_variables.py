"""Capture CLDR segmentation variable environments without executing expressions."""
from collections import namedtuple

from cldr_xml import DataError
from segment_sources import KINDS

Binding = namedtuple('Binding', 'identifier expression locale source environment')
Environment = namedtuple('Environment', 'bindings visible')


def capture(records, graph, locale, kind):
    """Return one immutable definition arena and its final name-to-slot table.

    A definition's environment contains only earlier slots, including its own
    previous value when redefined. The later expression compiler must resolve
    references through that snapshot, never through the final visible table.
    This step does not scan references or implement set/property/rule syntax.
    """
    if kind not in KINDS or locale not in graph.tags:
        raise DataError('unknown segmentation variable locale or kind')
    if 'segmentations' not in graph.components:
        raise DataError('undeclared segmentation inheritance component')
    groups = {}
    for row in records:
        key = row.locale, row.kind
        if row.locale not in graph.tags or row.kind not in KINDS or key in groups:
            raise DataError('unknown or repeated segmentation source group')
        groups[key] = row

    bindings, visible = [], {}
    # Apply root first; the component graph deliberately ignores exceptional
    # main-data parents. An empty variables container contributes no overrides.
    for source in reversed(tuple(graph.chain(locale, 'segmentations'))):
        row = groups.get((source, kind))
        if row is None or row.variables is None:
            continue
        for definition in row.variables:
            before = tuple(sorted(visible.items()))
            slot = len(bindings)
            bindings.append(Binding(definition.identifier, definition.expression,
                                    row.locale, row.source, before))
            visible[definition.identifier] = slot
    return Environment(tuple(bindings), tuple(sorted(visible.items())))
