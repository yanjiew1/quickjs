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

# Future Intl backends

These are retained plans, not available build profiles. Current builds select
`disabled`, `minimal` or `native`; see [current architecture](intl-design.md).
Version two adds **ICU4C**; version three considers **ICU4X** after feasibility
work. Keep these option names distinct. Native builds acquire no ICU or Rust
dependency, and normal builds never download a backend implicitly.

ISO arithmetic, UTC, numeric-offset identifiers and the host clock remain common.
ICU-backed profiles supply their own relevant data **and algorithms**, including
Unicode, calendar extensions and named zones. Do not silently mix newer native
aliases, plural rules or timezone data into an ICU profile. System ICU's genuinely
older data/algorithm discrepancies may be accepted and reported visibly; missing
required APIs are configuration errors, not permission for silent substitution.

## ICU4C dependency and build plan

Start with system ICU4C and probe actual APIs and behavior before choosing a
minimum version. Support older installations, including RHEL 8's, where adapters
can meet the contract. An explicit external ICU4C prefix provides a reproducible
alternative. Report dependency source/version and resolved Unicode, calendar,
timezone and Intl implementations in the Meson summary.

Add coherent `auto` dependency resolution where needed, retaining explicit
options' meaning with Intl disabled and rejecting incompatible combinations.
Temporal constructors remain independently selectable. Do not add per-module
backend selectors or runtime registration/loading/replacement in this version.

## Module dependencies and equal backend implementations

| Module or API entry point | Required composition or coordination |
| --- | --- |
| RelativeTimeFormat | Intrinsic NumberFormat and PluralRules behavior |
| DurationFormat | Intrinsic NumberFormat and ListFormat behavior |
| DateTimeFormat | Intrinsic NumberFormat behavior for numeric fields; calendar/timezone modules |
| PluralRules | Shared digit-option/rounding semantics; no requirement to construct public NumberFormat objects |
| NumberFormat | Locale plural/unit/currency grammar; do not invent a public PluralRules call where the spec does not require one |
| Intl.Locale and supportedValuesOf | Actual module capabilities, locale preferences and common canonical identity |
| String.localeCompare | Selected Collator |
| Number/BigInt.toLocaleString | Selected NumberFormat |
| Date/Temporal locale methods | Selected DateTimeFormat, with the prescribed type/calendar restrictions |
| String locale-sensitive case methods | Locale foundation and selected case-mapping operation |

Native modules can call other modules through their common interfaces. For example,
native RelativeTimeFormat can use NumberFormat and PluralRules facade handles.
No additional central dispatcher is needed for each call.

An ICU4C module can use ICU4C's internal dependencies. It need not route ICU4C's own
calls through our facades. The first profile keeps all relevant modules and
Unicode/calendar/timezone data coherent, while adapters align ECMAScript options,
locale resolution, precision and parts with the contract.

This is a behavioral equivalence requirement. It does not promise that ICU4C's
defaults or every high-level ICU4C function already implement the exact draft.
For example, ICU4C RelativeDateTimeFormatter accepts an ICU4C UNumberFormat object,
not an arbitrary callback into a native NumberFormat. Replacing number text
after ICU4C formatting cannot generally repair plural selection or parts.
Arbitrary mixing therefore needs separate dependency rules or finer adapters.

Normative composition: [RelativeTimeFormat](https://tc39.es/ecma402/#sec-Intl.RelativeTimeFormat),
[DurationFormat](https://tc39.es/ecma402/#sec-partitiondurationformatpattern),
[ListFormatParts](https://tc39.es/ecma402/#sec-listformatparts),
[FormatDateTimePattern](https://tc39.es/ecma402/#sec-formatdatetimepattern), and
[locale-sensitive entry points](https://tc39.es/ecma402/#locale-sensitive-functions).
ICU4C boundary: [RelativeDateTimeFormatter C API](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/ureldatefmt_8h.html).

## ICU4C lifetime, identity and allocation

Keep the existing immutable timezone identity/rule contract. The ICU4C adapter
needs a bounded, synchronized, fallible process-lifetime identity registry with
copied stable names. Mutable ICU4C calendars and formatters belong to individual
operations/handles or bounded owner caches, not shared rule records. Freeze
agent-visible identity/data assumptions and test initialization under workers.
The registry is bounded by the finite provider identifier set; it needs no
eviction policy. Add an explicit fallible preparation operation before the
existing non-fallible count/at enumeration. Publish a complete registry only
after success, preserve the previous state on failure, and permit a synchronized
retry after allocation failure. Fallible lookup/consumer entry points propagate
preparation errors. Never report initialization OOM as an empty zone list.

ICU4C's CLDR canonical zone ID is not automatically the ECMA-402 primary ID.
The identity adapter must implement the facade's Identifier/PrimaryIdentifier
distinction, including aliases and country rules, using ICU4C-supplied identity
information. Probe ICU4C IANA-identity APIs and older-version alternatives before
freezing this adapter; ucal_getCanonicalTimeZoneID alone is not sufficient.
See the [ICU4C timezone identity APIs](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/ucal_8h.html).

ICU4C owns its internal allocations; engine-owned handles/results use the caller's
allocator. Report ICU4C allocation failures and document external ICU4C heap costs
in memory accounting: those internal allocations do not automatically obey
JS_SetMemoryLimit. Do not install ICU4C global allocator hooks or call
`u_cleanup()` during runtime teardown. Embedders must keep ICU4C initialized while
dependent engines/handles exist.

Store default locale/zone policy on JSRuntime; contexts share that policy while
JS instances and bound functions retain their own realm. Constructors snapshot
defaults; changing a runtime default does not change existing formatters. Preserve
Windows territory-aware mapping and the configured host-discovery/fallback policy.
Do not mutate ICU process defaults.

Keep backend helpers in their actual owning module, with shared ICU conversion
and error helpers only for genuine cross-module use. Adapters may span several
translation units. Shared private contracts retain exact mathematical values,
UTF-16 lengths, explicit ownership and recoverable errors; they expose no ICU,
Rust or C++ object layout. The current native checked-data hint is a private
admission optimization, not an obligation for another backend to read QJIN.

ICU4C's intrinsic algorithm/data costs relative to native are accepted backend
costs; report them separately from regressions introduced within a provider.

## Version two ICU4C delivery

Retain the ICU4C architecture and contracts above, including system/external
dependency selection, coherent algorithms/data, timezone identity and lifetime,
and use of ICU4C internal module dependencies. Add this backend only after
version one's native delivery, without changing native's Rust/ICU-free builds.

1. Complete API feasibility probes and determine supported ICU4C versions.
   Cover calendar range/eras/monthCode, timezone identity/enumeration/failure,
   Unicode ranges/string properties, exact numeric input/ranges/parts,
   RelativeTimeFormat semantics and DateTimeFormat formatMatcher=basic.
   Record high-level call, finer adapter or unsupported for each operation.
2. Add the icu4c build profile and shared locale/Unicode adapters, with explicit
   dependency checks and no silent native data substitution.
3. Implement each module adapter behind existing C contracts. Verify numeric
   precision, parts, prescribed component options, ownership and realm behavior.
4. Deliver ICU4C calendar/timezone adapters and connect Date/Temporal/Intl to one
   default policy and consistent backend data, keeping ISO/UTC/offset core rules.
5. Run the complete ICU4C profile matrix, including Temporal on/off, platform,
   sanitizer, exports and system-version checks. Retain native regression gates.
   Report older system ICU4C discrepancies with visible Test262 counts.

ICU4X follows as version three, beginning with the feasibility assessment below;
its implementation remains outside both preceding versions' completion gates.

## ICU4X interface reservation and feasibility

ICU4X is a prospective peer of native and ICU4C behind the same private C
contracts. Keep shared headers free of ICU4C, ICU4X, Rust and C++ implementation
types. Use opaque handles, explicit data lifetimes, precise numeric values,
length-bearing strings, parts and provider-neutral errors. Do not make ICU4C's
object model, pattern syntax or resource layout the common contract.

The future adapter uses ICU4X's C FFI from C. Native and ICU4C builds acquire no
Rust/Cargo dependency. Building ICU4X from source needs a compatible Rust
toolchain; linking a matching prebuilt library is another path to evaluate.
Do not add unused adapter files or directories merely to reserve the design.

Version-one interface review records potential ICU4X gaps from available APIs.
Small isolated probes may inform that review without adding Rust or ICU4X to
the engine build. Version three begins with the full feasibility report and
compile/link/operation probes below, before attempting a complete backend:

| Area | Required evidence |
| --- | --- |
| Coverage | Functional/shared module matrix distinguishing Rust APIs, stable C FFI, experimental APIs and missing operations |
| Standard semantics | Exact decimal/BigInt handling, rounding, ranges/parts, locale negotiation, calendar range/era/monthCode and timezone identity/offset/transition behavior |
| Boundary safety | Handle/result ownership, borrowed data lifetime, embedded NUL/unpaired-surrogate behavior, reentrancy and thread rules |
| Failure behavior | Whether allocations can fail recoverably or abort, panic policy, and prevention of Rust unwinding across the C boundary |
| Build and platforms | Meson/Cargo integration, pinned dependency lockfiles, offline and cross builds, static/shared linking, private exports and target ABI compatibility |
| Validation | Available sanitizer instrumentation on both sides of FFI, 32-bit/Windows support, representative spec tests and unsupported configurations |
| Cost | Build time, code/data footprint, allocations and parent/provider benchmark comparisons |

Evaluate ICU4X compiled data and an immutable embedded binary blob. Keep its
data format/provider internal to the adapter; it need not read the native Intl
pack. Pin its actual source/data versions and licenses without silently mixing
native or ICU4C data. ICU4X's internal DataProvider is a data mechanism, not a
reason to add runtime backend loading or switching to QuickJS.

Record pass, adapter needed, or unsupported for each tested operation/version.
Missing stable C functionality, allocation semantics or full-domain behavior
must remain visible gaps. Do not claim readiness from Rust API availability
alone or silently fall back to a different backend. Version one's native and
version two's ICU4C delivery can complete while those gaps remain. A later ICU4X profile needs its
own complete contracts, data inventory and CI acceptance before `intl=icu4x`
becomes supported; arbitrary mixed profiles remain a separate future task.

References: [ICU4X C FFI](https://docs.rs/icu_capi/2.3.2/icu_capi/) and
[ICU4X data management](https://icu4x.unicode.org/2_3/tutorials/data-management/).

