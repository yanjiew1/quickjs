"""ECMA-402 AvailableNamedTimeZoneIdentifiers generation-time metadata.

No host tools or runtime timezone APIs are used by these pure data helpers.
CLDR geographic alias groups identify related names. Their country comes
from the group's authoritative zone.tab rows, with region consistency checked.
"""
import re
import xml.etree.ElementTree as ET


def zone_table(text):
    countries, rows = {}, {}
    for line in text.splitlines():
        fields = line.split('#', 1)[0].split()
        if not fields:
            continue
        if len(fields) < 3 or not re.fullmatch(r'[A-Z]{2}', fields[0]):
            raise ValueError('invalid zone.tab row: ' + line)
        country, identifier = fields[0], fields[2]
        if identifier in countries:
            raise ValueError('duplicate zone.tab identifier: ' + identifier)
        countries[identifier] = country
        rows.setdefault(country, []).append(identifier)
    return countries, rows


def cldr_countries(xml, table_countries):
    """Join CLDR aliases with zone.tab country rows, never primary hints."""
    root = ET.fromstring(xml)
    key = root.find("./keyword/key[@name='tz']")
    if key is None:
        raise ValueError('missing CLDR tz key')
    countries = {}
    for item in key.findall('type'):
        aliases = item.get('alias', '').split()
        regions = {table_countries[name] for name in aliases if name in table_countries}
        # At this pin each geographic group has exactly one zone.tab country.
        # No country is guessed from a short ID, a city name or an offset.
        # UTC/Etc, Factory and legacy pseudo-Zones need no geographic join.
        if not regions:
            continue
        if len(regions) != 1:
            raise ValueError('ambiguous CLDR alias territory: ' + item.get('name', ''))
        region = next(iter(regions))
        if item.get('region', region) != region:
            raise ValueError('CLDR region disagrees with zone.tab: ' + item.get('name', ''))
        for identifier in aliases:
            if identifier in countries and countries[identifier] != region:
                raise ValueError('conflicting CLDR territory: ' + identifier)
            countries[identifier] = region
    countries.update(table_countries)
    return countries


def backzone_links(text):
    links = {}
    for line in text.splitlines():
        if line.startswith('#PACKRATLIST'):
            fields = line.split()
            if len(fields) < 3 or fields[1] != 'zone.tab':
                continue
            line = ' '.join(fields[2:])
        fields = line.split('#', 1)[0].split()
        if not fields or fields[0] != 'Link':
            continue
        if len(fields) != 3 or fields[2] in links:
            raise ValueError('invalid or duplicate backzone Link: ' + line)
        links[fields[2]] = fields[1]
    return links


def resolve_zone(identifier, zones, links):
    visited = set()
    while identifier in links:
        if identifier in visited:
            raise ValueError('link cycle: ' + identifier)
        visited.add(identifier)
        identifier = links[identifier]
    if identifier not in zones:
        raise ValueError('missing link target: ' + identifier)
    return identifier


def available_named_primaries(zones, links, table_countries, country_rows,
                              countries, historical_links):
    """Implement ECMA-402 6.5.1, including country-boundary exceptions."""
    names = zones | links.keys()
    if zones & links.keys():
        raise ValueError('identifier is both Zone and Link')
    if len({name.lower() for name in names}) != len(names):
        raise ValueError('case-insensitive identifier collision')
    for identifier in links:
        resolve_zone(identifier, zones, links)
    for identifier in table_countries:
        if identifier not in names:
            raise ValueError('zone.tab identifier unavailable: ' + identifier)
    for identifier, target in historical_links.items():
        if identifier not in names or target not in names:
            raise ValueError('backzone Link identifier unavailable: ' + identifier)
    primaries = {}
    for identifier in sorted(names):
        primary = identifier
        if identifier in links and identifier not in table_countries:
            zone = resolve_zone(identifier, zones, links)
            if zone.startswith('Etc/'):
                primary = zone
            else:
                if identifier not in countries or zone not in countries:
                    raise ValueError('missing geographic territory: ' + identifier + ' -> ' + zone)
                country = countries[identifier]
                if country == countries[zone]:
                    primary = zone
                else:
                    rows = country_rows.get(country, [])
                    if len(rows) == 1:
                        primary = rows[0]
                    else:
                        if identifier not in historical_links:
                            raise ValueError('missing cross-country backzone Link: ' + identifier)
                        primary = historical_links[identifier]
                    if countries.get(primary) != country:
                        raise ValueError('cross-country primary: ' + identifier + ' -> ' + primary)
        if primary in ('Etc/UTC', 'Etc/GMT', 'GMT'):
            primary = 'UTC'
        primaries[identifier] = primary
    for identifier, primary in primaries.items():
        if primary not in primaries or primaries[primary] != primary:
            raise ValueError('non-primary target: ' + identifier + ' -> ' + primary)
    if primaries.get('UTC') != 'UTC':
        raise ValueError('missing canonical UTC')
    return primaries
