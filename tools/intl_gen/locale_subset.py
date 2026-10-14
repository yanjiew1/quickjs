"""Explicit CLDR locale selection with verified parent/default closure."""
import metadata as m


def tag(value):
    return 'root' if value == 'root' else m.canonical_tag(value)


def select(metadata, default_locale, available_locales, likely=()):
    requested = tuple(available_locales)
    m.require(requested and len(requested) == len(set(requested)),
              'empty/repeated explicit AvailableLocales')
    m.require(all(value != 'root' and tag(value) == value for value in requested),
              'AvailableLocales must contain canonical non-root tags')
    m.require(tag(default_locale) == default_locale and default_locale in requested,
              'actual DefaultLocale must be explicitly available')
    metadata = dict(metadata)
    metadata['parents'] = dict(metadata['parents'])
    metadata['component_parents'] = dict(metadata['component_parents'])
    parents = metadata['parents']
    # CLDR file names may retain a default script (zh_Hant_TW), while a
    # requested BCP47 id omits it (zh-TW). An exact pinned likely-subtag
    # mapping establishes the data parent; no guessed English fallback.
    likely = dict(likely)
    for value in requested:
        if value in parents:
            continue
        target = likely.get(value)
        if target is None and value.count('-') == 1:
            language, region = value.split('-')
            language_target = likely.get(language, '').split('-')
            if len(language_target) == 3:
                candidate = '-'.join(language_target[:2] + [region])
                if candidate in parents:
                    target = candidate
        m.require(target in parents, 'locale lacks exact released data parent: ' + value)
        parents[value] = target
        for component in {component for component, child in metadata['component_parents']}:
            metadata['component_parents'][(component, value)] = target
    m.require(set(requested) <= set(parents), 'available locale missing from CLDR metadata: ' + repr(sorted(set(requested) - set(parents))))
    closure = set(requested) | {'root'}
    # Required prefix fallback tags and both general/component parents use
    # the exact metadata graphs. No guessed data or default substitution.
    for value in requested:
        while '-' in value:
            value = value.rsplit('-', 1)[0]
            m.require(value in parents, 'required prefix fallback absent from metadata')
            closure.add(value)
    while True:
        before = set(closure)
        for value in tuple(closure):
            m.require(value in parents, 'locale closure leaves verified parent graph')
            parent = parents[value]
            if parent is not None: closure.add(parent)
            for (component, child), parent in metadata['component_parents'].items():
                if child == value and parent is not None: closure.add(parent)
        if before == closure: break
    result = dict(metadata)
    result['parents'] = {value: parents[value] for value in sorted(closure)}
    result['locales'] = {value: data for value, data in metadata['locales'].items() if value in closure}
    result['component_parents'] = {key: value for key, value in metadata['component_parents'].items()
                                  if key[1] in closure}
    result['default_content'] = tuple(value for value in metadata['default_content'] if value in closure)
    result['evidence'] = dict(metadata['evidence'])
    policy = {'default_locale': default_locale, 'available_locales': sorted(requested),
              'implementation_closure': sorted(closure),
              'default_content': list(result['default_content']),
              'policy': 'explicit CLDR subset; exact prefix/general/component parent closure; no silent default fallback',
              'service_activation': False}
    result['evidence']['locale_selection'] = policy
    return result, policy


def main_paths(manifest, metadata):
    """Existing collector algorithms receive only their verified locale graph."""
    result = []
    for path in sorted(manifest):
        if path.startswith('common/main/') and path.endswith('.xml'):
            value = tag(path.rsplit('/', 1)[1][:-4])
            if value in metadata['parents']: result.append(path)
    m.require('common/main/root.xml' in result, 'selected CLDR inputs lack root')
    return tuple(result)
