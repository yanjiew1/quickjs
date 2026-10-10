#!/usr/bin/env python3
"""One native Intl wire1.3 binary, one UTF8 pool and one optional C owner.

Composes source tables directly, with an invocation-owned verified input
context. Explicit initial en/en-US availability includes actual DefaultLocale
and exact fallback closure. Installed coverage/frontends stay unactivated.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

MODULE_ORDER = ('metadata', 'locale_subset', 'preferences', 'list_patterns',
                'list_bundle', 'locale_info_records', 'plural_rules',
                'display_names', 'segmenter_properties', 'relative_patterns',
                'number_extensions', 'number_patterns', 'duration_patterns',
                'date_patterns', 'collation')


def load_modules():
    directory = Path(__file__).resolve().parent
    pins = json.loads((directory / 'MODULE-PINS.json').read_text())
    if set(pins) != {name + '.py' for name in MODULE_ORDER}:
        raise ValueError('combined generator module inventory mismatch')
    # Authenticate every module before any dependency is imported/executed.
    for name, expected in sorted(pins.items()):
        if hashlib.sha256((directory / name).read_bytes()).hexdigest() != expected:
            raise ValueError('combined generator source hash mismatch: ' + name)
    result = {}
    for name in MODULE_ORDER:
        path = directory / (name + '.py')
        if name in sys.modules:
            module = sys.modules[name]
            if not getattr(module, '__file__', None) or Path(module.__file__).resolve() != path:
                raise ValueError('combined generator import owner mismatch: ' + name)
        else:
            spec = importlib.util.spec_from_file_location(name, path)
            module = importlib.util.module_from_spec(spec)
            sys.modules[name] = module
            spec.loader.exec_module(module)
        result[name] = module
    return result, pins


def encode_blob(sections, widths, required, m):
    m.require(set(required) <= set(sections) <= set(widths), 'combined section inventory mismatch')
    cursor = m.HEADER_SIZE + m.DIRECTORY_SIZE * len(sections)
    directory, payload, inventory = [], bytearray(), []
    for identifier, data in sorted(sections.items()):
        width = widths[identifier]
        m.require(isinstance(data, bytes) and width > 0 and len(data) % width == 0,
                  'combined section byte/width mismatch')
        padding = -cursor % 4
        payload.extend(b'\0' * padding); cursor += padding
        directory.append(b''.join(m.u32(value) for value in
            (identifier, cursor, len(data), len(data) // width, width, 0)))
        inventory.append({'id': identifier, 'offset': cursor, 'bytes': len(data),
                          'records': len(data) // width, 'record_size': width})
        payload.extend(data); cursor += len(data); m.u32(cursor)
    header = b'QJSINTL\0' + struct.pack('<HH', 1, 3)
    header += b''.join(m.u32(value) for value in (64, cursor, len(sections), 64, 24, 0,
                                                18 << 16, 49 << 16, 18 << 16)) + b'\0' * 16
    m.require(len(header) == 64, 'combined header layout drift')
    blob = header + b''.join(directory) + bytes(payload)
    m.require(len(blob) == cursor, 'combined payload layout drift')
    return blob, inventory


def require_default_rows(metadata, policy, services, modules):
    """Prevent an explicit DefaultLocale policy from hiding missing tables."""
    m, p = modules['metadata'], modules['preferences']
    indices = {tag: index for index, tag in enumerate(sorted(metadata['parents']))}
    systems = {tag: index for index, tag in enumerate(sorted(metadata['numbering']))}
    checks = {}
    for tag in policy['available_locales']:
        index = indices[tag]
        nu = p.resolve_default(tag, 'numbering', metadata['locales'],
                               metadata['parents'], metadata['component_parents'])
        m.require(nu in systems and not metadata['numbering'][nu][1],
                  'explicit available locale lacks supported default nu: ' + tag)
        number_index = systems[nu]
        tests = {
            'list': any(row[0] == index for row in services['list']['rows']),
            'plural': {row[1] for row in services['plural']['locales'] if row[0] == index} == {0, 1},
            'display': any(row[0] == index for row in services['display']['rows']),
            'relative': any(row[0] == index for row in services['relative']['patterns']),
            'number': any(row[:2] == (index, number_index) for row in services['number']['tables'][80]),
            'duration': any(row[:2] == (index, number_index) for row in services['duration']['rows']),
            'date': any(row[0] == index for row in services['date']['patterns']),
            'date_range_fallback': any(row[0] == index for row in services['date']['range_fallbacks']),
            'date_all_16_calendars': {row[1] for row in services['date']['patterns'] if row[0] == index} ==
                {calendar for calendar, unused in modules['date_patterns'].CALENDARS},
            'date_all_16_range_fallbacks': {row[1] for row in services['date']['range_fallbacks'] if row[0] == index} ==
                {calendar for calendar, unused in modules['date_patterns'].CALENDARS},
            'collation': {row[1] for row in services['collation']['capabilities'] if row[0] == index} == {0, 1},
        }
        m.require(all(tests.values()), 'explicit available/default locale lacks service rows: ' + tag)
        checks[tag] = {'default_numbering': nu, 'candidate_rows_present': tests}
    # This proves rows exist, not full capabilities/conformance. The report
    # retains every collector's omitted/unsupported records and coverage0.
    return checks


def generated_outputs(args):
    modules, dependencies = load_modules()
    m, p, subset = (modules[name] for name in ('metadata', 'preferences', 'locale_subset'))
    context = m.VerifiedInputContext()
    reference = Path(args.icu_listformatter_reference).read_bytes()
    base = modules['list_bundle']
    m.require(hashlib.sha256(reference).hexdigest() == base.ICU_SOURCE_SHA256,
              'combined List context reference hash mismatch')
    aliases, defaults, likely, bcp, consumed, identity, unused = m.load_sources(
        args.cldr_dir, args.ucd_dir, args.cldr_manifest, args.ucd_manifest, context)
    metadata = p.load_pinned(args, defaults, bcp, consumed, context)
    metadata, policy = subset.select(metadata, args.default_locale, args.available_locales)
    consumed['references/icu-listformatter'] = {'path': 'references/icu-listformatter',
        'sha256': base.ICU_SOURCE_SHA256, 'bytes': len(reference)}
    services = {}
    for label, name in [('list', 'list_patterns'), ('plural', 'plural_rules'),
                        ('display', 'display_names'), ('relative', 'relative_patterns'),
                        ('number', 'number_patterns'), ('date', 'date_patterns'),
                        ('collation', 'collation')]:
        services[label] = modules[name].load_pinned(args, metadata, consumed, context)
    directions = modules['locale_info_records'].load_pinned(args, consumed, context)
    segmenter = modules['segmenter_properties'].load_pinned(args, consumed, context)
    services['duration'] = modules['duration_patterns'].load_pinned(
        args, metadata, services['number'], consumed, context)
    default_checks = require_default_rows(metadata, policy, services, modules)
    cldr = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    license_data = m.verified_input(args.cldr_dir, cldr, 'LICENSE', consumed, 'cldr', context)
    identity['inputs'] = {path: item['sha256'] for path, item in sorted(consumed.items())}
    identity['uca_version'] = '18.0.0'
    identity['locale_selection'] = policy
    identity['generator_dependencies'] = dependencies
    identity['generator_entrypoint_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    identity['list_context_reference'] = {'commit': base.ICU_COMMIT, 'sha256': base.ICU_SOURCE_SHA256}
    identity['wire_schema'] = {'version': [1, 3], 'reader_recognized_sections': 61,
        'absent_capabilities': [20, 25, 26],
        'segmenter_ep': 'packed64' if segmenter['standalone_ep'] else 'sole existing libunicode18 owner',
        'collation': 'atomic90..95; paired sort/search shared root; optional96..98 excluded'}
    digest = hashlib.sha256(m.canonical_json(identity)).digest()
    pooled = [(p, metadata), (modules['locale_info_records'], directions)]
    for label, name in [('list', 'list_patterns'), ('plural', 'plural_rules'),
                        ('display', 'display_names'), ('relative', 'relative_patterns'),
                        ('number', 'number_patterns'), ('duration', 'duration_patterns'), ('date', 'date_patterns')]:
        pooled.append((modules[name], services[label]))

    def extension(pool):
        sections = {}
        for module, data in pooled:
            extra = module.encode(data, pool)
            m.require(not set(sections) & set(extra), 'duplicate pooled service section')
            sections.update(extra)
        for extra in (modules['segmenter_properties'].encode(segmenter),
                      modules['collation'].encode(services['collation'])):
            m.require(not set(sections) & set(extra), 'duplicate scalar service section')
            sections.update(extra)
        return sections

    all_strings = tuple(value for module, data in pooled for value in module.strings(data))
    # This is the sole string-pool construction and sole binary composition.
    sections = m.encode_sections(aliases, likely, bcp, digest, all_strings,
                                extension, pool_type=m.UTF8StringPool)
    widths = dict(m.SECTION_WIDTHS)
    for extra in (p.WIDTHS, {40: 40, 41: 8}, modules['locale_info_records'].WIDTHS,
                  modules['plural_rules'].WIDTHS, modules['display_names'].WIDTHS,
                  modules['segmenter_properties'].SECTION_WIDTHS, modules['relative_patterns'].WIDTHS,
                  modules['number_patterns'].WIDTHS, modules['duration_patterns'].WIDTHS,
                  modules['date_patterns'].WIDTHS, modules['collation'].WIDTHS):
        m.require(not set(widths) & set(extra), 'duplicate combined section width owner')
        widths.update(extra)
    required = set(widths) - {25, 26}
    if not segmenter['standalone_ep']: required.remove(64)
    blob, inventory = encode_blob(sections, widths, required, m)
    outputs = {'intl-data.bin': blob, 'unicode-data-LICENSE.txt': license_data}
    if args.embed_c:
        header, source = m.embedding(blob)
        outputs.update({'locale-metadata.h': header, 'locale-metadata.c': source})
    report = {'schema': [1, 3], 'identity': identity, 'inputs': consumed,
        'generator_dependencies': dependencies, 'input_digest_sha256': digest.hex(),
        'binary_bytes': len(blob), 'sections': inventory, 'pool_owners': 1,
        'binary_owners': 1, 'c_embedding_owners': int(args.embed_c),
        'verified_input_context': dict(context.stats), 'default_locale_checks': default_checks,
        'preferences_evidence': metadata['evidence'],
        'service_evidence': {label: data['evidence'] for label, data in sorted(services.items())},
        'display_uncovered': services['display']['uncovered'],
        'locale_info_evidence': {'script_direction_source': 'cldr/common/properties/scriptMetadata.txt',
                                'installed_calendar_and_service_sections_absent': [25, 26]},
        'segmenter_evidence': {'default_uax29_only': True, 'unicode': '18.0.0',
            'uax29_revision': 49, 'tailorings': segmenter['tailoring'],
            'ep_source': 'packed64' if segmenter['standalone_ep'] else 'existing-libunicode18'},
        'capability_missing': ['calendar source rows do not establish required 16-calendar arithmetic, rendering, alias or frontend functionality; fresh runtime gates required',
            'algorithmic numbering', 'owner-reported unsupported/omitted records',
            'installed LocaleInfo calendar/service inventories25/26',
            'complete native timezone primary/country section20',
            'native JS frontend/backend activation and full service gates'],
        'required_calendar_canonical_count': 16, 'required_calendar_alias_count': 2,
        'service_coverage': 0, 'provider_activation': False, 'full_generated_coverage': False,
        'generated_payloads': {name: {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                               for name, data in sorted(outputs.items())}}
    outputs['generated-input-manifest.json'] = m.canonical_json(report)
    return outputs


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('cldr-dir', 'ucd-dir', 'cldr-manifest', 'ucd-manifest',
                   'icu-listformatter-reference', 'uax29-reference', 'uca-reference',
                   'cldr-collation-reference', 'default-locale', 'output-dir'):
        parser.add_argument('--' + option, required=True)
    parser.add_argument('--available-locales', nargs='+', required=True)
    parser.add_argument('--standalone-extended-pictographic', action='store_true')
    parser.add_argument('--embed-c', action='store_true')
    args = parser.parse_args(argv)
    try:
        outputs = generated_outputs(args)
        destination = Path(args.output_dir)
        destination.mkdir(parents=True, exist_ok=True)
        for name, data in sorted(outputs.items()):
            target = destination / name
            if not target.exists() or target.read_bytes() != data: target.write_bytes(data)
    except (OSError, ValueError) as exc:
        parser.exit(1, 'intl combined bundle: {}\n'.format(exc))
    return 0


if __name__ == '__main__': sys.exit(main())
