#!/usr/bin/env python3
"""Compare an authenticated full-source cache with its actual release checkout.

Archive identity and Git identity are independent evidence. Check every selected
logical file, including working-tree changes, without executing repository tools.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import acquire


def verify(repository, sources):
    if sources.pin.get('distribution') != 'git':
        raise ValueError('Git comparison requires the full-Git source pin')
    command = ['git', '-C', str(repository)]
    commit = subprocess.check_output(command + ['rev-parse', 'HEAD'], timeout=30).decode().strip()
    if commit != sources.pin['commit']:
        raise ValueError('Git checkout differs from pinned release commit')
    tree = subprocess.check_output(command + ['ls-tree', '-r', '-z', '--full-tree', 'HEAD',
                                               '--', 'LICENSE', 'common'], timeout=30)
    seen = set()
    for row in tree.rstrip(b'\0').split(b'\0'):
        if not row:
            continue
        metadata, name = row.split(b'\t', 1)
        mode, kind, identity = metadata.split()
        name = name.decode('utf-8')
        if kind != b'blob' or mode not in (b'100644', b'100755'):
            raise ValueError('Git input is not a regular source file: ' + name)
        if name in seen or len(seen) >= acquire.MAX_MEMBERS:
            raise ValueError('invalid Git source inventory')
        seen.add(name)
        expected = sources.manifest['files'].get(name)
        if expected is None:
            raise ValueError('Git source is absent from pinned inventory: ' + name)
        path = acquire.source_path(repository, name)
        if not path.is_file() or path.stat().st_size > acquire.MAX_FILE:
            raise ValueError('missing or oversized Git source: ' + name)
        data = path.read_bytes()
        # Verify both the committed object and authenticated logical bytes, so
        # an edited checkout cannot masquerade as the pinned HEAD snapshot.
        blob = b'blob ' + str(len(data)).encode('ascii') + b'\0' + data
        if hashlib.sha1(blob).hexdigest() != identity.decode('ascii') or acquire.digest(data) != expected:
            raise ValueError('modified Git source: ' + name)
    if seen != set(sources.manifest['files']):
        raise ValueError('Git source inventory differs from pinned archive')
    return {'commit': commit, 'logical_files': len(seen),
            'manifest_sha256': sources.pin['manifest_sha256']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, required=True)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--source-pin', type=Path, default=Path(__file__).with_name('pin-git.json'))
    args = parser.parse_args()
    pin = acquire.load_pin(args.source_pin)
    result = verify(args.repository, acquire.Sources(args.sources, pin))
    print(json.dumps(result, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
