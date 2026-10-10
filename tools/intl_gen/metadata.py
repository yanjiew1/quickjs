#!/usr/bin/env python3
"""Host-only CLDR 49 / Unicode 18 locale binary generator (Python >= 3.6).

Runtime consumers read intl-data.bin or its optional single C definition.
XML and JSON are build inputs/reports only. This M01-M04 baseline contains
aliases, BCP47 inventories and likely subtags, not formatter service data.
"""
import argparse
import hashlib
import json
import os
from collections.abc import Mapping
from types import MappingProxyType
from pathlib import Path, PurePosixPath
import re
import struct
import sys
import xml.etree.ElementTree as ET

CLDR_COMMIT = '11f1f63d9390d1644c66a6b0704571de3cca4cbf'
CLDR_MANIFEST_SHA256 = '2111ca38d60cb01e7032d4bee5f5401239bd538bea85a9e3d071744a2daa429a'
UCD_MANIFEST_SHA256 = '7fd0614fa280b42a15cfdcc2251557090ed752bb2ba48ca1c8959904de1dd192'
U32_LIMIT = 0xffffffff
HEADER_SIZE = 64
DIRECTORY_SIZE = 24
SCHEMA_MAJOR = 1
SCHEMA_MINOR = 0
SECTION_WIDTHS = {1: 1, 2: 32, 10: 20, 11: 8, 12: 16, 13: 48}
OPTIONAL_WIDTHS = {14: 12, 15: 36, 16: 48, 17: 52, 18: 8, 19: 20,
                   20: 16, 21: 44, 22: 12, 23: 20}
ALIAS_KINDS = {'languageAlias': 0, 'scriptAlias': 1, 'territoryAlias': 2,
               'variantAlias': 3, 'subdivisionAlias': 4}
VALUE_TYPES = {'single': 0, 'multiple': 1, 'incremental': 2, 'any': 3}


class DataError(ValueError):
    pass


def require(ok, message):
    if not ok:
        raise DataError(message)


def u32(value):
    require(isinstance(value, int) and 0 <= value < U32_LIMIT,
            'integer exceeds v1 bounds')
    return struct.pack('<I', value)


def words(value):
    return tuple(value.split())


def ascii_text(value, empty=False):
    require(isinstance(value, str) and (empty or bool(value)),
            'missing ASCII metadata string')
    require(all(0x20 <= ord(c) < 0x7f for c in value),
            'metadata string is not printable ASCII')
    return value


def canonical_tag(value, strict=False, maximal=False):
    """Normalize source spelling; deliberately not an ECMA-402 tag parser.

    Alias source keys include i-*, sign-language and private-use legacy tags.
    Extlangs remain separate subtags; all provided script/region fields survive.
    Strict likely-subtag keys permit language + optional script + optional region.
    """
    ascii_text(value)
    parts = value.replace('_', '-').split('-')
    require(all(re.fullmatch('[A-Za-z0-9]{1,8}', p) for p in parts),
            'invalid source tag: ' + value)
    normalized = []
    private = False
    for index, part in enumerate(parts):
        if index == 0:
            normalized.append(part.lower())
        elif private:
            normalized.append(part.lower())
        elif len(part) == 1:
            normalized.append(part.lower())
            private = True
        elif len(part) == 4 and part.isalpha():
            normalized.append(part.title())
        elif (len(part) == 2 and part.isalpha()) or (len(part) == 3 and part.isdigit()):
            normalized.append(part.upper())
        else:
            normalized.append(part.lower())
    result = '-'.join(normalized)
    if strict:
        require(re.fullmatch('[a-z]{2,8}(?:-[A-Z][a-z]{3})?(?:-(?:[A-Z]{2}|[0-9]{3}))?', result),
                'invalid likely-subtag key: ' + value)
    if maximal:
        require(re.fullmatch('[a-z]{2,8}-[A-Z][a-z]{3}-(?:[A-Z]{2}|[0-9]{3})', result),
                'likely-subtag target must provide language/script/region')
    return result


def normalize_alias(kind, value, replacement=False):
    if kind == 0:
        return canonical_tag(value)
    if kind == 1:
        require(re.fullmatch('[A-Za-z]{4}', value), 'invalid script alias')
        return value.title()
    if kind == 2:
        require(re.fullmatch('(?:[A-Za-z]{2,3}|[0-9]{3})', value),
                'invalid region alias')
        return value.upper()
    if kind == 3:
        require(re.fullmatch('[A-Za-z0-9]{4,8}', value), 'invalid variant alias')
        return value.lower()
    if replacement and re.fullmatch('(?:[A-Za-z]{2}|[0-9]{3})', value):
        return value.upper()  # subdivision-to-region alias, not a subdivision
    require(re.fullmatch('[A-Za-z0-9]{2,8}', value), 'invalid subdivision alias')
    return value.lower()


def insert_unique(table, key, value, label):
    """Equivalent normalized duplicate records coalesce; conflicts abort."""
    require(key not in table or table[key] == value,
            'conflicting duplicate {}: {}'.format(label, key))
    table[key] = value


def check_cycles(graph, label):
    # Iterative DFS: neither input size nor chain length consumes Python stack.
    state = {}
    for origin in sorted(graph):
        if state.get(origin) == 2:
            continue
        stack = [(origin, False)]
        while stack:
            node, leaving = stack.pop()
            if leaving:
                state[node] = 2
                continue
            require(state.get(node) != 1, label + ' cycle: ' + str(node))
            if state.get(node) == 2:
                continue
            state[node] = 1
            stack.append((node, True))
            for target in reversed(sorted(graph.get(node, ()))):
                if target in graph:
                    require(state.get(target) != 1, label + ' cycle: ' + str(target))
                    if state.get(target) != 2:
                        stack.append((target, False))


def parse_xml(data, expected_root):
    # ElementTree does not resolve external DTDs. Reject custom entity/internal
    # subsets anyway: trusted upstream inputs need only a SYSTEM declaration.
    require(b'<!ENTITY' not in data, 'XML entity declarations are forbidden')
    for declaration in re.findall(br'<!DOCTYPE[^>]*>', data):
        require(b'[' not in declaration, 'XML internal subsets are forbidden')
    try:
        root = ET.fromstring(data)
    except ET.ParseError as exc:
        raise DataError('invalid XML: ' + str(exc))
    require(root.tag == expected_root, 'unexpected XML root')
    return root


def parse_aliases(data):
    root = parse_xml(data, 'supplementalData')
    alias = root.find('./metadata/alias')
    require(alias is not None, 'missing metadata aliases')
    records = {}
    for element in alias:
        if element.tag == 'zoneAlias':
            # IANA timezone owner owns these records and its canonical policy.
            # They cannot be squeezed into the five locale alias kind enums.
            continue
        require(element.tag in ALIAS_KINDS, 'unknown alias kind: ' + element.tag)
        kind = ALIAS_KINDS[element.tag]
        source = normalize_alias(kind, element.get('type', ''))
        replacements = tuple(normalize_alias(kind, item, replacement=True)
                             for item in words(element.get('replacement', '')))
        require(replacements and len(set(replacements)) == len(replacements),
                'empty or repeated alias replacement')
        insert_unique(records, (kind, source), replacements, 'alias')
    graph = {(kind, source): tuple((kind, r) for r in replacement)
             for (kind, source), replacement in records.items()}
    check_cycles(graph, 'alias')
    default = root.find('./metadata/defaultContent')
    default_content = () if default is None else tuple(sorted(set(
        canonical_tag(tag, strict=True) for tag in words(default.get('locales', '')))))
    return tuple((kind, source, records[(kind, source)])
                 for kind, source in sorted(records)), default_content


def parse_likely(data):
    root = parse_xml(data, 'supplementalData')
    parent = root.find('likelySubtags')
    require(parent is not None, 'missing likely subtags')
    table = {}
    for element in parent:
        require(element.tag == 'likelySubtag', 'unknown likely-subtag element')
        source = canonical_tag(element.get('from', ''), strict=True)
        target = canonical_tag(element.get('to', ''), strict=True, maximal=True)
        insert_unique(table, source, target, 'likely subtag')
    require('und' in table, 'missing likely-subtag und fallback')
    return tuple(sorted(table.items()))


def bcp_record(extension, key, element, inherited_value_type):
    name = ascii_text(element.get('name', ''))
    require(' ' not in name, 'BCP47 name must be one ASCII token')
    aliases = words(element.get('alias', ''))
    require(len(set(a.lower() for a in aliases)) == len(aliases),
            'repeated BCP47 alias')
    for alias in aliases:
        ascii_text(alias)
    preferred = ascii_text(element.get('preferred', ''), empty=True)
    require(' ' not in preferred, 'BCP47 preferred must be one ASCII token')
    value_type = element.get('valueType', inherited_value_type)
    require(value_type in VALUE_TYPES, 'unknown BCP47 valueType')
    deprecated = element.get('deprecated', 'false')
    require(deprecated in ('true', 'false'), 'invalid BCP47 deprecated flag')
    # Full ordered alias list preserved, with one ASCII space between tokens.
    return (extension, key, name, ' '.join(aliases), preferred,
            VALUE_TYPES[value_type], int(deprecated == 'true'))


def parse_bcp47(inputs):
    """inputs is an iterable of (logical path, XML bytes), never directory order."""
    table = {}
    for path, data in sorted(inputs):
        root = parse_xml(data, 'ldmlBCP47')
        for key in root.findall('./keyword/key'):
            name = ascii_text(key.get('name', ''))
            extension = key.get('extension', 'u')
            require(extension in ('u', 't'), 'unknown BCP47 extension')
            require(re.fullmatch('[a-z0-9]{2}', name), 'invalid BCP47 key')
            key_record = bcp_record(extension, name, key, 'single')
            # Empty name is reserved for the key-level inventory record.
            key_record = key_record[:2] + ('',) + key_record[3:]
            insert_unique(table, (extension, name, ''), key_record, 'BCP47 key')
            for element in key:
                require(element.tag == 'type', 'unknown BCP47 key child')
                record = bcp_record(extension, name, element,
                                    key.get('valueType', 'single'))
                insert_unique(table, record[:3], record, 'BCP47 type')
    require(table, 'empty BCP47 inventory')
    validate_bcp47_ambiguity(table)
    return tuple(table[key] for key in sorted(table))


def validate_bcp47_ambiguity(table):
    # Preserve inventories (including symbolic CODEPOINTS/REORDER_CODE), while
    # rejecting ambiguous aliases after resolving explicit preferred chains.
    graph = {}
    for identity, record in table.items():
        extension, key, name, alias, preferred, value_type, deprecated = record
        if preferred:
            target = (extension, key, preferred)
            require(target in table, 'unknown preferred BCP47 target')
            graph[identity] = (target,)
    check_cycles(graph, 'BCP47 preferred')

    def ultimate(identity):
        while identity in graph:
            identity = graph[identity][0]
        return identity

    claims = {}
    for identity in sorted(table):
        extension, key, name, alias, preferred, value_type, deprecated = table[identity]
        target = ultimate(identity)
        scope = (extension, key) if name else (extension, '')
        tokens = (name if name else key,) + words(alias)
        for token in tokens:
            insert_unique(claims, scope + (token.lower(),), target,
                          'BCP47 alias resolution')


def unique_json_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON manifest key')
        result[key] = value
    return result


def _load_pin_manifest(path, expected_digest):
    data = Path(path).read_bytes()
    require(hashlib.sha256(data).hexdigest() == expected_digest,
            'source manifest does not match exact pinned snapshot')
    try:
        manifest = json.loads(data.decode('utf-8'), object_pairs_hook=unique_json_pairs)
    except (UnicodeError, ValueError) as exc:
        raise DataError('invalid source manifest: ' + str(exc))
    require(isinstance(manifest, dict), 'source manifest must be a map')
    for path, digest in manifest.items():
        logical = PurePosixPath(path)
        require(not logical.is_absolute() and '..' not in logical.parts and
                str(logical) == path and '\\' not in path, 'unsafe source manifest path')
        require(isinstance(digest, str) and re.fullmatch('[0-9a-f]{64}', digest),
                'invalid source SHA256')
    return manifest


class _PinnedManifest(Mapping):
    """Read-only entries from one exact, verified manifest snapshot."""

    __slots__ = ('_entries', '_source_path', '_pin')

    def __init__(self, entries, source_path, pin):
        self._entries = MappingProxyType(dict(entries))
        self._source_path = source_path
        self._pin = pin

    def __getitem__(self, key):
        return self._entries[key]

    def __iter__(self):
        return iter(self._entries)

    def __len__(self):
        return len(self._entries)


class VerifiedInputContext:
    """Verified bytes owned by one generated_outputs invocation.

    Each source is read and hashed on first use. Later consumers parse the same
    immutable byte snapshot and receive fresh provenance records. Source edits
    after first use cannot alter this snapshot; a subsequent generation creates
    a new context and checks the files again. Do not share this object between
    processes or use it as a persistent input cache.

    A namespace binds to one resolved root and one manifest pin. An input key
    includes that binding, its logical path, and the expected content hash.
    Manifests are read-only and must have been loaded by this context.
    """

    def __init__(self):
        self._pid = os.getpid()
        self._manifests = {}
        self._bindings = {}
        self._inputs = {}
        self._counts = {'manifest_reads': 0, 'manifest_hits': 0,
                        'input_reads': 0, 'input_hits': 0,
                        'verified_input_bytes': 0}

    def _check_process(self):
        require(os.getpid() == self._pid,
                'verified input context cannot cross process boundaries')

    @property
    def stats(self):
        self._check_process()
        return MappingProxyType(dict(self._counts))

    def load_manifest(self, path, expected_digest):
        self._check_process()
        source_path = str(Path(path).resolve())
        key = (source_path, expected_digest)
        if key in self._manifests:
            self._counts['manifest_hits'] += 1
            return self._manifests[key]
        entries = _load_pin_manifest(source_path, expected_digest)
        manifest = _PinnedManifest(entries, source_path, expected_digest)
        self._manifests[key] = manifest
        self._counts['manifest_reads'] += 1
        return manifest

    def read(self, root, manifest, relative, consumed, namespace):
        self._check_process()
        require(isinstance(manifest, _PinnedManifest),
                'context requires its own verified manifest')
        manifest_key = (manifest._source_path, manifest._pin)
        require(self._manifests.get(manifest_key) is manifest,
                'manifest belongs to another verified input context')
        require(relative in manifest,
                'source missing from hash manifest: ' + relative)
        require(isinstance(namespace, str) and namespace and '/' not in namespace,
                'invalid source namespace')
        base = Path(root).resolve()
        path = base.joinpath(*PurePosixPath(relative).parts).resolve()
        require(base in path.parents, 'source path escapes pinned directory')
        binding = (str(base),) + manifest_key
        require(namespace not in self._bindings or self._bindings[namespace] == binding,
                'verified input namespace changed root or manifest pin: ' + namespace)
        self._bindings[namespace] = binding
        expected = manifest[relative]
        key = (namespace, binding, relative, expected)
        if key in self._inputs:
            data = self._inputs[key]
            self._counts['input_hits'] += 1
        else:
            data = path.read_bytes()
            require(hashlib.sha256(data).hexdigest() == expected,
                    'source hash mismatch: ' + relative)
            self._inputs[key] = data
            self._counts['input_reads'] += 1
            self._counts['verified_input_bytes'] += len(data)
        logical_path = namespace + '/' + relative
        record = {'sha256': expected, 'bytes': len(data),
                  'manifest_sha256': manifest._pin, 'path': logical_path}
        require(logical_path not in consumed or consumed[logical_path] == record,
                'conflicting consumed source provenance: ' + logical_path)
        consumed[logical_path] = record
        return data


def load_pin_manifest(path, expected_digest, context=None):
    if context is not None:
        return context.load_manifest(path, expected_digest)
    return _load_pin_manifest(path, expected_digest)


def verified_input(root, manifest, relative, consumed, namespace, context=None):
    if context is not None:
        return context.read(root, manifest, relative, consumed, namespace)
    require(relative in manifest, 'source missing from hash manifest: ' + relative)
    base = Path(root).resolve()
    path = base.joinpath(*PurePosixPath(relative).parts).resolve()
    require(base in path.parents, 'source path escapes pinned directory')
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    require(digest == manifest[relative], 'source hash mismatch: ' + relative)
    consumed[namespace + '/' + relative] = {'sha256': digest, 'bytes': len(data)}
    return data


def load_sources(cldr_dir, ucd_dir, cldr_manifest_path, ucd_manifest_path, context=None):
    cldr = load_pin_manifest(cldr_manifest_path, CLDR_MANIFEST_SHA256, context)
    ucd = load_pin_manifest(ucd_manifest_path, UCD_MANIFEST_SHA256, context)
    consumed = {}

    def read_cldr(relative):
        return verified_input(cldr_dir, cldr, relative, consumed, 'cldr', context)

    dtd = read_cldr('common/dtd/ldml.dtd')
    require(re.search(br'cldrVersion\s+CDATA\s+#FIXED\s+"49"', dtd),
            'CLDR DTD does not declare version 49')
    read_cldr('common/dtd/ldmlBCP47.dtd')
    read_cldr('common/dtd/ldmlSupplemental.dtd')
    license_data = read_cldr('LICENSE')
    require(license_data, 'empty CLDR license')
    readme = verified_input(ucd_dir, ucd, 'ReadMe.txt', consumed, 'ucd', context)
    require(b'final data files for version 18.0.0' in readme,
            'UCD is not final Unicode 18.0.0')
    aliases, defaults = parse_aliases(read_cldr('common/supplemental/supplementalMetadata.xml'))
    likely = parse_likely(read_cldr('common/supplemental/likelySubtags.xml'))
    paths = sorted(path for path in cldr
                   if path.startswith('common/bcp47/') and path.endswith('.xml'))
    # Discovery depends exclusively on the sealed manifest, never glob order.
    bcp = parse_bcp47((path, read_cldr(path)) for path in paths)
    identity = {'cldr_commit': CLDR_COMMIT, 'cldr_version': '49.0.0',
                'unicode_version': '18.0.0', 'uca_version': None,
                'license': {'source': 'cldr/LICENSE',
                            'sha256': consumed['cldr/LICENSE']['sha256']},
                'inputs': {path: item['sha256'] for path, item in sorted(consumed.items())}}
    identity_bytes = canonical_json(identity)
    return aliases, defaults, likely, bcp, consumed, identity, hashlib.sha256(identity_bytes).digest()


def canonical_json(value):
    return (json.dumps(value, ensure_ascii=True, sort_keys=True,
                       separators=(',', ':')) + '\n').encode('ascii')


class StringPool:
    def __init__(self, strings, allow_unicode=False):
        self.refs = {'': (0, 0)}
        pieces = [b'\0']
        offset = 1
        unique = set()
        for value in strings:
            # Validate before hashing/sorting/encoding, so malformed Unicode
            # and non-string members fail through the generator's DataError.
            if allow_unicode:
                require(isinstance(value, str), 'pool member must be a string')
                require(all(ord(c) != 0 and not 0xd800 <= ord(c) <= 0xdfff for c in value),
                        'invalid Unicode scalar or NUL in string pool')
            else:
                ascii_text(value, empty=True)
            unique.add(value)
        # Validated ASCII code-point order equals the previous UTF8 byte order.
        for value in sorted(unique - {''}, key=lambda item: item.encode('utf-8')):
            encoded = value.encode('utf-8')
            u32(offset + len(encoded) + 1)
            self.refs[value] = (offset, len(encoded))
            pieces.append(encoded + b'\0')
            offset += len(encoded) + 1
        self.data = b''.join(pieces)

    def ref(self, value):
        return b''.join(u32(n) for n in self.refs[value])


class UTF8StringPool(StringPool):
    """Service pattern interner; metadata identifier policy stays ASCII."""
    def __init__(self, strings):
        super().__init__(strings, allow_unicode=True)


def encode_sections(aliases, likely, bcp, input_digest,
                    extra_strings=(), extra_encoder=None, pool_type=StringPool):
    require(len(input_digest) == 32, 'input digest must be exactly 32 bytes')
    strings = []
    for kind, source, replacements in aliases:
        strings.extend((source,) + replacements)
    for source, target in likely:
        strings.extend((source, target))
    for record in bcp:
        strings.extend(record[:5])
    for value in strings:
        ascii_text(value, empty=True)
    strings.extend(extra_strings)
    pool = pool_type(strings)
    lists = sorted(set(record[2] for record in aliases))
    spans = {}
    replacement_records = []
    for replacements in lists:
        first = len(replacement_records)
        u32(first + len(replacements))
        spans[replacements] = (first, len(replacements))
        replacement_records.extend(pool.ref(value) for value in replacements)
    alias_records = [u32(kind) + pool.ref(source) + b''.join(u32(v) for v in spans[replacement])
                     for kind, source, replacement in aliases]
    likely_records = [pool.ref(source) + pool.ref(target) for source, target in likely]
    bcp_records = [b''.join(pool.ref(value) for value in record[:5]) +
                   u32(record[5]) + u32(record[6]) for record in bcp]
    sections = {1: pool.data, 2: input_digest, 10: b''.join(alias_records),
            11: b''.join(replacement_records), 12: b''.join(likely_records),
            13: b''.join(bcp_records)}
    if extra_encoder is not None:
        extra = extra_encoder(pool)
        require(not set(extra) & set(sections), 'duplicate generated section')
        sections.update(extra)
    return sections


def encode_blob(sections, minor=0):
    widths = dict(SECTION_WIDTHS)
    widths.update(OPTIONAL_WIDTHS)
    require(minor in (0, 1), 'unsupported schema minor')
    require(set(SECTION_WIDTHS) <= set(sections) <= set(widths), 'wrong metadata sections')
    require(minor == 1 or not set(sections) & {21, 22, 23},
            'schema1.0 cannot carry schema1.1 sections')
    count = len(sections)
    cursor = HEADER_SIZE + DIRECTORY_SIZE * count
    directories = []
    payload = bytearray()
    inventory = []
    for section_id in sorted(sections):
        data = sections[section_id]
        width = widths[section_id]
        require(len(data) % width == 0, 'invalid section record width')
        padding = (-cursor) % 4
        payload.extend(b'\0' * padding)
        cursor += padding
        records = len(data) // width
        entry = (section_id, cursor, len(data), records, width, 0)
        directories.append(b''.join(u32(value) for value in entry))
        inventory.append({'id': section_id, 'offset': cursor, 'bytes': len(data),
                          'records': records, 'record_size': width})
        payload.extend(data)
        cursor += len(data)
        u32(cursor)
    header = b'QJSINTL\0' + struct.pack('<HH', SCHEMA_MAJOR, minor)
    header += b''.join(u32(value) for value in
                      (HEADER_SIZE, cursor, count, HEADER_SIZE, DIRECTORY_SIZE,
                       0, 18 << 16, 49 << 16, 0))
    header += b'\0' * 16
    require(len(header) == HEADER_SIZE, 'header layout drift')
    blob = header + b''.join(directories) + bytes(payload)
    require(len(blob) == cursor, 'blob layout drift')
    return blob, inventory


def embedding(blob):
    header = ('/* Generated extern declarations only. */\n'
              '#ifndef QJS_INTL_LOCALE_METADATA_H\n#define QJS_INTL_LOCALE_METADATA_H\n'
              '#include <stddef.h>\n'
              'extern const unsigned char qjs_intl_locale_metadata_blob[];\n'
              'extern const size_t qjs_intl_locale_metadata_blob_size;\n#endif\n')
    source = ('/* Generated single owning definition. Do not edit. */\n'
              '#include "locale-metadata.h"\n'
              'const unsigned char qjs_intl_locale_metadata_blob[] = {\n')
    for offset in range(0, len(blob), 12):
        source += '    ' + ', '.join('0x{:02x}'.format(v) for v in blob[offset:offset + 12]) + ',\n'
    source += ('};\nconst size_t qjs_intl_locale_metadata_blob_size =\n'
               '    sizeof(qjs_intl_locale_metadata_blob);\n')
    return header.encode('ascii'), source.encode('ascii')


def generated_outputs(args):
    context = VerifiedInputContext()
    aliases, defaults, likely, bcp, consumed, identity, digest = load_sources(
        args.cldr_dir, args.ucd_dir, args.cldr_manifest, args.ucd_manifest, context)
    preference_data = None
    minor = 0
    if getattr(args, 'include_preferences', False):
        import preferences
        preference_data = preferences.load_pinned(args, defaults, bcp, consumed, context)
        identity['inputs'] = {path: item['sha256'] for path, item in sorted(consumed.items())}
        digest = hashlib.sha256(canonical_json(identity)).digest()
        sections = encode_sections(aliases, likely, bcp, digest,
                                   preferences.strings(preference_data),
                                   lambda pool: preferences.encode(preference_data, pool))
        minor = 1
    else:
        sections = encode_sections(aliases, likely, bcp, digest)
    blob, inventory = encode_blob(sections, minor=minor)
    outputs = {'intl-data.bin': blob}
    if args.embed_c:
        header, source = embedding(blob)
        outputs['locale-metadata.h'] = header
        outputs['locale-metadata.c'] = source
    report = {'schema': [SCHEMA_MAJOR, minor], 'identity': identity,
              'input_digest_sha256': digest.hex(), 'inputs': consumed,
              'raw_consumed_input_bytes': sum(v['bytes'] for v in consumed.values()),
              'binary_bytes': len(blob), 'sections': inventory,
              'generated_payloads': {name: {'bytes': len(data),
                                      'sha256': hashlib.sha256(data).hexdigest()}
                                     for name, data in sorted(outputs.items())},
              'default_content_handoff_to_M05': list(defaults),
              'service_coverage': 0,
              'scope': 'M01-M04 only; no formatter engines or service data'}
    if preference_data is not None:
        report['scope'] = 'M01-M07 metadata only; no formatter engines or service data'
        report['preferences_evidence'] = preference_data['evidence']
        report.pop('default_content_handoff_to_M05')
    report['verified_input_context'] = dict(context.stats)
    outputs['generated-input-manifest.json'] = canonical_json(report)
    return outputs


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cldr-dir', required=True)
    parser.add_argument('--ucd-dir', required=True)
    parser.add_argument('--cldr-manifest', required=True)
    parser.add_argument('--ucd-manifest', required=True)
    parser.add_argument('--output-dir', required=True)
    parser.add_argument('--embed-c', action='store_true')
    parser.add_argument('--include-preferences', action='store_true',
                        help='emit sparse M05-M07 metadata using additive wire1.1')
    parser.add_argument('--verify-output', action='store_true',
                        help='compare regeneration in memory against existing outputs')
    args = parser.parse_args(argv)
    try:
        outputs = generated_outputs(args)
        destination = Path(args.output_dir)
        if args.verify_output:
            for name, data in sorted(outputs.items()):
                path = destination / name
                require(path.is_file() and path.read_bytes() == data,
                        'generated output differs: ' + name)
        else:
            destination.mkdir(parents=True, exist_ok=True)
            # Build every byte and validate all inputs before writing any output.
            for name, data in sorted(outputs.items()):
                path = destination / name
                if not path.exists() or path.read_bytes() != data:
                    path.write_bytes(data)
    except (DataError, OSError) as exc:
        parser.exit(1, 'intl metadata: {}\n'.format(exc))
    return 0


if __name__ == '__main__':
    # The optional sibling module must share this module's DataError class.
    sys.modules['metadata'] = sys.modules[__name__]
    sys.exit(main())
