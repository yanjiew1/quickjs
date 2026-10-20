"""Literal two-operand source templates, without consumer-specific slot meaning."""
from cldr_xml import DataError


def validate(value):
    """Preserve text with exactly one {0}/{1}, matching the native pair parser.

    Apostrophes and letters are literal; this is not date-pattern or ICU
    MessageFormat grammar. Source inheritance/block/empty states have owners.
    """
    if not isinstance(value, str) or any(not ord(c) or 0xd800 <= ord(c) <= 0xdfff for c in value):
        raise DataError('pair template must be scalar text')
    if value.count('{0}') != 1 or value.count('{1}') != 1:
        raise DataError('pair template requires both placeholders exactly once')
    literal = value.replace('{0}', '').replace('{1}', '')
    if '{' in literal or '}' in literal:
        raise DataError('unsupported pair placeholder or brace')
    return value
