<!--
Copyright (c) 2017-2021 Fabrice Bellard
Copyright (c) 2017-2021 Charlie Gordon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->

# Logical root compiler input (QJCR)

QJCR version 1 is a private, generated **host compiler input**. It is independent
of ordered QJCI tailoring programs and the QJIN runtime format. It is not loaded
by the engine. `tools/intl/collation_root_input.py` owns its writer and structural
auditor; `tools/intl/collation-root-input.{h,c}` owns the C reader. Normal builds consume prepared runtime data rather than downloading
CLDR or generating QJCR.

The source is the authenticated CLDR 49 release `common/uca/FractionalUCA.txt`.
The collector retains original keys and resolves explicit references before
serializing logical root coordinates. Python does not normalize keys with its
host Unicode version. Lookup input and tailoring text use pinned C Unicode NFD.

## Envelope

All integers are unsigned little-endian 32-bit words, decoded without alignment
assumptions. Files have a 64 MiB limit. The 64-byte header contains:

| Offset | Contents |
| --- | --- |
| 0 | Four bytes `QJCR` |
| 4 | Format version, 1 |
| 8 | Unicode version, `18 << 16` |
| 12 | UCA version, `18 << 16` |
| 16 | Total file bytes |
| 20 | Section count, 12 |
| 24, 28 | Reserved words, zero |
| 32 | 32-byte pinned source-manifest SHA-256 fingerprint |

Twelve directory entries follow. Each is four words: section kind, byte offset,
byte length and record count. Kinds occur exactly once in ascending order.
Sections are contiguous, begin immediately after the directory, and have byte
lengths divisible by four. Unknown sections, gaps and trailing bytes are errors.

## Sections

| Kind | Records | Words per record / contents |
| --- | --- | --- |
| 1 | Metadata, exactly one | 18 words, listed below |
| 2 | Mappings | Key start, key count, CE start, CE count |
| 3 | Keys | One Unicode scalar per word |
| 4 | Contexts | Mapping ID, prefix scalar count, reserved zero |
| 5 | CEs | Primary, secondary, tertiary, quaternary, root origin, flags |
| 6 | Han ranges | Inclusive first scalar, inclusive last scalar, rank start |
| 7 | Han ranks | One relative radical-order position per word |
| 8 | Reset source facts, 14 | Kind, role, followed by six CE words |
| 9 | Partial weak-limit facts, four | Kind, axis, coordinate, reserved zero |
| 10 | Groups | Primary boundary, kind, token start, token count |
| 11 | Tokens | Byte start, byte count |
| 12 | Token bytes | Counted ASCII bytes, followed by zero padding to four bytes |

Metadata word order is primary/secondary/tertiary maxima; common secondary and
tertiary; protected secondary and tertiary boundaries; Han base and count;
generic base and count; generic exclusive limit; first and last implicit source
markers; variable top; numeric reservation; field separator; initial last regular.
Root coordinates have bit 31 clear. Zero is ignorable. Common aliases and
protected boundaries are explicit data, not inferred from maxima.

Mapping IDs are section-2 row indices. Rows sort by `(combined key, prefix count)`.
Key spans partition section 3 in row order; a key is prefix followed by mapped
text. There is at least one mapped scalar. Equal combined keys may have distinct
prefix splits, such as `abc`, `a|bc` and `ab|c`. Context rows are ordered unique
mapping IDs with `0 < prefix count < key count`. Missing context means zero
prefix. NUL and supplementary scalars are allowed; surrogates are not. FDD0
synthetic mappings are excluded, while FDD1 group markers are retained. Prefix
CEs are not included in mapping payloads; there is no raw-allkeys prefix skip.
Identical CE sequences may share spans. Keys do not share spans, bounding the
work needed to validate adjacent ordering.

Root CEs have quaternary zero and root origin equal to primary. Flags use bits
0–1 for case (lower 0, mixed 1, upper 2), bit 2 for default variable membership,
and bit 3 for a virtual implicit coordinate. Other bits are zero. Variable
membership is `field separator < primary <= variable top`. Numeric reservation
is not an executable root primary. WF1/WF2 checks use the protected boundaries:
primary CEs have positive ordinary secondary/tertiary; secondary-only CEs have
secondary above its boundary and ordinary positive tertiary; tertiary-only CEs
have tertiary above its boundary and upper case. Completely ignorable CEs have
lower case.

Han ranges are sorted, disjoint scalar intervals and partition the rank pool.
Ranks form an exact permutation of `1 .. Han count`. A Han scalar's primary is
`Han base + rank`. Generic fallback is `generic base + code point + 1`, except
for declared Han. The generic interval is strictly `(generic base, generic limit)`,
where `generic limit = generic base + 0x110001`. The source E4 unassigned-group
marker equals generic base and remains literal; the fixed E5 marker bounds the
other end. E0/E4 source marker facts are separate from these interval endpoints.
Fallback can order an ill-formed UTF-16 code unit even though source keys cannot
contain one.

Reset kinds are first/last pairs for tertiary ignorable, secondary ignorable,
primary ignorable, variable, regular, implicit and trailing, in that order.
Roles are source CE (1), artificial boundary (2) and derived initial (3).
Tertiary-ignorable, secondary-ignorable and implicit facts are artificial; initial
last regular is derived from first Hani rather than the footer's last-regular CE.
These facts are **initial baselines, not completed logical reset execution**.
The executor adjusts them for preceding rules, retaining established ordering
positions even after their original mappings are replaced or suppressed. Fixed E5 is
also distinct from the actual first-trailing source CE, `EF FD`. The auditor
checks each fact's role, CE class and ordered first/last pair. Implicit markers
and initial last regular must match their metadata anchors, common weak weights
and literal provenance.

Weak-limit kinds are first/last secondary in primary non-ignorable, then
first/last tertiary in secondary non-ignorable. Axis is 1 for secondary or 2 for
tertiary. Each first coordinate must not exceed its paired last coordinate.
Wildcard stronger fields are not ordinary zero CE fields.

Groups are ordered primary boundaries. Kind 1 is reorderable and has nonempty
tokens; kind 2 is the single unassigned boundary at generic base and has none.
Token spans partition section 11; byte spans partition the counted portion of
section 12. Unique tokens are four-letter script names or `space`, `punct`,
`symbol`, `currency`, `digit`; these five core groups are required in that order.
Hani starts at Han base, and the numeric reservation belongs to the digit group
interval. These redundant relationships prevent individually valid sections
from assigning protected coordinates to the wrong group.

## C reader lifetime and validation

`qjs_intl_root_input_open()` borrows immutable bytes and copies a CE-independent
key-index view. It checks every pool once and retains only CE spans per mapping,
with no expanded copy of shared vectors. Common metadata, key/CE/Han validation
and queries belong to `src/intl/collation/base.{h,c}`; the QJCR adapter projects
its decoded fields/spans into that owner. Compiler reset facts, weak facts,
source-marker constraints and token vocabulary remain in the host reader.
The base's temporary Han-permutation bitmap is released before the host audits
token uniqueness with its own bitmap. Neither allocation is retained, including
on failure. Allocation failure remains distinguishable from malformed input.
Lookup and implicit fallback allocate nothing; the wire layout is unchanged.

The reader checks both original context starts with pinned Unicode NFC boundary
properties. These checks belong in C, independently of Python's structural audit.
Isolated unsafe-prefix and unsafe-mapped-start fixtures retain valid key ordering
so an unrelated sort check cannot hide a missing boundary check. The host tool's
`--audit-root` command validates a complete QJCR file; CI runs it against the
artifact generated from the pinned source.

## Updating the format

Change the version when changing any layout or invariant, and update the writer,
auditors, readers, fixtures and this document together. QJCI version 2 carries the same source-manifest identity. The host tool's
`--audit-pair` requires all 32 bytes to match before normalization; matching
Unicode versions alone is insufficient. The identity labels do not authenticate
arbitrary modified artifacts. A semantic driver must preserve this pairing
before building program state. Source facts must not be
silently treated as implemented reset operations. A frozen runtime artifact will
have its own version and layout.
