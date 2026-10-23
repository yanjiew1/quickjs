"""Compile distinct inherited streams once and bind final shared locale IDs.

Keep source origins beside execution identities. Locale aliases/inventories
come from the owning formatter registry; this module uses segmentation parents
only and does not broaden the requested inventory or claim runtime capability.
"""
from collections import namedtuple

from cldr_xml import DataError
from segment_inheritance import capture_variables, ordered_rules, capture_suppressions
from segment_lower import lower_rules
from segment_profile import build, profile_key
from segment_rules import prepare_variables, prepare_rules
from segment_sources import KINDS
from segment_suppressions import prepare as prepare_suppressions
from segment_program_tables import MAX_BINDINGS, MAX_PROGRAMS
from segment_suppression_tables import MAX_LISTS

Stream = namedtuple('Stream', 'kind variables rules program')
SuppressionSource = namedtuple('SuppressionSource', 'entries origin list_id')
Catalog = namedtuple('Catalog', 'tags profiles program_bindings streams stream_routes '
                    'lists suppression_bindings suppression_sources suppression_origins')
# Bound attempted compilation too: many distinct streams can collapse to a
# small final program set, after already spending substantial compiler work.
MAX_STREAMS = 128
MAX_SUPPRESSION_SOURCES = 256


def prepare(records, graph, tags, resolver):
    """Bind selected locale indexes after all formatter aliases are present.

    Cache exact captured streams before parsing/lowering/automata construction,
    then deduplicate complete execution profiles. Equal semantics may have
    different source streams, so retain both receipts. Whole suppression lists
    deduplicate by canonical literal content; their raw order and declaration
    origins remain separate records, including absent versus explicit empty.
    """
    if (type(records) is not tuple or type(tags) is not tuple or not tags or
            len(tags) > MAX_BINDINGS // len(KINDS) or
            any(not isinstance(tag, str) or tag not in graph.tags for tag in tags) or
            tuple(sorted(set(tags))) != tags):
        raise DataError('invalid segmentation catalog locale inventory')
    streams, profiles, stream_routes, bindings = [], [], [], []
    captured, execution = {}, {}
    lists, list_ids = [], {}
    source_lists, source_ids = [], {}
    list_bindings, origins = [], []
    for locale_id, tag in enumerate(tags):
        for kind_id, kind in enumerate(KINDS):
            variables = capture_variables(records, graph, tag, kind)
            rules = ordered_rules(records, graph, tag, kind)
            key = kind, variables, rules
            stream_id = captured.get(key)
            if stream_id is None:
                if len(streams) >= MAX_STREAMS:
                    raise DataError('segmentation catalog distinct stream budget exceeded')
                typed_variables = prepare_variables(variables)
                typed_rules = prepare_rules(rules, typed_variables, kind)
                profile = build(kind, lower_rules(typed_rules, typed_variables, resolver), resolver)
                identity = profile_key(profile)
                program_id = execution.get(identity)
                if program_id is None:
                    if len(profiles) >= MAX_PROGRAMS:
                        raise DataError('segmentation catalog program budget exceeded')
                    program_id = len(profiles)
                    profiles.append(profile)
                    execution[identity] = program_id
                stream_id = len(streams)
                streams.append(Stream(kind, variables, rules, program_id))
                captured[key] = stream_id
            bindings.append((locale_id, kind_id, streams[stream_id].program))
            stream_routes.append(stream_id)

        environment = capture_suppressions(records, graph, tag)
        source_key = environment.locale, environment.source
        source_id = source_ids.get(source_key)
        if source_id is None:
            if len(source_lists) >= MAX_SUPPRESSION_SOURCES:
                raise DataError('segmentation catalog suppression source budget exceeded')
            prepared = prepare_suppressions(environment)
            identity = tuple(sorted(prepared.entries))
            list_id = list_ids.get(identity)
            if list_id is None:
                if len(lists) >= MAX_LISTS:
                    raise DataError('segmentation catalog suppression list budget exceeded')
                list_id = len(lists)
                lists.append(prepared.graph)
                list_ids[identity] = list_id
            source_id = len(source_lists)
            source_lists.append(SuppressionSource(prepared.entries, prepared.origin, list_id))
            source_ids[source_key] = source_id
        origins.append(source_id)
        list_bindings.append((locale_id, source_lists[source_id].list_id))
    return Catalog(tags, tuple(profiles), tuple(bindings), tuple(streams), tuple(stream_routes),
                   tuple(lists), tuple(list_bindings), tuple(source_lists), tuple(origins))
