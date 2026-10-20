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
