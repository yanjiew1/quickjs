#!/usr/bin/env python3
"""Resolve collation programs and expand imports without executing tailorings."""
import argparse
from collections import Counter
import json
from pathlib import Path
from types import MappingProxyType

import acquire
from cldr_xml import DataError
import collation_rules as rules
import locale_metadata
import parents


def identity(program):
    return program.locale, program.collation, program.alternate


def source_locale(locale):
    locale = parents.locale_id(locale)
    return 'root' if locale == 'und' else locale


class Catalog:
    """Immutable parsed programs; inheritance is the *collations* component.

    The graph and keyword records come from their validated collectors. This
    catalog establishes source resolution, not executable service coverage.
    Keep exact lookup/import separate from public-style type fallback: a broken
    import must not silently turn into an unrelated default tailoring.
    """
    def __init__(self, programs, defaults, graph, keyword_types):
        if any(key != identity(program) for key, program in programs.items()):
            raise DataError('collation program identity does not match catalog key')
        root = programs.get(('root', 'standard', ''))
        if root is None or root.nodes:
            raise DataError('root standard tailoring must be present and empty')
        self.programs = MappingProxyType(dict(programs))
        self.defaults = MappingProxyType(dict(defaults))
        self.graph = graph
        types = {row[2]: row for row in keyword_types if row[:2] == ('u', 'co')}
        if len(types) != sum(row[:2] == ('u', 'co') for row in keyword_types):
            raise DataError('duplicate collation keyword type')
        aliases, spellings = {}, {}
        for name, row in types.items():
            target, visited = name, set()
            if row[6]:
                raise DataError('collation keyword type cannot be a schema variable')
            while types[target][4]:
                if target in visited or types[target][4] not in types:
                    raise DataError('cyclic or missing preferred collation type')
                visited.add(target)
                target = types[target][4]
            for spelling in (name,) + row[3]:
                if spelling in aliases and aliases[spelling] != target:
                    raise DataError('conflicting collation type alias: ' + spelling)
                aliases[spelling] = target
            # Source files may use the legacy LDML type name, such as phonebook.
            # Do not select a spelling by guesswork or a hardcoded alias table.
            spellings[target] = tuple(sorted({target} | set(types[target][3])))
        self.aliases = MappingProxyType(aliases)
        self.spellings = MappingProxyType(spellings)
        for locale, default in self.defaults.items():
            selected = self.find(locale, default)
            if (default.startswith('private-') or
                    self.aliases.get(default, default).startswith('private-') or
                    (selected is not None and selected.collation.startswith('private-'))):
                raise DataError('default collation cannot be an incomplete private program')

    def find(self, locale, collation, alternate=''):
        """Find one requested type through parents; never concatenate ancestors.

        Explicit alternate lookup stays explicit. Missing short/proposed data
        does not silently become normal data, or vice versa.
        """
        locale = source_locale(locale)
        if not rules.TYPE.fullmatch(collation):
            raise DataError('invalid collation source type: ' + collation)
        canonical = self.aliases.get(collation, collation)
        names = self.spellings.get(canonical, (canonical,))
        for parent in self.graph.chain(locale, 'collations'):
            matches = [self.programs[(parent, name, alternate)] for name in names
                       if (parent, name, alternate) in self.programs]
            if len(matches) > 1:
                raise DataError('ambiguous collation type aliases at ' + parent)
            if matches:
                return matches[0]
        return None

    def default(self, locale):
        for parent in self.graph.chain(source_locale(locale), 'collations'):
            if parent in self.defaults:
                return self.defaults[parent]
        return 'standard'

    def select(self, locale, requested=None):
        """TR35 type fallback for a normal/full profile, not import resolution."""
        default = self.default(locale)
        selected = default if requested is None else requested
        canonical = self.aliases.get(selected, selected)
        candidates = [selected]
        if canonical.startswith('search') and canonical != 'search':
            candidates.append('search')
        candidates.extend((default, 'standard'))
        visited = set()
        for candidate in candidates:
            if candidate.startswith('private-'):
                continue
            if candidate in visited:
                continue
            visited.add(candidate)
            program = self.find(locale, candidate)
            if program is not None and not program.collation.startswith('private-'):
                return program
        return self.programs[('root', 'standard', '')]

    def expand(self, program, max_steps=1000000, max_depth=128):
        """Inline imports in order; preserve repeated execution and provenance.

        Count import nodes too: a branching DAG of imports of empty programs
        can otherwise take exponential work without producing output nodes.
        An explicit frame stack bounds nesting without Python recursion limits.
        """
        if (type(max_steps) is not int or max_steps <= 0 or
                type(max_depth) is not int or max_depth <= 0):
            raise DataError('invalid collation import expansion budget')
        key = identity(program)
        if self.programs.get(key) != program:
            raise DataError('expanding a program outside this catalog')
        active, stack, output, steps = {key}, [(key, iter(program.nodes))], [], 0
        while stack:
            try:
                node = next(stack[-1][1])
            except StopIteration:
                active.remove(stack.pop()[0])
                continue
            steps += 1
            if steps > max_steps:
                raise DataError('collation import expansion exceeds step budget')
            if not isinstance(node, rules.Import):
                output.append(node)
                continue
            p = node.position
            label = '{}:{}:{}: '.format(p.source, p.line, p.column)
            target = self.find(node.locale, node.collation)
            if target is None:
                raise DataError(label + 'missing imported collation ' + node.collation)
            key = identity(target)
            if key in active:
                raise DataError(label + 'collation import cycle at ' + '/'.join(key))
            if len(stack) >= max_depth:
                raise DataError(label + 'collation import nesting exceeds depth budget')
            active.add(key)
            stack.append((key, iter(target.nodes)))
        return tuple(output)


def load(sources):
    graph = parents.load(sources)
    metadata = locale_metadata.collect(sources, graph)
    programs, defaults = rules.collect(sources)
    return Catalog(programs, defaults, graph, metadata['types'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    catalog = load(acquire.Sources(args.sources))
    counts, expanded, maximum = Counter(), 0, 0
    for program in catalog.programs.values():
        nodes = catalog.expand(program)
        counts.update(type(node).__name__ for node in nodes)
        expanded += len(nodes)
        maximum = max(maximum, len(nodes))
    print(json.dumps({'source_programs': len(catalog.programs),
                      'expanded_nodes': expanded, 'maximum_program_nodes': maximum,
                      'node_kinds': dict(counts)}, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
