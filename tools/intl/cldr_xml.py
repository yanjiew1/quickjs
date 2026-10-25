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

"""Shared host XML/schema primitives for typed CLDR collectors.

This is not a validating XML processor or an XML-to-binary serializer.
Collectors retain ownership of their element structure, record selection,
text whitespace and ordered rules. The pinned DTD supplies attribute roles,
required fields, enumerations and explicit defaults that ElementTree omits.
"""
import argparse
from collections import namedtuple
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import acquire

NAME = r'[A-Za-z_][A-Za-z0-9_.:-]*'
ATTRIBUTE = re.compile(
    r'<!ATTLIST\s+(' + NAME + r')\s+(' + NAME + r')\s+'
    r'(CDATA|NMTOKEN|NMTOKENS|\([^)]*\))\s+'
    r'(#IMPLIED|#REQUIRED|(?:#FIXED\s+)?"[^"]*")\s*>', re.S)
TOKENS = re.compile(r'<!--.*?-->|<![^>]*>', re.S)
Attribute = namedtuple('Attribute', 'kind choices required fixed default role')


class DataError(ValueError):
    pass


def normalize_attribute(value):
    # ElementTree already replaces literal XML whitespace with U+0020.
    # Character references to tabs/CR/LF must survive the second stage of
    # non-CDATA normalization; do not turn an invalid token into a valid one.
    return re.sub(r' +', ' ', value).strip(' ')


def parse_xml(data, expected, label='XML input'):
    # External DTD references are not fetched by ElementTree. Do not admit
    # internal entity expansion either; CLDR text uses ordinary XML entities.
    try:
        text = data.decode('utf-8-sig')
    except UnicodeError:
        raise DataError('non-UTF-8 XML input: ' + label)
    if '\0' in text:
        raise DataError('NUL in XML input: ' + label)
    if '<!ENTITY' in text.upper():
        raise DataError('internal entity declaration: ' + label)
    try:
        root = ET.fromstring(text)
    except ET.ParseError as exc:
        raise DataError('malformed {}: {}'.format(label, exc))
    if root.tag != expected:
        raise DataError('unexpected root {} in {}'.format(root.tag, label))
    return root


def read_xml(sources, name, expected):
    return parse_xml(sources.read(name), expected, name)


class Schema:
    """Read the attribute declaration subset used by the pinned CLDR DTDs.

    Unknown declaration syntax fails explicitly instead of supplying guessed
    defaults or treating metadata/value attributes as identity. This reader
    does not expand DTD entities or interpret element content models.
    """
    def __init__(self, data):
        text = data.decode('utf-8-sig')
        self.elements = set()
        self.fields = {}
        current = None
        for match in TOKENS.finditer(text):
            token = match.group()
            if token.startswith('<!--'):
                annotation = token[4:-3].strip()
                if current is not None and annotation in ('@METADATA', '@VALUE'):
                    fields, name = current
                    previous = fields[name]
                    role = 'metadata' if annotation == '@METADATA' else 'value'
                    if previous.role != 'identity' and previous.role != role:
                        raise DataError('conflicting DTD attribute role')
                    fields[name] = previous._replace(role=role)
                continue
            current = None
            if token.startswith('<!ELEMENT'):
                element = re.match(r'<!ELEMENT\s+(' + NAME + r')\s', token)
                if element is None:
                    raise DataError('unsupported DTD element declaration')
                self.elements.add(element.group(1))
            elif token.startswith('<!ATTLIST'):
                declaration = ATTRIBUTE.fullmatch(token)
                if declaration is None:
                    raise DataError('unsupported DTD attribute declaration: ' + token)
                element, name, kind, default = declaration.groups()
                fields = self.fields.setdefault(element, {})
                if name in fields:
                    raise DataError('repeated DTD attribute: ' + element + '/' + name)
                choices = (tuple(part.strip() for part in kind[1:-1].split('|'))
                           if kind.startswith('(') else ())
                if choices and (not all(choices) or len(set(choices)) != len(choices)):
                    raise DataError('invalid DTD attribute enumeration')
                fixed = default.startswith('#FIXED')
                value = default.split('"')[1] if '"' in default else None
                if value is not None:
                    try:
                        value = ET.fromstring('<x value="' + value + '"/>').get('value')
                    except ET.ParseError:
                        raise DataError('unsupported entity in DTD default')
                    if kind != 'CDATA':
                        value = normalize_attribute(value)
                    if choices and value not in choices:
                        raise DataError('invalid enumerated DTD default')
                fields[name] = Attribute(kind, choices, default == '#REQUIRED',
                                         fixed, value, 'identity')
                current = (fields, name)
            else:
                raise DataError('unsupported DTD declaration: ' + token)
        if not self.elements or set(self.fields) - self.elements:
            raise DataError('missing DTD element declarations')

    def attributes(self, element):
        """Validate a consumed node's attributes and apply explicit defaults.

        CDATA values retain their whitespace. Non-CDATA XML attribute types
        collapse whitespace as a validating XML reader would. More specific
        field syntax, such as identifiers and numbers, belongs to collectors.
        """
        if element.tag not in self.elements:
            raise DataError('unknown consumed element: ' + element.tag)
        fields = self.fields.get(element.tag, {})
        if set(element.attrib) - set(fields):
            raise DataError('unknown attributes on ' + element.tag + ': ' +
                            ', '.join(sorted(set(element.attrib) - set(fields))))
        result = {}
        for name, field in fields.items():
            value = element.get(name, field.default)
            if value is None:
                if field.required:
                    raise DataError('missing required attribute: ' + element.tag + '/' + name)
                continue
            if field.kind != 'CDATA':
                value = normalize_attribute(value)
            if field.choices and value not in field.choices:
                raise DataError('invalid enumerated attribute: ' + element.tag + '/' + name)
            if field.fixed and value != field.default:
                raise DataError('changed fixed attribute: ' + element.tag + '/' + name)
            result[name] = value
        return result

    def key(self, element):
        """Identity for unordered records; never reorder rule/text children."""
        fields = self.fields.get(element.tag, {})
        attributes = self.attributes(element)
        return (element.tag, tuple(sorted((name, value) for name, value in attributes.items()
                                         if fields[name].role == 'identity')))


def audit(sources):
    schemas = {}
    for filename in ('ldml.dtd', 'ldmlBCP47.dtd', 'ldmlSupplemental.dtd'):
        schema = Schema(sources.read('common/dtd/' + filename))
        schemas[filename] = {
            'elements': len(schema.elements),
            'attributes': sum(len(fields) for fields in schema.fields.values()),
            'defaults': sum(field.default is not None
                            for fields in schema.fields.values() for field in fields.values()),
        }
    counts = {}
    for directory, expected in (('main', 'ldml'), ('bcp47', 'ldmlBCP47'),
                                 ('supplemental', 'supplementalData')):
        names = tuple(name for name in sources.paths('common/' + directory + '/')
                      if name.endswith('.xml'))
        for name in names:
            read_xml(sources, name, expected)
        counts[directory] = len(names)
    return {'schemas': schemas, 'xml_documents': counts,
            'release': sources.manifest['release']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(acquire.Sources(args.sources)), sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
