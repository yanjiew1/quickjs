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

"""Short-input language oracle for authored segmentation preparation tests.

This intentionally enumerates candidate suffixes and reachable positions. It
must not be used for production traversal or large source conformance corpora.
Memoization and a work cap also bound shared nullable/optional language DAGs.
"""


class _Oracle:
    def __init__(self, text, work_limit):
        if len(text) > 64:
            raise ValueError('short reference input budget exceeded')
        self.text, self.limit, self.work = text, work_limit, 0
        self.cache = {}

    def visit(self):
        self.work += 1
        if self.work > self.limit:
            raise ValueError('reference work budget exceeded')

    def ends(self, language, start):
        self.visit()
        key = id(language), start
        if key not in self.cache:
            self.cache[key] = (language, self.compute(language, start))
        return self.cache[key][1]

    def compute(self, language, start):
        kind, children = language.kind, language.children
        if kind == 'char':
            if start < len(self.text):
                code = ord(self.text[start])
                for a, b in language.character.ranges:
                    self.visit()
                    if a <= code < b:
                        return frozenset((start + 1,))
            return frozenset()
        if kind == 'epsilon' or kind == 'start' and start == 0:
            return frozenset((start,))
        if kind == 'start':
            return frozenset()
        if kind == 'concat':
            positions = frozenset((start,))
            for child in children:
                positions = frozenset(end for position in positions for end in self.ends(child, position))
            return positions
        if kind == 'alt':
            return frozenset(end for child in children for end in self.ends(child, start))
        if kind == 'optional':
            return self.ends(children[0], start) | frozenset((start,))
        if kind == 'star':
            positions, pending = {start}, [start]
            while pending:
                for end in self.ends(children[0], pending.pop()):
                    if end not in positions:
                        positions.add(end)
                        pending.append(end)
            return frozenset(positions)
        raise ValueError('unknown reference language')


def ends(language, text, start, work_limit=65536):
    return _Oracle(text, work_limit).ends(language, start)


def winning(rules, text, position, work_limit=65536):
    oracle = _Oracle(text, work_limit)
    if position == 0 or position == len(text):
        return True, 'endpoint'
    for rule in rules:
        if (any(position in oracle.ends(rule.left, start) for start in range(position + 1)) and
                oracle.ends(rule.right, position)):
            return rule.breaks, rule.source.source.identifier
    return True, None
