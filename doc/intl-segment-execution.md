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
commit still precedes JS result allocation. Native provider capability admission
and JS selection remain subsequent acceptance steps.

## Shared locale preparation

`tools/intl/segment_catalog.py` binds the final selected shared locale inventory
after formatter aliases have been completed. It resolves each kind through the
segmentation component's parent graph, not main-data exceptional parents.
Captured variable/rule streams are compiled once, then complete execution
profiles are deduplicated. Source-stream receipts survive semantic sharing.
Whole suppression lists share literal graphs while raw order and declaration
origins remain separate, including absent versus explicit empty declarations.
Program/list IDs follow first use in sorted shared-locale routes; the catalog
does not create an independent locale numbering scheme or public capability.

At most 128 distinct streams may be compiled in one catalog, independently of
the 64 retained-program limit. This also bounds attempts that later deduplicate.
At most 256 suppression source origins are prepared before graph deduplication.
Existing compiler/property/graph and serialized binding budgets still apply.

## Native dictionary routing resources

`segment/routing.[ch]` prepares immutable Script and Script_Extensions interval
caches through the Unicode owner's existing `unicode_script` API. Han, Hiragana
and Katakana map to the CJK family; Khmer, Lao, Myanmar and Thai map to their
respective dictionaries. No second script database is generated for Intl.

The cache owns a copy of the allocator in heap-stable storage. Unicode range
callbacks point there, not at a temporary stack allocator; zero-sized Unicode
release requests translate to Intl `free`. Partial initialization releases every
completed or partially built range. Queries use binary interval lookup without
allocation or shared traversal state. A future shared native configuration owner
must retain the cache until its last segmenter is gone; this helper introduces
no separate reference count.

The cache also retains Script_Extensions=Inherited for neutral mark attachment.
A zero family mask alone cannot distinguish neutral marks from foreign scripts.

Returned masks are candidates, not dictionary-core or boundary decisions.
Script_Extensions can include punctuation; the refinement owner must also check
character category, attachment to a surrounding script run, and protected base
and grapheme decisions. This native routing policy does not claim exact
Line_Break=SA classification. [Word refinement](intl-segment-word-refinement.md)
owns those decisions; native admission remains separate work.

## Final native input owner

`segment/native-input.[ch]` connects decoding, compiled rule scans, literal
sentence suppression and native grapheme protection into final boundary vectors.
The private pipeline connects grapheme, word and sentence preparation.
Public native provider admission remains
unfinished until production data and complete locale capability are validated.

Selected and grapheme scans share one decoded scalar/UTF-16 owner during
preparation. For sentence input, remove approved suppression cuts, then
intersect with the native grapheme decisions. Only after every fallible stage
succeeds are final UTF-16 indexes copied into one retained allocation. Temporary
text, scans and matcher scratch are all released, including on OOM.

Word input uses the same decoded owner and native grapheme scan, refines through
the selected dictionaries/routing cache, then applies final grapheme protection.
One monotonic pass classifies final spans by Letter or decimal-digit membership.
Word status is stored with the final vectors, so queries never rescan text.
The borrowed rule configuration names selected/grapheme programs and required
refinement views; it is not retained or silently replaced with fallback data.

The result borrows no program, list, text or service pointer. Binary-search
`next` and `containing` queries allocate nothing and carry no mutable traversal
cursor, so several clients may query the same prepared input independently.
The common service wrapper can retain the original JS string separately for
segment text extraction; its data is not needed to query these vectors.
