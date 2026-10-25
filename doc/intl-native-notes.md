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

# Native Intl decisions and interpretation notes

These are durable reasons for choices that are not obvious from the C contracts.
They complement [the architecture](intl-design.md), not a delivery checklist.
Wire layouts belong to the [data format references](intl-data-format.md);
segmentation algorithms have their own
[execution](intl-segment-execution.md),
[word refinement](intl-segment-word-refinement.md) and
[suppression](intl-segment-suppressions.md) guides.

The specification observations below were reviewed against ECMA-402
`730ca2b88c305862db7be2f83c510dcca33ab48c` and the unmerged era/monthCode
[PR 1044](https://redirect.github.com/tc39/ecma402/pull/1044), revision
`d026068ed653130a37d79a95c0122ecf5319f1a4`, on 2026-10-19 unless stated
otherwise. These identify the reviewed text, not a claim that an editor issue
remains unchanged forever. Recheck the applicable living draft or Stage 4 PR
before altering the behavior. Source comments and regression tests accompany
the affected operations.

## Source identity, selection and capabilities

Exact source readers preserve missing, blocked and explicit empty values.
Consumers choose fallback separately. An alias can change origin locale,
calendar, context or width; catalog indexes and alternative IDs belong to their
own catalog. Localized names do not establish arithmetic support or formatter
availability. Source-only calendar identities such as `generic` remain distinct
from sanctioned arithmetic calendars.

Canonical type projection uses metadata equivalence, not coincidental catalog
order: `gregory` reaches `gregorian`, `iso8601` keeps its own source inventory,
and historical Japanese presentation can use Gregorian labels. Raw era IDs are
explicit CLDR metadata; do not derive them from era order or create named eras
from Chinese/Dangi sequential epoch records.

Locale matching returns the matched request and its Unicode extension. The
shared `locale/resolve` operation resolves all relevant keys before composing
one final locale; repeated single-key composition would lose other retained
keys. Null, absent options and empty Boolean keywords differ. Canonicalize
aliases before SameValue/membership; propagate callback errors. Result strings
use the requested output allocator and can outlive the identity service.

`Locale.getCalendars()` filters the first selected source preference list through
DateTimeFormat capabilities without restarting regional preference selection.
An explicit calendar slot bypasses that filtering; an empty filtered list falls
back to Gregorian. Generic `islamic` is accepted for negotiation and resolves to
`islamic-tbla`, without being advertised as an available arithmetic calendar.
`getTimeZones()` uses the explicit region, not maximization or `rg`/`sd`/`tz`
overrides. Determine source territory before resolving links, then deduplicate
and sort supported primary identifiers. No region gives undefined; an unknown
explicit region gives an empty array.

## Mathematical values, rounding and plural operands

The binding's ToIntlMathematicalValue performs one number-hint ToPrimitive.
StringNumericLiteral conversion establishes grammar and specified infinity/zero
boundaries without replacing a finite nonzero exact decimal/radix value by a
rounded double. BigInts retain their exact values. Shared digit-option code
preserves property access/coercion order and absent versus effective defaults.

Plural evaluation consumes unsigned plain decimal text, including visible zeros,
without binary floating point or exponent-sized storage. Its shifted entry point
uses two integer spans and a virtual zero tail; `1.20050c3` has the operands of
`1200.50`, including `v=2`, `w=1`, `f=50`, `t=5`. The decimal shift is independent
of signed `c/e` metadata. Do not replace this with a shortest-double conversion.

PluralRules rounds the original mathematical value once, rather than reuse
NumberFormat's notation scale/round pipeline. Nonfinite values select `other`;
NaN range endpoints are errors. Equal rounded strings select the individual
category before CLDR range lookup. Native notation metadata uses zero for
standard/zero/fallback, decimal magnitude for scientific, floor-to-three for
engineering, and the selected standard-`other` divisor for compact. Missing
plural-range pairs use CLDR 49's end category.

### RelativeTimeFormat draft inconsistencies

The reviewed `PartitionRelativeTimePattern` passes signed `real(value)` into
number formatting even though past/future already expresses direction. Native
inserts unsigned magnitude, matching established behavior and negative-value
Test262 cases. Negative zero still selects past tense; automatic literal lookup
precedes rounding.

The same text passes Number into ResolvePlural after that operation changed to
Intl mathematical values. The directly reviewed
[PluralRules change](https://redirect.github.com/tc39/ecma402/pull/1026), merged
as `7344f4219a54dad6ecfe261d121f7463e016726c` on 2025-12-05, moved conversion
to the public `select`/`selectRange` boundaries without updating this internal
caller. Native Number and Plural therefore share the exact real magnitude.
Converting only the plural argument through a shortest public Number string
can make `1.0005` display `1` with the word `days`. This is an interpretation of
the stale caller, not an adopted editorial correction.

### Number notation and exponent localization

Scientific/engineering notation scales before rounding. Native emits the
localized separator/sign and the selected digits for **both mantissa and
exponent**, including zero and supplementary digits. This follows ICU/CLDR
presentation rather than the reviewed PartitionNotationSubPattern's ASCII
exponent text. [ECMA-402 issue 627](https://redirect.github.com/tc39/ecma402/issues/627)
tracks the missing transliteration; it is not an adopted spec correction.
This discrepancy was reviewed on **2026-10-22 (Asia/Taipei)** against the living
draft identified above, CLDR 49's
[number patterns](https://github.com/unicode-org/cldr/blob/96e54b273d2da4ec6fbad350ba19ff23fa24a588/docs/ldml/tr35-numbers.md#Number_Patterns)
and ICU's
[scientific formatter](https://github.com/unicode-org/icu/blob/ad484f3d6c036ba06193ac7c3bc2233a53ab68b8/icu4c/source/i18n/number_scientific.cpp).
The choice changes glyph selection; exponent arithmetic and rounding do not
change. UTF-16 part spans and output bounds include the localized exponent. Source
fraction defaults, exponent-plus flags and zero-padding do not override resolved
ECMAScript options; nonfinite values have no exponent body.

Compact selection fixes the standard-`other` divisor before rounding, then
selects exact 0/1 or plural affixes from the visible rounded mantissa with
`c=e=0`. Carry handling retains the final magnitude lookup. An incompatible
inherited plural divisor uses `other` without rescaling; raw pattern `0` means
ordinary notation. Literal-only forms omit numeric text/allocation. Internal
compact-name spaces/signs remain one compact part; only outer Unicode whitespace
becomes literal parts.

### Currency and unit composition

Currency names and placement templates select independently after rounding.
Names try exact 0/1, cardinal category, `other`, an uncounted name, then code;
counted names exclude terminal root, while uncounted fallback can include it.
Empty names are present; blocked fields do not resurrect that field from parents.
Template category is not changed merely because a name uses fallback.

Scientific/engineering currency names use the final visible rounded mantissa
with `c=e=0`, following the reviewed scaled-value flow. Thus zero-fraction English
`1E3 US dollar` differs from ICU's `1E3 US dollars`. Compact outer currency/unit
names instead use the expanded rounded quantity and final compact exponent,
without materializing an enormous decimal string: `1.20000c3` gives `1200.00`,
whereas `1.20c3` gives `1200`. Inner compact affixes retain mantissa operands.
These native CLDR choices govern implementation-dependent name text.

Modern `alphaNextToNumber` families replace legacy currency spacing even when
selecting an ordinary leaf. Standard families use the closest currency scalar's
General_Category L; compact prefix choice has its separate L-followed-by-marks
rule. Compact compatibility is per threshold: any usable alpha leaf suppresses
legacy gaps there. TR35 does not prescribe this fallback granularity.

Legacy families support the admitted currencySpacing predicates
`[[:^S:]&[:^Z:]]` and `[:digit:]`, rather than approximate arbitrary UnicodeSets.
Insertion requires direct number/currency token adjacency and a real adjacent
Nd scalar. Supplementary digits qualify; hanidec, NaN/infinity and notation
literals do not. Empty currency text creates no gap. Monetary symbols and
per-currency overrides remain distinct from decimal affixes and accounting.

Compact sign-envelope projection removes shared positive context at scalar
boundaries before applying a sign/accounting wrapper. Keep unsigned decorations
once, retain signed-only controls, and reject residual currency/percent tokens.
This is native composition policy, not a rewrite of raw LDML patterns.

Division units prefer a precomposed form, then perUnitPattern, then generic
layouts with derived component nouns. Each candidate uses its own source plural
rules; fixed grammatical counts do not select exact 0/1. If component extraction
cannot preserve the noun, retry the whole expression in root while retaining
the localized number. Do not hide malformed consulted templates or missing
cardinal rules with that retry. A unit named `percent` does not multiply by 100.

Number ranges initially keep complete endpoints and their part provenance,
as ECMA-402 permits. Equal displayed text receives the modern numeric
`approximatelySign` as a shared prefix. This native placement is not TR35's
sign-slot algorithm or its obsolete miscellaneous approximation template.
NaN is rejected; descending ranges and equal infinities retain their specified
handling. Do not infer that complete-endpoint fallback is a reader defect.

## List and duration composition

List expansion is iterative, preserving empty elements, NUL and lone surrogates.
Spanish conjunction/disjunction and Hebrew Script predicates retain their ICU
attribution in source and [`src/intl/LICENSE-ICU`](../src/intl/LICENSE-ICU).
Enable contextual replacements independently for pair/end templates only when
the resolved source template matches the CLDR default; one differing template
does not suppress the other's replacement. Hebrew uses the pinned ASCII hyphen.

Duration conversion is shared with Temporal without requiring its constructors.
It preserves exact integral binary64 fields and combines fractional seconds with
checked integer arithmetic before decimal formatting. Number/List composition
keeps Number-origin units on every part, including literals; digital/List
separators have no unit. Match List elements by position, not text equality.
Only the first displayed component receives a sign, and zero minutes bridge
hours and seconds. Resolved formatters/results retain independent lifetimes.

Digital hms source projection preserves two separate dequoted separators and
raw hour width. Do not substitute numeric `timeSeparator` for pattern colons;
[TR35 withdrew that interpretation](https://unicode.org/reports/tr35/tr35-numbers.html#timeseparator).
Malformed source is an error, not an implicit colon. Number localizes digits.

The reviewed living text and PR 1044 describe `TwoDigitHours` as numeric-hour
padding but unconditionally override hour style in GetDurationUnitOptions.
[Proposal issue 161](https://github.com/tc39/proposal-intl-duration-format/issues/161)
and the directly reviewed
[PR 188 comment](https://redirect.github.com/tc39/proposal-intl-duration-format/pull/188#discussion_r1570697492)
describe the conflict; merge `835300f1ee7a0a739ee2205dea2bae2657e67983`
did not add a guard. Native effective `TwoDigitHours` is false, independently
of retained raw `hh` facts; explicit `hours: "2-digit"` still pads. This preserves
explicit option behavior without presenting the inconsistency as resolved.

## DateTimeFormat preparation

DateTimeFormat retains type-specific Temporal format records and defaults,
including null/adjusted records, range-type compatibility and calendar checks.
Direct ZonedDateTime formatting throws; its locale method uses the prescribed
internal route. An effective hour cycle for Temporal records is distinct from
ordinary Date `resolvedOptions()`: expose hourCycle/hour12 only when that Date
format includes an hour, following the applicable living text rather than an
incidental shared-algorithm difference in an unmerged proposal.

The lexical compiler preserves source fields, run lengths and apostrophe
provenance independently of semantic admission. Date patterns, date/time
connectors and literal substitution templates have different grammars. Quoted
ASCII letters or braces must not become fields or substitution slots.

Native selection prefers a usable explicit `alt="ascii"` key, then ordinary
fallback if ASCII is missing, blocked, empty, malformed or unsupported. This is
cross-alternate fallback, not revival of a masked ancestor of the same key.
An inherited ASCII key can win over a nearer ordinary key; pattern and skeleton
choose alternatives independently in their own catalogs. Operational/OOM errors
propagate. Keep both variants/provenance; never normalize final whitespace.
This compatibility policy follows explicit CLDR variants, as documented in
[ICU's source conversion guide](https://github.com/unicode-org/icu/blob/ddc6b66926bfac6036144ca9902992a0d6d50af9/tools/cldr/cldr-to-icu/README.md#using-altascii-cldr-alternate-values-from-the-cldr-xml).

A CLDR-data workaround interprets unquoted week-year `Y` as calendar-year `y`
when there is a month field M/L and no week field w/W, preserving width and
punctuation. Weekday fields and quoted letters do not change the condition.
This is **not** a requirement of ECMA-402 or LDML. Raw strings, the lexical
compiler and provenance remain unchanged; apply it before projection and interval
splitting. If a remaining supported width-one Y style requires recovery, choose
a real whole y-only available pattern with compatible source/skeleton widths,
not a fabricated field relabelling. Arithmetic uses the requested calendar.

Source grouping retains every original index even when public component vectors
agree. Retain year kinds, period symbols, raw skeleton widths, month context,
zone style and literal layout. A real whole-source pair supplies normal and
12-hour programs; do not delete AM/PM punctuation to invent a complement.
Style composition uses its date width and usable atTime then standard connector,
not relative/other-width fallback. Explicitly synthesized records retain their
construction/source evidence and reproject to the final component vector.

Basic matching uses the specified finite-record scores and first equal match.
Native BestFit can adapt requested b/B widths, preserving intentional source
skeleton/pattern differences and replaying the adaptation on range operands.
Mixed date/clock candidates retain actual clock-source evidence. Source-declared
numeric/text month requests can waive BestFit's appearance penalty, so Hebrew
`yMd` can truthfully resolve to a short month name; month-only numeric requests
remain possible. Do not falsify resolved widths to satisfy a test.

Numeric fields reuse NumberFormat with grouping off and specified widths.
Two-digit clipping takes the last two Unicode code points, not UTF-16 units.
Related years retain signs; nonpositive calendar years apply the prescribed
1-year adjustment once. Hour-cycle choice overrides raw h/H/K/k; fractional
milliseconds truncate. Non-Gregorian local-time computation is implementation-defined in the reviewed
draft; native still follows its unconditional negative-year display adjustment rather than silently introducing an additional era.

Leap-month decoration applies after localized numeric formatting and clipping,
and the decorated result remains one month part. Chinese/Dangi use nominal
month codes, not ordinal positions; exact numeric/all leap templates distinguish
English `5` and `5bis`. The reviewed living FormatDateTimePattern numeric branch
calls FormatNumeric without explicitly applying this suffix (rechecked
2026-10-23). Native follows TR35's regular M/L month-pattern behavior and ICU;
record the text gap instead of claiming the numeric pseudocode mandates it.

Flexible-period rules use the complete local subsecond fact: noon plus one
nanosecond is not exact noon. Whole-list inheritance follows supplemental
simple-prefix parents, independently of main-data parents and translated names.
Formatting and selection rules remain separate. DateTimeFormat suppresses exact
midnight because it cannot distinguish the beginning of a day from the end of
the previous day: B uses its variable period, b uses AM; noon remains exact.
This follows TR35 caller choice/ICU display policy. Raw selectors can explicitly
allow midnight for other contexts; retain its data rather than delete it.

### Date ranges and spacing

Whole interval selection precedes synthesis. If dates agree, a proven factorization
can share one date and combine two full clocks using the actual fallback and
date/time connector. Recomposition must reproduce every token/literal, including
fractions, year kinds, zones and endpoint affixes. Otherwise keep complete
endpoints. Never trim equal formatted-string prefixes or split on punctuation.
See [TR35's interval algorithm](https://www.unicode.org/reports/tr35/tr35-79/tr35-dates.html#interval-format-algorithm).

Interval AM/PM spacing adapts only isolated unquoted U+202F/U+2009 where the
actual selected ASCII source proves the corresponding adjacency. Retain raw
apostrophe evidence, intersect it with the final program and include it in cache
identity. Preserve quoted/mixed literals, flexible b/B periods, range dashes and
all other punctuation. This is native compatibility policy, not output-wide
space normalization or a requirement that distinct patterns use identical spacing.

### Timezone arithmetic and localized names

DateTimeFormat retains the accepted canonical-cased named identifier, including
aliases, separately from its primary identifier and rule handle. Do not replace
the public option identity with a primary name merely to simplify name lookup.
The timezone facade owns identifiers, offsets and transitions. Localized names
use separate CLDR stable first-alias identities, metazone intervals and territory
preferences. Resolve source territory before primary-link mapping. Metazone
intervals are half-open; preserve gaps rather than revive an earlier period.
Presentation standard/daylight hints can override raw DST designation for name
classification (including Dublin), without changing timezone arithmetic.

Generic-name standard substitution requires proof of stable total offset/raw
DST over the closed +/-184-day window and compatible presentation in every
period/gap. Endpoint equality alone cannot prove that: transitions may return to
the starting state. The timezone stability primitive includes same-offset DST
changes; Temporal transition queries continue to report UTC-offset changes only.
Absent proof, use supported country/city/GMT alternatives. Geographic city
fallback replaces underscores; POSIX and Etc zones have distinct policies.

Localized GMT wraps inherited hourFormat/gmtFormat, preserving signs, bidi text,
quotes and separators. Short omits zero minutes unless seconds are needed; long
retains padded hours/minutes. Historical seconds reuse the source H/m separator.
An explicit gmtZeroFormat is authoritative; CLDR 49 common omits it, so absent,
blocked or empty zero labels use positive numeric GMT formatting rather than an
invented standalone label, following that release's TR35 examples GMT+00:00 and
UTC+0. Fixed `+00:00` retains its identifier while name styles may use Etc/GMT
names; explicit shortOffset/longOffset bypass named lookup even at zero.

All local fields come from the same checked instant, offset and calendar.
Negative instants use floor division and historical offsets retain seconds;
subsecond precision is separate from truncated display milliseconds. Date and
Temporal share default-zone discovery but retain their own specified input and
disambiguation rules.

## Collation compilation and matching

Python resolves CLDR source rules/imports; the host C compiler uses pinned
Unicode normalization and collation algorithms. Context prefixes retain their
original NFC-boundary checks before NFD, including Unicode 18's
backward-composing characters. Starred ranges require every scalar to be NFD-inert;
compatibility-only decomposition is not the same as canonical decomposition.
Source order matters. A normalization-off setting does not waive canonical
equivalence. Independent raw-root oracles must not be derived from ranked output.

Compiler stable IDs, final uint32 ranks, source primary provenance and case flags
are separate domains. Do not narrow ranked elements into the old raw16 diagnostic
format or infer a source group from a final reordered rank. Sparse root-gap
shifts count insertions strictly below a root coordinate, preserving zero and
same-gap order. The compiler's map copies expansions so later replacement cannot
mutate an earlier relation. Failure poisons a builder; discard compilation staging
rather than assume graph and map updates form one transaction.

The CE-independent matcher preserves normalized input and tracks consumption
separately. Try prefixes longest-first, then choose the longest complete suffix;
fall back when no suffix completes. Overlays replace only identical keys, so a
shorter tailoring does not hide a root contraction. Begin discontiguous extension
only from a complete mapping. Suppression preserves ordinary root singletons
while removing inherited prefixes/contractions; later tailored additions remain
eligible. Equal combined keys can
have distinct prefix splits, which remain meaningful during terminal selection.

A matched expansion drains completely before consuming another match. Empty
overlay output consumes input without root fallback; a zero CE remains an element.
Numeric mode obtains complete adjacent decimal-digit runs before digit tailoring.
Ranked iterators initialize at their final address because their matcher borrows
an embedded trie descriptor; do not copy/move a live ranked iterator.

Primary comparison is incremental; weak records are needed only while primaries
tie. Keep secondary, case and tertiary priorities distinct. Shifted handling uses
source provenance for maxVariable, then compares retained variable primaries at
quaternary using final ranks and the group permutation. Preserve Q-only ranks/alignment rather than infer differences from
CE counts. Backward-secondary reverses separator-delimited interiors, not segment
order. Identical comparison uses original NFD text, with U+FFFE below NUL and
exhaustion handled separately.
See [QJCL](intl-collation-compiled-format.md) and the owning private headers for
rank/trie layouts, borrowed lifetimes and reader bounds.

## Segmentation profile distinction

Minimal uses Unicode 18 default boundaries, while native executes the pinned
CLDR rules and dictionary/suppression refinements. The pinned CLDR root retains
the older consonant-led GB9c rule: leading `\u094d\u0915` separates there,
whereas Unicode 18 minimal joins it. The profile regression preserves that
distinction; a CLDR release label does not establish its break-algorithm version.
Both profiles classify word-like segments by Unicode Letter or Decimal_Number.

## DisplayNames selection

Style alternatives are tried within one source layer before walking parents.
For language/region/calendar/embedded-script names, short/narrow prefer short
then ordinary. Standalone scripts use standalone then ordinary for long, or
short/standalone/ordinary for short/narrow. The latter is native policy; do not
conflate menu/scope or `alt=long` with API styles. A block masks that alternative
only; explicit empty and code-identical text remain successful values.

Language dialect matching selects the longest actual available source candidate;
ties use earlier input-subtag positions, then alphabetical source key. Resolve
candidate labels before ranking, so inherited compounds can beat local generic
names. Standard mode selects the primary language. Compose unconsumed qualifiers
using actual templates and canonical input codes for missing qualifiers.
Bracket substitutions affect qualifier labels, not the base or pattern literals;
they are native locale-pattern policy, not a general Unicode nesting algorithm.
Display calendar aliases derive from BCP47 metadata rather than a second alias
set and remain independent of arithmetic support.
