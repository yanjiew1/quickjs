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

"""Literal fixed-operand source templates, without consumer-specific slot meaning."""
from cldr_xml import DataError


def validate(value, operands):
    """Preserve text with exactly one of each numbered operand.

    Apostrophes and letters are literal; this is not date-pattern or ICU
    MessageFormat grammar. Source inheritance/block/empty states have owners.
    """
    if not isinstance(value, str) or any(not ord(c) or 0xd800 <= ord(c) <= 0xdfff for c in value):
        raise DataError('literal template must be scalar text')
    if type(operands) is not int or not 1 <= operands <= 10:
        raise DataError('invalid literal template operand count')
    slots = tuple('{' + str(index) + '}' for index in range(operands))
    if any(value.count(slot) != 1 for slot in slots):
        raise DataError('literal template requires each placeholder exactly once')
    literal = value
    for slot in slots:
        literal = literal.replace(slot, '')
    if '{' in literal or '}' in literal:
        raise DataError('unsupported literal placeholder or brace')
    return value
