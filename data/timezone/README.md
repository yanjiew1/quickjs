# Native timezone data

`timezone-data.bin` is the single QJTZ pack generated from IANA tzdata **2026e**
and CLDR JSON **48.2.3**. Its manifest records exact source revisions,
generation settings, identifier/primary relationships and payload hashes.
See [the generator guide](../../tools/timezone/README.md) for acquisition,
offline regeneration and the binary format.

The pack retains the supplied pre-1970 histories, including `backzone`, and
POSIX recurrence rules for dates beyond explicit transitions. It applies no
leap-second corrections or generation-time date-range restriction. IANA's
historical records can be approximate; storing them does not establish
historical accuracy.

Canonical UTC remains in this source pack for complete source enumeration.
The common library owns UTC behavior; backends supply additional identifiers
and reuse core behavior for aliases whose primary identifier is UTC.

The snapshot has no active rename-waiting overrides. Review replacement-name
policy when updating the dataset; geographic splits are not renames.
Generation uses fixed pins and does not change with the current date.

The IANA data is public domain; see [LICENSE-IANA](LICENSE-IANA).
CLDR metadata and derived mappings use the
[Unicode license](sources/cldr/LICENSE). Raw CLDR JSON is retained as generator
input and is not parsed by the target runtime.
