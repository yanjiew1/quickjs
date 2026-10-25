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

# Compiled collation archive (QJCL)

QJCL version 2 is the runtime-oriented output of the host C collation compiler.
It contains one shared logical FractionalUCA root and sparse per-program ranked
overlays. It does **not** embed [QJCR compiler input](intl-collation-root-format.md):
reset source facts, wildcard weak-limit facts and script-token parsing tables
are absent. C structs, pointers, allocator state and compiler history are never
serialized.

The C compiler emits a standalone archive without resolved locale/type bindings.
The publication wrapper can attach a prepared profile's resolved bindings and
semantically read back the final image. This remains a data integration stage,
not advertised native Collator support. Prepared QJIN packs embed the complete
profile archive in [section 38](intl-data-sections.md#38--compiled-collation-programs-and-resolved-bindings)
and remain normal build inputs. Version 2 defines populated bindings; version 1
is not accepted.

## Envelope

All integers are unsigned little-endian 32-bit words. Readers must support
unaligned bytes. The archive is limited to 256 MiB. Its 64-byte header is:

| Offset | Field |
| --- | --- |
| 0 | Four bytes `QJCL` |
| 4 | Format version, 2 |
| 8, 12 | Unicode and UCA versions, both `18 << 16` |
| 16 | Total archive byte length |
| 20 | Directory count, 12 |
| 24 | Program count |
| 28 | Reserved zero |
| 32 | 32-byte acquired source-manifest fingerprint |

Twelve 16-byte directory entries follow: kind, byte offset, byte length and
record count. Kinds are exactly 1 through 12. Payloads are contiguous from
offset 256, with lengths divisible by four; no gaps or trailing bytes are
allowed. The source fingerprint records provenance and pairing, not content
authentication. The enclosing QJIN generation identity will cover final bytes.

## Shared root and directory

| Kind | Record size | Contents |
| --- | --- | --- |
| 1 | 80 bytes, exactly one | Runtime root metadata below |
| 2 | 16 bytes | Root mapping: key start/count, CE start/count |
| 3 | 4 bytes | Original root key scalars |
| 4 | 12 bytes | Root context: mapping ID, prefix count, zero |
| 5 | 24 bytes | Unshifted root CE: P, S, T, Q, root-primary provenance, flags |
| 6 | 12 bytes | Han range: inclusive first/last scalar, rank-pool start |
| 7 | 4 bytes | Relative Han radical-order rank |
| 8 | 8 bytes | Source group: primary boundary, kind |
| 9 | 32 bytes | Program identity and block spans |
| 10 | Bytes | Counted label pool, zero-padded to four bytes |
| 11 | Variable | Contiguous program blocks |
| 12 | 24 bytes | Locale/type bindings; empty in standalone compiler output |

Metadata word order is P/S/T root maxima; common S/T; protected S/T boundaries;
Han base/count; generic base/count/exclusive limit; default variable top;
numeric primary reservation; fixed field-separator primary; and five source
group IDs for space, punctuation, symbol, currency and digit, in that order.
The Q root maximum is zero. Group kind 1 is a named source group; kind 2 is the
final unassigned group. Both participate in reordering, including `others`.
Script aliases have already compiled to group identities.

Root mapping/key/context ordering is the same logical key-index layout as QJCR.
Rows sort by combined prefix-plus-suffix key, then prefix scalar count. Source
keys remain original: do not synthesize NFD contractions. Root CE spans may be
shared. Flags use bits 0–1 for lower/mixed/upper case values 0/1/2, bit 2 for
root-default variable classification and bit 3 for implicit origin. Higher bits
are zero. Root Q is zero; root-primary provenance equals unshifted P.

Han rank values are an exact permutation of 1 through Han count. Lookup selects
the range, then adds its relative radical-order rank to Han base. Other fallback
code points use `generic_base + code_point + 1`, strictly inside the generic
interval; fallback includes isolated UTF-16 surrogates even though stored keys
and suppression sets contain only Unicode scalars.

## Program identities

Each kind-9 row contains eight words: locale offset/length, source collation type
offset/length, alternate offset/length, block offset/length. Label offsets are
relative to kind 10; block offsets are relative to kind 11. Identity tuples sort
strictly by counted UTF-8 bytes. Locale/type are nonempty; alternate may be empty.
No C string terminators are implied. Labels contain no control characters.

Standalone compiler output appends the three labels in row order without
interning. Bound output appends binding labels after the program labels.
Kind 10's count excludes its at-most-three zero padding bytes. Program
blocks partition kind 11 exactly, and kind 9 and kind 11 counts equal the header
program count. Source identities can include private/import-only types and
diagnostic alternates; their presence does not establish public availability.

## Program blocks

Each block starts with 16 words (64 bytes):

1. Strength: 1 primary, 2 secondary, 3 tertiary, 4 quaternary, 5 identical.
2. Case first: 0 off, 1 upper first, 2 lower first.
3. Maximum variable group: 0 space, 1 punctuation, 2 symbol, 3 currency.
4. Boolean flags: shifted, backwards secondary, normalization, case level and
   numeric ordering in bits 0 through 4. Higher bits are zero.
5. Twelve array counts, in the following table's order.

| Array | Record size | Contents |
| --- | --- | --- |
| Mapping rows | 16 bytes | Key start/count, ranked CE start/count |
| Key pool | 4 bytes | NFD scalar words, including counted NUL |
| Context rows | 12 bytes | Mapping ID, prefix scalar count, zero |
| Ranked CEs | 24 bytes | P/S/T/Q final ranks, root-primary provenance, flags |
| Suppression | 4 bytes | Sorted unique mapped-first scalars |
| Group permutation | 4 bytes | Source group ID to reordered rank |
| P root offsets | 8 bytes | Root gap, cumulative additions |
| S root offsets | 8 bytes | Root gap, cumulative additions |
| T root offsets | 8 bytes | Root gap, cumulative additions |
| Q root offsets | 8 bytes | Root gap, cumulative additions |
| Trie nodes | 16 bytes | Edge start/count, suffix-root ID, terminal mapping ID |
| Trie edges | 8 bytes | Scalar, child node ID |

Arrays immediately follow the header in this order. Key and CE indices are local
to the block. Live rows sort by combined key and prefix split; each suffix is
nonempty. Keys and CE spans partition their pools in row order. Empty CE output
is a present mapping, not absence. Independent case/provenance flags retain the
root record encoding; `variable` remains the root default, not the result of
applying this program's maximum-variable policy.

Sparse offsets sort by strictly increasing root gap and cumulative additions.
The final rank of an unchanged root ordinal `r` is `r + added` from the last gap
**strictly below** `r`. This preserves root zero and places a root before
insertions in its own gap. Root, implicit, common, numeric and separator
ordinates all use the selected program's translation. The maximum possible
rank is `root_max + final_cumulative_added`, including additions in the last
root's own gap; it is not necessarily the translation of `root_max`. Q can have
positive tailored ranks despite its zero root maximum. Final ranks are full
uint32 values: bit 31 is not a symbolic-ID tag. UINT32_MAX remains reserved.

The group permutation contains every rank from zero through group count minus
one once. Grouping and variable policy use immutable root-primary provenance
before reordering. Fixed separator and trailing positions are outside that
permutation; a final numeric rank cannot reconstruct the source group.

Suppression filters inherited root contexts/contractions starting at its scalar,
but retains ordinary root singleton mappings and later overlay introductions.
It survives even when no overlay terminal remains.

Trie node 0 groups by first mapped scalar, then reversed prefix edges. Each
suffix-root link starts a forward suffix trie including the first mapped scalar.
NONE is UINT32_MAX. Every edge/suffix target is later than its parent, with one
owner; scalar edges are sorted within each complete child span. Prefix nodes
have no terminals; suffix nodes have no suffix links. A suffix terminal has
consumed at least one scalar. Terminals name mapping rows, not CE offsets.
Every mapping path must reach its exact terminal ID, and every node must belong
to a mapping path. An empty overlay still contains node 0. These invariants make
matcher callbacks non-failing after one complete validation pass.

## Locale/type bindings

Each kind-12 row has six words: locale label offset/length, public `co` label
offset/length, sort program ID and search program ID. IDs index kind 9;
UINT32_MAX means unsupported for that usage. Labels use kind 10, in binding-row
order after all program labels; each label span begins exactly where the
previous span ends. No independent string interning or C terminators are used.

Rows sort strictly by `(locale, co)`. Every locale starts with an empty-co row
and two valid usage targets: the semantic null/default. Further rows have
nonempty canonical public type labels and at least one valid usage target.
The initial producer exposes named types for sort only. A type may target the
same program as the default and must remain explicit for option resolution.
`default`, `standard`, `search` and private types are not public type labels.
Targets must be normal programs with no alternate or private source type.

Binding locales are canonical-cased base language IDs, without extensions or
private use; internal `root` never appears here. The generator owns alias and
collation-component inheritance resolution. Required synthetic locale mappings
are applied before source lookup. The reader checks fixed grammar/casing,
ordered defaults, label partitions and eligible target bounds, not aliases or
whether a public name is semantically supported by a particular tailoring.

The source fingerprint must match when attaching these records. Bindings do
not alter root/program bytes and do not imply the comparator has been enabled.
Locale matching and ResolveLocale consume them through the Collator service
availability/default/type interfaces; the low-level binding getter performs no
locale fallback.

## Production and publication

The C archive owner stages programs independently of snapshot/compiler lifetime.
A failed append poisons staging; abort and clear it. Successful finish returns
an independent byte image. `--compile-pair` emits no bytes until all programs and
serialization and runtime semantic readback succeed; Windows stdout is placed
in binary mode. `--audit-compiled archive.qjcl` runs the same reader independently
on an existing archive.

`tools/intl/collation_archive.py` captures the complete process output, checks
framing and source identity, then atomically replaces its destination. It does
not allocate weights, normalize text or claim full semantic validation.

## Runtime reader and lifetime

`src/intl/collation/compiled-data.c` validates the complete archive before
publishing any borrowed view. It shares root validation with `base.c`, and checks
program policies, sparse offsets, NFD/canonical ordering, provenance and rank
domains, group permutations and exact trie/key agreement. Temporary audit
allocations are released on success and failure; malformed data and allocation
failure have distinct statuses. Opening retains no heap storage.

Program selection decodes fixed descriptors and the final offset per axis;
element, root and implicit queries neither allocate nor repeat whole-data audits.
Each program copies its root and group descriptors, so the parent archive
descriptor need not remain alive. The immutable archive bytes must outlive every
borrowed view. Once a matcher borrows a program's trie descriptor, that program
must stay at a stable address. A successful data audit does not advertise public
locale/type availability; executable service publication remains a separate
integration step.
