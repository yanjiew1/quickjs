"""Pair compiled native segmentation with exact producer inputs and locale IDs.

The standalone archive reuses QJIN framing and sections 39/40/41. Its local
string table is not inserted into the final formatter pack; route locale IDs
already name the same checked inventory. Pairing is not source equivalence or
authentication: generation/reproduction and C readers supply separate checks.
"""
import acquire
import bundle
from cldr_xml import DataError
import parents as locale_parents
import segment_dictionary_sources
import segment_dictionary_tables
import segment_program_tables
import segment_suppression_tables
import tables

SECTIONS = (bundle.SEGMENT_DICTIONARIES, bundle.SEGMENT_PROGRAMS, bundle.SEGMENT_SUPPRESSIONS)
ARCHIVE_SECTIONS = frozenset((bundle.STRINGS, bundle.LOCALES) + SECTIONS)


def _inventory(tags):
    if (type(tags) is not tuple or not 1 <= len(tags) <= segment_program_tables.MAX_BINDINGS // 3 or
            any(not isinstance(tag, str) or not tag for tag in tags) or
            tuple(sorted(set(tags))) != tags or 'root' not in tags or
            any(locale_parents.locale_id(tag) != tag for tag in tags)):
        raise DataError('invalid segmentation locale inventory')


def recipe(cldr_files, dictionary_files, tags):
    """Identify data inputs; output hashes identify a particular compilation."""
    _inventory(tags)
    if (not isinstance(cldr_files, dict) or not isinstance(dictionary_files, dict) or
            set(dictionary_files) != set(segment_dictionary_sources.FAMILIES) or
            any(not isinstance(value, str) or len(value) != 64 or
                any(c not in '0123456789abcdef' for c in value)
                for value in dictionary_files.values())):
        raise DataError('invalid segmentation input receipts')
    return {'format': 1, 'profile': 'native', 'unicode': '18.0.0', 'semantic_profile': 1,
            'cldr_inventory': acquire.digest(acquire.json_bytes(cldr_files)),
            'dictionaries': dict(dictionary_files), 'locales': tags}


def fingerprint(inputs):
    return bytes.fromhex(acquire.digest(acquire.json_bytes(inputs)))


def validate(data, tags, parents, expected):
    """Validate complete framing, typed tables, routes and pairing metadata.

    Every locale needs all three programs and a complete suppression policy;
    every dictionary family must be present. The final native service still
    owns executable capability and runtime input behavior.
    """
    _inventory(tags)
    if (type(parents) is not tuple or len(parents) != len(tags) or
            any(type(value) is not int or not 0 <= value <= 0xffffffff or
                (value != 0xffffffff and value >= len(tags)) for value in parents) or
            type(expected) is not bytes or len(expected) != 32):
        raise DataError('invalid compiled segmentation pairing inputs')
    envelope = bundle.unpack(data)
    if (envelope['profile'] != bundle.PROFILES['native'] or
            set(envelope['sections']) != ARCHIVE_SECTIONS or
            envelope['generation'] != expected):
        raise DataError('compiled segmentation archive identity/inventory mismatch')
    sections = envelope['sections']
    if sections[bundle.SEGMENT_DICTIONARIES][0] != len(segment_dictionary_tables.FAMILIES):
        raise DataError('compiled segmentation lacks dictionary families')
    if (sections[bundle.LOCALES][0] != len(tags) or
            len(sections[bundle.LOCALES][1]) != 8 * len(tags) or
            sections[bundle.STRINGS] != tables.encode_strings(('',) + tags)):
        raise DataError('compiled segmentation shared inventory framing mismatch')
    strings = ('',) + tags
    found_tags, found_parents = tables.decode_locales(sections[bundle.LOCALES], strings)
    if found_tags != tags or found_parents != parents:
        raise DataError('compiled segmentation archive identity/inventory mismatch')
    # Reject cheap identity/coverage failures before materializing large
    # dictionary graphs. This archive owns exactly these five section schemas.
    for key, codec in ((bundle.SEGMENT_PROGRAMS, segment_program_tables),
                        (bundle.SEGMENT_SUPPRESSIONS, segment_suppression_tables)):
        payload = sections[key][1]
        if len(payload) < codec.HEADER.size or codec.HEADER.unpack_from(payload)[-1] != expected:
            raise DataError('compiled segmentation recipe mismatch')
    programs = segment_program_tables.decode(sections[bundle.SEGMENT_PROGRAMS], len(tags))
    suppressions = segment_suppression_tables.decode(sections[bundle.SEGMENT_SUPPRESSIONS], len(tags))
    if programs.fingerprint != expected or suppressions.fingerprint != expected:
        raise DataError('compiled segmentation recipe mismatch')
    if tuple(row[:2] for row in programs.bindings) != tuple((locale, kind)
                                                         for locale in range(len(tags)) for kind in range(3)):
        raise DataError('compiled segmentation lacks locale/granularity routes')
    if tuple(row[0] for row in suppressions.bindings) != tuple(range(len(tags))):
        raise DataError('compiled segmentation lacks whole suppression routes')
    dictionaries = segment_dictionary_tables.decode(sections[bundle.SEGMENT_DICTIONARIES])
    if set(dictionaries) != set(segment_dictionary_tables.FAMILIES):
        raise DataError('compiled segmentation lacks dictionary families')
    return {identity: envelope['sections'][identity] for identity in SECTIONS}


def pair(sources, cldr_files, graph, tags, data, manifest):
    """Check the final registry before transferring only native data sections."""
    if (not isinstance(manifest, dict) or manifest.get('bytes') != len(data) or
            manifest.get('sha256') != acquire.digest(data) or
            not isinstance(manifest.get('inputs'), dict)):
        raise DataError('compiled segmentation differs from its preparation manifest')
    inputs = recipe(cldr_files, manifest['inputs'].get('dictionaries'), tags)
    identity = fingerprint(inputs)
    if (manifest.get('fingerprint') != identity.hex() or
            fingerprint(manifest['inputs']) != identity):
        raise DataError('compiled segmentation input receipt mismatch')
    ids = {tag: index for index, tag in enumerate(tags)}
    parents = tuple(0xffffffff if graph.parent(tag) is None else ids[graph.parent(tag)] for tag in tags)
    # Record the actual segmentation and inheritance inputs in the enclosing
    # generation receipt too; earlier acquisition/cache activity is irrelevant.
    for name in sorted(sources.paths('common/segments/')):
        if name.endswith('.xml'):
            sources.read(name)
    sources.read('common/dtd/ldml.dtd')
    return validate(data, tags, parents, identity), inputs
