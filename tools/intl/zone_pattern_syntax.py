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

"""Lex the signed hour/minute source patterns used by localized GMT formats."""
from cldr_xml import DataError
from date_pattern_syntax import compile_pattern


def compile_hour_format(value):
    """Keep positive/negative tokens and literal spacing, signs and bidi marks.

    This parses source hourFormat, not derived short/long output or seconds.
    Apostrophe quoting applies before recognizing its separating semicolon.
    """
    if not isinstance(value, str):
        raise DataError('hour format must be scalar text')
    parts, quoted, start, index = [], False, 0, 0
    while index < len(value):
        char = value[index]
        if char == "'":
            if index + 1 < len(value) and value[index + 1] == "'":
                index += 2
                continue
            quoted = not quoted
        elif char == ';' and not quoted:
            parts.append(value[start:index])
            start = index + 1
        index += 1
    if quoted:
        raise DataError('unclosed hour-format quote')
    parts.append(value[start:])
    if len(parts) != 2:
        raise DataError('hour format requires two subpatterns')
    result = tuple(compile_pattern(part) for part in parts)
    for tokens in result:
        fields = tuple((token.symbol, token.width) for token in tokens if token.symbol)
        if fields not in ((('H', 1), ('m', 2)), (('H', 2), ('m', 2))):
            raise DataError('hour format requires one H/HH and mm field')
    return result
