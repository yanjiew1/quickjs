"""Small authored languages, independent of CLDR parsing/property producers."""
from segment_lower import Character, Language


def node(kind, *children):
    return Language(kind, tuple(children), None, 1 + max((child.height for child in children), default=0))


def char(text):
    values, ranges = sorted(set(map(ord, text))), []
    for value in values:
        if ranges and ranges[-1][1] == value:
            ranges[-1] = ranges[-1][0], value + 1
        else:
            ranges.append((value, value + 1))
    return Language('char', (), Character(tuple(ranges), False, None), 1)
