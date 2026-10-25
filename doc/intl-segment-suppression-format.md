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

# Prepared sentence suppression lists

Optional QJIN section **41** binds shared locale IDs to complete, resolved
sentence suppression lists. Production packs do not include it yet. Its host
owner is `tools/intl/segment_suppression_tables.py`; common scalar graph arrays
belong to `scalar_trie_tables.py`. List inheritance and source provenance remain
with the preparation stage described in [sentence suppressions](intl-segment-suppressions.md).
Presence alone establishes no executable Segmenter capability.

The private C reader is `src/intl/segment/suppression-data.[ch]`. It opens
against validated shared QJIN data, retains no allocation and publishes copied
borrowed views only after the complete section validates. The immutable pack
bytes must outlive these views. Queries allocate nothing; temporary validation
scratch is released on both success and failure.

All integers are unsigned little-endian. Read bytes explicitly, without casting
native structures. The section is at most 32 MiB.

## Header and directories

The 72-byte header is followed by `L` 48-byte list descriptors, `B` eight-byte
locale bindings, then contiguous graph blocks.

| Header offset | Field |
| --- | --- |
| 0 | Eight bytes `QJSSUPP\0` |
| 8 | Format version, 1 |
| 12 | Unicode version, `18 << 16` (18.0.0) |
| 16 | Semantic profile, 1 |
| 20, 24 | List count `L`, binding count `B` |
| 28 | Exact section length |
| 32, 36 | Reserved zero words |
| 40 | 32-byte source/compiler recipe fingerprint |

The fingerprint is pairing metadata, not authentication or proof of source
equivalence. It is distinct from the enclosing pack generation digest.

Each descriptor contains twelve u32 words:

```
root, words, max_scalars, max_utf16,
alphabet_count, value_count, state_count, edge_count,
block_offset, block_length, reserved_zero, reserved_zero
```

Bindings contain `(locale_id, list_id)`. Locale IDs index the validated shared
QJIN locale table and increase strictly. List IDs follow first use in that order:
the first unseen ID is 0, then 1, and so on. Every list is bound. Several locales
may use one list. Equal lists, including empty lists, need not be deduplicated.

An explicit empty list has eight zero metadata/count words and a zero-length
block. It is a present policy, distinct from a missing section or locale route.
It masks inherited entries; it is not an instruction to append a parent's list.

Block offsets are relative to the section. The first block starts immediately
after the bindings; every later block starts exactly at the preceding end.
Empty lists consume no bytes. No gaps, overlaps, trailing bytes or nonzero
reserved fields are allowed.

## Literal graph blocks

A nonempty block uses the six arrays and alignment rules in the
[dictionary format](intl-segment-dictionary-format.md#dictionary-arrays):
alphabet u32, values u32, edge starts u32, terminal IDs u16, labels u16 and
targets u32. The sole raw terminal value is zero, meaning membership. This
does not import dictionary input restrictions: spaces, punctuation, case and
supplementary scalars remain literal. There is no normalization or case folding.

The generator emits deterministic suffix-shared postorder graphs. Readers
validate postorder, productivity, reachability, exact language counts and
scalar/UTF-16 length maxima. They do not prove graph minimality or unique
canonical state numbering. Empty keys and accepting roots are forbidden.

## Resource gates and ownership

There are 1–256 lists and 1–65,536 bindings. Per nonempty list: at most 65,536
words, 256 scalars per key, 512 UTF-16 units, 65,536 alphabet entries, one value,
262,144 states and 524,288 edges. Metadata must also satisfy
`max_scalars <= max_utf16 <= 512` and `root == state_count - 1`.
The aggregate limits are 1,000,000 states, 2,000,000 edges and 1,000,000 words.
Preflight all descriptor totals before copying graph arrays or allocating graph
validation scratch; check complete extents before forming array views.

Containers own locale routes, explicit empty policy and source/recipe pairing.
The common graph layer owns array safety and literal lookup. The sentence
matcher owns full-entry context and which base boundaries may be suppressed;
the table cannot by itself authorize removal of paragraph or endpoint breaks.
