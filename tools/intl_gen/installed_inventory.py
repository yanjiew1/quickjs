"""Pure installed-calendar and territory inventories for fresh Intl bundles.

Runtime admission remains separate. This module never consumes or manufactures
runtime acceptance states and never executes the engine or host timezone tools.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import metadata as m

TZ_COMMIT = '039ef27cc5f062a2055cb67435d6d71adbefd27d'
CLDR_COMMIT = '11299982335beb974c1c63c45265184e759c0f41'
WIDTHS = {20: 16, 25: 8}
ALIASES = {'ethiopic-amete-alem': 'ethioaa', 'islamicc': 'islamic-civil'}
HELPERS = {
    'primary.py': '2547d9b8cd5e1596ae0eb807fc8711dbb6957026ee25f8a79ab9a5e3c3681e2e',
    'bundle.py': '5548ec0e4e40d3d540d5708cca87a6fc37e13b99c93c2ef1408f4746e3f600f4',
}
PIN_SHA256 = '0414752cc42df38bfe1e71caa1f83d6af9324d8b8f84e4124a0c91cd0b5a3b59'
PROVENANCE_SHA256 = 'a05fcdd2c41d448a8672273957642bdf3b511e695269e179681d64e98b91b093'


def authenticated(path, digest, consumed, name):
    data = Path(path).read_bytes()
    m.require(hashlib.sha256(data).hexdigest() == digest, 'inventory input drift: ' + name)
    consumed[name] = dict(path=name, sha256=digest, bytes=len(data))
    return data


def load_helpers(directory, consumed):
    # Authenticate every helper before importing either one.
    for name, digest in HELPERS.items():
        authenticated(directory / name, digest, consumed, 'timezone-tools/' + name)
    result = {}
    for name in HELPERS:
        spec = importlib.util.spec_from_file_location('inventory_' + name[:-3], directory / name)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        result[name[:-3]] = module
    return result


def calendars_from_release(xml, installed):
    values = tuple(installed)
    m.require(values == tuple(sorted(set(values))) and 'gregory' in values,
              'invalid installed calendar declaration')
    types = {item.get('name'): item for item in
             ET.fromstring(xml).findall("./keyword/key[@name='ca']/type")}
    m.require(set(values) <= types.keys(), 'installed calendar missing from released CLDR')
    m.require('ethioaa' in types and 'islamicc' in types and
              'ethiopic-amete-alem' in types['ethioaa'].get('alias', '').split() and
              types['islamicc'].get('preferred') == 'islamic-civil', 'calendar alias drift')
    return values


def regions_from_primaries(primary, zone_table, xml, records):
    countries, rows = primary.zone_table(zone_table.decode('utf-8'))
    countries = primary.cldr_countries(xml, countries)
    by_name = {record['identifier']: record for record in records}
    m.require(len(by_name) == len(records), 'duplicate timezone identifier')
    m.require(all(name in by_name and by_name[name]['primary'] == name
                  for names in rows.values() for name in names), 'zone.tab primary missing')
    regions = {}
    for record in records:
        name = record['identifier']
        if record['primary'] == name and name in countries:
            regions.setdefault(countries[name], set()).add(name)
    m.require(set(regions) == set(rows) and
              all(set(names) <= regions[region] for region, names in rows.items()),
              'timezone territory coverage mismatch')
    return {region: tuple(sorted(names)) for region, names in sorted(regions.items())}


def load_pinned(args, installed, consumed, context=None):
    helpers = load_helpers(Path(args.timezone_tools_dir), consumed)
    pin = json.loads(authenticated(Path(args.timezone_tools_dir) / 'pin.json',
                                  PIN_SHA256, consumed, 'timezone-tools/pin.json'))
    report = json.loads(authenticated(Path(args.timezone_tools_dir) / 'generated-data-provenance.json',
                                     PROVENANCE_SHA256, consumed, 'timezone-tools/provenance.json'))
    m.require(pin['commit'] == report['pin'] == TZ_COMMIT and
              pin['release'] == report['release'] == '2026e', 'timezone release mismatch')
    blob = authenticated(args.timezone_binary, report['data_sha256'], consumed, 'timezone/data.bin')
    records = helpers['bundle'].unpack(blob)
    m.require(records == report['records'], 'timezone binary inventory mismatch')
    table = authenticated(Path(args.tz_dir) / 'zone.tab', pin['files']['zone.tab'], consumed, 'tz/zone.tab')
    manifest = m.load_pin_manifest(args.cldr_manifest, m.CLDR_MANIFEST_SHA256, context)
    def cldr(name):
        return m.verified_input(args.cldr_dir, manifest, name, consumed, 'cldr', context)
    calendar_xml = cldr('common/bcp47/calendar.xml')
    timezone_xml = cldr('common/bcp47/timezone.xml')
    m.require(report['country_metadata']['cldr_commit'] == CLDR_COMMIT and
              report['country_metadata']['xml_sha256'] == hashlib.sha256(timezone_xml).hexdigest(),
              'timezone country source mismatch')
    return dict(calendars=calendars_from_release(calendar_xml, installed),
                regions=regions_from_primaries(helpers['primary'], table, timezone_xml, records),
                evidence=dict(tz_commit=TZ_COMMIT, cldr_commit=CLDR_COMMIT,
                              runtime_accepted=False, aliases=ALIASES))


def strings(data):
    return list(data['calendars']) + list(data['regions']) + [
        name for names in data['regions'].values() for name in names]


def append(data, pool, sections):
    m.require(not {20, 25} & sections.keys() and 18 in sections,
              'inventory section ownership conflict')
    lists = bytearray(sections[18])
    m.require(len(lists) % 8 == 0, 'invalid shared list width')
    territory = []
    for region, names in data['regions'].items():
        first = len(lists) // 8
        lists.extend(b''.join(pool.ref(name) for name in names))
        territory.append(pool.ref(region) + m.u32(first) + m.u32(len(names)))
    sections[18] = bytes(lists)
    sections[20] = b''.join(territory)
    sections[25] = b''.join(pool.ref(name) for name in data['calendars'])
