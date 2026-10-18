#!/usr/bin/env python3
"""Acquire hash-pinned calendar publications into an explicit cache.

Normal builds use vendored normalized data and never run this command.
"""
# Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import urllib.error
import urllib.request

TOOLS = Path(__file__).resolve().parent
SOURCES = TOOLS.parents[1] / 'data' / 'calendar' / 'sources'
MAX_DOWNLOAD = 8 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=str(path.parent), delete=False) as out:
            temporary = out.name
            out.write(data)
        os.replace(temporary, str(path))
        temporary = None
    finally:
        if temporary is not None:
            os.unlink(temporary)


def fetch(path, source, offline=False):
    if path.is_file() and digest(path.read_bytes()) == source['sha256']:
        return
    if offline:
        raise ValueError('missing or modified calendar source: ' + path.name)
    if not source['url'].startswith('https://'):
        raise ValueError('calendar source URL must use HTTPS')
    for attempt in range(3):
        try:
            request = urllib.request.Request(source['url'], headers={
                'User-Agent': 'QuickJS-calendar-generator'})
            with urllib.request.urlopen(request, timeout=30) as response:
                raw = response.read(MAX_DOWNLOAD + 1)
            break
        except (OSError, urllib.error.URLError):
            if attempt == 2:
                raise
    if len(raw) > MAX_DOWNLOAD or digest(raw) != source['sha256']:
        raise ValueError('calendar download differs from pin: ' + path.name)
    atomic_write(path, raw)


def checked_sources():
    pin = json.loads((TOOLS / 'pin.json').read_text())
    if pin['schema'] != 1:
        raise ValueError('unsupported calendar pin schema')
    for name, item in pin['sources'].items():
        if Path(name).name != name or name in ('', '.', '..'):
            raise ValueError('invalid source name')
        if item['vendored'] and digest((SOURCES / name).read_bytes()) != item['sha256']:
            raise ValueError('vendored calendar source differs from pin: ' + name)
    return pin


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    pin = checked_sources()
    for name, item in pin['sources'].items():
        if not item['vendored']:
            fetch(args.cache / name, item, args.offline)
    print('Verified pinned calendar sources')


if __name__ == '__main__':
    main()
