# Native Intl binary data format

This document describes the prepared `QJIN` pack used by the minimal and native
Intl builds. The [section reference](intl-data-sections.md) gives exact layouts
for every current section. These are internal formats, not a stable embedding
API. Keep this reference synchronized with the Python encoders and C readers.
A section's presence records available source data; it does not establish that
an Intl service implements every required algorithm or supports every locale.

## Data ownership and related formats

| Data | Generator / reader | Format reference |
| --- | --- | --- |
| Intl locale and presentation data | `tools/intl/`; `src/intl/` | This document and the section reference |
| IANA timezone rules and identities | `tools/timezone/`; `src/timezone/` | [QJTZ container](../tools/timezone/README.md#primary-identities-and-the-binary-format) |
| Published calendar records and Japanese eras | `tools/calendar/`; `src/calendar/` | [QJCA container](../tools/calendar/README.md#single-binary-pack) |
| Windows timezone mapping | `tools/timezone/`; `src/timezone/` | [Windows mapping](../tools/timezone/README.md#windows-host-mappings) |
| Logical collation root compiler input | `tools/intl/` | [QJCR host input](intl-collation-root-format.md) |
| Unicode algorithm tables | Unicode generator; `src/unicode/` | Generated C tables, not a QJIN section |

`data/intl/minimal.bin` and `data/intl/native.bin` use the same envelope and
section schemas, with different selected inventories and profile identifiers.
Normal builds embed the selected immutable pack as one generated C data owner.
They do not parse CLDR XML, download data or decompress an archive at runtime.
The generated C source's textual byte literals are larger than the binary
payload; that source size is not the embedded payload size.

Localized timezone names and metazone periods in QJIN are presentation data.
They do not replace IANA offset transitions in QJTZ. Similarly, a stored calendar
name or pattern is not an arithmetic calendar implementation.

## Byte encoding and envelope

All QJIN multibyte integers are **little-endian**. This differs from the
big-endian QJTZ and QJCA containers. Never serialize a native C structure or
read the blob through an unaligned integer/structure cast. The C wire accessors
assemble integer values from bytes on every supported host.

The envelope is 64 bytes, followed by 16 bytes per directory entry:

| Byte offset | Width | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII magic `QJIN` |
| 4 | 2 | Major version, currently `1` |
| 6 | 2 | Minor version, currently `0` |
| 8 | 4 | Exact total byte length |
| 12 | 4 | Directory entry count, from 1 through 128 |
| 16 | 32 | Generation identity bytes |
| 48 | 4 | Profile: `0` minimal, `1` native |
| 52 | 4 | Flags, currently zero |
| 56 | 4 | Reserved, zero |
| 60 | 4 | Reserved, zero |

Each directory entry contains four unsigned 32-bit words:

```
(section_id, absolute_file_offset, byte_length, record_count)
```

Section IDs are positive and strictly increasing. The first payload follows the
directory, aligned to 4 bytes; each later payload starts at the previous end
rounded up to 4 bytes. Padding bytes must be zero. There are no other gaps,
overlaps or trailing bytes. A section's length need not be divisible by four:
the string pool ends with UTF-8 bytes. Total pack size is bounded to 256 MiB.

`record_count` is **section-specific**, not a universal payload-record count.
Many complex sections use the selected locale count even though their internal
tables contain many more records. Section 33 counts zone descriptors; section
34 counts explicit rule-list origins; section 35 counts explicit era mappings;
section 36 counts root collation mappings independently of locale count;
section 37 counts their preceding-context metadata records; section 38 counts
compiled executable collation programs, not locale bindings or root mappings.
Sections 36/37 are reserved for the independent test oracle and are absent
from shipped packs. The section reference defines each case.

The generation identity is not an authentication signature and is not checked
as a content checksum by `qjs_intl_pack_open()`. Pinned source hashes and the
accompanying manifest provide separate provenance/reproducibility evidence.

## References, missing values and validation

String IDs index section 1; they are not byte offsets. String 0 is the empty
string. UTF-8 strings have explicit lengths, no terminating NUL, no embedded
NUL and no invalid scalar encodings. Strings are unique and sorted by their
UTF-8 bytes. Locale indexes refer to section 2 only where a schema says so;
source-origin locale strings may name locales outside the selected profile.

Ordinary table indexes are zero-based, so index 0 can be valid. Layered-map
heads and parents are one-based node numbers: head 0 means no chain, and head
`h` names `nodes[h - 1]`. Parent nodes precede children. Map descriptors use
record indexes/counts, not byte offsets.

Do not apply a global interpretation to `0xffffffff`, `0xfffffffe` or
`0xfffffffd`. Different schemas allow different sentinels. In sparse layered
text maps, `0xfffffffe` means blocked, string ID 0 is an explicitly empty leaf,
and a missing key means no leaf. In dense source tables, a separate state word
records missing, value or blocked. An explicit block or empty leaf can stop
inheritance; replacing it with absence changes formatting behavior.

`qjs_intl_pack_open()` validates only the envelope. The shared data reader
validates strings/locales, and each owning service validates its section's
schema, dimensions, references, ordering and reachability before use. Unknown
section IDs can be envelope-valid without implying support for their contents.
Reader views borrow immutable pack memory: the owner must remain unchanged and
alive until all views are released. Temporary validation allocations do not
become a second persistent copy of the data.

## Existing size reductions

These are structural reductions, with no general-purpose compression library:

- **Shared UTF-8 strings:** identical text across sections is stored once and
  referenced by a 32-bit ID; strings have no per-value NUL terminator.
- **Interned records, rows and maps:** identical locale vectors, pattern/affix
  sequences and other typed records share canonical tables and indexes.
- **Sparse source layers:** store actual leaves and shared parent/alias chains
  instead of duplicating a fully expanded inherited map for every locale.
  Preserve source origin, explicit empty values and blocking semantics.
- **Factored dimensions:** date intervals share locale/calendar blocks;
  timezone names keep separate family row tables and omit absent families.
  This avoids storing repeated large rectangular matrices.
- **Profile selection:** the minimal pack selects a smaller inventory. This is
  a coverage choice, not a compression ratio for the full native inventory.

QJIN currently uses mostly fixed-width 32-bit words, not variable-length integer
coding, a compressed string stream or a general binary compressor. No measured
unshared baseline is maintained, so the pack size alone does not establish a
percentage saving for each technique.

Related packs use additional owner-specific reductions: QJTZ shares identical
TZif payload bytes between identities; QJCA stores month lengths using 2 bits
per month; the Windows mapping uses interned strings and 16-bit indexes. These
are separate formats, not QJIN encodings.

## Changing a format safely

1. Read the encoder, Python decoder, C reader and owning header together.
   Numeric enum order, field widths and each count's meaning are wire contracts.
2. For an incompatible section change, update its section version and reader
   rejection. Sections 1 through 16 have no local version word: changing their
   layout needs an explicit compatibility design rather than inserting a word.
   Change the outer version when changing the envelope contract.
3. Update authored fixtures, Python round-trip/corruption checks and C bounds,
   corruption, ownership and allocation-failure tests. Check 32-bit, unaligned
   and big-endian reads. Do not infer safety from a valid envelope alone.
4. Regenerate both prepared profiles and their manifests from authenticated
   pinned inputs. Verify reproducible bytes and unchanged source semantics;
   update this document and the affected section reference together.

[The generator guide](../tools/intl/README.md) describes acquisition, collectors,
regeneration and tests. [The design](intl-design.md) defines the separation
between ECMAScript bindings, functional services, shared modules and backends.
