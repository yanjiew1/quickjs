"""Canonical section envelope for prepared native Intl data, without compression.

All integers are little-endian. Sections have stable numeric identities; their
owners validate record schemas separately. Opening an envelope never claims
service completeness or authenticates the generation identity.
"""
import struct

HEADER = struct.Struct('<4sHHII32sIIII')
DIRECTORY = struct.Struct('<4I')
VERSION = (1, 0)
PROFILES = {'minimal': 0, 'native': 1}
MAX_SIZE = 256 * 1024 * 1024
MAX_SECTIONS = 128
STRINGS = 1
LOCALES = 2
ALIASES = 3
LIKELY = 4
KEYS = 5
TYPES = 6
WEEKS = 7
CALENDAR_PREFERENCES = 8
HOUR_PREFERENCES = 9
LISTS = 10
SCRIPT_DIRECTIONS = 11
LIST_CONTEXTS = 12
NUMBERING_SYSTEMS = 13
NUMBERING_PREFERENCES = 14
NUMBER_SYMBOLS = 15
PLURALS = 16
NUMBER_PATTERNS = 17
COMPACT_PATTERNS = 18
CURRENCIES = 19
NUMBER_LAYOUTS = 20
UNIT_PATTERNS = 21


def uint32(value):
    if type(value) is not int or not 0 <= value <= 0xffffffff:
        raise ValueError('Intl data integer outside uint32 range')
    return value


def pack(sections, generation, profile):
    """Encode {section_id: (record_count, bytes)} and a 32-byte generation ID."""
    if (profile not in PROFILES or not isinstance(generation, bytes) or
            len(generation) != 32 or not 1 <= len(sections) <= MAX_SECTIONS):
        raise ValueError('invalid Intl generation/profile/section count')
    offset = HEADER.size + len(sections) * DIRECTORY.size
    entries, payload = bytearray(), bytearray()
    for identity in sorted(sections):
        if not uint32(identity):
            raise ValueError('Intl section zero is reserved')
        count, data = sections[identity]
        uint32(count)
        if not isinstance(data, bytes):
            raise ValueError('Intl section must contain immutable bytes')
        padding = -offset % 4
        payload.extend(bytes(padding))
        offset += padding
        if len(data) > MAX_SIZE - offset:
            raise ValueError('Intl pack exceeds size limit')
        entries.extend(DIRECTORY.pack(identity, offset, len(data), count))
        payload.extend(data)
        offset += len(data)
    binary = HEADER.pack(b'QJIN', *VERSION, offset, len(sections), generation,
                         PROFILES[profile], 0, 0, 0) + entries + payload
    unpack(binary)
    return binary


def unpack(binary):
    """Validate the envelope and return immutable section payloads and counts."""
    if not isinstance(binary, bytes) or not HEADER.size <= len(binary) <= MAX_SIZE:
        raise ValueError('invalid Intl pack size/type')
    (magic, major, minor, size, count, generation, profile,
     flags, reserved1, reserved2) = HEADER.unpack_from(binary)
    if (magic != b'QJIN' or (major, minor) != VERSION or size != len(binary) or
            profile not in PROFILES.values() or flags or reserved1 or reserved2 or
            not 1 <= count <= MAX_SECTIONS):
        raise ValueError('invalid Intl pack header')
    offset = HEADER.size + count * DIRECTORY.size
    if offset > size:
        raise ValueError('truncated Intl section directory')
    sections, previous = {}, 0
    for index in range(count):
        identity, start, length, records = DIRECTORY.unpack_from(
            binary, HEADER.size + index * DIRECTORY.size)
        aligned = (offset + 3) & ~3
        if (identity <= previous or start != aligned or start > size or
                length > size - start or any(binary[offset:start])):
            raise ValueError('invalid Intl section identity/extent/padding')
        sections[identity] = (records, binary[start:start + length])
        previous, offset = identity, start + length
    if offset != size:
        raise ValueError('unclaimed Intl trailing bytes')
    return {'generation': generation, 'profile': profile, 'sections': sections}
