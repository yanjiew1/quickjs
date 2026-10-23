# Prepared segmentation dictionaries

QJIN optional section **39** stores self-contained scalar dictionaries. It
needs no shared string or locale indexes. The host encoder and validator are
`tools/intl/segment_dictionary_tables.py`; suffix preparation belongs to
`segment_dictionary_trie.py`. The private C reader is
`src/intl/segment/dictionary.[ch]`. Production packs do not include this section yet.
Presence alone establishes no executable segmentation or locale capability.

Common aligned arrays, graph validation and literal traversal belong to
`scalar_trie_tables.py` on the host and `src/intl/segment/literal-graph.[ch]` in C.
Dictionary containers own family identities and membership/weighted payload
policy; the shared graph layer does not select segmentation boundaries.

This is an internal format, independent of ICU's serialized tries. All integers
are unsigned little-endian. Read bytes explicitly rather than casting a native
C structure. The section inherits QJIN's 256 MiB size bound.

## Header and directory

The header is 16 bytes:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | Magic `QJSDICT` followed by a zero byte |
| 8 | 4 | Format version, currently `1` |
| 12 | 4 | Dictionary count, equal to the enclosing section's directory count |

There are between 1 and 5 dictionaries, sorted by family ID. Each following
descriptor is 48 bytes, containing twelve u32 words in this order:

```
family, valued, root, words, max_scalars, max_utf16,
alphabet_count, value_count, state_count, edge_count, block_offset, block_length
```

Family IDs are `0` CJK, `1` Khmer, `2` Lao, `3` Myanmar, `4` Thai. Each occurs
at most once. `valued` is exactly 1 for CJK and 0 for the other families. CJK
values are raw additive path costs, not frequencies to maximize. Membership
dictionaries use value pool `[0]`; that does not make their mode valued.

Block offsets are relative to the section's first byte. The first block starts
immediately after the descriptor array; each later block follows the previous
one exactly. No overlaps, unused gaps or trailing bytes are allowed. Blocks end
on a four-byte boundary. `words` and both maximum lengths are positive and
describe the accepted language, not the number of terminal states.

## Dictionary arrays

Let A, V, S and E be the alphabet, value, state and edge counts. Each block
contains these arrays in order; every array starts at a four-byte boundary.
Any alignment bytes between arrays are zero.

| Array | Element type | Count | Meaning |
| --- | --- | --- | --- |
| alphabet | u32 | A | Sorted unique Unicode scalars, excluding surrogate code points |
| values | u32 | V | Sorted unique raw values |
| edge_starts | u32 | S + 1 | Half-open edge ranges for each state |
| terminals | u16 | S | Zero if nonterminal, otherwise value-pool index plus 1 |
| labels | u16 | E | Zero-based alphabet IDs |
| targets | u32 | E | Target state IDs |

A is between 1 and 65,536: zero-based u16 alphabet IDs use the whole range.
V is between 1 and 65,535 because terminal tags reserve zero. A valued terminal
with raw cost zero is therefore distinct from a nonterminal. Every alphabet and
value entry is used. S is at least 2 and E is positive; dimensions and computed
byte extents must fit the enclosing section before allocating or reading arrays.
The supported profile additionally bounds aggregate states to 1,000,000 and
aggregate edges to 2,000,000, checked before decoding each block. These limits
bound host object and graph-validation memory, not just serialized bytes; future
source upgrades must explicitly review them rather than truncate data.

State IDs are deterministic postorder IDs. `root == S - 1`; the root is not
terminal, so empty words are excluded. Every edge targets a strictly lower
state ID. Each state's outgoing alphabet IDs are strictly increasing. The
first edge offset is zero, offsets are monotonic, and the final offset equals E.

Equivalent suffix states share one representation only when both their
terminal value identity and complete ordered outgoing edges agree. Prefix
words can still be terminal states with children. No normalization, case
folding, UTF-16 surrogate splitting or script restriction occurs here.

## Validation and ownership

Validate header/version, family ordering, modes, all extents and zero padding
before interpreting arrays. Check scalar/value ordering, terminal/label IDs and
the strict child-before-parent invariant. Every state must be productive and
reachable from the root.

Compute accepted path counts bottom-up: a terminal contributes 1 and **each
outgoing edge** contributes its child's count. Counting unique terminal states
would undercount shared suffixes. Check addition against the declared word
count. Compute maximum scalar depth and maximum UTF-16 depth separately; an
edge contributes one scalar and either one or two UTF-16 code units. The root's
computed count and maxima must equal the descriptor exactly. Graph validation
does not enumerate the accepted words or recursively traverse deep paths.

The prepared arrays own dictionary lookup structure. The segmentation consumer
owns grapheme-safe boundaries, unknown-word progress, script-run policy and any
weighted path selection. Source authentication and release/license identity
belong to acquisition and the QJIN generation receipt. Derivatives must retain
the full distributed dictionary license and applicable source-header notices.

The C reader accepts an already opened `QJSIntlPack`, avoiding an unnecessary
shared-string or locale-table dependency. Opening borrows immutable bytes and
uses only temporary validation scratch. Querying edges or terminal values
allocates nothing. A dictionary view may be copied by value; the underlying
pack bytes must remain alive. An accessor's pointer into a data owner also
requires that owner object to remain unchanged and alive.
