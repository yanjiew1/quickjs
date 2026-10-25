#!/usr/bin/env python3
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

"""CLDR locale inheritance graphs for typed native Intl data collectors."""
import argparse
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml


def locale_id(value):
    """Normalize CLDR source identifiers, not arbitrary BCP47 language tags."""
    if value == 'root':
        return value
    parts = value.replace('_', '-').split('-')
    if not parts or not re.fullmatch(r'(?:[A-Za-z]{2,3}|[A-Za-z]{5,8})', parts[0]):
        raise DataError('invalid CLDR source locale: ' + value)
    result = [parts[0].lower()]
    at = 1
    if at < len(parts) and re.fullmatch('[A-Za-z]{4}', parts[at]):
        result.append(parts[at].title())
        at += 1
    if at < len(parts) and re.fullmatch(r'(?:[A-Za-z]{2}|[0-9]{3})', parts[at]):
        result.append(parts[at].upper())
        at += 1
    variants = set()
    for part in parts[at:]:
        if not re.fullmatch(r'(?:[A-Za-z0-9]{5,8}|[0-9][A-Za-z0-9]{3})', part):
            raise DataError('invalid CLDR source variant: ' + value)
        if part.lower() in variants:
            raise DataError('repeated CLDR source variant: ' + value)
        variants.add(part.lower())
        result.append(part.lower())
    return '-'.join(result)


def structural_parent(locale):
    if locale == 'root':
        return None
    return locale.rsplit('-', 1)[0] if '-' in locale else 'root'


def insert_unique(records, key, value):
    if key in records and records[key] != value:
        raise DataError('conflicting locale metadata: ' + str(key))
    records[key] = value


class ParentGraph:
    def __init__(self, locales, general, components, likely, default_content,
                 nonlikely_script):
        self.general = general
        self.components = components
        self.likely = likely
        self.default_content = tuple(sorted(default_content))
        self.nonlikely_script = nonlikely_script
        self.source_locales = tuple(sorted(set(locales)))
        if 'root' not in locales or 'und' not in likely:
            raise DataError('locale metadata lacks root or und likely subtags')
        tags = set(locales) | set(default_content) | set(general) | set(general.values())
        for records in components.values():
            tags.update(records)
            tags.update(records.values())
        pending = list(tags)
        while pending:
            parent = structural_parent(pending.pop())
            if parent is not None and parent not in tags:
                tags.add(parent)
                pending.append(parent)
        self.tags = tuple(sorted(tags))
        for records in (general,) + tuple(components.values()):
            if 'root' in records or any(child == parent for child, parent in records.items()):
                raise DataError('invalid root/self parent override')
        for component in ('main',) + tuple(components):
            for tag in self.tags:
                tuple(self.chain(tag, component))

    def parent(self, locale, component='main'):
        if locale == 'root':
            return None
        if component in self.components:
            # Component maps stand alone, including explicitly empty maps.
            # They must not inherit exceptional main-component parents.
            return self.components[component].get(locale, structural_parent(locale))
        if component != 'main':
            raise DataError('undeclared inheritance component: ' + component)
        if locale in self.general:
            return self.general[locale]
        parts = locale.split('-')
        if (self.nonlikely_script and len(parts) == 2 and
                re.fullmatch('[A-Z][a-z]{3}', parts[1])):
            likely = self.likely.get(parts[0], self.likely['und'])
            if parts[1] != likely.split('-')[1]:
                return 'root'
        return structural_parent(locale)

    def chain(self, locale, component='main'):
        visited = set()
        while locale is not None:
            if locale in visited:
                raise DataError('locale inheritance cycle in ' + component)
            visited.add(locale)
            yield locale
            locale = self.parent(locale, component)


def load(sources):
    schema = Schema(sources.read('common/dtd/ldmlSupplemental.dtd'))
    general, components, likely = {}, {}, {}
    nonlikely_script = False
    data = read_xml(sources, 'common/supplemental/supplementalData.xml', 'supplementalData')
    for group in data.findall('parentLocales'):
        fields = schema.attributes(group)
        names = fields.get('component', '')
        names = names.split(' ') if names else []
        if (len(set(names)) != len(names) or 'main' in names or
                any(not re.fullmatch('[A-Za-z][A-Za-z0-9]*', name) for name in names)):
            raise DataError('invalid parent locale component list')
        for name in names:
            components.setdefault(name, {})
        for entry in group:
            if entry.tag != 'parentLocale' or len(entry):
                raise DataError('unknown parentLocales content')
            fields = schema.attributes(entry)
            parent = locale_id(fields['parent'])
            children = tuple(locale_id(value) for value in fields['locales'].split(' '))
            if not children or len(set(children)) != len(children):
                raise DataError('empty or repeated parent locale children')
            rule = fields.get('localeRules', '')
            if rule:
                if rule != 'nonlikelyScript' or names or parent != 'root' or nonlikely_script:
                    raise DataError('unsupported parent locale rule')
                if any(not re.fullmatch('[a-z]{2,8}-[A-Z][a-z]{3}', child)
                       for child in children):
                    raise DataError('nonlikelyScript example is not a language/script pair')
                # The listed locales are a migration sample, not the complete
                # rule. Apply it to every lang_script using likely-subtag data.
                nonlikely_script = True
            else:
                for child in children:
                    for records in (tuple(components[name] for name in names)
                                    if names else (general,)):
                        insert_unique(records, child, parent)
    data = read_xml(sources, 'common/supplemental/likelySubtags.xml', 'supplementalData')
    groups = data.findall('likelySubtags')
    if len(groups) != 1:
        raise DataError('missing/repeated likely subtags container')
    schema.attributes(groups[0])
    for entry in groups[0]:
        if entry.tag != 'likelySubtag' or len(entry):
            raise DataError('unknown likely subtags content')
        fields = schema.attributes(entry)
        key, target = locale_id(fields['from']), locale_id(fields['to'])
        if not re.fullmatch(r'[a-z]{2,8}-[A-Z][a-z]{3}-(?:[A-Z]{2}|[0-9]{3})', target):
            raise DataError('likely-subtag target must include language, script and region')
        insert_unique(likely, key, target)
    data = read_xml(sources, 'common/supplemental/supplementalMetadata.xml', 'supplementalData')
    defaults = []
    for entry in data.findall('./metadata/defaultContent'):
        if len(entry):
            raise DataError('unknown default-content children')
        fields = schema.attributes(entry)
        defaults.extend(locale_id(value) for value in fields['locales'].split(' '))
    if len(set(defaults)) != len(defaults) or 'root' in defaults:
        raise DataError('repeated/root default-content locale')
    locales = tuple(locale_id(Path(name).stem) for name in sources.paths('common/main/')
                    if name.endswith('.xml'))
    if len(set(locales)) != len(locales):
        raise DataError('repeated normalized source locale')
    return ParentGraph(locales, general, components, likely, defaults, nonlikely_script)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    graph = load(acquire.Sources(args.sources))
    print(json.dumps({'source_locales': len(graph.source_locales),
                      'graph_locales': len(graph.tags),
                      'default_content': len(graph.default_content),
                      'components': sorted(graph.components),
                      'nonlikely_script_rule': graph.nonlikely_script},
                     sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
