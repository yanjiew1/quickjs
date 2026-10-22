# Native segmentation program format

QJIN section **40** contains compiled segmentation rules and refinement metadata.
Dictionary DAGs remain in section 39. These are prepared routes: neither section
presence nor a locale binding establishes executable Segmenter coverage.

All integers are unsigned little-endian. All offsets below are section-relative
unless stated otherwise. Every reserved field, alignment byte and unused
acceptance bit is zero. The section is at most 32 MiB.

## Header and directories

The 72-byte header is followed by `P` 40-byte program descriptors, `B` 12-byte
locale bindings, then contiguous four-byte-aligned program blocks. There are no
gaps, shared sub-blocks or trailing bytes.

| Header offset | Field |
| --- | --- |
| 0 | Eight bytes `QJSSEGM\0` |
| 8 | Format version, 1 |
| 12 | Unicode version, `18 << 16` (18.0.0) |
| 16 | Semantic profile, 1 |
| 20, 24 | Program count `P`, binding count `B` |
| 28 | Exact section length |
| 32, 36 | Reserved zero words |
| 40 | 32-byte source/compiler recipe fingerprint |

Semantic profile 1 identifies the reviewed native rule/refinement policy; it is
distinct from the QJIN envelope's minimal/native profile. The recipe pairs tables
with authenticated source manifests, compiler semantics and explicit source
correction receipts. It is an identity, **not authentication**, and is separate
from the outer pack generation digest.

Each program descriptor contains ten words, in order:

```
kind, requirements, classes C, spans R, rules Q, machines M,
block_offset, block_length, reserved_zero, reserved_zero
```

Each binding contains three words `(locale_id, kind, program_id)`. Locale IDs
index the validated shared QJIN locale table. Bindings sort strictly by
`(locale_id, kind)`; program IDs are assigned by first occurrence in that order.
Every program must be used. Resolving inheritance is a generator responsibility,
using the **segmentations** component graph, not main-data parents.

## Program blocks

Compute arrays in this order, aligning each start to four bytes:

| Array | Encoding |
| --- | --- |
| Exclusive class interval ends | `u32[R]` |
| Class IDs | `u16[R]` |
| Sentence context flags | `u8[C]` |
| Ordered rules | `Q` eight-byte records |
| Machine descriptors | `M` sixteen-byte records |
| Machine bodies, in ID order | Complete transitions, then acceptance bits |

Interval starts are implicit: zero for the first interval, the previous end
thereafter. Ends increase strictly to `0x110000`, including surrogate code points.
Adjacent class IDs differ. IDs are assigned by first occurrence, starting at
zero; later intervals can reuse a class. Every class occurs.

A rule is `(u16 left_machine, u16 right_machine, u8 breaks, u8 role,
u16 reserved_zero)`. `breaks` is exactly 0 or 1. Array order supplies priority.
Machine IDs are first-use dense when scanning rules left then right. Every
machine is referenced. A program may have no rules and no machines, expressing
only the implicit default break; it still has a complete alphabet.

A machine descriptor is four words `(states S, mode, transitions_offset,
acceptance_offset)`, with offsets **relative to its program block**. The initial
state is implicitly zero. The body has `u16[S*C]` complete row-major transitions,
then an independently aligned `ceil(S/8)`-byte acceptance vector, low bit first.
The next body and final block end are aligned with zero padding. Descriptor
offsets must equal these computed positions.

Modes are 0 left/restart, 1 right/reversed/restart, and 2 left/BOS-only. Left rule
references require mode 0 or 2; right references require mode 1. Restart behavior
is already compiled into transitions. Runtime execution starts each directional
pass once; it must not reset a machine at every candidate boundary.

States use breadth-first discovery numbering, scanning classes in ascending
order. A validator starts `reached=1`; before scanning row `s`, require
`s < reached`. Every target is below `S` and at most `reached`; equality advances
`reached`. The final value is `S`. This proves reachability with constant space.
Cycles, rejecting sinks, no accepting states and initial acceptance are valid.

## Refinement policy and limits

| Kind | Requirements | Permitted roles | Class flags |
| --- | --- | --- | --- |
| 0 grapheme | 0 | Preserve (0) | 0 |
| 1 word | 13 | Preserve, lexical (1) on non-break rows | 0 |
| 2 sentence | 10 | Preserve, soft (2) on break rows | 0, 1, 2, 4, 8 or 16 |

Requirements are dictionary 1, suppressions 2, word status 4 and grapheme
protection 8. Sentence class flags are respectively none, SATerm, Close, Sp,
ParaSep or ignored. Categories are disjoint in this reviewed profile. Structural
validation cannot prove source-rule roles or homogeneous Unicode membership;
the semantic compiler and source receipts establish those properties.

Limits are 64 programs, 65,536 bindings, 16,384 classes, 1,024 rules and 2,048
machines per program. Each machine has 1–4,096 states and at most 1,048,576
transition cells. Aggregate limits are 262,144 class spans, 262,144 states and
8,388,608 cells. Check dimensions and aggregate budgets before materializing
host tables. Program compiler resource limits remain additional gates.

`tools/intl/segment_program_tables.py` separates wire records from host AST,
NFA, subset and construction-work records. Wire decoding does not invent proof
records. The reader proves bounds, complete graphs and policy domains; source
equivalence, UTF-16 execution, refinement and locale admission are separate.

`src/intl/segment/program.[ch]` checks the entire optional section without
allocating. It requires an unchanged validated shared `QJSIntlData` view for
locale references, then retains only fixed borrowed views of the immutable pack
bytes. The original shared descriptor can leave scope; the bytes must remain
alive. Queries allocate nothing, preserve independent traversal state and reset
outputs on failure. Missing sections/routes and unsupported versions, Unicode
versions or semantic profiles return `UNSUPPORTED`; bad magic and malformed
known-format records return `DATA_ERROR`. No source-language equivalence or
native locale capability is inferred from a successful open.

The [base executor](intl-segment-execution.md) consumes these views in two
directional passes over an explicitly owned decoded input.
