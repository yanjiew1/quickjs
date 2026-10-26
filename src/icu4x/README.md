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
