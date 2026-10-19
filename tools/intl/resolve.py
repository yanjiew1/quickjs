"""Resolve typed collector fields without retaining or serializing XML trees."""
from collections import namedtuple

from cldr_xml import DataError

INHERIT = '\u2191\u2191\u2191'
BLOCK = '\u2205\u2205\u2205'
Alias = namedtuple('Alias', 'target source')
Resolution = namedtuple('Resolution', 'value locale key blocked')


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
