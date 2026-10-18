#!/usr/bin/env python3
"""Acquire the pinned released timezone inputs into an explicit cache.

Normal builds never invoke this tool. Existing verified inputs work offline;
no Git state or installed timezone files are consulted.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import urllib.error
import urllib.request

MAX_DOWNLOAD = 8 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    name = None
    try:
        with tempfile.NamedTemporaryFile(dir=str(path.parent), delete=False) as f:
            name = f.name
            f.write(data)
        os.replace(name, str(path))
        name = None
    finally:
        if name is not None:
            os.unlink(name)


def fetch(path, source, offline=False):
    expected = source['sha256']
    if path.is_file():
        cached = path.read_bytes()
        if digest(cached) == expected:
            return cached
    if offline:
        raise ValueError('missing or modified pinned input: ' + str(path))
    if not source['url'].startswith('https://'):
        raise ValueError('source URL must use HTTPS')
    for attempt in range(3):
        try:
            request = urllib.request.Request(source['url'],
                                             headers={'User-Agent': 'QuickJS-data-generator'})
            with urllib.request.urlopen(request, timeout=30) as response:
                data = response.read(MAX_DOWNLOAD + 1)
            break
        except (OSError, urllib.error.URLError):
            if attempt == 2:
                raise
    if len(data) > MAX_DOWNLOAD or digest(data) != expected:
        raise ValueError('download differs from source pin: ' + source['url'])
    atomic_write(path, data)
    return data


def extract_sources(archive, destination, release, files):
    """Extract only named, hash-checked regular files, never archive paths."""
    prefix = 'tz-' + release + '/'
    staged = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as tar:
        for name, expected in files.items():
            if Path(name).name != name or name in ('', '.', '..'):
                raise ValueError('invalid pinned source name')
            member = tar.getmember(prefix + name)
            if not member.isfile() or member.size > MAX_DOWNLOAD:
                raise ValueError('source is not a bounded regular file: ' + name)
            with tar.extractfile(member) as stream:
                data = stream.read(MAX_DOWNLOAD + 1)
            if digest(data) != expected:
                raise ValueError('archive source differs from pin: ' + name)
            staged[name] = data
    # Validate the entire input set before replacing any extracted source.
    for name, data in staged.items():
        path = destination / name
        if not path.is_file() or path.read_bytes() != data:
            atomic_write(path, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    pin = json.loads(Path(__file__).with_name('pin.json').read_text())
    tz = pin['tz']
    archive = fetch(args.cache / ('tz-' + tz['release'] + '.tar.gz'),
                    tz['archive'], args.offline)
    extract_sources(archive, args.cache / ('tz-' + tz['release']),
                    tz['release'], tz['files'])
    metadata = Path(__file__).resolve().parents[2] / 'data' / 'timezone' / 'sources' / 'cldr'
    for name, source in pin['cldr']['files'].items():
        if digest((metadata / name).read_bytes()) != source['sha256']:
            raise ValueError('vendored CLDR source differs from pin: ' + name)
    print('Verified tzdata ' + tz['release'] + ', CLDR ' + pin['cldr']['release'])


if __name__ == '__main__':
    main()
