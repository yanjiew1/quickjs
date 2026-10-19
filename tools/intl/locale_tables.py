"""Native wire tables for complete locale identity metadata in both profiles."""
import re
import struct

import bundle
import locale_metadata as metadata
from parents import locale_id

SECTIONS = (bundle.ALIASES, bundle.LIKELY, bundle.KEYS, bundle.TYPES)


def strings(data):
    result = {''}
    for kind, key, replacements in data['aliases']:
        result.update((key, ' '.join(replacements)))
    for key, value in data['likely']:
        result.update((key, value))
    for extension, key, mode, aliases, preferred, deprecated in data['keys']:
        result.update((extension, key, ' '.join(aliases), preferred))
    for extension, key, name, aliases, preferred, deprecated, variable, iana, region in data['types']:
        result.update((extension, key, name, ' '.join(aliases), preferred, iana, region))
    return result


def encode(data, ids):
    def table(rows, width):
        return len(rows), b''.join(struct.pack('<' + 'I' * width, *row) for row in rows)

    aliases = sorted((metadata.ALIASES.index(kind), ids[key], ids[' '.join(replacements)], len(replacements))
                     for kind, key, replacements in data['aliases'])
    likely = [(ids[key], ids[value]) for key, value in data['likely']]
    keys = [(ids[extension], ids[key], metadata.VALUE_TYPES.index(mode), ids[' '.join(aliases)],
             ids[preferred], int(deprecated)) for extension, key, mode, aliases, preferred, deprecated in data['keys']]
    types = [(ids[extension], ids[key], ids[name], ids[' '.join(aliases)], ids[preferred],
              int(deprecated) | int(variable) << 1, ids[iana], ids[region])
             for extension, key, name, aliases, preferred, deprecated, variable, iana, region in data['types']]
    return {bundle.ALIASES: table(aliases, 4), bundle.LIKELY: table(likely, 2),
            bundle.KEYS: table(keys, 6), bundle.TYPES: table(types, 8)}


def decode(sections, pool):
    if not any(identity in sections for identity in SECTIONS):
        return None
    if not all(identity in sections for identity in SECTIONS):
        raise ValueError('incomplete Intl locale metadata sections')

    def rows(identity, width):
        count, data = sections[identity]
        if count * width * 4 != len(data):
            raise ValueError('invalid Intl locale metadata extent')
        return tuple(struct.iter_unpack('<' + 'I' * width, data))

    def text(index):
        if index >= len(pool):
            raise ValueError('invalid Intl locale metadata string ID')
        return pool[index]

    def ordered(identities):
        if any(a >= b for a, b in zip(identities, identities[1:])):
            raise ValueError('noncanonical Intl metadata row order')

    alias_rows, identities = [], []
    for kind, key, replacement, count in rows(bundle.ALIASES, 4):
        if kind >= len(metadata.ALIASES):
            raise ValueError('invalid Intl alias kind')
        source, raw = text(key), text(replacement)
        targets = tuple(metadata.identifier(value) for value in metadata.tokens(raw))
        if (metadata.identifier(source) != source or not targets or len(targets) != count or
                ' '.join(targets) != raw or len(set(targets)) != len(targets) or
                kind not in (2, 4) and count != 1):
            raise ValueError('invalid Intl alias record')
        identities.append((kind, key))
        alias_rows.append((metadata.ALIASES[kind], source, targets))
    ordered(identities)
    likely, identities = [], []
    for key, value in rows(bundle.LIKELY, 2):
        source, target = text(key), text(value)
        if (locale_id(source) != source or locale_id(target) != target or not
                re.fullmatch(r'[a-z]{2,8}-[A-Z][a-z]{3}-(?:[A-Z]{2}|[0-9]{3})', target)):
            raise ValueError('invalid Intl likely-subtag row')
        identities.append(key)
        likely.append((source, target))
    ordered(identities)
    if not any(key == 'und' for key, value in likely):
        raise ValueError('Intl likely subtags lack und')
    keys, identities = [], []
    for extension, key, mode, aliases_text, preferred, deprecated in rows(bundle.KEYS, 6):
        ext, keyword = text(extension), text(key)
        grammar = r'[0-9a-z][a-z]' if ext == 'u' else r'[a-z][0-9]'
        if ext not in ('u', 't') or not re.fullmatch(grammar, keyword) or mode >= len(metadata.VALUE_TYPES) or deprecated > 1:
            raise ValueError('invalid Intl BCP47 key row')
        aliases = metadata.tokens(text(aliases_text))
        identities.append((extension, key))
        keys.append((ext, keyword, metadata.VALUE_TYPES[mode], aliases, text(preferred), bool(deprecated)))
    ordered(identities)
    key_ids = {(ext, key) for ext, key, mode, aliases, preferred, deprecated in keys}
    types, identities = [], []
    for extension, key, name, aliases_text, preferred, flags, iana, region in rows(bundle.TYPES, 8):
        ext, keyword, spelling = text(extension), text(key), text(name)
        if (ext, keyword) not in key_ids or flags > 3:
            raise ValueError('invalid Intl BCP47 type owner/flags')
        variable = spelling in metadata.VARIABLE_TYPES
        if bool(flags & 2) != variable or not variable and not re.fullmatch(r'[a-z0-9]{3,8}(?:-[a-z0-9]{3,8})*', spelling):
            raise ValueError('invalid Intl literal/indirect type')
        iana, region = text(iana), text(region)
        if iana and not re.fullmatch(r'[A-Za-z]+(?:/[A-Za-z_-]+)+', iana) or region and not re.fullmatch('[A-Z]{2}', region):
            raise ValueError('invalid Intl timezone type metadata')
        aliases = metadata.tokens(text(aliases_text))
        identities.append((extension, key, name))
        types.append((ext, keyword, spelling, aliases, text(preferred), bool(flags & 1), variable, iana, region))
    ordered(identities)
    metadata.check_preferred({(ext, key): preferred for ext, key, mode, aliases, preferred, deprecated in keys})
    metadata.check_preferred({(ext, key, name): preferred for ext, key, name, aliases, preferred,
                              deprecated, variable, iana, region in types})
    return {'aliases': tuple(sorted(alias_rows)), 'likely': tuple(likely),
            'keys': tuple(keys), 'types': tuple(types)}
