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

# ICU4C implementation notes

Select `intl=icu4c` at build time; see [build instructions](../BUILDING.md#icu4c-intl).
Select matching `calendars=icu4c` and `timezone_backend=icu4c` with this
profile. The common calendar facade must provide the same complete chronology
advertised by ICU locale negotiation, including the approved extreme-date
extension. Reduced or mixed ICU profiles are outside this first full profile.
The adapter implements the same private C contracts as the native providers.
ICU types stay in provider implementation files, including the small C++
boundaries needed for public Locale, DateFormat and compact-number APIs. There is no
runtime backend registry and no dependency on native QJIN locale data.

## Ownership and composition

Each module enumerates its actual ICU locales. Metadata availability does not
prove that another module supports the same locale, and module-local indexes
cannot be interchanged. Runtime defaults use the common capability checks;
formatters snapshot their own locale and options.

Engine-owned handles, output text and parts use the supplied allocator. ICU's
internal objects use ICU's allocator and do not obey `JS_SetMemoryLimit()`.
Formatters own mutable ICU state; per-call calendar clones and immutable
segmentation results avoid sharing mutable cursors. Do not call `u_cleanup()`
while any engine, borrowed zone handle or formatter can still use ICU.

The common ECMAScript layer retains coercion, realms, exact mathematical input,
rounding options and required composition. NumberFormat uses ICU's exact-decimal
formatting APIs. RelativeTimeFormat combines ICU number formatting, ICU-loaded exact plural
selection and ICU relative-pattern resources. DurationFormat uses the shared prescribed
Number/List composition, with both underlying formatters provided by ICU.
Basic DateTime matching scores projected source records with the common
BasicFormatMatcher; ICU's best-fit pattern generator is not used as a substitute
for that algorithm.

## Source-policy differences

ICU owns its locale inventory, aliases, patterns and Unicode data. It can differ
from the native data release without indicating an adapter error. Repository
tests retain native-specific expectations separately from common semantic
checks. For example:

- English AM/PM patterns can contain a narrow no-break space instead of the
  native provider's available ASCII-space alternative.
- Basic DateTime matching chooses among actual source records. A requested
  numeric hour can resolve to `2-digit`; `resolvedOptions()` must report the
  width that formatting actually uses.
- ICU's flexible day periods check displayed minutes and seconds, and ignore
  subseconds. A bare period can therefore remain “noon” despite hidden minutes
  or nonzero subsecond fields.
- ICU and native may partition a bidirectional sign mark differently between a
  sign part and a literal. Joining parts must still reproduce the exact text.
- ICU's Spanish list matcher implements a narrower optional contextual rule
  than the native provider's reviewed CLDR refinement for grouped numbers.

These policies do not excuse required coercion order, fields, abrupt completion,
loss of input precision, or missing mandatory calendar data.

## Narrow compatibility repairs

ICU79 inserts supplementary numbering-system digits into scientific exponents
in the wrong UTF-16 order. The numeric field adapter recognizes only malformed
exponent spans consisting of real supplementary decimal digits and repairs them
in an equal-length owned copy. Healthy output remains borrowed; part and range
source offsets do not change.

The ICU Indian calendar truncates fractional negative Julian days. The calendar
adapter shifts affected dates through a positive Gregorian 400-year cycle,
delegates arithmetic to ICU, and translates back. It does not replace ICU's
calendar algorithm with the native one.

## Explicit calendar exceptions

ICU4C calendar algorithms remain the default. Two narrow, shared exceptions are
required to cover the Temporal calendar contract:

- Chinese lunar years 1900–2100 and Dangi lunar years 1900–2050 use the project's
  official PMO/KASI publication records. Stock ICU can disagree inside these
  mandatory ranges (for example Chinese 1987's leap month and Dangi 2017).
  The compact calendar publication pack has one compiled owner; this does not
  load native Intl locale data. Outside those complete lunar years, use ICU.
- At extreme Chinese/Dangi dates that ICU cannot construct, use the documented
  shared approximation beyond checked, continuous joins near ±10000. A missing
  official row or an ICU allocation/resource error is never approximation policy.

DateTimeFormat projects the same corrected calendar fields onto the unmodified
instant. It retains ICU's names, numbering, weekday, clock and timezone data.
Corrected ranges use ICU's complete-endpoint fallback pattern when the ICU
interval calendar would give different fields. Equal relevant fields still
collapse to shared parts; month identity includes its leap flag.

## External-library error limits

Status-bearing ICU operations are preferred. Collation uses `ucol_strcollIter()`
because the simpler comparison API discards internal failures. Engine-owned
allocation failures remain recoverable and have targeted tests.

Some ICU internals still cannot report all allocation failures: break iteration
can hide traversal failures, while decimal range construction and era-symbol
array setters contain unchecked foreign allocations. Installing process-wide ICU allocator hooks would
change embedding behavior and is not a safe per-runtime repair. These foreign
heap limitations must remain visible; engine OOM tests do not prove that all
ICU allocations are recoverable.

### Exact plural evaluation

ICU owns the locale mappings, cardinal/ordinal rules, categories, plural-range
pairs and compact-notation data. The common exact decimal relation evaluator
executes the ICU-loaded rules: ICU's public selector loses significant integer
and fractional operands and cannot preserve some negative engineering exponents.
RelativeTimeFormat shares that ICU-backed selector so large integers do not
select singular affixes through truncated operands. This explicit algorithm
exception does not import native rule data. Only ICU's
public resource API is used; bundle schemas are validated because they are not
publicly stable interfaces.

### Test262 numbering-system literal

The pinned Test262 `intl402/DateTimeFormat/prototype/format/numbering-system.js` includes an
ASCII space and `AM` only in its hanidec two-digit expected substring. The other
numbering systems already test just the numeric clock prefix. The local patch
makes that expectation consistent without changing any digit or width check.
ECMA-402 `FormatDateTimePattern` does not require this literal to be ASCII:
CLDR 49 English supplies U+202F in the default pattern and a separate ASCII
alternative; ICU 79 exposes the default. V8 converts the space in `format()` but
retains U+202F in `formatToParts()`, a compatibility policy rather than a missing
number-formatting algorithm. This adapter preserves ICU literals and consistent
format/parts output.

Reviewed against ECMA-402 living draft
`3691634ebc1794f2af74cb26e59824b21090c3ad` on 2026-10-26, Test262
`d53ebc8b1931d9f1541e43d59650776e4b766248`, and CLDR 49
`96e54b273d2da4ec6fbad350ba19ff23fa24a588`.

- <https://tc39.es/ecma402/#sec-formatdatetimepattern>
- <https://github.com/unicode-org/cldr/blob/96e54b273d2da4ec6fbad350ba19ff23fa24a588/common/main/en.xml>
