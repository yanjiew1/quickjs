"""Prepare immutable scalar dictionaries with shared equivalent suffix states.

Terminal identity includes the raw path cost. This is a lookup representation,
not a longest-match or minimum-cost word-boundary algorithm, nor an ICU trie.
"""
from collections import namedtuple

from cldr_xml import DataError
from segment_dictionary import Dictionary

Trie = namedtuple('Trie', 'alphabet values edge_starts terminals labels targets root '
                         'valued words max_scalars max_utf16')
MAX_ALPHABET = 0x10000
MAX_VALUES = 0xffff
MAX_COUNT = 0xffffffff


class _Node:
    __slots__ = ('terminal', 'edges')

    def __init__(self):
        self.terminal = 0
        self.edges = []


def prepare(data):
    """Minimize a scalar-sorted dictionary using only an active prefix frontier.

    Children are finalized before their parents. Intern their complete terminal
    and ordered-edge identity, assigning deterministic postorder state IDs.
    Thus every target is below its parent, without recursion or a full mutable
    trie for the whole input. All returned arrays are independently owned tuples.
    """
    if (not isinstance(data, Dictionary) or type(data.valued) is not bool or
            not data.entries or len(data.entries) > MAX_COUNT):
        raise DataError('invalid dictionary trie input')
    alphabet, values = set(), set()
    previous = ''
    max_scalars, max_utf16 = 0, 0
    for word, cost in data.entries:
        if (not isinstance(word, str) or not word or word <= previous or
                any(ord(char) <= 0x20 or ord(char) == 0x7f or
                    0xd800 <= ord(char) <= 0xdfff or ord(char) == 0xfeff for char in word)):
            raise DataError('invalid, duplicate or unordered dictionary trie key')
        if data.valued:
            if type(cost) is not int or not 0 <= cost <= MAX_COUNT:
                raise DataError('invalid dictionary trie cost')
        elif cost is not None:
            raise DataError('cost on a membership dictionary')
        alphabet.update(map(ord, word))
        values.add(cost if data.valued else 0)
        max_scalars = max(max_scalars, len(word))
        max_utf16 = max(max_utf16, len(word) + sum(ord(char) > 0xffff for char in word))
        previous = word
    if (data.max_scalars != max_scalars or data.max_utf16 != max_utf16 or
            max_utf16 > MAX_COUNT):
        raise DataError('invalid dictionary trie length metadata')
    if len(alphabet) > MAX_ALPHABET or len(values) > MAX_VALUES:
        raise DataError('dictionary trie alphabet/value ID budget exceeded')
    alphabet, values = tuple(sorted(alphabet)), tuple(sorted(values))
    alphabet_ids = {scalar: index for index, scalar in enumerate(alphabet)}
    value_ids = {value: index + 1 for index, value in enumerate(values)}
    states, interned = [], {}
    frontier = [_Node()]
    previous = ''

    def intern(node):
        key = node.terminal, tuple(node.edges)
        found = interned.get(key)
        if found is not None:
            return found
        if len(states) == MAX_COUNT:
            raise DataError('dictionary trie state budget exceeded')
        found = len(states)
        states.append(key)
        interned[key] = found
        return found

    def finish_suffix(common):
        while len(frontier) > common + 1:
            depth = len(frontier) - 1
            child = intern(frontier.pop())
            label = alphabet_ids[ord(previous[depth - 1])]
            frontier[-1].edges.append((label, child))

    for word, cost in data.entries:
        common = 0
        while common < min(len(previous), len(word)) and previous[common] == word[common]:
            common += 1
        finish_suffix(common)
        frontier.extend(_Node() for _ in range(len(word) - common))
        frontier[-1].terminal = value_ids[cost if data.valued else 0]
        previous = word
    finish_suffix(0)
    root = intern(frontier.pop())

    starts, terminals, labels, targets = [0], [], [], []
    for terminal, edges in states:
        if len(edges) > MAX_COUNT - len(labels):
            raise DataError('dictionary trie edge budget exceeded')
        terminals.append(terminal)
        labels.extend(label for label, _ in edges)
        targets.extend(target for _, target in edges)
        starts.append(len(labels))
    return Trie(alphabet, values, tuple(starts), tuple(terminals), tuple(labels),
                tuple(targets), root, data.valued, len(data.entries), max_scalars, max_utf16)
