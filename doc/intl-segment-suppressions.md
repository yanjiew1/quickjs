# Literal sentence suppression preparation

The source collector preserves raw CLDR suppression entries. The inheritance
owner selects the nearest declared **whole list** along the `segmentations`
parent graph. It does not union entries or use exceptional main-data parents.
An explicit empty list stops inheritance. A missing declaration and a declared
empty list can both execute as empty policy, but retain different origins.

`segment_suppressions.py` keeps the original ordered entries and declaration
origin beside a canonical literal graph. It sorts keys only for graph
construction. Spaces, punctuation, case and Unicode spelling remain unchanged;
multi-part abbreviations are not split or interpreted as regular expressions.
Empty keys, duplicate keys and malformed scalar text are errors. An empty list
has no graph, rather than an accepting epsilon path.

## Shared mechanics, separate source policy

`scalar_trie.py` owns the scalar-key graph mechanics used by both suppression
lists and dictionaries. It retains only the current sorted-key prefix frontier,
finalizes children before parents and interns equivalent suffix states.
Terminal payload identity participates in sharing. Keys are Unicode scalar
strings; payloads are uint32 values. A `None` source payload means membership
and maps to zero. Postorder IDs
ensure every edge points to a smaller state. This is a host preparation shape,
not a new runtime wire format.

The dictionary frontend retains its stricter source rules: no separator/control
keys, and exact membership versus weighted-cost modes. Sharing graph mechanics
does not make dictionary input accept suppression phrases. The dictionary wire
format and payload identity remain unchanged.

## Resource bounds

Common default limits are 1,000,000 entries, 16,777,216 total key scalars,
4,096 scalars per key, 1,000,000 retained states, 2,000,000 retained plus active
frontier edges, 65,536 alphabet entries and 65,535 payload values. The frontier
contains at most one node per active key position. Count/key/scalar preflights
precede character scans and graph construction; limits cannot be loosened by a
caller. Both frontend policy scans and shared mechanics enforce relevant caps.

Suppression preparation tightens these to 65,536 entries, 1,048,576 total
scalars, 256 scalars per key, 262,144 states, 524,288 edges and one membership
payload. The reviewed CLDR profile has seven declared lists, with a longest key
of 11 scalars; bounds leave explicit room for future source updates without
silently truncating them.

## Runtime matching

`src/intl/segment/suppression.[ch]` applies complete literal graphs to prepared
sentence decisions. It matches raw entries at original scalar positions,
including overlapping entries and internal cuts in multi-part abbreviations.
Ignored characters inside a literal do not disappear. Its graph owner is the
[section 41 reader](intl-segment-suppression-format.md); native locale admission
and final grapheme protection remain separate integration steps.

At most `M` cursors are live, where `M` is the checked longest key length. A
terminal records a full-match end and the furthest end for its start. A monotonic
interval sweep recognizes candidates strictly inside complete matches without
revisiting every covered position per match. Work is `O(n * M * log D)`, for `n`
scalars and maximum outgoing degree `D`; scratch is `O(n + M)`. There is no
arbitrary text-length or context cutoff.

Only source-qualified root SB11 soft decisions may be removed. Endpoints,
paragraph breaks and other rule roles stay unchanged. The original SATerm anchor
survives ignored text, then Close characters before the first space, then spaces
with ignored text between them. A paragraph separator invalidates the anchor,
including through following ignored text. A full match ending at that original
anchor can remove the following soft candidate; a complete entry spanning an
internal candidate supplies its own context. Case folding and whitespace
stripping are not part of this policy.
Paragraph separators also terminate literal cursors: an entry spanning a hard
paragraph boundary cannot authorize removal of soft cuts on either side.

Matching and removal masks use temporary allocator-owned scratch. The scan's
break vector changes only after the whole pass succeeds; OOM or invalid input
cannot publish partial removals. No state or allocation is retained by the
filter, and original UTF-16 mapping stays with the decoded text owner.

Sources reviewed 2026-10-23: CLDR 49 common release and matching Git commit
`96e54b273d2da4ec6fbad350ba19ff23fa24a588`, `common/segments/*.xml`, and
[UAX #29 revision 49: sentence boundaries](https://www.unicode.org/reports/tr29/tr29-49.html#Sentence_Boundaries).
CLDR's literal-list role is described in
[TR35 segmentation suppressions](https://unicode.org/reports/tr35/tr35-general.html#Segmentation_Suppressions).
