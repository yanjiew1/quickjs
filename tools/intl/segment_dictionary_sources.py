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

"""Acquire released segmentation dictionaries as data, without linking ICU."""
import argparse
import copy
import json
from pathlib import Path
import re


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
                not matches(r'[0-9a-f]{64}', row.get('sha256'))):
            raise ValueError('invalid pinned dictionary file: ' + name)
    return result


def load_pin(path=None):
    path = (Path(__file__).resolve().parents[2] / 'data/intl/segment-dictionary-pin.json'
            if path is None else Path(path))
    return validate_pin(json.loads(path.read_text()))


def verify(data, row):
    # Pin validation can be imported by maintenance without loading acquisition.
    import acquire

    if len(data) != row['bytes'] or acquire.digest(data) != row['sha256']:
        raise ValueError('modified dictionary source: ' + row['name'])
    return data


class Sources:
    """Read supplied dictionaries and record the bytes actually consumed."""
    def __init__(self, root, pin=None):
        self.root = Path(root)
        self.pin = validate_pin(load_pin() if pin is None else pin)
        self.files = {row['name']: dict(row) for row in self.pin['files']}
        self.consumed = set()
        # This release's license also contains libtabe/IPADIC and other
        # dictionary notices; the generic Unicode license is not a substitute.
        self.read('LICENSE')

    def read(self, name):
        import acquire

        row = self.files.get(name)
        if row is None:
            raise ValueError('unlisted dictionary source: ' + str(name))
        path = acquire.source_path(self.root, name)
        if not path.is_file():
            raise ValueError('missing or nonregular dictionary source: ' + name)
        with path.open('rb') as stream:
            data = stream.read(MAX_FILE + 1)
        if len(data) > MAX_FILE:
            raise ValueError('dictionary source exceeds byte limit: ' + name)
        if name == 'LICENSE' and not data:
            raise ValueError('dictionary license must not be empty')
        actual = acquire.digest(data)
        if name in self.consumed and actual != row['sha256']:
            raise ValueError('dictionary changed during generation: ' + name)
        # The pin remains declared metadata. Consumed records describe actual
        # supplied bytes and need not match its size or checksum.
        row['sha256'], row['bytes'] = actual, len(data)
        self.consumed.add(name)
        return data

    def identity(self):
        return {'release': self.pin['release'], 'commit': self.pin['commit'],
                'files': {name: self.files[name]['sha256'] for name in sorted(self.consumed)}}


def prepare(cache, offline=False, pin=None):
    import acquire

    pin = validate_pin(load_pin() if pin is None else pin)
    root = acquire.source_path(Path(cache), 'icu-dictionaries-' + pin['release'])
    for row in pin['files']:
        path = acquire.source_path(root, row['name'])
        verify(acquire.fetch(path, row, offline, limit=MAX_FILE), row)
    # Acquisition authenticates the pin; generator readers only record their
    # actual supplied inputs and keep each read session consistent.
    sources = Sources(root, pin)
    for name in FAMILIES:
        sources.read(name)
    return sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--pin', type=Path, help='selected dictionary acquisition pin')
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    sources = prepare(args.cache, args.offline, load_pin(args.pin))
    print('Verified ICU dictionary data ' + sources.pin['release'] + ': ' + str(sources.root))


if __name__ == '__main__':
    main()
