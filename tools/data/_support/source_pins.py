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

"""Candidate source locks; hashing is separate from semantic acceptance."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
from .io import read_bytes

ROOT = Path(__file__).resolve().parents[3]
MAX_FILE = 64 * 1024 * 1024
MAX_ARCHIVE = 128 * 1024 * 1024


def load_tool(name, relative):
    spec = importlib.util.spec_from_file_location(name, str(ROOT / relative))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_file(path, limit=MAX_FILE):
    return read_bytes(path, limit)


def kind_of(pin):
    if not isinstance(pin, dict):
        raise ValueError('source pin must be a JSON object')
    version = pin.get('schema') if 'schema' in pin else pin.get('format')
    if type(version) is not int or version != 1:
        raise ValueError('unsupported source-pin schema version')
    if 'tz' in pin and 'cldr' in pin:
        return 'timezone'
    if 'cldr' in pin and 'archive' in pin['cldr']:
        return 'cldr'
    if isinstance(pin.get('files'), list):
        return 'dictionaries'
    if isinstance(pin.get('sources'), dict):
        return 'calendar'
    raise ValueError('unrecognized source-pin schema')


def cldr_inventory(pin, archive):
    acquire = load_tool('qjs_pin_cldr_acquire', 'tools/intl/acquire.py')
    source = pin['cldr']
    if not re.fullmatch('[0-9a-f]{40}', source['commit']):
        raise ValueError('CLDR requires an immutable 40-digit commit')
    if source.get('distribution') not in ('common', 'git'):
        raise ValueError('CLDR distribution must be common or git')
    source['archive']['sha256'] = digest(archive)
    _, manifest = acquire.archive_inventory(archive, source)
    source['manifest_sha256'] = digest(manifest)
    return manifest


def refresh(template, candidate=None, files=None, archive=None,
            files_root=None, source_tree=None, data_dir=None):
    """Produce a candidate without editing the accepted lock or raw sources."""
    pin = copy.deepcopy(template if candidate is None else candidate)
    kind = kind_of(pin)
    if kind != kind_of(template):
        raise ValueError('candidate must use the template source family')
    supplied = dict(files or {})
    manifest = None

    if kind == 'cldr':
        if archive is None:
            raise ValueError('CLDR inventory generation requires --archive')
        manifest = cldr_inventory(pin, archive)
        if supplied or files_root or source_tree:
            raise ValueError('CLDR consumes an archive, not individual file replacements')
    else:
        if kind == 'timezone':
            if source_tree:
                raise ValueError('timezone inputs use --archive and --files-root')
            if archive is not None:
                pin['tz']['archive']['sha256'] = digest(archive)
                timezone = load_tool('qjs_pin_timezone_archive', 'tools/timezone/acquire.py')
                if len(archive) > timezone.MAX_DOWNLOAD:
                    raise ValueError('timezone archive exceeds acquisition limit')
                timezone.archive_sources(archive, pin['tz']['release'], timezone.SOURCE_NAMES)
            elif pin['tz'] != template['tz']:
                raise ValueError('changing a timezone release requires its archive')
            records = pin['cldr']['files']
        elif kind == 'calendar':
            if archive is not None or source_tree:
                raise ValueError('calendar sources use --file or --files-root')
            records = pin['sources']
        else:
            if archive is not None:
                raise ValueError('dictionaries use --files-root or --source-tree')
            records = {row['name']: row for row in pin['files']}
            if any(pin[key] != template[key] for key in ('commit', 'release', 'tag')):
                if not (files_root or source_tree or set(records) <= set(supplied)):
                    raise ValueError('changing dictionary identity requires its complete source set')
                for row in records.values():
                    row['url'] = ('https://raw.githubusercontent.com/unicode-org/icu/' +
                                  pin['commit'] + '/' + row['source'])

        if set(supplied) - set(records):
            raise ValueError('unknown input names: ' + ', '.join(sorted(set(supplied) - set(records))))
        old_records = (template['cldr']['files'] if kind == 'timezone' else
                       template['sources'] if kind == 'calendar' else
                       {row['name']: row for row in template['files']})
        if kind == 'timezone':
            identity = {key: value for key, value in pin['cldr'].items() if key != 'files'}
            old_identity = {key: value for key, value in template['cldr'].items() if key != 'files'}
            if identity != old_identity and not files_root and not set(records) <= set(supplied):
                raise ValueError('changing CLDR identity requires every vendored timezone input')
        for name, row in records.items():
            if (Path(name).name != name or name in ('', '.', '..') or
                    '\\' in name or ':' in name):
                raise ValueError('unsafe logical input name')
            path = supplied.get(name)
            if path is None and source_tree:
                parts = row['source'].split('/')
                if any(part in ('', '.', '..') for part in parts) or '\\' in row['source'] or ':' in row['source']:
                    raise ValueError('unsafe source-tree path')
                path = Path(source_tree) / row['source']
            if path is None and files_root:
                path = Path(files_root) / name
            if path is None:
                if row != old_records[name]:
                    raise ValueError('changed source metadata requires input bytes: ' + name)
                continue
            data = path if isinstance(path, bytes) else read_file(path)
            if len(data) > MAX_FILE:
                raise ValueError('input exceeds byte limit: ' + name)
            row['sha256'] = digest(data)
            if kind == 'dictionaries':
                row['bytes'] = len(data)
                row.pop('git_blob', None)
        if kind == 'dictionaries':
            tool = load_tool('qjs_pin_dictionary_sources',
                             'tools/intl/segment_dictionary_sources.py')
            tool.validate_pin(pin)

    if data_dir is not None:
        if kind != 'calendar' or not isinstance(pin.get('normalized'), dict) or not pin['normalized']:
            raise ValueError('--data-dir requires a calendar template with normalized facts')
        # Parsed facts are maintained inputs too. Their hashes must be computed
        # from supplied JSON, rather than copied manually into the lock.
        for name, row in pin['normalized'].items():
            if (Path(name).name != name or name in ('', '.', '..') or
                    '\\' in name or ':' in name or not isinstance(row, dict)):
                raise ValueError('invalid normalized calendar input name/record')
            row['sha256'] = digest(read_file(Path(data_dir) / name))

    # A calculated hash is not semantic review. Preserve dates only for an
    # unchanged lock. Review dates are filled after inspecting the candidate.
    if pin != template:
        for name in ('reviewed', 'review_date'):
            if name in pin:
                pin[name] = None
    report = {'kind': kind, 'changed': pin != template,
              'semantic_review_performed': False,
              'note': 'Candidate hashes do not certify source authenticity or generator compatibility.'}
    return pin, manifest, report
