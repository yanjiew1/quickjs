"""Typed numbering repertoires and resolved source preferences in QJIN.

Rule references remain metadata; their presence does not advertise algorithmic
number formatting. Optional absent and explicitly blocked preferences differ.
"""
import re
import struct

import bundle
import numbering
from parents import locale_id

SECTIONS = (bundle.NUMBERING_SYSTEMS, bundle.NUMBERING_PREFERENCES)
KINDS = ('numeric', 'algorithmic')
ABSENT = 0xffffffff
BLOCKED = 0xfffffffe


def strings(data):
    result = {''}
    result.update(text for name, kind, value in data['systems'] for text in (name, value))
    result.update(row[0] for row in data['locales'])
    return result


def encode(data, ids):
    systems = {row[0]: index for index, row in enumerate(data['systems'])}
    definitions = [(ids[name], KINDS.index(kind), ids[value])
                   for name, kind, value in data['systems']]

    def reference(value):
        return ABSENT if value is None else BLOCKED if value == '' else systems[value]

    preferences = [(ids[row[0]],) + tuple(reference(value) for value in row[1:])
                   for row in data['locales']]

    def table(rows, width):
        return len(rows), b''.join(struct.pack('<' + 'I' * width, *row) for row in rows)

    return {bundle.NUMBERING_SYSTEMS: table(definitions, 3),
            bundle.NUMBERING_PREFERENCES: table(preferences, 5)}


def decode(sections, pool):
    if not any(identity in sections for identity in SECTIONS):
        return None
    if not all(identity in sections for identity in SECTIONS):
        raise ValueError('incomplete Intl numbering sections')

    def rows(identity, width):
        count, data = sections[identity]
        if not count or count * width * 4 != len(data):
            raise ValueError('invalid Intl numbering extent')
        result = tuple(struct.iter_unpack('<' + 'I' * width, data))
        if any(a[0] >= b[0] for a, b in zip(result, result[1:])):
            raise ValueError('noncanonical Intl numbering order')
        return result

    def text(index):
        if index >= len(pool):
            raise ValueError('invalid Intl numbering string reference')
        return pool[index]

    systems = []
    for name, kind, value in rows(bundle.NUMBERING_SYSTEMS, 3):
        name, value = numbering.system_id(text(name)), text(value)
        if kind >= len(KINDS):
            raise ValueError('invalid Intl numbering kind')
        if kind == 0:
            if (len(value) != 10 or len(set(value)) != 10 or
                    any(ord(c) == 0 or 0xd800 <= ord(c) <= 0xdfff for c in value)):
                raise ValueError('invalid Intl numbering digits')
        elif not re.fullmatch('[A-Za-z0-9_-]+(?:/[A-Za-z0-9_-]+)*', value):
            raise ValueError('invalid Intl numbering rule reference')
        systems.append((name, KINDS[kind], value))
    if ('latn', 'numeric', '0123456789') not in systems:
        raise ValueError('Intl numbering lacks Latin digits')

    preferences = []
    for key, default, native, traditional, finance in rows(bundle.NUMBERING_PREFERENCES, 5):
        key = text(key)
        if locale_id(key) != key:
            raise ValueError('noncanonical Intl numbering locale')
        values = []
        for index, value in enumerate((default, native, traditional, finance)):
            if value >= len(systems):
                if index < 2 or value not in (ABSENT, BLOCKED):
                    raise ValueError('invalid Intl numbering system reference')
                values.append(None if value == ABSENT else '')
            else:
                values.append(systems[value][0])
        preferences.append((key,) + tuple(values))
    if 'root' not in {row[0] for row in preferences}:
        raise ValueError('Intl numbering lacks root preferences')
    return {'systems': tuple(systems), 'locales': tuple(preferences)}
