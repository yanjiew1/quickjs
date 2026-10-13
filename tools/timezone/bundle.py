"""The deterministic, endian-independent QJTZ version 1 binary container.

Header (32 bytes) and index records (16 bytes) use big-endian uint32_t.
Header: magic, version, total_size, record_count, index_start, strings_start,
payload_start, flags. Record: identifier_start, primary_start, TZif_start,
TZif_length. All starts are absolute byte offsets. Strings are ASCII/NUL.
Payloads are deduplicated complete fat TZif files, not filesystem paths.
"""
import hashlib
import re
import struct

HEADER = struct.Struct(">4s7I")
INDEX = struct.Struct(">4I")
MAGIC = b"QJTZ"
VERSION = 1
MAX_FILE = 65536
MAX_UINT32 = (1 << 32) - 1
NAME = re.compile(r"[A-Za-z0-9_+./-]+\Z")


def _name(name):
    if not isinstance(name, str) or not 0 < len(name) < 256 or not NAME.fullmatch(name):
        raise ValueError("invalid identifier")
    if any(part in ("", ".", "..") for part in name.split("/")):
        raise ValueError("invalid identifier path")
    return name.encode("ascii") + b"\0"


def _blob(blob):
    if not isinstance(blob, bytes) or not 44 <= len(blob) <= MAX_FILE or \
            blob[:4] != b"TZif" or blob[4:5] not in (b"2", b"3", b"4"):
        raise ValueError("payload must be a complete-size TZif v2/v3/v4 candidate")


def pack(zones):
    """zones maps each identifier to (primary_identifier, complete_TZif)."""
    names = sorted(zones)
    if not names or len(names) > 65535:
        raise ValueError("invalid record count")
    if len({name.lower() for name in names}) != len(names):
        raise ValueError("case-insensitive identifier collision")
    strings_start = HEADER.size + len(names) * INDEX.size
    strings, string_offsets = bytearray(), {}
    for name in names:
        encoded = _name(name)
        string_offsets[name] = strings_start + len(strings)
        strings.extend(encoded)
    payload_start = strings_start + len(strings)
    payload, blob_offsets, records = bytearray(), {}, []
    for name in names:
        primary, blob = zones[name]
        if primary not in zones or zones[primary][0] != primary:
            raise ValueError("primary does not resolve to itself")
        _blob(blob)
        if blob != zones[primary][1]:
            raise ValueError("alias payload differs from its primary")
        # Bytes, rather than a digest alone, are the deduplication key.
        if blob not in blob_offsets:
            blob_offsets[blob] = payload_start + len(payload)
            payload.extend(blob)
        records.append((string_offsets[name], string_offsets[primary], blob_offsets[blob], len(blob)))
    total = payload_start + len(payload)
    if total > MAX_UINT32:
        raise ValueError("container exceeds uint32 offsets")
    binary = HEADER.pack(MAGIC, VERSION, total, len(names), HEADER.size,
                         strings_start, payload_start, 0)
    binary += b"".join(INDEX.pack(*record) for record in records) + strings + payload
    return bytes(binary)


def unpack(binary):
    """Validate the complete container and return decoded index views."""
    if len(binary) < HEADER.size:
        raise ValueError("truncated header")
    magic, version, total, count, index, strings, payload, flags = HEADER.unpack_from(binary)
    if magic != MAGIC or version != VERSION or flags or total != len(binary) or \
            not 0 < count <= 65535 or index != HEADER.size or \
            strings != index + count * INDEX.size or not strings < payload <= total:
        raise ValueError("invalid header")

    def string_at(offset):
        if not strings <= offset < payload or (offset != strings and binary[offset - 1] != 0):
            raise ValueError("invalid string offset")
        end = binary.find(b"\0", offset, payload)
        if end < 0:
            raise ValueError("unterminated identifier")
        try:
            value = binary[offset:end].decode("ascii")
        except UnicodeError as error:
            raise ValueError("non-ASCII identifier") from error
        _name(value)
        return value

    records, extents = [], set()
    for row in range(count):
        name_at, primary_at, offset, length = INDEX.unpack_from(binary, index + row * INDEX.size)
        name, primary = string_at(name_at), string_at(primary_at)
        if offset < payload or offset > total or length > total - offset:
            raise ValueError("invalid payload extent")
        _blob(binary[offset:offset + length])
        records.append({"identifier": name, "primary": primary,
                        "identifier_offset": name_at, "primary_offset": primary_at,
                        "offset": offset, "length": length,
                        "sha256": hashlib.sha256(binary[offset:offset + length]).hexdigest()})
        extents.add((offset, length))
    names = [record["identifier"] for record in records]
    if names != sorted(names) or len({name.lower() for name in names}) != count:
        raise ValueError("invalid identifier ordering")
    by_name = {record["identifier"]: record for record in records}
    for record in records:
        primary = by_name.get(record["primary"])
        if primary is None or primary["primary"] != primary["identifier"] or \
                (primary["offset"], primary["length"]) != (record["offset"], record["length"]):
            raise ValueError("invalid primary relationship")
    cursor = payload
    for offset, length in sorted(extents):
        if offset != cursor:
            raise ValueError("overlapping or noncontiguous payloads")
        cursor += length
    if cursor != total:
        raise ValueError("unindexed trailing bytes")
    # Canonical string storage has exactly one entry per sorted identifier.
    canonical_strings = b"".join(_name(name) for name in names)
    if binary[strings:payload] != canonical_strings:
        raise ValueError("noncanonical string storage")
    expected = strings
    for record in records:
        if record["identifier_offset"] != expected or \
                record["primary_offset"] != by_name[record["primary"]]["identifier_offset"]:
            raise ValueError("noncanonical string references")
        expected += len(_name(record["identifier"]))
    return records
