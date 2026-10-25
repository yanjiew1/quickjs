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

"""Parse the pinned CLDR tailoring subset into ordered, source-located nodes.

This is not a general ICU rule/UnicodeSet parser. Unknown executable syntax is
rejected, not dropped. NFD-inert star operands, aliases/imports, canonical
closure and CE weight allocation require later semantic compilation.
"""
import argparse
from bisect import bisect_right
from collections import Counter, namedtuple
import json
from pathlib import Path
import re

import acquire
from cldr_xml import DataError, Schema, read_xml
import parents

# Positions are relative to the rule text, with a source label identifying its
# XML collation. ElementTree does not provide original XML source coordinates.
Position = namedtuple('Position', 'source line column')
Reset = namedtuple('Reset', 'position before text special')
Relation = namedtuple('Relation', 'position strength prefix text extension')
Starred = namedtuple('Starred', 'position strength ranges')
Setting = namedtuple('Setting', 'position name values')
Import = namedtuple('Import', 'position locale collation')
SuppressContractions = namedtuple('SuppressContractions', 'position ranges')
Optimize = namedtuple('Optimize', 'position ranges')
Program = namedtuple('Program', 'locale collation alternate draft references nodes')

WHITE = '\t\n\v\f\r \x85\u200e\u200f\u2028\u2029'  # Pattern_White_Space
TYPE = re.compile(r'[a-z0-9]+(?:-[a-z0-9]+)*\Z')
SCRIPT = re.compile(r'[A-Z][a-z]{3}\Z')
MAX_NODES = 1000000
SETTINGS = {
    'strength': ('1', '2', '3', '4', 'I'),
    'alternate': ('non-ignorable', 'shifted'),
    'backwards': ('2',),
    'normalization': ('on', 'off'),
    'caseLevel': ('on', 'off'),
    'caseFirst': ('upper', 'lower', 'off'),
    'hiraganaQ': ('on', 'off'),
    'numericOrdering': ('on', 'off'),
    'maxVariable': ('space', 'punct', 'symbol', 'currency'),
}
SPECIALS = frozenset(a + ' ' + b for a in ('first', 'last') for b in
                    ('tertiary ignorable', 'secondary ignorable', 'primary ignorable',
                     'variable', 'regular', 'implicit', 'trailing'))


def import_target(value):
    """A missing co type means standard, not the imported locale's default."""
    parts = value.split('-u-co-')
    if len(parts) > 2:
        raise DataError('invalid collation import: ' + value)
    locale = parents.locale_id(parts[0])
    if locale == 'und':
        locale = 'root'
    collation = parts[1] if len(parts) == 2 else 'standard'
    if not TYPE.fullmatch(collation):
        raise DataError('invalid imported collation type: ' + value)
    # Preserve the requested type. Resolving BCP47 aliases against source type
    # names belongs to the catalog resolver, not the rule grammar.
    return locale, collation


def unescape(text, source):
    """CLDR tool escapes are decoded BEFORE the collation grammar is parsed.

    Keep original character offsets for diagnostics. This is distinct from
    quoting/escaping inside the UnicodeSet subgrammar. Do not tag a decoded
    syntax character as a literal: it still needs collation apostrophe quotes.
    """
    result, offsets, at = [], [], 0
    common = {'a': '\a', 'b': '\b', 'e': '\x1b', 'f': '\f', 'n': '\n',
              'r': '\r', 't': '\t', 'v': '\v'}
    while at < len(text):
        start, c = at, text[at]
        at += 1
        if c == '\\':
            if at == len(text):
                raise DataError(source + ': trailing tool escape')
            c = text[at]
            at += 1
            if c in ('u', 'U'):
                size = 4 if c == 'u' else 8
                value = text[at:at + size]
                if len(value) != size or not re.fullmatch('[0-9a-fA-F]+', value):
                    raise DataError(source + ': invalid Unicode tool escape')
                at += size
                code = int(value, 16)
                if 0xd800 <= code <= 0xdbff and text[at:at + 2] == '\\u':
                    value = text[at + 2:at + 6]
                    if re.fullmatch('[0-9a-fA-F]{4}', value) and 0xdc00 <= int(value, 16) <= 0xdfff:
                        at += 6
                        code = 0x10000 + ((code - 0xd800) << 10) + int(value, 16) - 0xdc00
                if code > 0x10ffff:
                    raise DataError(source + ': out-of-range tool escape')
                c = chr(code)
            elif c in common:
                c = common[c]
            elif ord(c) < 128 and c.isalnum():
                raise DataError(source + ': unsupported alphanumeric tool escape')
        if 0xd800 <= ord(c) <= 0xdfff:
            raise DataError(source + ': non-scalar rule text')
        result.append(c)
        offsets.append(start)
    offsets.append(len(text))
    return ''.join(result), offsets


class Parser:
    def __init__(self, text, source):
        if not isinstance(text, str):
            raise DataError('collation rules must be text')
        self.text, self.offsets = unescape(text, source)
        self.source, self.at = source, 0
        self.lines = [0] + [m.end() for m in re.finditer(r'\r\n|[\n\r\f\x85\u2028\u2029]', text)]

    def position(self, at=None):
        at = self.at if at is None else at
        original = self.offsets[at]
        line = bisect_right(self.lines, original)
        return Position(self.source, line, original - self.lines[line - 1] + 1)

    def fail(self, message):
        p = self.position()
        raise DataError('{}:{}:{}: {}'.format(p.source, p.line, p.column, message))

    def peek(self):
        return self.text[self.at:self.at + 1]

    def skip_space(self):
        while self.peek() and self.peek() in WHITE:
            self.at += 1

    def skip(self):
        while self.at < len(self.text):
            if self.peek() in WHITE:
                self.at += 1
            elif self.peek() == '#':
                while self.at < len(self.text) and self.peek() not in '\r\n\f\x85\u2028\u2029':
                    self.at += 1
            else:
                break

    def set_escape(self):
        self.at += 1
        if not self.peek():
            self.fail('trailing UnicodeSet escape')
        c = self.peek()
        if c.isalnum():
            self.fail('unsupported executable UnicodeSet escape/property')
        self.at += 1
        return ord(c)

    def quoted(self):
        self.at += 1
        if self.peek() == "'":
            self.at += 1
            return [(ord("'"), True)]
        result = []
        while self.at < len(self.text):
            c = self.peek()
            if c == "'":
                self.at += 1
                if self.peek() == "'":
                    result.append((ord("'"), True))
                    self.at += 1
                else:
                    return result
            else:
                code = ord(c)
                if 0xd800 <= code <= 0xdfff:
                    self.fail('quoted non-scalar value')
                result.append((code, True))
                self.at += 1
        self.fail('unterminated quoted literal')

    def string(self, starred=False):
        result = []
        self.skip_space()
        while True:
            c = self.peek()
            if c in WHITE or c == '#' or not c or c in '&<=[|/]':
                break
            if c == "'":
                result.extend(self.quoted())
            elif c == '-' and starred:
                result.append((ord(c), False))
                self.at += 1
            elif (0x21 <= ord(c) <= 0x2f or 0x3a <= ord(c) <= 0x40 or
                  0x5b <= ord(c) <= 0x60 or 0x7b <= ord(c) <= 0x7e):
                self.fail('ASCII punctuation must be quoted or escaped')
            elif 0xd800 <= ord(c) <= 0xdfff:
                self.fail('non-scalar rule literal')
            else:
                result.append((ord(c), False))
                self.at += 1
        if not result:
            self.fail('missing rule string')
        return result

    def bracket_words(self):
        assert self.peek() == '['
        start = self.at = self.at + 1
        while self.peek() and self.peek() not in '[]':
            self.at += 1
        words = tuple(re.split('[' + re.escape(WHITE) + ']+',
                               self.text[start:self.at].strip(WHITE)))
        if self.peek() != ']' or not words:
            self.fail('invalid bracket directive')
        self.at += 1
        return words

    def ranges(self, tokens):
        result, at = [], 0
        while at < len(tokens):
            first, protected = tokens[at]
            if first == ord('-') and not protected:
                self.fail('range lacks its first scalar')
            if at + 1 < len(tokens) and tokens[at + 1] == (ord('-'), False):
                if at + 2 >= len(tokens):
                    self.fail('range lacks its final scalar')
                last = tokens[at + 2][0]
                if last < first or (first <= 0xdfff and last >= 0xd800):
                    self.fail('invalid scalar range')
                at += 3
            else:
                last = first
                at += 1
            result.append((first, last))
        return tuple(result)

    def unicode_set(self):
        self.skip_space()
        if self.peek() != '[':
            self.fail('expected scalar UnicodeSet')
        self.at += 1
        tokens = []
        while True:
            while self.peek() and self.peek() in WHITE:
                self.at += 1
            c = self.peek()
            if not c:
                self.fail('unterminated UnicodeSet')
            if c == ']':
                self.at += 1
                return self.ranges(tokens)
            if c == '\\':
                tokens.append((self.set_escape(), True))
            elif ord(c) < 128 and not c.isalnum() and c != '-':
                self.fail('unsupported executable UnicodeSet operator/literal')
            elif 0xd800 <= ord(c) <= 0xdfff:
                self.fail('non-scalar UnicodeSet literal')
            else:
                tokens.append((ord(c), False))
                self.at += 1

    def directive(self, position):
        # Set-valued commands contain a nested bracket, unlike word settings.
        saved = self.at
        self.at += 1
        start = self.at
        while self.peek() and ord(self.peek()) < 128 and self.peek().isalpha():
            self.at += 1
        name = self.text[start:self.at]
        if name in ('suppressContractions', 'optimize'):
            ranges = self.unicode_set()
            self.skip_space()
            if self.peek() != ']':
                self.fail('unclosed set-valued directive')
            self.at += 1
            return (SuppressContractions if name == 'suppressContractions' else Optimize)(position, ranges)
        self.at = saved
        words = self.bracket_words()
        name, values = words[0], words[1:]
        if name == 'import':
            if len(values) != 1:
                self.fail('import requires one locale/type')
            try:
                locale, collation = import_target(values[0])
            except DataError as error:
                self.fail(str(error))
            return Import(position, locale, collation)
        if name == 'reorder':
            if not values or len(set(values)) != len(values) or any(
                    value not in ('space', 'punct', 'symbol', 'currency', 'digit', 'others') and
                    not SCRIPT.fullmatch(value) for value in values):
                self.fail('invalid reorder list')
        elif name not in SETTINGS or len(values) != 1 or values[0] not in SETTINGS[name]:
            self.fail('unknown or invalid collation setting')
        return Setting(position, name, values)

    def parse(self):
        result, have_reset, before, pending_reset = [], False, 0, False
        while True:
            self.skip()
            if self.at == len(self.text):
                if pending_reset:
                    self.fail('reset chain lacks a relation')
                return tuple(result)
            if len(result) == MAX_NODES:
                self.fail('tailoring program exceeds node budget')
            position, c = self.position(), self.peek()
            self.at += 1
            if c == '&':
                if pending_reset:
                    self.fail('reset chain lacks a relation')
                pending_reset = True
                self.skip_space()
                before = 0
                if self.peek() == '[' and self.text.startswith('[before', self.at):
                    words = self.bracket_words()
                    if len(words) != 2 or words[0] != 'before' or words[1] not in ('1', '2', '3'):
                        self.fail('invalid before reset')
                    before = int(words[1])
                    self.skip_space()
                if self.peek() == '[':
                    special = ' '.join(self.bracket_words())
                    if special not in SPECIALS:
                        self.fail('unknown special reset position')
                    text = ()
                else:
                    text, special = tuple(c for c, _ in self.string()), ''
                result.append(Reset(position, before, text, special))
                have_reset = True
            elif c in '<=':
                if not have_reset:
                    self.fail('relation lacks reset')
                strength = 0 if c == '=' else 1
                if c == '<':
                    while self.peek() == '<':
                        strength += 1
                        self.at += 1
                    if strength > 4:
                        self.fail('invalid relation strength')
                if before and strength != before:
                    self.fail('first relation after before reset has different strength')
                before = 0
                pending_reset = False
                self.skip_space()
                if self.peek() == '*':
                    self.at += 1
                    result.append(Starred(position, strength, self.ranges(self.string(True))))
                else:
                    text = tuple(c for c, _ in self.string())
                    self.skip_space()
                    prefix, extension = (), ()
                    if self.peek() == '|':
                        self.at += 1
                        prefix, text = text, tuple(c for c, _ in self.string())
                        self.skip_space()
                    if self.peek() == '/':
                        self.at += 1
                        extension = tuple(c for c, _ in self.string())
                    result.append(Relation(position, strength, prefix, text, extension))
            elif c == '[':
                self.at -= 1
                result.append(self.directive(position))
            else:
                self.fail('expected reset, relation or directive')


def parse(text, source='<rules>'):
    return Parser(text, source).parse()


def collect(sources):
    schema = Schema(sources.read('common/dtd/ldml.dtd'))
    programs, defaults = {}, {}
    for name in sources.paths('common/collation/'):
        if not name.endswith('.xml'):
            continue
        locale = parents.locale_id(Path(name).stem)
        root = read_xml(sources, name, 'ldml')
        containers = root.findall('collations')
        if len(containers) > 1:
            raise DataError('repeated collations container: ' + name)
        for container in containers:
            if schema.attributes(container):
                raise DataError('unsupported collations metadata: ' + name)
            if container.text and container.text.strip(WHITE):
                raise DataError('unwrapped collation text: ' + name)
            for child in container:
                if child.tail and child.tail.strip(WHITE):
                    raise DataError('unexpected collation tail: ' + name)
                fields = schema.attributes(child)
                if child.tag == 'defaultCollation':
                    if fields:
                        raise DataError('unsupported defaultCollation metadata: ' + name)
                    if locale in defaults or len(child) or not TYPE.fullmatch(child.text or ''):
                        raise DataError('invalid/repeated default collation: ' + name)
                    defaults[locale] = child.text
                    continue
                if child.tag != 'collation' or not TYPE.fullmatch(fields.get('type', '')):
                    raise DataError('unsupported collation record: ' + name)
                if set(fields) - {'type', 'alt', 'draft', 'references'}:
                    raise DataError('unsupported collation metadata: ' + name)
                kind, alt = fields['type'], fields.get('alt', '')
                key = locale, kind, alt
                if child.text and child.text.strip(WHITE):
                    raise DataError('unwrapped tailoring text: ' + name)
                if key in programs or len(child) > 1:
                    raise DataError('duplicate/ambiguous collation: ' + name)
                nodes = ()
                for cr in child:
                    if cr.tag != 'cr' or len(cr) or cr.tail and cr.tail.strip(WHITE):
                        raise DataError('unsupported collation child: ' + name)
                    if schema.attributes(cr):
                        raise DataError('unsupported rule-text metadata: ' + name)
                    nodes = parse(cr.text or '', name + '#' + kind + (':' + alt if alt else ''))
                programs[key] = Program(locale, kind, alt, fields.get('draft', ''),
                                        fields.get('references', ''), nodes)
    if ('root', 'standard', '') not in programs:
        raise DataError('missing root standard program')
    return programs, defaults


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    programs, defaults = collect(acquire.Sources(args.sources))
    counts = Counter(type(node).__name__ for program in programs.values() for node in program.nodes)
    print(json.dumps({'programs': len(programs), 'defaults': defaults,
                      'nodes': dict(counts)}, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
