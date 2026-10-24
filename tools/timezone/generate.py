#!/usr/bin/env python3
"""Build one native timezone pack offline from supplied source data.

Only a newly compiled build-host zic is executed. Target programs, installed
timezone files, and installed zic implementations are never used.
"""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

from acquire import PIN, SOURCE_NAMES, MAX_DOWNLOAD, atomic_write, extract_sources
from bundle import pack, unpack
from primary import available_named_primaries, backzone_links, cldr_countries, zone_table

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parents[1]
DATA = ('africa', 'antarctica', 'asia', 'australasia', 'europe',
        'northamerica', 'southamerica', 'etcetera', 'factory', 'backward')


def read_input(path):
    with path.open('rb') as stream:
        raw = stream.read(MAX_DOWNLOAD + 1)
    if len(raw) > MAX_DOWNLOAD:
        raise ValueError('timezone input exceeds byte limit: ' + path.name)
    return raw


def read_sources(cache, pin, work, archive=None, cldr_dir=None):
    # Acquisition authenticates official inputs. Generation also accepts custom
    # bytes, but never uses mutable, previously extracted host-compiler sources.
    archive = archive or cache / ('tz-' + pin['tz']['release'] + '.tar.gz')
    raw = read_input(archive)
    directory = cldr_dir or ROOT / 'data' / 'timezone' / 'sources' / 'cldr'
    cldr = read_input(directory / 'timezone.xml')
    source = work / 'sources'
    extract_sources(raw, source, pin['tz']['release'], SOURCE_NAMES)
    return source, cldr, hashlib.sha256(raw).hexdigest()


def generate(cache, output, manifest, host_cc, awk, archive=None, cldr_dir=None,
             pin_path=None):
    pin = json.loads((pin_path or PIN).read_text())
    environment = dict(os.environ, LC_ALL='C', TZ='UTC')
    cache.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='generate-', dir=str(cache)) as directory:
        work = Path(directory)
        source, cldr, archive_hash = read_sources(cache, pin, work, archive, cldr_dir)
        table_countries, country_rows = zone_table((source / 'zone.tab').read_text())
        countries = cldr_countries(cldr, table_countries)
        historical = backzone_links((source / 'backzone').read_text())
        (work / 'version.h').write_text(
            '#define PKGVERSION "QuickJS "\n#define TZVERSION ' +
            json.dumps(pin['tz']['release']) + '\n' +
            '#define REPORT_BUGS_TO "https://github.com/eggert/tz"\n')
        (work / 'tzdir.h').write_text(
            '#define TZDEFAULT "localtime"\n#define TZDIR "."\n')
        zic = work / ('zic.exe' if os.name == 'nt' else 'zic')
        command = shlex.split(host_cc) + ['-std=gnu11', '-O2',
            '-I' + str(source.resolve()), '-I' + str(work.resolve()),
            str(TOOLS / 'host-zic.c'), '-o', str(zic)]
        subprocess.run(command, env=environment, check=True, timeout=300)
        # Keep the country-zone identities and pre-1970 histories recommended
        # for ECMA-402. All intermediate per-zone files stay in this cache.
        tailored = subprocess.run([awk, '-v', 'DATAFORM=main', '-v',
            'PACKRATDATA=backzone', '-v', 'PACKRATLIST=zone.tab', '-f',
            'ziguard.awk', *DATA, 'backzone'], cwd=str(source),
            env=environment, check=True, timeout=120, stdout=subprocess.PIPE)
        main_zi = work / 'main.zi'
        main_zi.write_bytes(tailored.stdout)
        zoneinfo = work / 'zoneinfo'
        subprocess.run([str(zic), '-b', 'fat', '-d', str(zoneinfo), str(main_zi)],
                       env=environment, check=True, timeout=120)
        zones, links = set(), {}
        for line in tailored.stdout.decode('utf-8').splitlines():
            fields = line.split('#', 1)[0].split()
            if fields and fields[0] == 'Zone':
                zones.add(fields[1])
            elif fields and fields[0] == 'Link':
                links[fields[2]] = fields[1]
        primaries = available_named_primaries(zones, links, table_countries,
            country_rows, countries, historical)
        compiled = {name: (primary, (zoneinfo / primary).read_bytes())
                    for name, primary in primaries.items()}
        binary = pack(compiled, pin['tz']['release'], pin['cldr']['release'])
        parsed = unpack(binary)
        receipt = {
            'format': 'QJTZ', 'format_version': 2, 'algorithm_version': 1,
            'tz_release': pin['tz']['release'], 'tz_commit': pin['tz']['commit'],
            'tz_archive_sha256': archive_hash,
            'cldr_timezone_sha256': hashlib.sha256(cldr).hexdigest(),
            'cldr_release': pin['cldr']['release'], 'cldr_commit': pin['cldr']['commit'],
            'primary_algorithm': 'ECMA-402 AvailableNamedTimeZoneIdentifiers',
            'spec_commit': '730ca2b88c305862db7be2f83c510dcca33ab48c',
            'packratdata': 'backzone', 'packratlist': 'zone.tab',
            'payload_format': 'fat TZif', 'leaps': False, 'range_limited': False,
            'runtime_tzcode': False, 'final_binary_files': 1,
            'data_sha256': hashlib.sha256(binary).hexdigest(),
            'binary_size': len(binary), 'records': parsed['records'],
        }
        atomic_write(output, binary)
        atomic_write(manifest, (json.dumps(receipt, indent=2) + '\n').encode('utf-8'))
        print('Generated %d identifiers in %d bytes' % (len(compiled), len(binary)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--archive', type=Path, help='supplied IANA release archive')
    parser.add_argument('--cldr-dir', type=Path, help='directory containing supplied timezone.xml')
    parser.add_argument('--pin', type=Path, help='declared source versions and identities')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--host-cc', default=os.environ.get('HOST_CC', 'cc'))
    parser.add_argument('--awk', default='awk')
    args = parser.parse_args()
    generate(args.cache.resolve(), args.output, args.manifest, args.host_cc, args.awk,
             args.archive, args.cldr_dir, args.pin)


if __name__ == '__main__':
    main()
