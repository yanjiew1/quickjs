"""Candidate source locks; hashing is separate from semantic acceptance."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
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
    # Bound the read itself: a size check alone races a growing local file.
    with Path(path).open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('input exceeds byte limit: ' + str(path))
    return data


def set_pointer(document, assignment):
    """Set an existing JSON Pointer, avoiding ambiguous dotted filenames."""
    pointer, separator, text = assignment.partition('=')
    if not separator or not pointer.startswith('/'):
        raise ValueError('--set requires /existing/json/pointer=value')
    keys = pointer[1:].split('/')
    if any(re.search(r'~(?![01])', key) for key in keys):
        raise ValueError('invalid JSON Pointer escape')
    keys = [key.replace('~1', '/').replace('~0', '~') for key in keys]
    owner = document
    for key in keys[:-1]:
        if isinstance(owner, list):
            if not re.fullmatch('0|[1-9][0-9]*', key):
                raise ValueError('invalid JSON Pointer array index')
            owner = owner[int(key)]
        else:
            owner = owner[key]
    key = keys[-1]
    if isinstance(owner, list):
        if not re.fullmatch('0|[1-9][0-9]*', key):
            raise ValueError('invalid JSON Pointer array index')
        key = int(key)
    # Unknown fields are normally typos; additions need an explicit schema change.
    previous = owner[key]
    try:
        value = json.loads(text)
    except ValueError:
        value = text
    # Release numbers and all-numeric Git IDs remain strings without requiring
    # shell-escaped JSON quotation marks around every metadata assignment.
    if value is None:
        if keys[-1] not in ('reviewed', 'review_date', 'published', 'released'):
            raise ValueError('only optional provenance dates can be cleared')
    elif isinstance(previous, str) and not isinstance(value, str):
        value = text
    elif previous is not None and type(value) is not type(previous):
        raise ValueError('metadata assignment changes field type')
    owner[key] = value
    return pointer


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


def refresh(template, assignments=(), files=None, archive=None,
            files_root=None, source_tree=None):
    """Produce a candidate without editing the accepted lock or raw sources."""
    pin = copy.deepcopy(template)
    pointers = {set_pointer(pin, item) for item in assignments}
    kind = kind_of(pin)
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
                row['git_blob'] = hashlib.sha1(
                    b'blob ' + str(len(data)).encode('ascii') + b'\0' + data).hexdigest()
        if kind == 'dictionaries':
            # Existing tools are standalone scripts with a sibling acquire import.
            # Bind that dependency only during loading, without changing sys.path
            # or leaking an alias into another source family's maintenance work.
            previous = sys.modules.get('acquire')
            sys.modules['acquire'] = load_tool('qjs_pin_dictionary_acquire',
                                               'tools/intl/acquire.py')
            try:
                tool = load_tool('qjs_pin_dictionary_sources',
                                 'tools/intl/segment_dictionary_sources.py')
            finally:
                if previous is None:
                    del sys.modules['acquire']
                else:
                    sys.modules['acquire'] = previous
            tool.validate_pin(pin)

    # A calculated hash is not semantic review. Preserve dates only for an
    # unchanged lock, or when the user explicitly supplies the review field.
    if pin != template:
        for name in ('reviewed', 'review_date'):
            if name in pin and '/' + name not in pointers:
                pin[name] = None
    report = {'kind': kind, 'changed': pin != template,
              'semantic_review_performed': False,
              'note': 'Candidate hashes do not certify source authenticity or generator compatibility.'}
    return pin, manifest, report
