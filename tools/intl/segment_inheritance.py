"""Prepare inherited CLDR segmentation definitions without executing expressions."""
from collections import namedtuple
from decimal import Decimal

from cldr_xml import DataError
from segment_sources import KINDS, RULE

Binding = namedtuple('Binding', 'identifier expression locale source environment')
VariableEnvironment = namedtuple('VariableEnvironment', 'bindings visible')
InheritedRule = namedtuple('InheritedRule', 'identifier expression locale source')


def _groups(records, graph, locale, kind):
    if kind not in KINDS or locale not in graph.tags:
        raise DataError('unknown segmentation locale or kind')
    if 'segmentations' not in graph.components:
        raise DataError('undeclared segmentation inheritance component')
    groups = {}
    for row in records:
        key = row.locale, row.kind
        if row.locale not in graph.tags or row.kind not in KINDS or key in groups:
            raise DataError('unknown or repeated segmentation source group')
        groups[key] = row

    return groups


def capture_variables(records, graph, locale, kind):
    """Return one immutable definition arena and its final name-to-slot table.

    A definition's environment contains only earlier slots, including its own
    previous value when redefined. The later expression compiler must resolve
    references through that snapshot, never through the final visible table.
    This step does not scan references or implement set/property/rule syntax.
    """
    groups = _groups(records, graph, locale, kind)
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
    return VariableEnvironment(tuple(bindings), tuple(sorted(visible.items())))


def ordered_rules(records, graph, locale, kind):
    """Overlay rule IDs, honor explicit deletion, then sort by exact numeric ID.

    Rule expressions remain uncompiled. Their variable references will use the
    final visible table; variable expressions use their individual snapshots.
    Implicit start/end/default rules belong to the later semantic compiler.
    """
    groups = _groups(records, graph, locale, kind)
    selected = {}
    for source in reversed(tuple(graph.chain(locale, 'segmentations'))):
        row = groups.get((source, kind))
        if row is None or row.rules is None:
            continue
        seen = set()
        for definition in row.rules:
            if not RULE.fullmatch(definition.identifier):
                raise DataError('invalid segmentation rule ID')
            # Decimal construction/comparison is exact, independent of the
            # arithmetic precision context. Floats would merge nearby IDs.
            key = Decimal(definition.identifier)
            if key in seen:
                raise DataError('repeated numeric segmentation rule ID')
            seen.add(key)
            # Empty source elements can contain XML indentation whitespace.
            # Preserve every nonempty expression, including non-XML spaces.
            if not definition.expression.strip(' \t\r\n'):
                selected.pop(key, None)
            else:
                selected[key] = InheritedRule(definition.identifier, definition.expression,
                                              row.locale, row.source)
    return tuple(selected[key] for key in sorted(selected))
