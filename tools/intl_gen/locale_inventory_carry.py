#!/usr/bin/env python3
"""Bounded native Intl inventory carry; root executes after TZ acceptance.

Carry every accepted formatter payload exactly. Extend pool1/list18 prefixes,
refresh provenance2, append installed calendars25 and territory timezones20.
No Number/Collator/Date collector, zic, compiler, network or Git is invoked.
Section26 stays absent: the provider bridge queries installed service owners.
XML/JSON are generation/proof inputs only; runtime has one Intl binary owner.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import xml.etree.ElementTree as ET

CLDR_COMMIT = '11f1f63d9390d1644c66a6b0704571de3cca4cbf'
TZ_COMMIT = '039ef27cc5f062a2055cb67435d6d71adbefd27d'
CALENDARS = ('buddhist', 'chinese', 'coptic', 'dangi', 'ethioaa', 'ethiopic',
             'gregory', 'hebrew', 'indian', 'islamic-civil', 'islamic-tbla',
             'islamic-umalqura', 'iso8601', 'japanese', 'persian', 'roc')
ALIASES = {'ethiopic-amete-alem': 'ethioaa', 'islamicc': 'islamic-civil'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def authenticated(path, pins):
    path = Path(path)
    data = path.read_bytes()
    require(sha(data) == pins[str(path)], 'immutable input drift: ' + str(path))
    return data


def module(name, path, pins):
    authenticated(path, pins)
    require(name not in sys.modules, 'preloaded generator helper: ' + name)
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    sys.modules[name] = result
    spec.loader.exec_module(result)
    return result


def decode(blob):
    require(blob[:12] == b'QJSINTL\0\x01\x00\x03\x00', 'carry wire identity')
    require(len(blob) >= 64, 'carry truncated header')
    size, count, start, stride = struct.unpack_from('<IIII', blob, 16)
    require(size == len(blob) and start == 64 and stride == 24 and
            count <= (len(blob) - 64) // 24, 'carry directory bounds')
    sections, widths = {}, {}
    last_id, end = 0, start + count * stride
    for row in range(count):
        identifier, offset, length, records, width, reserved = struct.unpack_from(
            '<6I', blob, start + row * stride)
        require(identifier > last_id and not reserved and width and
                records * width == length and offset >= end and offset % 4 == 0 and
                offset <= len(blob) and length <= len(blob) - offset,
                'carry directory record')
        require(not any(blob[end:offset]), 'carry nonzero padding')
        sections[identifier], widths[identifier] = blob[offset:offset + length], width
        last_id, end = identifier, offset + length
    require(end == len(blob) and {1, 2, 16, 18, 100, 107} <= sections.keys() and
            not {20, 25, 26} & sections.keys(), 'carry section inventory')
    return sections, widths


def extend_pool(m, accepted, strings):
    pool = m.UTF8StringPool(strings)
    refs, at = {'': (0, 0)}, 1
    require(accepted[:1] == accepted[-1:] == b'\0', 'carry pool framing')
    while at < len(accepted):
        end = accepted.index(0, at)
        value = accepted[at:end].decode('utf-8')
        require(value and value not in refs, 'carry pool duplicate member')
        refs[value] = (at, end - at)
        at = end + 1
    suffix = bytearray()
    for value in sorted(set(pool.refs) - refs.keys(), key=lambda text: text.encode('utf-8')):
        encoded = value.encode('utf-8')
        offset = len(accepted) + len(suffix)
        m.u32(offset + len(encoded) + 1)
        refs[value] = (offset, len(encoded))
        suffix.extend(encoded + b'\0')
    pool.refs, pool.data = refs, accepted + bytes(suffix)
    return pool


def encode(blob, sections, widths):
    cursor = 64 + len(sections) * 24
    directory, payload, inventory = [], bytearray(), []
    for identifier, data in sorted(sections.items()):
        width = widths[identifier]
        require(width > 0 and len(data) % width == 0, 'section width')
        padding = -cursor % 4
        payload.extend(b'\0' * padding)
        cursor += padding
        directory.append(struct.pack('<6I', identifier, cursor, len(data), len(data) // width, width, 0))
        inventory.append(dict(id=identifier, offset=cursor, bytes=len(data),
                              records=len(data) // width, record_size=width))
        payload.extend(data)
        cursor += len(data)
    header = bytearray(blob[:64])
    struct.pack_into('<II', header, 16, cursor, len(sections))
    return bytes(header) + b''.join(directory) + bytes(payload), inventory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('bindings', 'carry-dir', 'carry-state', 'calendar-proof',
                 'metadata-source', 'primary-source', 'tz-bundle-source',
                 'calendar-state', 'calendar-provider-source',
                 'cldr-dir', 'cldr-manifest', 'tz-dir', 'tz-pin',
                 'timezone-state', 'timezone-generated', 'output-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(__debug__, 'required checks disabled')
    pins = json.loads(args.bindings.read_text())
    # All imports and all consumed source/proof bytes authenticate first.
    m = module('inventory_metadata', args.metadata_source, pins)
    primary = module('inventory_primary', args.primary_source, pins)
    tz_bundle = module('inventory_tz_bundle', args.tz_bundle_source, pins)
    carry_state = json.loads(authenticated(args.carry_state, pins))
    timezone_state = json.loads(authenticated(args.timezone_state, pins))
    calendar_state = json.loads(authenticated(args.calendar_state, pins))
    calendar_provider = authenticated(args.calendar_provider_source, pins)
    require(all(state['status'] == 'passed' and state['commands_drained'] is True
                for state in (carry_state, timezone_state, calendar_state)), 'unaccepted runtime dependency')
    old_blob = authenticated(args.carry_dir / 'intl-data.bin', pins)
    old_report = json.loads(authenticated(args.carry_dir / 'generated-input-manifest.json', pins))
    old_sections, widths = decode(old_blob)
    sections = dict(old_sections)
    require(old_report['identity']['cldr_commit'] == CLDR_COMMIT and
            old_report['pool_owners'] == old_report['binary_owners'] == 1,
            'accepted Intl input owner/version')
    calendar_proof = json.loads(authenticated(args.calendar_proof, pins))
    require(calendar_proof['commit'] == 'd026068ed653130a37d79a95c0122ecf5319f1a4' and
            tuple(calendar_proof['public_canonical_values']) == CALENDARS and
            calendar_proof['raw_count'] == 18 and calendar_proof['public_count'] == 16 and
            set(calendar_proof['raw_table']) == set(CALENDARS) | ALIASES.keys(),
            'adopted Era calendar contract')
    cldr_manifest = json.loads(authenticated(args.cldr_manifest, pins))
    consumed = {}

    def cldr(name):
        data = authenticated(args.cldr_dir / name, pins)
        require(sha(data) == cldr_manifest[name], 'CLDR source manifest mismatch')
        consumed['cldr/' + name] = dict(path=name, sha256=sha(data), bytes=len(data))
        return data

    calendar_xml = ET.fromstring(cldr('common/bcp47/calendar.xml'))
    types = {item.get('name'): item for item in calendar_xml.findall("./keyword/key[@name='ca']/type")}
    require(set(CALENDARS) <= types.keys(), 'CLDR canonical calendar missing')
    require('ethiopic-amete-alem' in types['ethioaa'].get('alias', '').split() and
            types['islamicc'].get('preferred') == 'islamic-civil', 'CLDR calendar alias drift')
    # Inventory cannot substitute for formatter or arithmetic functionality.
    # The carried accepted Date bank must contain every installed calendar for
    # every locale closure row, in both patterns100 and range fallbacks107.
    date_calendars = {}
    for identifier in (100, 107):
        rows = {}
        for at in range(0, len(sections[identifier]), widths[identifier]):
            locale, offset, length = struct.unpack_from('<3I', sections[identifier], at)
            require(offset < len(sections[1]) and length < len(sections[1]) - offset and
                    sections[1][offset + length] == 0, 'Date calendar string bounds')
            calendar = sections[1][offset:offset + length].decode('ascii')
            rows.setdefault(locale, set()).add(calendar)
        require(set(rows) == set(range(len(sections[16]) // 48)) and
                all(values == set(CALENDARS) for values in rows.values()),
                'accepted all16 Date capability bank incomplete')
        date_calendars[str(identifier)] = {str(row): sorted(values) for row, values in rows.items()}
    tz_pin = json.loads(authenticated(args.tz_pin, pins))
    require(tz_pin['commit'] == TZ_COMMIT and tz_pin['release'] == '2026e', 'TZ release pin')
    zone_data = authenticated(args.tz_dir / 'zone.tab', pins)
    require(sha(zone_data) == tz_pin['files']['zone.tab'], 'zone.tab pin')
    table_countries, country_rows = primary.zone_table(zone_data.decode('utf-8'))
    countries = primary.cldr_countries(cldr('common/bcp47/timezone.xml'), table_countries)
    tz_binary = authenticated(args.timezone_generated / 'timezone-data.bin', pins)
    tz_report = json.loads(authenticated(args.timezone_generated / 'timezone-data.json', pins))
    records = tz_bundle.unpack(tz_binary)
    require(records == tz_report['records'] and tz_report['pin'] == TZ_COMMIT and
            tz_report['release'] == '2026e' and tz_report['runtime_tzcode'] is False and
            tz_report['final_binary_files'] == 1 and tz_report['data_sha256'] == sha(tz_binary) and
            tz_report['country_metadata']['cldr_commit'] == CLDR_COMMIT and
            tz_report['country_metadata']['xml_sha256'] == consumed['cldr/common/bcp47/timezone.xml']['sha256'],
            'accepted TZ binary/provenance mismatch')
    by_name = {record['identifier']: record for record in records}
    require(all(name in by_name and by_name[name]['primary'] == name for name in table_countries),
            'ECMA402 zone.tab primary promotion missing')
    regions = {}
    for record in records:
        name = record['identifier']
        if record['primary'] == name and name in countries:
            regions.setdefault(countries[name], set()).add(name)
    require(set(regions) == set(country_rows), 'timezone territory coverage mismatch')
    require(all(set(names) <= regions[region] for region, names in country_rows.items()),
            'zone.tab geographic inventory incomplete')
    regions = {region: sorted(names) for region, names in sorted(regions.items())}
    strings = list(CALENDARS) + list(regions) + [name for names in regions.values() for name in names]
    pool = extend_pool(m, sections[1], strings)
    # One list owner: append after the exact existing section18 record prefix.
    list_payload, territorial = bytearray(sections[18]), []
    for region, names in regions.items():
        first = len(list_payload) // 8
        list_payload.extend(b''.join(pool.ref(name) for name in names))
        territorial.append(pool.ref(region) + m.u32(first) + m.u32(len(names)))
    sections[1], sections[18] = pool.data, bytes(list_payload)
    sections[20] = b''.join(territorial)
    sections[25] = b''.join(pool.ref(name) for name in CALENDARS)
    widths.update({20: 16, 25: 8})
    preserved = {identifier: sha(data) for identifier, data in old_sections.items() if identifier not in (1, 2, 18)}
    identity = dict(old_report['identity'])
    identity['inventory_carry'] = dict(generator_sha256=sha(Path(__file__).read_bytes()),
        accepted_blob_sha256=sha(old_blob), accepted_state_sha256=sha(authenticated(args.carry_state, pins)),
        accepted_timezone_state_sha256=sha(authenticated(args.timezone_state, pins)),
        accepted_calendar_state_sha256=sha(authenticated(args.calendar_state, pins)),
        accepted_calendar_provider_sha256=sha(calendar_provider),
        timezone_blob_sha256=sha(tz_binary), timezone_manifest_sha256=sha(authenticated(args.timezone_generated / 'timezone-data.json', pins)),
        sections_sha256=preserved, extended_prefixes={str(i): sha(old_sections[i]) for i in (1, 18)},
        installed_calendars=list(CALENDARS), aliases=ALIASES, territory_regions=len(regions),
        actual_carried_Date_bank=date_calendars,
        service26_present=False, service26_reason='General bridge queries actual installed Number/Collator registry; section16 coverage stays byte-exact')
    identity['inputs'] = dict(identity['inputs'])
    identity['inputs'].update({name: value['sha256'] for name, value in consumed.items()})
    identity['inputs']['tz/zone.tab'] = sha(zone_data)
    identity['wire_schema'] = dict(identity['wire_schema'])
    identity['wire_schema']['absent_capabilities'] = [26]
    sections[2] = hashlib.sha256(m.canonical_json(identity)).digest()
    require(all(sha(sections[i]) == value for i, value in preserved.items()), 'accepted payload changed')
    require(all(sections[i].startswith(old_sections[i]) for i in (1, 18)), 'accepted shared prefix changed')
    binary, inventory = encode(old_blob, sections, widths)
    header, source = m.embedding(binary)
    outputs = {'intl-data.bin': binary, 'locale-metadata.h': header, 'locale-metadata.c': source,
               'unicode-data-LICENSE.txt': authenticated(args.carry_dir / 'unicode-data-LICENSE.txt', pins)}
    report = dict(old_report)
    report.update(identity=identity, sections=inventory, binary_bytes=len(binary),
        input_digest_sha256=sections[2].hex(), inventory_generation_inputs=consumed,
        inventory_regions=regions, installed_calendar_inventory=list(CALENDARS),
        installed_calendar_aliases=ALIASES, installed_service_section26=False,
        inventory_runtime_pass=False, formatter_collectors_executed=False,
        deterministic_generation_scope='append20/25; exact old payloads and pool1/list18 prefixes',
        pool_owners=1, binary_owners=1, c_embedding_owners=1)
    report['locale_info_evidence'] = dict(report['locale_info_evidence'])
    report['locale_info_evidence']['installed_calendar_and_service_sections_absent'] = [26]
    report['locale_info_evidence']['territory_source'] = 'tzdata2026e zone.tab + verified CLDR49 geographic aliases + actual accepted QJTZ primaries'
    # Retain other reported limitations. Their metadata-only inventory wording
    # is superseded explicitly; this standalone carry proves no JS activation.
    report['capability_missing'] = [value for value in report['capability_missing']
        if value not in ('installed LocaleInfo calendar/service inventories25/26',
                         'complete native timezone primary/country section20')]
    report['capability_missing'].append('optional26 absent; actual service inventories supplied by native General bridge')
    report['generated_payloads'] = {name: dict(bytes=len(data), sha256=sha(data)) for name, data in sorted(outputs.items())}
    outputs['generated-input-manifest.json'] = m.canonical_json(report)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    for name, data in sorted(outputs.items()):
        (args.output_dir / name).write_bytes(data)


if __name__ == '__main__':
    main()
