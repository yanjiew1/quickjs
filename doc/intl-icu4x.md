<!--
Copyright (c) 2026 Yan-Jie Wang

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

# ICU4X Intl adapter

The build selects one full Intl provider. `intl=icu4x` uses ICU4X 2.4.0,
requires `calendars=icu4x` and `timezone_backend=icu4x`, and does not link ICU4C.
Rust 1.88 or newer builds the locked PIC static companion. Meson records its
native link dependencies for the shared library, static embedding and qjsc.
The existing calendar/timezone-only configuration can still disable Intl.

## Ownership and algorithms

The JavaScript frontends keep ECMA coercion, option resolution, realms and
exception behavior. Independent C module contracts own immutable services,
formatter lifetimes and typed results. Rust handles retain public ICU4X
payloads and never expose foreign objects to JavaScript. All C construction
failures unwind partial owners. A formatter can outlive its service.

ICU4X supplies locale aliases and preferences, segmentation, collation, list
patterns, case conversion, names, numeric symbols and plural rule data. Shared
exact C arithmetic retains arbitrary-precision Number/BigInt inputs, rounding,
scientific operands and range/parts composition. Plural evaluation consumes
selected ICU4X rules instead of reducing exact inputs to binary64.

DateTimeFormat reuses the C date-program catalog, matching, chronology and
range/parts composition. This preserves requested field widths and the official
Chinese/Dangi corrections rather than reconstructing dates through a different
calendar algorithm. Public ICU4X calendar and timezone names are retained at
construction. Only equivalent selected text replaces a companion leaf; alias
origins, blocked/empty values and interval proofs remain unchanged. Dataset-local
metazone IDs are obtained through public timezone periods with checked name/data
checksums, not copied from CLDR indexes.

The stock ICU4X bake omits search collation tailoring. The declared search
policy uses German phonebook data and the locale's default collation elsewhere.
Unpaired surrogates receive replacement-character weights, an allowed UCA
policy; native/ICU4C can distinguish them. ICU4X also classifies underscore as
word-like through its ExtendNumLet status. Repository tests retain each
provider's declared behavior where ECMA-402 leaves these choices unspecified.

## Explicit supplemental data

ICU4X public APIs do not expose every ECMA/CLDR operation or source distinction.
Six scoped generated C owners supply these gaps at the selected CLDR version:

- Locale: source locale inventory, week metadata and presentation identities.
- Display: unsupported display-name categories, calendar fields and leaf policy.
- Number: exact layout, currency spacing and source-qualified numeric patterns.
- Unit: sanctioned-unit overlays and grammatical composition.
- Plural: separate component source identities and missing rule/range families.
  Public data is retried at the correct inherited source first; a matching CLDR
  family fills category sets lost by the stock bake, including Manx.
- Date/time: complete styles, available/append/interval programs, precise flexible
  day periods, raw GMT grammar, qualified location composition and name fallbacks
  removed or transformed by ICU4X data compaction.

These companions omit unrelated native collation and segmentation data. They
reuse the existing C layouts/readers rather than a second runtime data format.
Generation accepts compatible custom sources without authenticating acquisition
pins. Acquisition is responsible for source integrity. The normal reproducibility
job regenerates all five owners from production CLDR and compares them with the
repository. See `src/icu4x/README.md` and `doc/data-updates.md` for commands.

Timezone transitions use the compact matching IANA supplement because ICU4X
2.4 does not supply the required transition queries. Calendar official tables
and explicitly documented out-of-range approximation policies are shared; they
are not silently replaced by another provider's disagreeing lunar results.

## Validation and distribution

Repository contracts cover each selected service, retained payload lifetimes,
source distinctions, exact formatting, ranges/parts and C allocation failures.
The ICU4X sanitizer CI runs the complete repository suite and pinned baseline
and Intl Test262 entry points using the runner's available threads. Rust bridge
CI checks both the minimum supported compiler and current stable compiler.
No complete Test262 suite is required locally.

Install/distribution must include the generated ICU4X and Rust copyright notices
alongside the optional companion archive. The public QuickJS export list remains
unchanged; private C/Rust bridge symbols are hidden in shared-library builds.
