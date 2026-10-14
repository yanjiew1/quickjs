"""Authenticate committed files and the approved tzdata release gitlink.

Git directories never enter the source file census. Their exact commit and
committed blob/mode census have a separate receipt; files enter the complete
current SHA-256 map under their full repository-relative names.
"""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys

TZ_PATH = 'third_party/tz'
TZ_COMMIT = '039ef27cc5f062a2055cb67435d6d71adbefd27d'
TZ_RELEASE = '2026e'
TZ_INPUTS = frozenset(('africa', 'antarctica', 'asia', 'australasia', 'europe',
    'northamerica', 'southamerica', 'etcetera', 'factory', 'backward', 'backzone',
    'zone.tab', 'ziguard.awk', 'zic.c', 'private.h', 'tzfile.h'))


def require(value, reason):
    if not value:
        raise ValueError(reason)


def git(repo, *args, **kwargs):
    return subprocess.check_output(['git', '-C', str(repo), *args], **kwargs)


def entries(repo, command=git, permit_timezone=False):
    result, links = {}, {}
    for line in command(repo, 'ls-tree', '-rz', 'HEAD').split(b'\0'):
        if not line:
            continue
        metadata, path = line.split(b'\t', 1)
        mode, kind, blob = metadata.decode('ascii').split()
        name = path.decode('utf-8')
        relative = Path(name)
        require(name and not relative.is_absolute() and
                '..' not in relative.parts and name not in result and
                name not in links, 'Unsafe or duplicate Git path: ' + name)
        if kind == 'commit':
            require(permit_timezone and name == TZ_PATH and mode == '160000'
                    and blob == TZ_COMMIT, 'Unapproved Git link: ' + name)
            links[name] = dict(mode=mode, commit=blob)
        else:
            require(kind == 'blob' and mode in ('100644', '100755'),
                    'Unsupported Git entry: ' + name)
            result[name] = (mode, blob)
    return result, links


def executable_mode_capability(repo, command=git):
    """Git modes stay exact when Windows stat bits cannot represent them."""
    core_filemode = None
    if sys.platform.startswith(('win32', 'cygwin', 'msys')):
        try:
            value = command(repo, 'config', '--bool', '--get',
                            'core.filemode').decode('ascii').strip()
        except subprocess.CalledProcessError as error:
            if error.returncode != 1:
                raise
            value = ''
        require(value in ('', 'true', 'false'), 'Invalid core.filemode capability')
        if value:
            core_filemode = value == 'true'
    return dict(platform=sys.platform, core_filemode=core_filemode,
                stat_execute_bits_enforced=core_filemode is not False)


def authenticate_files(repo, files, command=git, verify_execute_bits=True):
    repo = Path(repo).resolve()
    sources, modes, blobs = {}, {}, {}
    query = ('\n'.join(blob for _, blob in files.values()) + '\n').encode()
    stream = io.BytesIO(command(repo, 'cat-file', '--batch', input=query))
    for name, (mode, blob) in files.items():
        path = repo / name
        require(path.is_file() and not path.is_symlink() and
                repo in path.resolve().parents,
                'Missing or unsafe committed file: ' + name)
        header = stream.readline().decode('ascii').strip().split()
        require(len(header) == 3 and header[:2] == [blob, 'blob'],
                'Unexpected committed blob header: ' + name)
        length = int(header[2])
        require(length >= 0, 'Invalid committed blob length')
        data = stream.read(length)
        require(len(data) == length and stream.read(1) == b'\n',
                'Truncated committed blob: ' + name)
        require(path.read_bytes() == data,
                'Checkout bytes differ from committed blob: ' + name)
        if verify_execute_bits:
            require(bool(path.stat().st_mode & 0o111) == (mode == '100755'),
                    'Checkout executable mode differs: ' + name)
        sources[name] = hashlib.sha256(data).hexdigest()
        modes[name] = mode
        blobs[name] = dict(mode=mode, blob=blob)
    require(not stream.read(), 'Unexpected extra committed blob output')
    return sources, modes, blobs


def inspect_checkout(root='.', command=git):
    root = Path(root).resolve()
    head = command(root, 'rev-parse', 'HEAD').decode('ascii').strip()
    top, links = entries(root, command, permit_timezone=True)
    require('tools/timezone/pin.json' not in top or set(links) == {TZ_PATH},
            'Missing approved timezone release gitlink')
    capabilities = {'root': executable_mode_capability(root, command)}
    sources, modes, blobs = authenticate_files(root, top, command,
        capabilities['root']['stat_execute_bits_enforced'])
    receipts = {}
    if links:
        repo = root / TZ_PATH
        require(repo.is_dir() and not repo.is_symlink() and
                root in repo.resolve().parents, 'Missing tzdata checkout')
        require(command(repo, 'rev-parse', 'HEAD').decode().strip() == TZ_COMMIT,
                'Changed tzdata checkout commit')
        files, nested = entries(repo, command)
        require(not nested and files, 'Missing tzdata committed inputs')
        capabilities[TZ_PATH] = executable_mode_capability(repo, command)
        sub_sources, sub_modes, sub_blobs = authenticate_files(repo, files, command,
            capabilities[TZ_PATH]['stat_execute_bits_enforced'])
        pin = json.loads((root / 'tools/timezone/pin.json').read_bytes())
        require(pin.get('commit') == TZ_COMMIT and pin.get('release') ==
                pin.get('tag') == TZ_RELEASE and set(pin.get('files', {})) ==
                TZ_INPUTS, 'Changed tzdata release or input census')
        require(all(sub_sources.get(name) == value for name, value in
                    pin['files'].items()), 'tzdata release input pin differs')
        for name, value in sub_sources.items():
            full = TZ_PATH + '/' + name
            require(full not in sources, 'Gitlink file overlaps top-level blob')
            sources[full] = value
            modes[full] = sub_modes[name]
        receipts[TZ_PATH] = dict(links[TZ_PATH], release=TZ_RELEASE,
                pin_sha256=sources['tools/timezone/pin.json'], files=sub_blobs,
                source_sha256=sub_sources)
    require(command(root, 'rev-parse', 'HEAD').decode('ascii').strip() == head,
            'Checkout commit changed during authentication')
    return dict(head=head, entries=top, sources=sources, modes=modes, blobs=blobs,
                gitlinks=receipts, mode_capabilities=capabilities)
