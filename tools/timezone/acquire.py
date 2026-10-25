#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Acquire the pinned released timezone inputs into an explicit cache.

Normal builds never invoke this tool. Existing verified inputs work offline;
no Git state or installed timezone files are consulted.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

import sys

# Keep sibling generator imports ahead of the shared maintenance helpers.
sys.path.append(str(Path(__file__).resolve().parents[1] / 'data'))
from _support import io as data_io
from _support.io import write_atomic as atomic_write

ROOT = Path(__file__).resolve().parents[2]
PIN = ROOT / 'data' / 'timezone' / 'pin.json'
MAX_DOWNLOAD = 8 * 1024 * 1024
SOURCE_NAMES = (
    'africa', 'antarctica', 'asia', 'australasia', 'europe', 'northamerica',
    'southamerica', 'etcetera', 'factory', 'backward', 'backzone', 'zone.tab',
    'ziguard.awk', 'zic.c', 'private.h', 'tzfile.h', 'LICENSE',
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fetch(path, source, offline=False):
    expected = source['sha256']
    if path.is_file():
        try:
            cached = data_io.read_bytes(path, MAX_DOWNLOAD)
        except ValueError:
            cached = None
        if cached is not None and digest(cached) == expected:
            return cached
    if offline:
        raise ValueError('missing or modified pinned input: ' + str(path))
    if not source['url'].startswith('https://'):
        raise ValueError('source URL must use HTTPS')
    data = data_io.download_bytes(source['url'], MAX_DOWNLOAD)
    if len(data) > MAX_DOWNLOAD or digest(data) != expected:
        raise ValueError('download differs from source pin: ' + source['url'])
    atomic_write(path, data)
    return data


def extract_sources(archive, destination, release, names):
    """Materialize only required regular files from authenticated archive bytes."""
    prefix = 'tz-' + release + '/'
    wanted = {}
    for name in names:
        if Path(name).name != name or name in ('', '.', '..') or name in wanted.values():
            raise ValueError('invalid required source name')
        wanted[prefix + name] = name
    staged = {}
    members = total = 0
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as tar:
        for member in tar:
            members += 1
            total += member.size
            # Bound traversal too: unrelated archive entries never become paths.
            if member.size < 0 or members > 4096 or total > 8 * MAX_DOWNLOAD:
                raise ValueError('timezone archive exceeds source bounds')
            name = wanted.get(member.name)
            if name is None:
                continue
            if name in staged or not member.isfile() or not 0 <= member.size <= MAX_DOWNLOAD:
                raise ValueError('duplicate or invalid timezone source: ' + name)
            with tar.extractfile(member) as stream:
                data = stream.read(MAX_DOWNLOAD + 1)
            if len(data) != member.size:
                raise ValueError('incomplete timezone source: ' + name)
            staged[name] = data
    if len(staged) != len(wanted):
        raise ValueError('timezone archive is missing required sources')
    # Validate the complete set before publishing any path, ignoring archive paths.
    for name, data in staged.items():
        atomic_write(destination / name, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    pin = json.loads(PIN.read_text())
    tz = pin['tz']
    fetch(args.cache / ('tz-' + tz['release'] + '.tar.gz'),
          tz['archive'], args.offline)
    metadata = ROOT / 'data' / 'timezone' / 'sources' / 'cldr'
    for name, source in pin['cldr']['files'].items():
        if digest((metadata / name).read_bytes()) != source['sha256']:
            raise ValueError('vendored CLDR source differs from pin: ' + name)
    print('Verified tzdata ' + tz['release'] + ', CLDR ' + pin['cldr']['release'])


if __name__ == '__main__':
    main()
