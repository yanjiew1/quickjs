#!/usr/bin/env python3
"""Acquire released segmentation dictionaries as data, without linking ICU."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re

import acquire

FAMILIES = {'cjdict.txt': 'cjk', 'khmerdict.txt': 'khmer',
            'laodict.txt': 'lao', 'burmesedict.txt': 'myanmar',
            'thaidict.txt': 'thai', 'LICENSE': 'license'}
MAX_FILE = 16 * 1024 * 1024


def matches(pattern, value):
    return isinstance(value, str) and re.fullmatch(pattern, value) is not None


def validate_pin(pin):
    """Keep the reviewed release, logical names and immutable URLs inseparable."""
    result = copy.deepcopy(pin)
    if (not isinstance(result, dict) or type(result.get('format')) is not int or
            result.get('format') != 1 or
            not matches(r'[0-9]+(?:\.[0-9]+)*', result.get('release')) or
            result.get('tag') != 'release-' + result['release'] or
            not matches(r'[0-9a-f]{40}', result.get('commit'))):
        raise ValueError('invalid dictionary release identity')
    rows = result.get('files')
    if not isinstance(rows, list) or len(rows) != len(FAMILIES):
        raise ValueError('incomplete dictionary source inventory')
    names = set()
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError('invalid dictionary source record')
        name = row.get('name')
        if (not isinstance(name, str) or name not in FAMILIES or
                name in names or row.get('family') != FAMILIES[name]):
            raise ValueError('invalid or repeated dictionary source')
        names.add(name)
        source = 'LICENSE' if name == 'LICENSE' else 'icu4c/source/data/brkitr/dictionaries/' + name
        url = 'https://raw.githubusercontent.com/unicode-org/icu/' + result['commit'] + '/' + source
        size = row.get('bytes')
        if (row.get('source') != source or row.get('url') != url or
                type(size) is not int or not 0 < size <= MAX_FILE or
                not matches(r'[0-9a-f]{64}', row.get('sha256')) or
                not matches(r'[0-9a-f]{40}', row.get('git_blob'))):
            raise ValueError('invalid pinned dictionary file: ' + name)
    return result


def load_pin():
    path = Path(__file__).with_name('segment-dictionary-pin.json')
    return validate_pin(json.loads(path.read_text()))


def verify(data, row):
    # SHA-256 authenticates bytes. The independently reviewed Git blob records
    # which release file those bytes represent, not a second download source.
    if len(data) != row['bytes'] or acquire.digest(data) != row['sha256']:
        raise ValueError('modified dictionary source: ' + row['name'])
    blob = hashlib.sha1(b'blob ' + str(len(data)).encode('ascii') + b'\0' + data).hexdigest()
    if blob != row['git_blob']:
        raise ValueError('modified dictionary source: ' + row['name'])
    return data


class Sources:
    """Authenticate each consumed file and retain its complete license identity."""
    def __init__(self, root, pin=None):
        self.root = Path(root)
        self.pin = validate_pin(load_pin() if pin is None else pin)
        self.files = {row['name']: row for row in self.pin['files']}
        self.consumed = set()
        # This release's license also contains libtabe/IPADIC and other
        # dictionary notices; the generic Unicode license is not a substitute.
        self.read('LICENSE')

    def read(self, name):
        row = self.files.get(name)
        if row is None:
            raise ValueError('unlisted dictionary source: ' + str(name))
        path = acquire.source_path(self.root, name)
        if not path.is_file() or path.stat().st_size != row['bytes']:
            raise ValueError('missing or resized dictionary source: ' + name)
        data = verify(path.read_bytes(), row)
        self.consumed.add(name)
        return data

    def identity(self):
        return {'release': self.pin['release'], 'commit': self.pin['commit'],
                'files': {name: self.files[name]['sha256'] for name in sorted(self.consumed)}}


def prepare(cache, offline=False, pin=None):
    pin = validate_pin(load_pin() if pin is None else pin)
    root = acquire.source_path(Path(cache), 'icu-dictionaries-' + pin['release'])
    for row in pin['files']:
        path = acquire.source_path(root, row['name'])
        verify(acquire.fetch(path, row, offline), row)
    # Consumers verify again at read time; a previous successful preparation
    # never authorizes modified files or a different release.
    sources = Sources(root, pin)
    for name in FAMILIES:
        sources.read(name)
    return sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    sources = prepare(args.cache, args.offline)
    print('Verified ICU dictionary data ' + sources.pin['release'] + ': ' + str(sources.root))


if __name__ == '__main__':
    main()
