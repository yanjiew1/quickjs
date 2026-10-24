#!/usr/bin/env python3
"""Acquire hash-pinned calendar publications into an explicit cache.

Normal builds use vendored normalized data and never run this command.
"""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import hashlib
import io
import json
from pathlib import Path
import zipfile

import sys

# Keep sibling generator imports ahead of the shared maintenance helpers.
sys.path.append(str(Path(__file__).resolve().parents[1] / 'data'))
from _support import io as data_io
from _support.io import write_atomic as atomic_write

TOOLS = Path(__file__).resolve().parent
SOURCES = TOOLS.parents[1] / 'data' / 'calendar' / 'sources'
PIN = SOURCES.parent / 'pin.json'
MAX_DOWNLOAD = 8 * 1024 * 1024
MAX_ARCHIVE = 128 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fetch(path, source, offline=False):
    if path.is_file():
        try:
            cached = data_io.read_bytes(path, MAX_DOWNLOAD)
        except ValueError:
            cached = None
        if cached is not None and digest(cached) == source['sha256']:
            return
    if offline:
        raise ValueError('missing or modified calendar source: ' + path.name)
    if not source['url'].startswith('https://'):
        raise ValueError('calendar source URL must use HTTPS')
    # A source can be a member of the official release ZIP. Authenticate the
    # archive before opening it; materialize only the requested member.
    limit = MAX_ARCHIVE if 'path' in source else MAX_DOWNLOAD
    expected = source.get('archive_sha256', source['sha256'])
    raw = data_io.download_bytes(source['url'], limit)
    if len(raw) > limit or digest(raw) != expected:
        raise ValueError('calendar download differs from pin: ' + path.name)
    if 'path' in source:
        with zipfile.ZipFile(io.BytesIO(raw)) as archive:
            entry = archive.getinfo(source['path'])
            if entry.file_size > MAX_DOWNLOAD:
                raise ValueError('calendar archive member exceeds byte limit')
            raw = archive.read(entry)
    if digest(raw) != source['sha256']:
        raise ValueError('calendar archive member differs from pin: ' + path.name)
    atomic_write(path, raw)


def load_pin(path=None):
    pin = json.loads((PIN if path is None else path).read_text())
    if pin['schema'] != 1:
        raise ValueError('unsupported calendar pin schema')
    return pin


def checked_sources():
    pin = load_pin()
    for name, item in pin['sources'].items():
        if Path(name).name != name or name in ('', '.', '..'):
            raise ValueError('invalid source name')
        if item['vendored'] and digest((SOURCES / name).read_bytes()) != item['sha256']:
            raise ValueError('vendored calendar source differs from pin: ' + name)
    for name, item in pin['normalized'].items():
        if Path(name).name != name or name in ('', '.', '..'):
            raise ValueError('invalid normalized calendar name')
        if digest((SOURCES.parent / name).read_bytes()) != item['sha256']:
            raise ValueError('normalized calendar differs from pin: ' + name)
    return pin


def parse_normalized(raw):
    """Parse one supplied byte snapshot without authenticating its identity."""
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('duplicate normalized calendar key: ' + key)
            result[key] = value
        return result

    return json.loads(raw.decode('utf-8'), object_pairs_hook=unique_object)


def read_normalized(name, directory=None):
    """Parse supplied facts; acquisition alone enforces pinned identities."""
    if Path(name).name != name or name in ('', '.', '..'):
        raise ValueError('invalid normalized calendar name')
    root = SOURCES.parent if directory is None else Path(directory)
    return parse_normalized((root / name).read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--source', action='append', default=[],
                        help='acquire only this source name (repeatable)')
    args = parser.parse_args()
    pin = checked_sources()
    if set(args.source) - pin['sources'].keys():
        raise ValueError('unknown calendar source selection')
    for name, item in pin['sources'].items():
        if not item['vendored'] and (not args.source or name in args.source):
            fetch(args.cache / name, item, args.offline)
    print('Verified pinned calendar sources')


if __name__ == '__main__':
    main()
