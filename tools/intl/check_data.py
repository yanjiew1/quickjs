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

"""Audit a generated pack's registry and global metadata against supplied sources.

Runtime readers check wire structure; this host check separately proves that
projection did not drop a selected locale or its numbering-preference binding.
"""
import argparse
import json
from pathlib import Path

import acquire
import availability
import bundle
import parents
import tables


def verify(sources, binary, manifest):
    pin = sources.pin
    if (manifest['inputs']['cldr'] != pin or manifest['sha256'] != acquire.digest(binary) or
            manifest['bytes'] != len(binary)):
        raise ValueError('prepared pack differs from source metadata or manifest')
    generation = acquire.digest(acquire.json_bytes(manifest['inputs']))
    decoded = tables.validate(binary)
    envelope = decoded['envelope']
    profile = manifest['inputs']['profile']
    if (manifest['generation'] != generation or envelope['generation'].hex() != generation or
            envelope['profile'] != bundle.PROFILES[profile]):
        raise ValueError('prepared pack generation/profile mismatch')
    if set(manifest['inputs']['sections']) != {str(key) for key in envelope['sections']}:
        raise ValueError('prepared section inventory differs from manifest')
    if manifest['strings'] != len(decoded['strings']):
        raise ValueError('prepared string inventory differs from manifest')
    for identity, (count, data) in envelope['sections'].items():
        expected = manifest['inputs']['sections'][str(identity)]
        if expected != {'count': count, 'sha256': acquire.digest(data)}:
            raise ValueError('prepared section differs from manifest')
    graph, _ = availability.complete(parents.load(sources))
    # Native promises the full graph, independently of profile_locales' policy.
    expected = tuple(sorted(graph.tags)) if profile == 'native' else availability.profile_locales(graph, profile)
    if decoded['locales'] != expected or manifest['locales'] != len(expected):
        raise ValueError('prepared pack omits or changes selected source locales')
    if tuple(row[0] for row in decoded['numbering']['locales']) != expected:
        raise ValueError('prepared pack omits or changes selected numbering locales')
    for name, expected_hash in manifest['inputs']['sources'].items():
        if sources.manifest['files'].get(name) != expected_hash:
            raise ValueError('prepared source receipt differs from source inventory')
        sources.read(name)
    return {'profile': profile, 'locales': len(expected),
            'numbering_locales': len(expected), 'sha256': manifest['sha256']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--source-pin', type=Path)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    sources = acquire.Sources(args.sources, acquire.load_pin(args.source_pin))
    result = verify(sources, args.input.read_bytes(), json.loads(args.manifest.read_text()))
    print(json.dumps(result, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
