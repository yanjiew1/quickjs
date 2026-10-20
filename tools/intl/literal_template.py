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
