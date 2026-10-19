"""Resolve typed collector fields without retaining or serializing XML trees."""
from collections import namedtuple

from cldr_xml import DataError

INHERIT = '\u2191\u2191\u2191'
BLOCK = '\u2205\u2205\u2205'
Alias = namedtuple('Alias', 'target source')
Resolution = namedtuple('Resolution', 'value locale key blocked')
Overlay = namedtuple('Overlay', 'locale group fields parent')


class Records:
    """Sparse values and aliases over a collector's finite typed key inventory.

    Collectors translate supported LDML aliases into field-to-field mappings.
    They own XPath interpretation and structural validation; these are not
    generic XML paths. Each alias targets a known key. Resolution consequently
    has a finite state space and detects cycles without recursion/depth caps.
    """
    def __init__(self, graph, keys, component='main'):
        self.graph = graph
        self.keys = frozenset(keys)
        self.locales = frozenset(graph.tags)
        self.component = component
        self.values = {}
        self.aliases = {}
        if any(not isinstance(key, tuple) or not key for key in self.keys):
            raise DataError('collector keys must be nonempty typed tuples')
        if component != 'main' and component not in graph.components:
            raise DataError('undeclared collector inheritance component')

    def state(self, locale, key):
        if locale not in self.locales or key not in self.keys:
            raise DataError('unknown collector locale/key: ' + repr((locale, key)))
        return locale, key

    def vacant(self, state):
        if state in self.values or state in self.aliases:
            raise DataError('repeated or mixed collector field: ' + repr(state))

    def add(self, locale, key, value):
        state = self.state(locale, key)
        self.vacant(state)
        if value is None:
            raise DataError('use an absent record or explicit block, not None')
        self.values[state] = value

    def alias(self, locale, key, target, source=None):
        state = self.state(locale, key)
        self.vacant(state)
        self.state(locale if source is None else source, target)
        # None means CLDR source="locale": restart at the requesting locale
        # when this alias is reached through that locale's parent chain.
        self.aliases[state] = Alias(target, source)

    def resolve(self, locale, key):
        self.state(locale, key)
        original = locale
        visited = set()
        while locale is not None:
            state = (locale, key, original)
            if state in visited:
                raise DataError('collector alias/parent cycle: ' + repr(state))
            visited.add(state)
            field = (locale, key)
            if field in self.values:
                value = self.values[field]
                if value == BLOCK:
                    return Resolution(None, locale, key, True)
                if value != INHERIT:
                    return Resolution(value, locale, key, False)
            alias = self.aliases.get(field)
            if alias is not None:
                key = alias.target
                if alias.source is not None:
                    original = alias.source
                locale = original
            else:
                locale = self.graph.parent(locale, self.component)
        return None

    def require(self, locale, key):
        result = self.resolve(locale, key)
        if result is None or result.blocked:
            raise DataError('missing/blocked required collector field: ' + repr((locale, key)))
        return result


class SparseMaps:
    """Typed whole-group aliases over sparse leaves, with requesting context.

    Each state has one parent/alias dependency. Resolve it iteratively to
    avoid a Python recursion cap. Empty overlays and aliases share dictionaries;
    a genuine leaf override copies its inherited map before modification.
    overlays instead preserves local fields in a persistent child-first chain
    for consumers requiring lateral alternatives before parent lookup.

    Keep records and graph unchanged while reusing a cache. resolve and
    overlays require separate caches; treat their returned data as immutable.
    """
    def __init__(self, graph, keys, label):
        self.graph, self.label = graph, label
        self.locales = frozenset(graph.tags)
        self.keys = frozenset(keys)
        self.values, self.aliases = {}, {}

    def state(self, locale, key):
        if locale not in self.locales or key not in self.keys:
            raise DataError('unknown {} locale/group'.format(self.label))
        return locale, key

    def add(self, locale, group, key, value):
        state = self.state(locale, group)
        if state in self.aliases:
            raise DataError('mixed {} alias/leaves'.format(self.label))
        fields = self.values.setdefault(state, {})
        if key in fields:
            raise DataError('repeated {} field'.format(self.label))
        fields[key] = value

    def alias(self, locale, group, target, source):
        state = self.state(locale, group)
        self.state(locale if source is None else source, target)
        if state in self.aliases or state in self.values:
            raise DataError('mixed/repeated {} alias'.format(self.label))
        self.aliases[state] = target, source

    def _dependency_path(self, locale, group, cache):
        self.state(locale, group)
        start = (locale, locale, group)
        state, path, active = start, [], set()
        while state not in cache:
            if state in active:
                raise DataError('{} alias/parent cycle'.format(self.label))
            active.add(state)
            path.append(state)
            anchor, current, key = state
            alias = self.aliases.get((current, key))
            if alias is not None:
                target, source = alias
                anchor = anchor if source is None else source
                state = anchor, anchor, target
            else:
                parent = self.graph.parent(current)
                if parent is None:
                    base = None
                    break
                state = anchor, parent, key
        else:
            base = cache[state]
        return start, path, base

    def resolve(self, locale, group, cache):
        start, path, base = self._dependency_path(locale, group, cache)
        if base is None:
            base = {}
        for state in reversed(path):
            anchor, current, key = state
            fields = self.values.get((current, key), {})
            overrides = {key: value for key, value in fields.items() if value != INHERIT}
            if overrides:
                base = base.copy()
                base.update(overrides)
            cache[state] = base
        return cache[start]

    def overlays(self, locale, group, cache):
        """Return local field snapshots in lookup order, or None if absent.

        Group aliases preserve requesting/source context, including original
        child overrides preceding an inherited alias's restarted target chain.
        Skip empty and INHERIT-only layers, not BLOCK or explicit empty leaves.
        Nodes share suffixes; walk iteratively rather than recursively hashing,
        comparing or serializing the linked namedtuples. No fallback among
        selectors is performed here. Use a separate cache from resolve.
        """
        start, path, base = self._dependency_path(locale, group, cache)
        for state in reversed(path):
            anchor, current, key = state
            fields = self.values.get((current, key), {})
            overrides = {key: value for key, value in fields.items() if value != INHERIT}
            if overrides:
                base = Overlay(current, key, overrides, base)
            cache[state] = base
        return cache[start]
