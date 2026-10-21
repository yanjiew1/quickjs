# Unicode boundary properties

Text boundary algorithms use the engine-independent property reader in
`src/unicode/break-properties.c`. This layer supplies data; it does not implement
Intl locale negotiation, CLDR tailoring, dictionary segmentation or JS objects.

## Sources and regeneration

`tools/unicode_download.sh` downloads the Unicode release identified by
`LIBUNICODE_UNICODE_VERSION_*`, over HTTPS. `tools/unicode-data.sha256` pins the
inputs. Boundary data comes from:

- `auxiliary/GraphemeBreakProperty.txt`;
- `auxiliary/WordBreakProperty.txt`;
- `auxiliary/SentenceBreakProperty.txt`;
- the enumerated `InCB` field of `DerivedCoreProperties.txt`;
- `Extended_Pictographic` in `emoji-data.txt`, including reserved code points.

The downloader also acquires the three matching `*BreakTest.txt` files for
algorithm conformance testing. Source files stay in the download/build directory.
Meson's `unicode_data` option regenerates the checked-in `libunicode-table.c`
and its declarations. The Unicode CI job verifies source hashes, compares the
generated files, and checks the compiled reader against every source code point.

## Table representation

Each property word is a `uint16_t`, with fixed IDs from
`break-properties-def.h` and field positions from `break-properties.h`:

| Bits | Property | Default |
| --- | --- | --- |
| 0–3 | Grapheme_Cluster_Break | Other |
| 4–5 | Indic_Conjunct_Break | None |
| 6 | Extended_Pictographic | false |
| 7–11 | Word_Break | Other |
| 12–15 | Sentence_Break | Other |

Three arrays share the existing single generated C translation unit:

- `unicode_break_starts`: ascending `uint32_t` code points where a word changes,
  beginning at zero;
- `unicode_break_values`: the corresponding `uint16_t` words;
- `unicode_break_index`: `uint16_t` indexes of the last transition at or before
  each 1,024-code-point bucket boundary, plus a terminal bucket boundary.

Lookup bounds a binary search using adjacent bucket entries. Parallel arrays
avoid padding each transition to eight bytes. Unicode 18 uses 4,855 transitions
and 1,089 bucket entries: 31,308 bytes in total. These counts are observations,
not fixed limits; generation rejects an index that would require wider entries.

The generator rejects unknown values, malformed/overlapping ranges and changed
`@missing` defaults. Enumerated InCB remains distinct from the existing binary
union property. These tables do not add property names to ECMAScript RegExp.
Lookup accepts lone surrogate code units and returns the neutral word outside
the Unicode code-point domain; UTF-16 decoding and original offsets belong to
the boundary algorithm.

## Algorithm version

Use [UAX #29 revision 49](https://www.unicode.org/reports/tr29/tr29-49.html),
the Unicode 18 rules, when consuming these tables. In particular, GB9c changed
from Unicode 17. Importing an older implementation without updating its rules
does not establish conformance to the pinned data release.

## Default grapheme clusters

`src/unicode/grapheme.c` implements the default **extended** grapheme rules.
Call `unicode_grapheme_next()` starting at zero, then at each returned boundary.
Positions are offsets into the original UTF-16 sequence; valid surrogate pairs
remain intact and lone surrogates retain their original units. The function
does not allocate, normalize, or apply locale tailoring.

The scan keeps the previous GCB class, RI parity, Indic-linker context and
emoji-ZWJ context. Context is updated as input is consumed rather than recovered
by repeatedly scanning backward. Iterating the whole input is linear even for
long flag, combining-mark or ZWJ sequences. Case conversion shares the same
bounded UTF-16 decoding helper without sharing its algorithm-specific state.

`test_unicode_grapheme` contains quick UTF-16/context/stress regressions. Passing
the official `GraphemeBreakTest.txt` path additionally checks every source case,
including the absence of boundaries inside supplementary pairs. Unicode CI
runs that conformance mode against the same pinned release as the tables.

## Default word boundaries

`src/unicode/word.c` supplies the default word rules with the same forward,
original-UTF-16 boundary contract. It distinguishes raw adjacency for WB3c and
WB3d from the significant-character context after WB4 ignores. Punctuation
rules use bounded additional lookahead across an ignored run; RI parity is
updated without scanning backward. Full iteration is linear and allocation-free.

These positions alone do not identify meaningful words, apply locale rules or
split dictionary-script text. Those decisions belong to the segmentation service.
`test_unicode_word` exercises punctuation, ignored characters, Hebrew quotes,
surrogates and long runs; Unicode CI additionally checks the pinned
`WordBreakTest.txt` through the same boundary-vector oracle used for graphemes.

## Default sentence boundaries

`src/unicode/sentence.c` keeps raw paragraph adjacency separate from the
significant context after ignored characters. It retains a terminal's Close/Sp
prefix and caches SB8's lowercase lookahead while that prefix remains active.
This prevents repeated scanning of an arbitrarily long punctuation/space run.
The scan is allocation-free and preserves the same UTF-16 boundary contract.

The default full-stop rules recognize numeric, case and continuation contexts;
they do not incorporate locale abbreviation lists. `test_unicode_sentence`
covers those contexts, paragraph ordering, lone surrogates and long prefixes.
Unicode CI also checks every pinned `SentenceBreakTest.txt` case using the shared
boundary-vector oracle.
