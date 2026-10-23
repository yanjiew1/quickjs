# Prepared segmentation input

`src/intl/segment/scan.[ch]` executes validated base rule programs from section
40. It does not expose native locale capability or apply dictionary/suppression
refinement. Those policies must be complete before a native Segmenter is admitted.

## Owners and original indexes

`QJSIntlSegmentText` owns decoded code points and a map of scalar boundaries to
original UTF-16 offsets. It combines valid surrogate pairs, preserves lone
surrogates and embedded NUL, and records the original EOF offset. It borrows the
source only during preparation; later scans can share the decoded owner.

`QJSIntlSegmentScan` owns one class ID per scalar, plus base break decisions and
the first winning rule ID per boundary. `DEFAULT_RULE` represents mandatory
endpoints or the implicit default break, rather than a fabricated source row.
The vectors do not borrow the decoded text or a caller's program descriptor.
Native refinement can use the original rule ID to protect or replace a decision.

Each owner has one retained allocation and a copied creating allocator. Clear
frees that allocation and zeros the owner. Copying an owned struct does not
duplicate ownership; do not clear shallow copies. All preparation is explicit
and fallible, with checked size arithmetic and transactional output publication.

## Directional passes

Preparation maps each scalar to the program's complete alphabet, then partitions
the immutable machine views into left and right runs. Source machine IDs map to
those run positions; duplicate right owners share one acceptance bit.

Starting at original EOF, the backward pass records each right machine's
acceptance at boundary `p` **before** consuming scalar `p-1`. It stores a compact
right-machine bitset for each boundary. Starting at original BOS, the forward
pass checks left acceptance at boundary `p` **before** consuming scalar `p`.
The first source rule whose two sides accept supplies the base decision.

Compiled restart behavior is already in the transition tables. A BOS-only
machine initializes once at absolute input start, including when later boundaries
occur after newlines. The executor neither restarts such a machine nor imposes
a fixed lookbehind limit. BOS and EOF remain mandatory breaks independently of
authored rules.

For `n` scalars, `I` class intervals, `M` machines, `Q` rules and `R` distinct
right machines, work is `O(n * (log I + M + Q))`. Temporary storage is
`O(M + Q + n * ceil(R/8))`; retained text
and decision vectors are linear in `n`. The temporary allocation is freed before
publication, including error paths. A default-only program needs no machine
scratch. Classification uses binary search in the prepared interval map.

The service interface in `segment.h` explicitly prepares a `QJSIntlSegmentInput`
before allocation-free `next`/`containing` queries. Minimal copies raw-input
metadata without allocating. A native backend can own final vectors after
refinement, enabling independent iterators and binary-search lookup. Its prepared
state must not borrow service or segmenter handles: input can outlive both.

The JS binding shares this prepared input between Segments and their iterators,
retaining the original string and UTF-16 storage. Backend teardown precedes
release of that borrowed storage. Failed preparation leaves zero output, so the
binding frees its local UTF-16 reference before wrapper cleanup. Iterator cursor
commit still precedes JS result allocation. Native refinement, word classification
and capability admission remain subsequent acceptance steps.
