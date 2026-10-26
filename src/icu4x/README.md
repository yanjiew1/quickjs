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

# ICU4X bridge

This private Rust static library adapts pinned ICU4X public APIs to the engine's
plain C provider contracts. It is optional; ordinary native/disabled profiles
do not need Rust. `Cargo.lock` records exact crate checksums. Sources are fetched
by Cargo rather than copied into the repository.

The calendar feature uses ICU4X 2.4.0, whose minimum Rust version is 1.88. Use
`cargo test --manifest-path src/icu4x/Cargo.toml --locked --features calendars`
for the bridge's direct unit tests. Set `CARGO_TARGET_DIR` to a build directory;
do not put generated objects in the source tree. `--offline` works after the
locked dependencies have been fetched.

## C boundary

- Calendar IDs mirror `QJSCalendarId`; ISO arithmetic remains in C core.
- Arguments and calendar results are fixed-layout C records. Formatters use
  owned opaque handles with explicit close functions. Rust references, strings
  and enum layouts never become C-visible values.
- Calls publish outputs only after success, including caught Rust panics.
  Borrowed Japanese era names have process lifetime; failed initialization is
  retryable. Concurrent initialization cannot publish a partial record.
- Metadata years are deliberately limited to ±900000, safely covering all
  Temporal dates without bypassing ICU4X's fundamental date bounds. Conversion
  validates roundtrips to detect a foreign date clamp.
- Month addition delegates to ICU4X's public date arithmetic, including its
  own distant Chinese/Dangi model. It does not assume the native provider's
  astronomical mean phase or iterate over days in C.

ICU4X's internal infallible allocations can abort on OOM. Catching a panic is
not a promise that every Rust allocation failure becomes a recoverable engine
exception. Engine-owned buffers still follow the ordinary allocator contract.

The Meson build collects the resolved crates' full license notices and the
selected Rust standard-library copyright/license notices alongside the
companion archive. Retain those when redistributing linked binaries.

The C provider uses the shared required publication records for Chinese
1900–2100 and Dangi 1900–2050, because ICU4X's approximation can disagree with
an official row (Chinese 2057). Other dates retain ICU4X arithmetic. Every
required year's month count agrees with the pinned ICU4X month sequence;
repository tests check this, official seams and month-addition roundtrips.
Large lunar month additions traverse calendar years in the upstream API;
ordinary and large-delta behavior have separate regression coverage.

## Timezone identity

The `timezones` feature uses ICU4X's real `IanaParserExtended` and
`WindowsParser`. The same compiled Windows payload provides a bounded
case-insensitive fallback for the private host helper. All 505 CLDR 49 Windows
rows were cross-checked against the existing country-aware IANA policy; modern
alias spelling differences preserve the same primary identity.

ICU4X supplies no exact transition engine. The C provider explicitly uses the
shared IANA TZif companion for rules, admission and country-aware primary IDs.
IANA 2026e and CLDR 49 match the selected ICU4X data release. Obsolete ICU-only
names are rejected; `Factory` is a valid current IANA zone supplied by that
companion, not a substitute for unknown input. Raw DST flags come from TZif,
independently of ICU4X's display-name variants.

The private List provider uses ICU4X's actual list-marker inventories and
contextual patterns. Its parts sink copies original UTF-16 elements directly,
including empty strings, NUL and lone surrogates; only contextual inspection
uses replacement scalars. C allocates final text and parts in two checked
passes. Rust library allocations retain the bridge's documented foreign-OOM
policy. This module is a foundation for the complete Intl profile; adding it
does not silently change the currently selected engine backend.

Locale casing delegates to ICU4X CaseMapper. Maximal valid UTF-16 runs are
converted with fallible temporary allocation; original lone surrogates remain
uncased boundaries. A persistent UTF-16 buffer joins scalar writes before the
C callback, preserving order and exact callback errors. The C adapter checks
output bounds before allocating its owned result.

## CLDR metadata supplement

ICU4X public data does not contain complete Unicode/transformed key aliases,
allowed hour-cycle lists, ordered calendar preferences or minimum week days.
The narrow supplement in `data/intl/icu4x-locale.c` supplies these facts, four
listed-unknown script identities, and a declared CLDR locale registry for global
segmentation rules. Formatting data and base alias/likely-subtag algorithms
remain in ICU4X. Registry membership alone is not executable service coverage.

Use the existing data front door to acquire and regenerate compatible sources:

```sh
python3 tools/data/acquire.py cldr --cache /path/to/cache
python3 tools/data/generate.py intl-icu4x --sources /path/to/cache/cldr-49 \
    --output data/intl/icu4x-locale.c
```

Acquisition uses `data/intl/pin.json`; `--source-pin` selects an alternative pin.
Generation accepts supplied local sources without checking acquisition hashes.
When upgrading CLDR, update its pin and the matching ICU4X data dependencies;
this supplement's declared version must match the selected ICU4X CLDR version.
See the existing data maintenance guide for pin/source upgrade commands.

The private layout is documented by `src/intl/icu4x/locale-data.h`. Fixed-width
rows reference one interned NUL-delimited ASCII pool by 32-bit offsets. The C
compiler supplies target alignment and endianness, so this is compiled data,
not a versioned wire format. One translation unit owns all arrays. The production
CLDR 49 payload is about 60 KB before linking. `tests/test_icu4x_metadata.py`
checks actual compiled fields and string escaping using a small authored fixture.

## Locale identity

The private `locales` feature delegates base aliases and likely subtags to
ICU4X, and borrows the matching compiled supplement for key/type aliases and
region preferences. Admission verifies its declared CLDR version and preferred
chains once. Source-type and territory queries then return immutable borrowed
text without allocation. Timezone metadata preserves the source territory
before consumers resolve an IANA primary identifier.

A narrow extension adapter preserves embedded `true` values which ICU4X 2.4's
public parser otherwise drops. Only an entire Unicode value `true` is elided;
transformed values retain it. Public ICU4X APIs still canonicalize the base,
other extension kinds, region/subdivision aliases and transformed language.
See TR35 Annex C, Definition 5; this is not a replacement language-tag parser.

`tests/icu4x/locale.c` exercises the real compiled C/Rust layout, identity,
source facts, callback failures and engine-owned allocation failures. CI runs
it with combined ASan/UBSan on both the minimum and stable Rust toolchains.
As with the other bridge foundations, this does not yet expose a complete
`intl=icu4x` runtime profile. Its final default candidates must be admitted by
all selected formatting modules rather than inferred from metadata membership.

## Segmentation

The private `segments` feature uses ICU4X's locale-aware word and sentence
constructors, automatic complex-script word models, and grapheme rules.
The matching CLDR registry defines locale candidates; admission verifies all
three real constructors for every candidate and checks the data version.
A root-only rule marker and success for an arbitrary language tag are not used
as proofs of available locales.

Preparation enumerates original UTF-16 boundaries and preceding-segment word
statuses, then copies them into one engine-allocated immutable array. Queries
share the provider-independent binary search with ICU4C, allocate nothing and
retain no foreign iterator. Prepared inputs may outlive their segmenter while
the caller keeps the original UTF-16 text alive. Tests cover Greek sentence
tailoring, Thai words, callback aborts, lone surrogates/NUL and all engine-owned
constructor allocation failures.

## Exact plural source data

The private plural provider snapshots public ICU4X rule serialization and
category range maps. Borrowed rule spans remain valid until that handle closes.
The common C evaluator consumes these rules with exact decimal operands;
ICU4X's bounded integer/fraction operand representation is not used. This is the
explicit shared exact-evaluation exception: all rules and range results still
come from the selected ICU4X library, never the native Intl pack.

The compiled CLDR registry is shared metadata rather than Segmenter-owned data.
Its membership defines candidates; each consuming module must independently
verify its required public data/operations. The plural contract checks every
exported relation, plus large Arabic integer and late-fraction examples,
visible zeros, negative notation exponents and Slovenian category ranges.

The C provider uses common exact rounding once on the unscaled value, then
adds scientific/engineering or compact exponent metadata. Compact thresholds
come from ICU4X's public decimal formatter; no foreign formatter rounds the
value again. Formatter snapshots own their foreign rule handles independently
of the source service. This provider still awaits final profile composition.

To run the private C contracts separately from a full engine build, build the
locked Rust archive with `calendars,timezones,lists,casing,locales,segments,plurals,decimals,names,currency_names,currency_numbers`,
then invoke `tools/ci/test_icu4x_contracts.sh compile ARCHIVE OUTPUT_DIR` and
`tools/ci/test_icu4x_contracts.sh test OUTPUT_DIR` from the repository root.
Compilation uses combined ASan/UBSan; the two modes preserve separate CI build
and test steps. All generated executables stay in the supplied build directory.

## Public display-name foundation

The private `names` feature adapts ICU4X's public heavy language/script and
light region names. It uses the library's localized language composition and
dialect/standard distinction. Short names use ICU4X's own baked fallback policy;
narrow currently shares that short representation. ECMA-402 leaves these
localized strings implementation-defined, so they need not equal the native
provider's source-selection policy.

Language composition can report a missing qualifier after producing a known
prefix. A discard-only preflight detects absence before invoking the bounded
C sink. Missing is never inferred from code-identical or empty output. Errors
retain their original callback status, and returned text uses buffered counted
UTF-16 output. The C DisplayNames provider adds standalone/context handling and the narrow
calendar/date-field supplement described below. It does not replace ICU4X
language-name composition or insert standalone script labels into qualifiers.

The optional `currency_names` feature uses public ICU4X experimental 0.7.0
plain-name payloads, from the same pinned release revision and CLDR version.
Its sorted currency inventory comes from those actual payload IDs, including
plain-name-only codes such as Chuvash XAD. Currency symbols, plural unit labels
and ISO digit metadata cannot establish DisplayNames name presence. Payload
lookup retains public locale-preference fallback and distinguishes absence
from an empty or code-identical name.

The same small generated provider owns both currency enumeration and lookup.
It enables only the matching marker data and locale fallback dependencies;
the experimental crate's broad `compiled_data` feature is deliberately unused.
This avoids enabling unrelated providers solely to read plain currency names.

## Display supplement

Generate the missing display facts with the same entry point:

```sh
python3 tools/data/generate.py intl-icu4x --component display \
    --sources /path/to/cache/cldr-49 --output data/intl/icu4x-display.c
```

This component supplies standalone script alternatives, calendar labels, twelve
date-field labels and standalone capitalization flags for language, region,
script and calendar names. Plain currency/date-field names retain formatting
context. The ordinary public ICU4X name data and its short-name policy remain
in ICU4X. Do not insert standalone script names into language qualifiers.

Projection uses the original CLDR source layers before interning final maps.
A nearer ordinary script name suppresses a parent's standalone alternative;
blocked alternatives remain blocked and explicit empty names remain present.
Calendar lookup keys include valid public aliases and deprecated names, since
CanonicalCodeForDisplayNames lowercases without alias canonicalization. API
fallback still uses the original core code.

`src/intl/display/icu4x-data.h` documents the private compiled layout: one
NUL-delimited UTF-8 pool, label maps and locale-to-map indexes. Label records
contain code/text offsets and long/short/narrow presence bits (1/2/4); equal
text across widths shares one row. Missing has no row/bit, while an explicit
empty label has text offset zero. Locale capitalization bits follow the C
display type enum. Version is a CLDR string offset, not a wire-format version.
The target compiler supplies alignment/endianness; no runtime pack decoder is
introduced. Compatible source edits are accepted without acquisition-hash checks.

The C DisplayNames provider admits the intersection of actual public name
payload locales and the matching supplement registry. Its currency inventory
is restricted to plain names reachable from those admitted locales. The
common inventory helper only captures stable sorted identifiers; each module
still owns its domain and admission policy.

Standalone capitalization uses the selected ICU4X lowercase property and
locale-sensitive titlecasing with trailing text unchanged. The supplement's
declared CLDR version must match the selected library. Formatter handles own
their preferences and borrow only immutable process-lifetime supplemental
rows, so they remain usable after the service closes. Output uses two bounded
UTF-16 passes with one final engine allocation; missing, explicit empty and
code-identical labels remain distinct. Private C contracts cover all six name
types, constructor/result allocation failures, output bounds, aliases and
formatter/result lifetimes. Full JavaScript profile composition is still pending.

## Exact numeric source data

The private decimal adapter wraps the public decimal constructor with a data
inspection provider. ICU4X still resolves locale and numbering-system fallback;
the wrapper retains only successful symbol/digit payloads and the actual digit
identifier. An explicitly requested unsupported or algorithmic system is
rejected instead of silently accepting fallback digits. Borrowed symbol spans
remain valid until their owning handle closes.

The common C numeric body consumes this data and an already rounded exact
decimal. It does not pass mathematical inputs through ICU4X's i16 magnitude
representation, so large integers remain exact. Grouping metadata preserves the
locale's minimum for auto grouping, while the common formatter implements the
separate ECMA always/min2 policies. This is a numeric-data foundation, not yet
the full NumberFormat provider: nonfinite/scientific/range and outer style
composition remain separate work. The contract includes a 40,000-digit input
with supplementary-plane Adlam digits and UTF-16 part offsets.

The low-level public-payload inventory comes from the same generated
provider used by actual ICU4X queries. Tests construct every advertised
system and verify its resolved identity. Explicit symbol locale IDs are not
used as an available-locale list: they omit locales legitimately inheriting
root symbols. Complete NumberFormat admission must validate its declared locale
candidates across all required operations.

The separate `currency_numbers` feature reads public regular currency fraction
data and short/narrow symbol payloads. It does not enable ordinary display-name
or casing data merely to query numeric metadata, and does not enable the broad
experimental compiled-data feature. Standard digits remain distinct from cash
digits/rounding. Unknown symbols remain absent for core code fallback; explicit
empty symbols remain present for the composing formatter's no-currency pattern.
Symbols retain the library's letter-boundary flags for currency spacing. The
query never formats an amount or changes resolved numeric precision.

## Number composition supplement

Generate the remaining number facts with `tools/data/generate.py intl-icu4x
--component number --sources /path/to/cache/cldr-49 --output
data/intl/icu4x-number.c`. The component reuses the existing CLDR symbol,
pattern and layout collectors. It supplies nonfinite/exponent/atomic sign
symbols, range/approximation templates and typed outer affixes; the public
ICU4X formatters do not expose all required tokens and would re-round some
amounts. Public ICU4X numeric data remains preferred; regular currency precision/
symbols and exact plural rules also remain in ICU4X. Required numeric systems
absent from the public bake use the compact matching CLDR fallback below.

The private layout in `src/intl/number/icu4x-data.h` uses one UTF-8 pool,
shared field records, shared system-index vectors and deduplicated typed
affixes. String offset zero means explicit empty; reserved references
distinguish absent, blocked and explicitly empty patterns. Quoted literal
hyphens remain distinct from semantic minus-sign tokens. Numeric skeleton
defaults, including LDML exponent width/plus signs, do not override ECMA
exponent/precision rules or public ICU4X grouping. The arrays
are compiled once for the target, without a runtime pack decoder or wire
version. Locale rows provide source coverage, not executable service admission.

The initial private NumberFormat service combines these sources with common
exact rounding, numeric bodies and typed affix composition. It currently
covers decimal/percent styles, standard/scientific/engineering notation and
source-preserving ranges. It admits declared locale candidates through their
actual public constructor/default system; tests execute every admitted default.
Resolved symbol text is copied into one C-owned UTF-16 arena and temporary
Rust handles are released during construction. Formatters/results outlive the
service. Currency/unit styles and compact composition remain to be completed
before enabling the JavaScript ICU4X profile.

ECMA-402 Table 30 requires all 78 numeric systems in the selected draft, while
the ICU4X 2.4 bake contains 23. The number companion therefore carries the
complete CLDR numeric digit map (about 3 KB) and fallback decimal/group
separators and grouping sizes. Retaining the tiny complete map avoids another
hand-maintained library capability pin. Public payloads are preferred for
systems they actually provide; the remaining 55 use the shared exact body.
Fallback follows the requested system's resolved CLDR data, not the locale's
default system: Persian with mathematical digits must not accidentally inherit
the default Arabic-extended numbering separators. Algorithmic systems remain excluded.

Currency-specific decimal/group symbols are queried through public
`CurrencyDecimalSymbolsV1`, preserving their numeric grouping metadata. The
bake can share a default-system record under an unqualified currency key;
an explicit request reuses it only when its reported system matches exactly.
An absent override is distinct from an unsupported currency and leaves normal
symbol selection to the caller. Portuguese PTE is covered independently for
its empty currency symbol and its `$` decimal separator. No amount is formatted
or rounded by this metadata query.

## Private collation bridge

`collations` uses the selected ICU4X collator and normalizer payloads directly.
Named collation support comes from the actual metadata inventory. Its public
constructor normally retries a failed named request with the default tailoring;
a small provider guard prevents that retry for metadata, tailoring and
reordering. It does not inspect private metadata bits or reject legitimate
root/normalizer/diacritics requests.

The stock bake omits CLDR search tailorings. The explicit implementation-defined
search policy uses German phonebook tailoring for German locales (also the
German-specific component of CLDR search), and ordinary locale tailoring
elsewhere, honoring all explicit options. This is not complete CLDR root search
support. Search still reports semantic `default` collation. Canonical
normalization and counted UTF-16 comparisons stay in ICU4X; malformed surrogate
units receive U+FFFD weights, as permitted by UCA's ill-formed-input policy.

## Private relative-time metadata

`relatives` retains the public ICU4X experimental 0.7.0 payloads for eight
units and three widths. It exposes automatic labels and grammatical pattern
affixes, keeping placeholder items distinct from literal braces. It does not
send amounts through ICU4X's bounded Decimal or high-level relative formatter.
Exact numeric rounding, plural classification and unit-tagged numeric parts
belong to the shared C modules; all relative labels/patterns still come from
the selected ICU4X data. Both Number zero signs use automatic offset zero;
numeric tense keeps the original sign independently.

The C adapter composes those affixes with the selected NumberFormat and
PluralRules interfaces. It snapshots the 96 unit/tense/category choices at
construction, retaining their payload owner, so formatting does not allocate
foreign pattern selectors. Auto lookup precedes rounding; numeric output and
plural operands share the exact binary64 magnitude and matching digit policy.
No additional relative-time data supplement is needed.

## Unit composition supplement

The `unit` supplement contains only shared strings/locales, sanctioned unit
source layers, reachable alias targets and plural/case division grammar. It
omits display names, gender, native plural rules and unrelated Intl sections.
The C composer uses source layers to preserve blocked/empty overrides,
selector origins and complete-expression fallback; public ICU4X unit payloads
can supply decoded ordinary pattern affixes through the shared materializer.
The canonical single/precomposed inventory is `intl/unit/identifiers.def`.

Regenerate from compatible supplied CLDR files with:

```sh
python3 tools/intl/icu4x_metadata.py --component unit --sources CLDR_ROOT \
    --output data/intl/icu4x-unit.c
```

Generation does not authenticate an acquisition pin. The embedded QJIN content
identity is descriptive; the selected ICU4X CLDR version must match the separate
version label. The existing unit codecs/readers own the layout, not a second
ICU4X-specific runtime parser.

The private `units` feature retains public Core/Extended/Outlier payloads and
projects six grammatical categories to borrowed prefix/suffix spans. It accepts
only the source locale chosen by the C overlay resolver, checking all tiers
before declining a source. It never replaces a missing exact source with an
inherited payload. A successful category query can still use ICU4X's `other`
fallback; the C materializer must preserve companion leaf/case provenance.
Literal-only patterns and literal braces remain distinguishable from numeric
placeholders. The guarded C bridge contract checks all sanctioned singles and
widths, a later-tier exact match, output preservation and payload lifetime.

NumberFormat now composes every sanctioned unit through the shared C unit
selector. Its formatter retains each reachable source's public plural rules
and exact-source unit payloads before formatting. A public affix replaces a
companion leaf only when the decoded representation matches; public `other`
fallback cannot invent a missing category or grammatical case. Compact labels
use virtually expanded rounded operands; scientific/engineering use the
visible mantissa. Percent-as-unit does not scale the amount by 100.

## DurationFormat

The private `durations` feature supplies the public digital-duration separator
and raw hms hour width. The C adapter quotes decoded literals and hands an LDML
pattern to the existing duration parser. Shared service admission intersects the
selected Number/List inventories; the common composition preserves exact
fractional amounts, sign placement and typed parts. Raw padding never overrides
resolved text-hour options. No separate duration supplement is needed.

Provider contracts execute every admitted locale and all four styles, in
addition to fractional nanoseconds, negative durations, Danish separators,
allocation failures and formatter lifetime. This remains private until the
complete DateTimeFormat provider enables the full ICU4X profile.

## Date-program companion

ICU4X semantic skeletons do not expose the complete style, available-format,
append and interval catalog used by the shared C date service. The scoped
`data/intl/icu4x-datetime.c` companion retains those original programs, precise
day-period rules, calendar-name fallback and timezone presentation facts. Its
region-name section leaves unrelated display-name columns empty. It contains
no native collation, segmentation, lists, units or plural rules.

The CLDR 49 binary payload is 14,673,144 bytes; its one generated C owner is
about 60 MB of source. It reuses the existing layouts and readers documented
in `doc/intl-data-format.md` and `doc/intl-data-sections.md`. Regenerate with:

```sh
python3 tools/data/generate.py intl-icu4x --component datetime \
    --sources CLDR_ROOT --output data/intl/icu4x-datetime.c
```

The production CI regenerates every ICU4X companion and compares it with the
repository owner. A private sanitizer contract admits the actual date payload
through all required C readers and checks temporary allocation failures. These
source views alone do not enable a complete ICU4X DateTimeFormat provider.

## Private date-name payloads

`datetimes` retains public year, month, weekday and fixed day-period payloads
for exact source identities selected by C. It maps LDML widths through ICU4X's
semantic attribute helpers: LDML wide 4 uses ICU attribute 5, narrow 5 uses 4.
Indexed text excludes encoded month-pattern stores and flexible-rule headers.
Decoded month patterns keep ordinary/leap/base phases and numeric offsets
separate; Hebrew offsets cannot be applied blindly to C ordinal months.

The generic decoded-pattern projection is shared independently of plural
category selectors. The boundary contracts cover wide/narrow and standalone
forms, retained spans, parent-only rejection, Chinese leap affixes and Hebrew
literal Adar I/6a/6b forms. Canonical flexible day-period labels and exact
minute rules remain explicit matching-CLDR companion gaps.
