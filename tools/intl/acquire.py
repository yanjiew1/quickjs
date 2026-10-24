#!/usr/bin/env python3
"""Acquire pinned CLDR source data; normal builds never download inputs.

Production uses the released common data ZIP; a fixed Git archive is available
for comparison. Acquisition authenticates archive inputs. Collectors snapshot
supplied logical files, including DTDs, test data and license, independently of
an acquisition manifest or previous cache history.
"""
import argparse
from collections import namedtuple
import copy
import hashlib
import io
import json
import os
import stat
import struct
from pathlib import Path, PurePosixPath
import tarfile
import sys
import zipfile

# Keep sibling generator imports ahead of the shared maintenance helpers.
sys.path.append(str(Path(__file__).resolve().parents[1] / 'data'))
from _support import io as data_io
from _support.io import write_atomic as atomic_write

MAX_DOWNLOAD = 128 * 1024 * 1024
MAX_FILE = 64 * 1024 * 1024
MAX_EXPANSION = 768 * 1024 * 1024
MAX_MEMBERS = 20000


def digest(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode('utf-8')


def fetch(path, source, offline=False, limit=None):
    """Authenticate bounded cache/download bytes before atomic replacement."""
    limit = MAX_DOWNLOAD if limit is None else limit
    if path.is_file() and path.stat().st_size <= limit:
        data = data_io.read_bytes(path, limit)
        if digest(data) == source['sha256']:
            return data
    if offline:
        raise ValueError('missing or modified pinned archive: ' + str(path))
    data = data_io.download_bytes(source['url'], limit,
                                  user_agent='QuickJS-Intl-data-generator')
    if digest(data) != source['sha256']:
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


def _zip_preflight(data):
    """Bound the actual central directory before ZipFile creates its objects.

    ZipFile trusts the central-directory byte length, not just the EOCD member
    count. Check both so a forged small count cannot bypass the member budget.
    Release data needs neither ZIP64, split disks nor executable prefixes.
    """
    end = data.rfind(b'PK\x05\x06', max(0, len(data) - 65557))
    if end < 0 or len(data) - end < 22:
        raise ValueError('missing ZIP end record')
    disk, central_disk, local_count, count, size, offset, comment = struct.unpack_from('<4H2IH', data, end + 4)
    if (disk or central_disk or local_count != count or count == 0xffff or
            size == 0xffffffff or offset == 0xffffffff):
        raise ValueError('unsupported split or ZIP64 source archive')
    if count > MAX_MEMBERS:
        raise ValueError('too many archive members')
    if end + 22 + comment != len(data) or offset > end or size != end - offset:
        raise ValueError('invalid ZIP central directory extent')
    at, actual = offset, 0
    while at < end:
        if end - at < 46 or data[at:at + 4] != b'PK\x01\x02':
            raise ValueError('invalid ZIP central directory record')
        name, extra, note = struct.unpack_from('<3H', data, at + 28)
        length = 46 + name + extra + note
        if length > end - at:
            raise ValueError('truncated ZIP central directory record')
        actual += 1
        if actual > MAX_MEMBERS:
            raise ValueError('too many archive members')
        at += length
    if actual != count:
        raise ValueError('ZIP member count mismatch')


_Member = namedtuple('Member', 'name size directory regular token')


class _Archive:
    """One bounded reader interface; members never become extraction paths."""
    def __init__(self, data, pin):
        self.data = data
        self.format = pin['archive'].get('format', 'tar.gz')
        self.prefix = 'cldr-' + pin['commit'] if self.format == 'tar.gz' else None

    def __enter__(self):
        if self.format == 'zip':
            _zip_preflight(self.data)
            try:
                self.handle = zipfile.ZipFile(io.BytesIO(self.data))
            except zipfile.BadZipFile as error:
                raise ValueError('invalid ZIP source archive: ' + str(error)) from error
        elif self.format == 'tar.gz':
            self.handle = tarfile.open(fileobj=io.BytesIO(self.data), mode='r:gz')
        else:
            raise ValueError('unsupported source archive format')
        return self

    def __exit__(self, kind, value, traceback):
        self.handle.close()

    def members(self):
        if self.format == 'zip':
            for member in self.handle.infolist():
                kind = stat.S_IFMT(member.external_attr >> 16)
                directory = member.is_dir()
                if (member.flag_bits & 1 or member.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED) or
                        (directory and (kind not in (0, stat.S_IFDIR) or member.file_size))):
                    raise ValueError('unsupported ZIP source member')
                # ZipInfo.filename truncates at NUL; orig_filename preserves
                # the name that must pass the common safe-path checks.
                yield _Member(member.orig_filename, member.file_size, directory,
                              not directory and kind in (0, stat.S_IFREG), member)
        else:
            for member in self.handle:
                yield _Member(member.name, member.size, member.isdir(), member.isfile(), member)

    def read(self, member):
        try:
            stream = (self.handle.open(member.token) if self.format == 'zip'
                      else self.handle.extractfile(member.token))
            with stream:
                data = stream.read(MAX_FILE + 1)
        except zipfile.BadZipFile as error:
            raise ValueError('invalid ZIP source payload: ' + str(error)) from error
        if len(data) != member.size:
            raise ValueError('truncated archive source: ' + member.name)
        return data


def archive_inventory(archive, pin):
    """Inspect hash-checked archive bytes without accepting a manifest pin.

    Maintenance tools use this to propose a new inventory. Normal acquisition
    additionally checks the accepted inventory identity in archive_files().
    """
    if len(archive) > MAX_DOWNLOAD or digest(archive) != pin['archive']['sha256']:
        raise ValueError('archive differs from source pin')
    selected = {}
    names = set()
    total = 0
    with _Archive(archive, pin) as reader:
        for count, member in enumerate(reader.members(), 1):
            if count > MAX_MEMBERS:
                raise ValueError('too many archive members')
            parts = safe_name(member.name)
            if reader.prefix is not None:
                if parts[0] != reader.prefix:
                    raise ValueError('unexpected archive root')
                parts = parts[1:]
            if member.directory:
                continue
            if not member.regular or member.size < 0 or member.size > MAX_FILE:
                raise ValueError('source is not a bounded regular file: ' + member.name)
            total += member.size
            if total > MAX_EXPANSION:
                raise ValueError('archive expansion exceeds resource bound')
            logical = '/'.join(parts)
            # The complete Git comparison archive also contains source programs
            # and tooling. Consume only common data and its release license.
            if logical != 'LICENSE' and not logical.startswith('common/'):
                continue
            folded = logical.casefold()
            if folded in names:
                raise ValueError('duplicate source path: ' + logical)
            names.add(folded)
            selected[logical] = member
        if 'LICENSE' not in selected or 'common/dtd/ldml.dtd' not in selected:
            raise ValueError('archive lacks CLDR source data or license')
        hashes = {name: digest(reader.read(member)) for name, member in sorted(selected.items())}
    manifest = {'archive_sha256': pin['archive']['sha256'],
                'files': hashes, 'release': pin['release']}
    encoded = json_bytes(manifest)
    return selected, encoded


def archive_files(archive, pin):
    """Return the complete pinned data inventory, without extracting paths."""
    selected, encoded = archive_inventory(archive, pin)
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
    # modifying the cache. Read only regular selected files, never archive links.
    for name in selected:
        source_path(root, name)
    with _Archive(archive, pin) as reader:
        for name, member in sorted(selected.items()):
            data = reader.read(member)
            target = source_path(root, name)
            if (not target.is_file() or target.stat().st_size != len(data) or
                    target.read_bytes() != data):
                atomic_write(target, data)
    atomic_write(source_path(root, 'source-manifest.json'), manifest)


def load_pin(path=None):
    path = (Path(__file__).resolve().parents[2] / 'data/intl/pin.json'
            if path is None else Path(path))
    return json.loads(path.read_text())['cldr']


class Sources:
    """Snapshot supplied CLDR files; acquisition alone authenticates the pin.

    Compiler stages share the calculated inventory identity. Reject changes
    during a read session, rather than mixing bytes from different snapshots.
    No acquisition manifest is required or written for a custom source tree.
    """
    def __init__(self, root, pin=None):
        self.root = Path(root)
        self.pin = copy.deepcopy(load_pin() if pin is None else pin)
        common = source_path(self.root, 'common')
        license_bytes = self._read('LICENSE')
        files = {'LICENSE': digest(license_bytes)}
        names = {'license'}
        total = len(license_bytes)
        pending = [common]
        count = 1  # LICENSE is already in the snapshot.
        while pending:
            # scandir propagates unreadable-directory errors. A glob that
            # silently skips a subtree would produce an incomplete identity.
            with os.scandir(pending.pop()) as entries:
                for entry in entries:
                    count += 1
                    if count > MAX_MEMBERS:
                        raise ValueError('too many source paths')
                    name = Path(entry.path).relative_to(self.root).as_posix()
                    path = source_path(self.root, name)
                    folded = name.casefold()
                    if folded in names:
                        raise ValueError('duplicate source path: ' + name)
                    names.add(folded)
                    if entry.is_dir(follow_symlinks=False):
                        pending.append(path)
                        continue
                    raw = self._read(name)
                    total += len(raw)
                    if total > MAX_EXPANSION:
                        raise ValueError('source tree exceeds resource bound')
                    files[name] = digest(raw)
        if 'common/dtd/ldml.dtd' not in files:
            raise ValueError('source tree lacks CLDR data or DTD')
        # Archive/release fields are declared provenance. Only the file digests
        # below describe this supplied tree; they need not equal accepted pins.
        self.manifest = {'archive_sha256': self.pin['archive']['sha256'],
                         'files': files, 'release': self.pin['release']}
        self.pin['manifest_sha256'] = digest(json_bytes(self.manifest))

    def _read(self, name):
        path = source_path(self.root, name)
        if not path.is_file():
            raise ValueError('missing or nonregular source: ' + name)
        with path.open('rb') as stream:
            data = stream.read(MAX_FILE + 1)
        if len(data) > MAX_FILE:
            raise ValueError('oversized source: ' + name)
        return data

    def paths(self, prefix):
        return tuple(name for name in sorted(self.manifest['files'])
                     if name.startswith(prefix))

    def read(self, name):
        expected = self.manifest['files'].get(name)
        if expected is None:
            raise ValueError('source absent from input snapshot: ' + name)
        data = self._read(name)
        if digest(data) != expected:
            raise ValueError('source changed during generation: ' + name)
        return data


def cache_root(cache, pin):
    """Allow release and full-Git owners to coexist under one cache directory."""
    suffix = '-git' if pin.get('distribution') == 'git' else ''
    return source_path(cache, 'cldr-' + pin['release'] + suffix)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--source-pin', type=Path,
                        help='explicit alternate source pin for release/Git comparison')
    args = parser.parse_args()
    pin = load_pin(args.source_pin)
    suffix = pin['archive'].get('format', 'tar.gz')
    if suffix not in ('zip', 'tar.gz'):
        parser.error('unsupported source archive format')
    archive = fetch(args.cache / ('cldr-' + pin['release'] + '.' + suffix),
                    pin['archive'], args.offline)
    root = cache_root(args.cache, pin)
    materialize(archive, root, pin)
    print('Verified CLDR ' + pin['release'] + ': ' + str(root))


if __name__ == '__main__':
    main()
