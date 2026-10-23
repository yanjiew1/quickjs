#!/usr/bin/env python3
"""Generate reviewable data-source pins; never silently update accepted inputs."""
import argparse
import copy
import json
import os
from pathlib import Path
import re
import sys
import time
import urllib.request

from _support import source_pins
from _support.outputs import encoded, write_atomic


def named_file(text):
    name, separator, path = text.partition('=')
    if not separator or not name or not path:
        raise argparse.ArgumentTypeError('expected logical-name=local-path')
    return name, Path(path)


def download(url, limit=source_pins.MAX_ARCHIVE):
    """Fetch a candidate, not a trusted lock; provenance still needs review."""
    if not url.startswith('https://'):
        raise ValueError('candidate downloads require HTTPS')
    deadline = time.monotonic() + 120
    request = urllib.request.Request(url, headers={'User-Agent': 'QuickJS-data-maintenance'})
    with urllib.request.urlopen(request, timeout=30) as response:
        if not response.geturl().startswith('https://'):
            raise ValueError('candidate redirected away from HTTPS')
        chunks, total = [], 0
        while True:
            if time.monotonic() > deadline:
                raise ValueError('candidate download exceeded time limit')
            chunk = response.read1(min(128 * 1024, limit + 1 - total))
            if not chunk:
                break
            chunks.append(chunk)
            total += len(chunk)
            if total > limit:
                raise ValueError('candidate archive exceeds byte limit')
    return b''.join(chunks)


def resolve_tag(repository, tag):
    """Resolve annotated tags to a fixed commit, with bounded indirection."""
    from urllib.parse import quote
    print('Resolving ' + repository + ' tag ' + tag, file=sys.stderr, flush=True)
    base = 'https://api.github.com/repos/' + repository + '/git/'
    item = json.loads(download(base + 'ref/tags/' + quote(tag, safe=''), 1024 * 1024))['object']
    for _ in range(8):
        if not re.fullmatch('[0-9a-f]{40}', item['sha']):
            raise ValueError('upstream tag has an invalid object identity')
        if item['type'] == 'commit':
            return item['sha']
        if item['type'] != 'tag':
            break
        item = json.loads(download(base + 'tags/' + item['sha'], 1024 * 1024))['object']
    raise ValueError('upstream tag does not resolve to a commit')


def select_release(args, pin):
    """Update known release fields; other provenance belongs in the JSON template."""
    kind = source_pins.kind_of(pin)
    if not any((args.release, args.commit, args.resolve_tag, args.distribution)):
        return
    if kind == 'calendar':
        raise ValueError('calendar publications have independent versions; edit source metadata in a template')
    source = pin['cldr'] if kind == 'cldr' else pin['tz'] if kind == 'timezone' else pin
    previous_release = source['release']
    release = args.release or previous_release
    if not re.fullmatch('[0-9][0-9A-Za-z_.-]*', release):
        raise ValueError('invalid source release identifier')
    if args.distribution and kind != 'cldr':
        raise ValueError('--distribution applies only to CLDR')
    tag = release if kind == 'timezone' else 'release-' + release
    repository = {'cldr': 'unicode-org/cldr', 'timezone': 'eggert/tz',
                  'dictionaries': 'unicode-org/icu'}[kind]
    if args.commit and args.resolve_tag:
        raise ValueError('choose --commit or --resolve-tag')
    commit = resolve_tag(repository, tag) if args.resolve_tag else args.commit
    if commit is None:
        if release != previous_release:
            raise ValueError('a new release requires --commit or --resolve-tag')
        commit = source['commit']
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('source commit must have 40 lowercase hexadecimal digits')
    source.update(release=release, tag=tag, commit=commit)
    if kind == 'cldr':
        distribution = args.distribution or source['distribution']
        source['distribution'] = distribution
        source['archive'].update(
            format='zip' if distribution == 'common' else 'tar.gz',
            url=('https://unicode.org/Public/cldr/{0}/cldr-common-{0}.zip'.format(release)
                 if distribution == 'common' else
                 'https://codeload.github.com/unicode-org/cldr/tar.gz/' + commit))
    elif kind == 'timezone':
        source['archive']['url'] = 'https://codeload.github.com/eggert/tz/tar.gz/refs/tags/' + tag
    # A new release must not inherit the previous publication date.
    if release != previous_release:
        for field in ('published', 'released'):
            if field in source:
                source[field] = None


def propagate_cldr(args, pin):
    """Copy selected CLDR provenance and return the actual consumed member bytes."""
    if bool(args.cldr_pin) != bool(args.cldr_sources):
        raise ValueError('--cldr-pin and --cldr-sources are required together')
    if not args.cldr_pin:
        return {}
    kind = source_pins.kind_of(pin)
    if kind not in ('timezone', 'calendar'):
        raise ValueError('CLDR propagation applies to timezone/calendar pins')
    selected = json.loads(source_pins.read_file(args.cldr_pin).decode('utf-8'))
    if source_pins.kind_of(selected) != 'cldr':
        raise ValueError('--cldr-pin must select a CLDR archive')
    cldr = selected['cldr']
    acquire = source_pins.load_tool('qjs_pin_cldr_propagation', 'tools/intl/acquire.py')
    sources = acquire.Sources(args.cldr_sources, cldr)
    files = {}
    if kind == 'timezone':
        pin['cldr'].update(release=cldr['release'], commit=cldr['commit'],
                           archive=copy.deepcopy(cldr['archive']), released=cldr.get('published'))
        for name, row in pin['cldr']['files'].items():
            files[name] = sources.read(row['path'])
            row['sha256'] = source_pins.digest(files[name])
    else:
        for name, row in pin['sources'].items():
            if not (name.startswith('cldr-') or name == 'LICENSE-CLDR'):
                continue
            files[name] = sources.read(row['path'])
            row.update(url=cldr['archive']['url'], archive_sha256=cldr['archive']['sha256'],
                       revision=cldr['commit'], sha256=source_pins.digest(files[name]))
            if 'release' in row:
                row['release'] = cldr['release']
        if not files:
            raise ValueError('calendar pin contains no CLDR archive members')
    return files


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument('--pin', type=Path, required=True, help='source-lock template, including an edited copy for custom provenance')
    result.add_argument('--output', type=Path, required=True, help='candidate pin; accepted files are not implicit outputs')
    result.add_argument('--release', help='CLDR, IANA or ICU release; derives the standard tag and archive URL')
    result.add_argument('--commit', help='immutable source commit (offline alternative to --resolve-tag)')
    result.add_argument('--resolve-tag', action='store_true', help='explicitly resolve the release tag through the upstream GitHub API')
    result.add_argument('--distribution', choices=['common', 'git'], help='CLDR source distribution')
    result.add_argument('--file', action='append', type=named_file, default=[],
                        help='hash a replacement calendar/dictionary/CLDR-timezone input')
    result.add_argument('--files-root', type=Path, help='directory containing every named raw file')
    result.add_argument('--source-tree', type=Path, help='ICU checkout with dictionary source paths')
    result.add_argument('--data-dir', type=Path, help='calendar factual JSON directory to rehash')
    result.add_argument('--cldr-pin', type=Path, help='derive timezone/calendar CLDR metadata from this candidate pin')
    result.add_argument('--cldr-sources', type=Path, help='supplied source root for the selected CLDR metadata')
    result.add_argument('--archive', type=Path, help='local CLDR or tz Git archive; works offline')
    result.add_argument('--download', action='store_true', help='explicitly fetch the candidate archive URL')
    result.add_argument('--expected-sha256', help='independently obtained archive digest, when available')
    result.add_argument('--manifest-output', type=Path, help='optional CLDR logical inventory output')
    result.add_argument('--report', type=Path, help='optional candidate/review report')
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    template = json.loads(source_pins.read_file(args.pin).decode('utf-8'))
    candidate = copy.deepcopy(template)
    select_release(args, candidate)
    cldr_files = propagate_cldr(args, candidate)
    kind = source_pins.kind_of(candidate)
    if args.archive is not None and args.download:
        raise ValueError('choose either --archive or --download')
    if args.files_root and args.source_tree:
        raise ValueError('choose --files-root or --source-tree')
    archive = None
    downloaded_files = {}
    if args.archive is not None:
        archive = source_pins.read_file(args.archive, source_pins.MAX_ARCHIVE)
    elif args.download:
        print('Downloading candidate ' + kind + ' inputs', file=sys.stderr, flush=True)
        if kind == 'dictionaries':
            if args.file or args.files_root or args.source_tree:
                raise ValueError('dictionary download cannot also replace local inputs')
            for row in candidate['files']:
                print('  ' + row['name'], file=sys.stderr, flush=True)
                url = ('https://raw.githubusercontent.com/unicode-org/icu/' +
                       candidate['commit'] + '/' + row['source'])
                downloaded_files[row['name']] = download(url, 16 * 1024 * 1024)
        elif kind in ('cldr', 'timezone'):
            item = candidate['cldr'] if kind == 'cldr' else candidate['tz']
            archive = download(item['archive']['url'])
        else:
            raise ValueError('calendar publications use their individual --file inputs')
    if args.expected_sha256 is not None:
        if not re.fullmatch('[0-9a-f]{64}', args.expected_sha256):
            raise ValueError('expected SHA-256 must have 64 lowercase hexadecimal digits')
        if archive is None or source_pins.digest(archive) != args.expected_sha256:
            raise ValueError('candidate does not match independently supplied SHA-256')
    names = [name for name, _ in args.file]
    if len(names) != len(set(names)):
        raise ValueError('repeated --file logical name')
    if set(names) & set(cldr_files):
        raise ValueError('--file must not replace a member verified through --cldr-pin')
    inputs_by_name = downloaded_files or dict(args.file)
    inputs_by_name.update(cldr_files)
    pin, manifest, report = source_pins.refresh(
        template, candidate, inputs_by_name, archive,
        args.files_root, args.source_tree, args.data_dir)
    if args.manifest_output is not None and manifest is None:
        raise ValueError('--manifest-output applies only to CLDR')
    outputs = [args.output, args.report, args.manifest_output]
    paths = [os.path.normcase(str(path.resolve())) for path in outputs if path is not None]
    if len(paths) != len(set(paths)):
        raise ValueError('pin, manifest and report must have different output paths')
    inputs = [args.pin, args.archive, args.cldr_pin] + [path for _, path in args.file]
    if args.data_dir:
        inputs += [args.data_dir / name for name in candidate['normalized']]
    if args.cldr_sources:
        inputs += [args.cldr_sources / 'source-manifest.json']
        rows = template['cldr']['files'] if kind == 'timezone' else template['sources']
        inputs += [args.cldr_sources / row['path'] for row in rows.values() if 'path' in row]
    if kind != 'cldr':
        records = (candidate['cldr']['files'] if kind == 'timezone' else
                   candidate['sources'] if kind == 'calendar' else
                   {row['name']: row for row in candidate['files']})
        if args.files_root:
            inputs += [args.files_root / name for name in records]
        if args.source_tree:
            inputs += [args.source_tree / row['source'] for row in records.values()]
    if set(paths) & {os.path.normcase(str(path.resolve())) for path in inputs if path is not None}:
        raise ValueError('write a separate candidate instead of overwriting an input')

    # Validate the entire candidate before publishing any output. A hash locks
    # bytes; it does not waive schema, license, compatibility or semantic review.
    if args.manifest_output is not None:
        write_atomic(args.manifest_output, manifest)
    write_atomic(args.output, encoded(pin))
    if args.report is not None:
        write_atomic(args.report, encoded(report))
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, IndexError, TypeError) as error:
        print('update_pin: ' + str(error), file=sys.stderr)
        sys.exit(1)
