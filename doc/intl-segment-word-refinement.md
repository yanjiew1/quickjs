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

# Native dictionary word refinement

`src/intl/segment/dictionary-refine.[ch]` refines compiled native word decisions
using validated dictionary DAGs, Unicode routing caches and a matching native
grapheme scan. It changes only admitted boundaries and publishes a complete
result transactionally. Native provider admission remains separate.

## Runs and legal endpoints

Route complete native grapheme clusters, preserving raw scalars and marks. A
cluster needs a Letter core and compatible Script/Script_Extensions evidence.
Map evidence to dictionary families before checking conflicts: Han, Hiragana,
Katakana and shared Japanese letters may all be CJK-compatible. Unrelated
letters, incompatible marks and non-Letter/Mark scalars are conservative
barriers. Dictionary keys containing punctuation remain valid data; this policy
does not activate them across punctuation-only clusters.

A mark without one of the selected family candidates is neutral only when its
Script_Extensions contains Inherited. No candidate can also mean an uncached
foreign script or a restricted inherited extension set. A neutral mark may
attach inside a letter cluster; a mark-only cluster does not become a core.
Routing uses the shared Unicode owner's ranges, not a second generated script
database or an exact Line_Break=SA classification.

An interior endpoint must be both a native grapheme boundary and mutable under
the prepared word program. Mutable cases are an implicit default break or an
explicitly approved lexical no-break rule. All other roles remain protected.
A protected break splits a run; a protected no-break may be traversed by a word
but cannot end one, including at a leaf terminal. Adjacent eligible clusters of
one family may form a run across several default base segments, enabling CJK
dictionary words to join default cuts.

Run endpoints are virtual path vertices. They never force a break next to
outside ignored text or a different family. Preserve endpoints and all
non-grapheme/protected decisions throughout refinement.
The final native-input pipeline separately intersects with grapheme decisions;
this helper never introduces a cut inside a cluster or rewrites those base bits.

## CJK costs and Southeast Asian matching

CJK uses backward dynamic programming with additive raw uint32 terminal costs
and checked uint64 totals. Unknown progress costs 255 per traversed native
cluster and reaches the next legal endpoint. The unknown path supplies a finite
upper bound; an overflowing known candidate cannot win. A run too long for the
unknown cost bound returns OVERFLOW. Choose lower total cost; equal costs prefer
a known edge, then a longer known edge. This is a deterministic local policy,
not a claim of matching every ICU heuristic.

The four Southeast Asian families greedily choose the latest legal complete
terminal. A failed longer prefix retains an earlier complete match. Unknown
progress reaches the next legal native-cluster endpoint; it cannot split a
surrogate pair or grapheme or stop at a protected no-break.

Begin with copied base decisions. Within a selected known word, remove mutable
cuts and set its legal interior run endpoints. Unknown-to-unknown endpoints
leave their original bits unchanged. Entirely unknown Han therefore retains
default cuts, while an unknown lexical run retains its base joins. Unknown
steps cannot silently create joins or arbitrary cluster-by-cluster cuts.

## Bounds and ownership

Decode and route each cluster once. Precompute the next legal endpoint once,
so a long protected chain does not cause repeated whole-chain searches.
Each dictionary lookup walks at most its checked maximum key scalar length,
even when one grapheme contains a very long mark tail. Refinement uses bounded
linear scratch plus dictionary lookups; it does not normalize or omit raw text.

All source/program/cache views are borrowed during the call. Scratch contains
cluster metadata, paths and copied decisions. Only after every run succeeds
are the original scan bits updated. OOM or a missing later dictionary releases
scratch and leaves all original decisions unchanged. Final prepared inputs
must copy boundary/word-status results and retain none of these borrowed views.
