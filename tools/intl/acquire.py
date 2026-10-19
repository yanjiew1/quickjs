#!/usr/bin/env python3
"""Acquire pinned CLDR source data; normal builds never download inputs.

The complete common/ tree comes from a particular release commit, including
the DTDs, test data and license. Collectors consume verified individual files;
they do not depend on a Git checkout or machine-specific source manifests.
"""
import argparse
import copy
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import tarfile
import tempfile
import time
import urllib.error
import urllib.request

MAX_DOWNLOAD = 128 * 1024 * 1024
MAX_FILE = 64 * 1024 * 1024
MAX_EXPANSION = 768 * 1024 * 1024
MAX_MEMBERS = 20000
MAX_MANIFEST = 8 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode('utf-8')


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=str(path.parent), delete=False) as f:
            temporary = f.name
            f.write(data)
        os.replace(temporary, str(path))
        temporary = None
    finally:
        if temporary is not None:
            os.unlink(temporary)


def fetch(path, source, offline=False):
    """Verify a bounded cache or download before replacing it atomically."""
    if path.is_file() and path.stat().st_size <= MAX_DOWNLOAD:
        data = path.read_bytes()
        if digest(data) == source['sha256']:
            return data
    if offline:
        raise ValueError('missing or modified pinned archive: ' + str(path))
    if not source['url'].startswith('https://'):
        raise ValueError('data sources must use HTTPS')
    deadline = time.monotonic() + 120
    for attempt in range(3):
        try:
            request = urllib.request.Request(
                source['url'], headers={'User-Agent': 'QuickJS-Intl-data-generator'})
            with urllib.request.urlopen(request, timeout=30) as response:
                if not response.geturl().startswith('https://'):
                    raise ValueError('data source redirected away from HTTPS')
                chunks, length = [], 0
                while True:
                    if time.monotonic() > deadline:
                        raise TimeoutError('data download exceeded time bound')
                    # read() can wait for the whole requested length while a
                    # slow peer keeps resetting the socket timeout. read1()
                    # returns after at most one underlying buffered read so
                    # progress cannot bypass the overall deadline checks.
                    chunk = response.read1(min(128 * 1024, MAX_DOWNLOAD + 1 - length))
                    if not chunk:
                        break
                    chunks.append(chunk)
                    length += len(chunk)
                    if length > MAX_DOWNLOAD:
                        raise ValueError('data download exceeded size bound')
                data = b''.join(chunks)
            break
        except (OSError, urllib.error.URLError):
            if attempt == 2 or time.monotonic() > deadline:
                raise
    if len(data) > MAX_DOWNLOAD or digest(data) != source['sha256']:
        raise ValueError('download differs from source pin: ' + source['url'])
    atomic_write(path, data)
    return data


def safe_name(name):
    """Archive paths are data, never arguments to an extraction operation."""
    parts = PurePosixPath(name).parts
    if (not parts or name.startswith('/') or '\\' in name or ':' in name or
            '\0' in name or any(part in ('.', '..') for part in name.split('/'))):
        raise ValueError('unsafe source path: ' + name)
    return parts


def archive_files(archive, pin):
    """Return the complete pinned data inventory, without extracting paths."""
    if len(archive) > MAX_DOWNLOAD or digest(archive) != pin['archive']['sha256']:
        raise ValueError('archive differs from source pin')
    prefix = 'cldr-' + pin['commit']
    selected = {}
    names = set()
    total = 0
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as tar:
        for count, member in enumerate(tar, 1):
            if count > MAX_MEMBERS:
                raise ValueError('too many archive members')
            parts = safe_name(member.name)
            if parts[0] != prefix:
                raise ValueError('unexpected archive root')
            if member.isdir():
                continue
            if not member.isfile() or member.size < 0 or member.size > MAX_FILE:
                raise ValueError('source is not a bounded regular file: ' + member.name)
            total += member.size
            if total > MAX_EXPANSION:
                raise ValueError('archive expansion exceeds resource bound')
            logical = '/'.join(parts[1:])
            # This archive also contains Java tooling, documentation and build
            # files. No source program from it is executed or installed.
            if logical != 'LICENSE' and not logical.startswith('common/'):
                continue
            folded = logical.casefold()
            if folded in names:
                raise ValueError('duplicate source path: ' + logical)
            names.add(folded)
            selected[logical] = member
        if 'LICENSE' not in selected or 'common/dtd/ldml.dtd' not in selected:
            raise ValueError('archive lacks CLDR source data or license')
        hashes = {}
        for name, member in sorted(selected.items()):
            with tar.extractfile(member) as stream:
                data = stream.read(MAX_FILE + 1)
            if len(data) != member.size:
                raise ValueError('truncated archive source: ' + name)
            hashes[name] = digest(data)
    manifest = {'archive_sha256': pin['archive']['sha256'],
                'files': hashes, 'release': pin['release']}
    encoded = json_bytes(manifest)
    if digest(encoded) != pin['manifest_sha256']:
        raise ValueError('source inventory differs from pin')
    return selected, encoded


def source_path(root, name):
    """Do not traverse cache symlinks, including an existing output leaf."""
    parts = safe_name(name)
    if root.is_symlink():
        raise ValueError('source root is a symlink')
    path = root
    for part in parts:
        path = path / part
        if path.is_symlink():
            raise ValueError('source path is a symlink: ' + str(path))
    return path


def materialize(archive, root, pin):
    selected, manifest = archive_files(archive, pin)
    # All paths and the complete content inventory have been verified before
    # modifying the cache. Read only regular selected files, never tar links.
    for name in selected:
        source_path(root, name)
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as tar:
        for name, member in sorted(selected.items()):
            with tar.extractfile(member) as stream:
                data = stream.read(MAX_FILE + 1)
            target = source_path(root, name)
            if (not target.is_file() or target.stat().st_size != len(data) or
                    target.read_bytes() != data):
                atomic_write(target, data)
    atomic_write(source_path(root, 'source-manifest.json'), manifest)


def load_pin():
    return json.loads(Path(__file__).with_name('pin.json').read_text())['cldr']


class Sources:
    """Read only authenticated logical files for subsequent XML collectors."""
    def __init__(self, root, pin=None):
        self.root = Path(root)
        pin = load_pin() if pin is None else pin
        self.pin = copy.deepcopy(pin)
        path = source_path(self.root, 'source-manifest.json')
        if not path.is_file() or path.stat().st_size > MAX_MANIFEST:
            raise ValueError('missing or oversized source manifest')
        manifest = path.read_bytes()
        if digest(manifest) != pin['manifest_sha256']:
            raise ValueError('source manifest differs from pin')
        self.manifest = json.loads(manifest.decode('utf-8'))
        self.consumed = set()

    def paths(self, prefix):
        return tuple(name for name in sorted(self.manifest['files'])
                     if name.startswith(prefix))

    def read(self, name):
        expected = self.manifest['files'].get(name)
        if expected is None:
            raise ValueError('unlisted source: ' + name)
        path = source_path(self.root, name)
        if not path.is_file() or path.stat().st_size > MAX_FILE:
            raise ValueError('missing or oversized source: ' + name)
        data = path.read_bytes()
        if digest(data) != expected:
            raise ValueError('modified source: ' + name)
        self.consumed.add(name)
        return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    pin = load_pin()
    archive = fetch(args.cache / ('cldr-' + pin['release'] + '.tar.gz'),
                    pin['archive'], args.offline)
    root = args.cache / ('cldr-' + pin['release'])
    materialize(archive, root, pin)
    print('Verified CLDR ' + pin['release'] + ': ' + str(root))


if __name__ == '__main__':
    main()
