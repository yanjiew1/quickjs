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

"""Prepare immutable scalar dictionaries with shared equivalent suffix states.

Terminal identity includes the raw path cost. This is a lookup representation,
not a longest-match or minimum-cost word-boundary algorithm, nor an ICU trie.
"""
from collections import namedtuple

from cldr_xml import DataError
from segment_dictionary import Dictionary
import scalar_trie

Trie = namedtuple('Trie', 'alphabet values edge_starts terminals labels targets root '
                         'valued words max_scalars max_utf16')
MAX_ALPHABET = 0x10000
MAX_VALUES = 0xffff
MAX_COUNT = 0xffffffff


def prepare(data):
    """Keep dictionary key/cost policy outside shared scalar graph mechanics."""
    if (not isinstance(data, Dictionary) or type(data.valued) is not bool or
            type(data.entries) is not tuple or not data.entries or len(data.entries) > MAX_COUNT):
        raise DataError('invalid dictionary trie input')
    if len(data.entries) > scalar_trie.DEFAULT_LIMITS.entries:
        raise DataError('dictionary trie entry budget exceeded')
    total = max_scalars = max_utf16 = 0
    for entry in data.entries:
        if not isinstance(entry, tuple) or len(entry) != 2:
            raise DataError('invalid dictionary trie entry')
        word, cost = entry
        if not isinstance(word, str) or not word:
            raise DataError('invalid dictionary trie key')
        total += len(word)
        if (len(word) > scalar_trie.DEFAULT_LIMITS.key_length or
                total > scalar_trie.DEFAULT_LIMITS.scalars):
            raise DataError('dictionary trie key/scalar budget exceeded')
        # Dictionary source keys exclude separators/control text. Suppression
        # phrases use a different owner; sharing the DAG must not relax this.
        if any(ord(char) <= 0x20 or ord(char) == 0x7f or
               0xd800 <= ord(char) <= 0xdfff or ord(char) == 0xfeff for char in word):
            raise DataError('invalid dictionary trie key')
        if data.valued:
            if type(cost) is not int or not 0 <= cost <= MAX_COUNT:
                raise DataError('invalid dictionary trie cost')
        elif cost is not None:
            raise DataError('cost on a membership dictionary')
        max_scalars = max(max_scalars, len(word))
        max_utf16 = max(max_utf16, len(word) + sum(ord(char) > 0xffff for char in word))
    if (data.max_scalars != max_scalars or data.max_utf16 != max_utf16 or
            max_utf16 > MAX_COUNT):
        raise DataError('invalid dictionary trie length metadata')
    graph = scalar_trie.prepare(data.entries)
    return Trie(*graph[:6], graph.root, data.valued, graph.words,
                graph.max_scalars, graph.max_utf16)
