# Intl and shared internationalization module design

Version one adds complete standard Intl support to the existing Temporal fork
using native C algorithms with minimal and full locale-data profiles. Version
two adds ICU4C, and version three considers ICU4X, as equal implementations of
the same private C contracts. Preserve their detailed interface, feasibility
and delivery plans while keeping both backends out of version-one builds.
The first version selects one coherent backend profile at build time. Functional
modules retain separate ownership; arbitrary combinations of module backends
are a future extension. Implementation proceeds in small, testable steps; this plan
does not change the engine or claim the proposed interfaces already exist.

## Scope and source baselines

The implementation target is the latest ECMA-262 and ECMA-402 living drafts
plus all unmerged Stage 4 ECMA-402 specification text. Include every standard Intl
API, its current methods, locale information, and integration with String,
Number, BigInt, Date and Temporal. Native full-data mode supports every CLDR
locale with the data relevant to those operations. Intermediate milestones may
have narrower coverage, identified explicitly until the full acceptance gates.

Reviewed on 2026-10-19:

| Source | Revision or release |
| --- | --- |
| [ECMA-262](https://github.com/tc39/ecma262/tree/4feb0272b799ea795c5eadee9b482aea0c10ce45) | `4feb0272b799ea795c5eadee9b482aea0c10ce45`, 2026-10-16 |
| [ECMA-402](https://github.com/tc39/ecma402/tree/730ca2b88c305862db7be2f83c510dcca33ab48c) | `730ca2b88c305862db7be2f83c510dcca33ab48c`, 2026-10-12 |
| [Temporal Stage 4 PR](https://redirect.github.com/tc39/ecma262/pull/3966) | Open; head `3d4a6e7124a6878cb5af3132af7e01e01a88317f` |
| [Era and monthCode Stage 4 PR](https://redirect.github.com/tc39/ecma402/pull/1044) | Open; head `d026068ed653130a37d79a95c0122ecf5319f1a4` |
| [CLDR releases](https://cldr.unicode.org/index/downloads) | Latest published release checked: 48.2, tag `release-48-2`, 2026-03-17 |

Recheck these before implementing each feature. Record the actual specification
and data revisions in source manifests and relevant commits. For missing
algorithm details, consult the referenced Unicode/CLDR standards, then ICU4X,
then ICU4C. Audit reusable QuickJS/QuickJS-NG work before writing replacements.
Copied or adapted code/data retains its authors, licenses and exact provenance.

The living drafts govern existing functionality. Use an unmerged Stage 4 PR as
the primary specification for the feature being implemented, checking its scope
and compatibility intent when it refactors shared algorithms. An incidental
shared-algorithm difference is not sufficient reason to change an established
observable result. For example, DateTimeFormat keeps an effective hour cycle
for Temporal-specific records internally, while `resolvedOptions()` exposes
`hourCycle` and `hour12` only when its ordinary Date format includes an hour.

### Stage 4 acceptance inventory

Stage 4 is part of version one's required scope even before its specification
PR merges. Do not defer such work to the ICU4C/ICU4X versions or treat the living
draft alone as the complete acceptance target. For unmerged work, use the latest
Stage 4 PR text first, its discussion for unresolved details, and the proposal
specification as supporting provenance. Pin the actual reviewed revisions in
implementation commits and refresh this inventory before final acceptance.

Rechecked on 2026-10-19 against the
[ECMA-402 finished-proposals inventory](https://github.com/tc39/proposals/blob/07002d4d999558b9c7326f98e5134e0222b13808/ecma402/finished-proposals.md)
(revision `07002d4d999558b9c7326f98e5134e0222b13808`, 2026-10-14):

| Work | Specification and acceptance requirements |
| --- | --- |
| Locale Info | Stage 4 and already in the pinned living draft; deliver every information method, not only the methods implemented so far. |
| Intl Era and MonthCode | Unmerged Stage 4 text in [ECMA-402 PR 1044](https://redirect.github.com/tc39/ecma402/pull/1044), head `d026068ed653130a37d79a95c0122ecf5319f1a4`, updated 2026-06-17; include the calendar requirements and Temporal/DateTimeFormat integration. |
| Time Zone Canonicalization | Stage 4, absorbed into Temporal and the same PR 1044; DateTimeFormat must retain the canonical-cased accepted named identifier, including aliases, rather than always store its primary identifier. Test alias spelling/identity separately from primary-zone enumeration. |

PR 1044 acceptance must also explicitly cover:

- DateTimeFormat's Temporal-specific records/defaults, all four formatting/range
  methods, matching range input types and calendar-compatibility checks. Direct
  ZonedDateTime formatting throws; its locale method uses the prescribed internal
  route. Include style compatibility, adjusted/null format records and errors.
- All eight Temporal locale methods: Duration, Instant, PlainDate, PlainDateTime,
  PlainMonthDay, PlainTime, PlainYearMonth and ZonedDateTime.
- The sanctioned calendar capability table and aliases, proleptic reckoning,
  era/arithmetic-year behavior, leap month-code constraints and non-ISO calendar
  operations. Audit the existing calendar implementation before adding code.
  DateTimeFormat's CLDR `islamic` preference falls back to `islamic-tbla` where
  prescribed; supportedValuesOf uses the sanctioned capability contract.
- DurationFormat's ToTemporalDuration path, including Temporal.Duration,
  accepted duration strings, object coercion and errors. Intl-on builds with
  Temporal constructors disabled still need the shared conversion semantics.

Earlier Stage 4 features and additions already merged into the living draft
remain required through the full feature checklist below. An open normative PR
is not automatically a Stage 4 proposal. Track normative clarifications
separately, including the multiple-numbering-system text in
[PR 1074](https://redirect.github.com/tc39/ecma402/pull/1074), and verify approval
of its latest text when implementing the affected operation; approval of an
earlier revision does not establish approval of a substantial revision.

Implementation starts on `intl`, copied from `temporal` at
`a315958f6b6f69d456a48d0ad9c32e6956e5f5ba`. Calendar, timezone and Unicode
libraries already exist; Intl is absent from that baseline. The saved
`intl-bak` branch at `01ff95c0b0bb3f4bbcc1b938afa18fe32b68645c` is a reuse
source. Its data pack covers only a locale subset, with restricted admission
and incomplete collation/segmentation coverage. Audit algorithms, tests and
generators separately; do not replay its provider architecture or treat its
pack size as proof of full-native coverage.

## Architecture and ownership

Terminology distinguishes code structure from runtime state:

- A **functional module** owns a coherent responsibility and its dependencies,
  such as number formatting or collation. A **shared module** supplies genuinely
  common operations; one module may serve both roles, as plural evaluation does.
- A **backend/provider** implements a module's selected algorithms and data.
- A **service instance** is initialized runtime state exposed by a module, such
  as `QJSIntlCollationService` holding checked data and capability inventories.
  An operation handle, such as `QJSIntlCollator`, holds resolved configuration.
  These objects are not dynamically loaded code modules.

Use module terminology for architecture and ownership. Existing `*Service` C
types name stateful runtime instances; stateless modules need no artificial
service object. ECMA-402's term “service constructor” refers to its JavaScript
API category and does not prescribe the project's code boundaries.

Use three layers:

1. **QuickJS bindings** own JS coercion, observable getter order, internal slots,
   realms, bound functions, exceptions, GC and JS result objects.
2. **Shared C cores and facades** own fixed standard rules, common value/result
   contracts, specified API composition and shared policies.
3. **Selected backends** own versioned data and replaceable computation. Native
   and ICU4C implement the same contracts and may use different internal paths.

API names do not define module boundaries. A binding or operation coordinator
may query several functional modules without making those modules depend on
one another. For example, an Intl.Locale information method may combine locale
preferences with a formatter's capability inventory; the locale foundation stays
independent of formatter initialization. Each module owns its
actual supported locales and options. A functional module may also serve other
modules: plural evaluation remains in `plural/`, rather than moving into a
generic common directory merely because several formatters use it.

`Locale.getCalendars()` follows that coordination rule. Shared locale preference
selection returns the first raw language-region/region list; the binding then
canonicalizes, deduplicates and filters against DateTimeFormat's global calendar
capability. It does not retry region selection after filtering. An explicit
calendar slot bypasses capability filtering, and an empty filtered list falls
back to Gregorian. Raw preference readers do not depend on DateTimeFormat.
Canonical availability is distinct from negotiation acceptance: generic islamic
is accepted for negotiation and resolves to islamic-tbla, but is not advertised
by AvailableCalendars or included in implicit regional preference results.

`Locale.getTimeZones()` uses only the locale's explicit region, without
maximization or rg/sd/tz keyword overrides. Shared metadata enumerates terminal
CLDR timezone identities and their source territories; the binding then resolves
supported primary identifiers through the selected timezone facade, deduplicates
and sorts them. Determine territory before resolving links, so a source's country
is not inferred from its target. No region yields undefined; an unknown explicit
region yields a fresh empty array.

ECMAScript-defined semantics belong to the common contract. Fixed pure
algorithms normally have a shared implementation. A module operation may
delegate a complete calculation to its backend when the observable behavior
remains equivalent. All backend implementations are allowed that granularity;
the interface is not a special bypass for ICU4C. Keep the chosen boundary explicit
for each operation, especially numeric rounding and date-format matching.

Implementation-defined operations still have standard constraints. Shared
runtime policy, such as DefaultLocale, is coordinated centrally even when the
standard leaves its selection open. Backend-specific helpers shared only by
native modules stay in native support code; ICU4C adapters may likewise share
their own helpers.

There is no runtime provider registration, loading, replacement, or vtable.
Meson selects direct function definitions. Selecting a locale, calendar, or
timezone within the compiled provider remains a runtime operation.

## Shared modules beyond Intl constructors

These are private C interfaces, independently usable by their actual consumers.
They do not require new JavaScript constructors or public embedding APIs.

| Owner and interface | Responsibility | Consumers |
| --- | --- | --- |
| Timezone facade | Identity, primary IDs, offsets, transitions and local-time candidates | Date, Temporal, DateTimeFormat |
| Calendar facade | ISO core plus non-ISO conversion, months, eras and arithmetic | Temporal, DateTimeFormat; Date reuses the ISO core |
| Locale foundation | Syntax, canonical identity, aliases, maximize/minimize and matching | Intl functional modules, Intl.Locale, locale-sensitive casing |
| Unicode normalization | Separate NFC/NFD/NFKC/NFKD operations | String.normalize and relevant backend algorithms |
| Unicode case operations | Default/locale-sensitive case mapping and distinct simple/full folding | String, RegExp and relevant backend algorithms |
| Unicode properties | Character properties and script/range queries | Parser, RegExp, segmentation and case algorithms |
| Locale preferences | Hour cycles, calendars, numbering systems, week/text information | Intl.Locale and module-specific locale resolution |
| Runtime environment coordinator | Common default locale/zone, embedder overrides, host discovery policy | Bindings across realms, Date, Temporal, Intl |
| Host clock | Obtain the current instant, independently of timezone lookup | Date.now, Temporal.Now |

Keep Unicode available with Intl disabled. Preserve ECMAScript-specific RegExp
canonicalization and strict property-name grammar above generic Unicode queries.
Default case conversion, locale-sensitive conversion, case folding and
normalization have distinct contracts; a generic conversion flag must not erase
their differences. A provider must support UTF-16 inputs with embedded NUL and
unpaired surrogates where the consuming operation allows them.

Exact decimal arithmetic, buffers and parts builders are support libraries,
not additional independently selectable backends. Currency/numbering-system
metadata has an owning private interface when several consumers need it.
Capability/version aggregation queries providers; native pack decoding remains
inside the native data owner.

## Core and backend algorithm boundaries

| Operation | Shared responsibility | Backend responsibility |
| --- | --- | --- |
| Options and mathematical values | JS access/coercion order, ranges, defaults, exact numeric representation, negative zero/nonfinite distinctions | Execute an accepted module operation with those normalized inputs |
| Identifiers | Structural grammar, fixed ASCII rules, standard allowed-unit checks | Versioned aliases and locale identity operations requiring provider data |
| Locale negotiation | Lookup, extension precedence, ResolveLocale/ResolveOptions orchestration | Available locales, extension capabilities, best-fit matching and preferences |
| Number/Plural | Digit-option rules, rounding semantics and module contracts | Number presentation, currency/unit grammar and plural rules; native may share a decimal engine, ICU4C may use its formatter |
| RelativeTimeFormat | Required checks and specified number/plural/pattern composition | Relative patterns and special forms, or an equivalent complete provider operation |
| DurationFormat | Duration validation, signs, unit options and specified component composition | Digital preferences and selected number/list operations, or equivalent provider composition |
| DateTimeFormat | Input rules, fixed BasicFormatMatcher behavior and common zone/calendar policy | Candidate formats, best-fit, localized fields/names and range presentation |
| Collation/segmentation | JS semantics, option/result contracts and original UTF-16 indices | Unicode algorithms, tailoring, dictionaries/models and locale data |
| Defaults and capabilities | One runtime policy and coherent advertised results | Actual provider capability and version information |

Locale canonicalization includes data-dependent steps; syntax parsing alone
does not replace Unicode LocaleId Canonicalization. BasicFormatMatcher has fixed
scoring rules and cannot silently become ICU4C best-fit. Numeric providers must
avoid double rounding and preserve exact BigInt/decimal-string inputs.

See [CanonicalizeUnicodeLocaleId](https://tc39.es/ecma402/#sec-canonicalizeunicodelocaleid),
[locale negotiation](https://tc39.es/ecma402/#sec-resolvelocale),
[BasicFormatMatcher](https://tc39.es/ecma402/#sec-basicformatmatcher),
[ToRawFixed](https://tc39.es/ecma402/#sec-torawfixed) and
[Unicode normalization](https://unicode.org/reports/tr15/).

Native scientific/engineering exponents use the selected numbering system's
digits, including zero and supplementary characters, matching ICU/CLDR
presentation. This is an intentional interoperability policy: ECMA-402's
[PartitionNotationSubPattern](https://tc39.es/ecma402/#sec-partitionnotationsubpattern)
still emits ASCII exponent digits without transliteration. Reviewed living
draft `730ca2b88c305862db7be2f83c510dcca33ab48c` on 2026-10-22
(Asia/Taipei); [issue 627](https://redirect.github.com/tc39/ecma402/issues/627)
tracks the discrepancy, without an adopted correction. The reference behavior
is CLDR 49-beta4's
[number patterns](https://github.com/unicode-org/cldr/blob/7cdeb9e7fac52c59f22fc7248b05f989ee5689c2/docs/ldml/tr35-numbers.md#Number_Patterns)
and ICU's
[scientific formatter](https://github.com/unicode-org/icu/blob/ad484f3d6c036ba06193ac7c3bc2233a53ab68b8/icu4c/source/i18n/number_scientific.cpp).
This choice changes glyph selection only; exponent calculation, rounding,
signs, separators and part types retain their existing rules.

`collation/collation.h` separates the module's runtime interfaces from JS coercion
and locale matching. The native owner opens the checked compiled profile once,
cross-checks public default binding rows against its available locale registry,
and exposes exact per-usage named types plus a sorted global type inventory.
Source-parent program labels never become resolved public locale identities.
Shared matching/ResolveLocale algorithms consume these capabilities separately.

Locale/usage defaults come from the semantic null binding, independently of a
later named collation selection. Sort sensitivity is variant; search projects
source strength to the four public sensitivities, using variant when secondary
plus case requires retaining case distinctions. Fully resolved options override
numeric, case-first and shifted handling; ignoring punctuation uses the punct
variable boundary and never adds a quaternary tie-breaker. Handles copy their
program, policy and allocator, borrow only static prepared bytes, and can outlive
service close. Comparison retains no input or scratch after return. The JS
binding owns option ordering, resolved identity, realm state and bound callbacks.

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

## Build profiles

Version one adds one Meson selector, `intl=disabled|minimal|native`.
Version two adds `icu4c`; version three may add `icu4x` after its acceptance
gates pass. Reserve both names now without exposing them as working version-one
choices or silently substituting another backend.
Use `icu4c`, not an ambiguous `icu` alias, for the ICU4C profile and corresponding
provider selections. These names describe planned options, not existing ones.
Initially keep the default disabled to preserve the current feature set until
the implementation gates pass. `minimal` is a native data profile using the
same algorithms, initially with `en` and `en-US`; it is not a third algorithm
backend. Include all required semantics, numbering systems and appropriate
calendar/zone coverage even in that profile.

| Profile | Delivery | Intl algorithms/data | Unicode operations | Calendar extensions | Named zones |
| --- | --- | --- | --- | --- | --- |
| disabled | Version one | No Intl bindings | Existing native support | Existing ISO/native choice | Existing minimal/native choice |
| minimal | Version one | Native, reduced locale data | Native | Native standard set | Embedded native IANA |
| native | Version one | Native, all CLDR locales and relevant data | Native | Native standard set | Embedded native IANA |
| icu4c | Version two | ICU4C data and algorithms with ECMA adapters | ICU4C-backed operations | ICU4C adapter | ICU4C adapter |
| icu4x | Version three, subject to feasibility | ICU4X through the same contracts | Adapter feasibility required | Adapter feasibility required | Adapter feasibility required |

ISO arithmetic, UTC, numeric-offset handling and host clock remain shared.
`intl=minimal` does not mean `timezone_backend=minimal`: the latter has no
additional named zones and is not the full-Intl profile.

Extend existing calendar/timezone options with `auto` resolution where
appropriate; ICU4C choices are a version-two addition. `auto` preserves today's
native behavior with Intl disabled
and selects the coherent dependencies with Intl enabled. Reject explicit
incompatible combinations at setup. Do not expose per-module backend
selectors in version one. Temporal remains independently enabled or disabled.

Source selection uses actual consumers, not `temporal` alone. Intl-on/Temporal-off
must compile calendar/timezone support without pulling in Temporal bindings.
Date's supported zone policy must remain correct when both features are off.
Use readable Meson source lists, separate build directories and host-only Python
generators; preserve Meson 0.58 and cross-build support.

Print the resolved Intl, Unicode, calendar and timezone providers, data versions,
and ICU4C dependency source/version in the setup summary. Test option resolution
without requiring complete module implementations. Existing explicit options
retain their meaning with Intl disabled; enabled profiles reject conflicts.

For version two, system ICU4C is the first dependency path. Probe required APIs and usable
features; do not choose a minimum version merely from one API's introduction.
Support older ICU4C, including RHEL 8's, where adapters can meet the contract.
Otherwise fail unsupported API configurations clearly. Accept and report genuine
old-data/old-algorithm discrepancies under the user's system-ICU4C policy, without
hiding their Test262 counts. A pinned external ICU4C prefix is a reproducible
alternative; ordinary builds never silently download ICU4C.

## Private C contracts

Modules expose their shared contracts in owning private headers. Stateful
interfaces can provide an opaque service instance and operation handles with
open/operate/close functions; stateless helpers remain plain functions. Selected
backends implement direct symbols at build time. Contracts expose no ICU4C/ICU4X
types, JSValue, JSContext or backend-specific layout. A backend may span multiple
translation units and use its own private headers.

Shared contract families are:

- Length-bearing UTF-16 strings; index units always explicit.
- Normalized C option records, with supported defaults resolved by the proper
  layer, separate from JS objects whose property reads are observable.
- Exact numeric input, distinguishing finite decimal coefficients/exponents,
  negative zero, infinities and NaN; never convert BigInt to double for convenience.
- Parts with type, text span, optional unit and range source. Segment positions
  refer to the original input's UTF-16 code units.
- Allocator callbacks and owned result buffers; borrowed data states its owner
  and lifetime. Result storage remains valid for the documented operation scope.
- Explicit invalid-input, unsupported, allocation, data and provider errors.
  Binding code maps them to the correct JS exception; OOM is never a fallback.

Open returns either a complete instance/handle or failure. Each interface states
whether an error clears an output or preserves it; cursor and value outputs can
have different contracts. Builders require a fresh output and leave it empty
and clearable on failure. Close/clear
functions are safe for empty state and release only their own allocations.
Each handle documents thread/reentrancy rules; immutable configuration does not
make mutable result scratch buffers shareable. Do not hold borrowed pointers
across JS callbacks or free them with another provider's allocator.

Keep callbacks out of backend selection. Allocator callbacks and existing
bounded visitor contracts are functional inputs, not runtime provider tables.
Share only real cross-TU declarations; keep local helpers/types in their owner.

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

## Runtime defaults and version two ICU4C lifetime

Store common default-locale/default-zone policy on JSRuntime. Contexts and
realms share the host policy, while formatter JS objects, bound functions and
exceptions belong to their own realms. Constructors snapshot defaults where
required; changing a default does not retroactively change existing formatters.

Prefer a supported canonical host locale in the intersection of enabled
Intl service constructors' available locale sets, with an explicitly documented common fallback such as
`en-US`. Validate embedder overrides against that intersection. Other requested
locale sets may differ across constructors; preserve specified nested negotiation.
Keep API capabilities and metadata consistent with executable behavior.
See [service locale requirements](https://tc39.es/ecma402/#sec-internal-slots).

Extend the existing Date/Temporal zone coordinator to serve Intl independently
of Temporal types. Preserve Unix TZ handling and the Windows stable-key plus
GetUserGeoID/GetGeoInfoW mapping policy. Native calculations use embedded IANA
rules, not installed zoneinfo; the explicit host-offset discovery fallback keeps
its existing limitations. ICU4C mode uses ICU4C data/mapping and algorithms with
the same runtime override/fallback policy. Never mutate ICU4C process defaults.

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

### Shared locale-key negotiation

`locale/resolve.[ch]` owns the fixed ResolveLocale key-processing algorithm after
matching. The caller provides the matched base, requested Unicode extension,
ordered relevant keys, supported defaults and membership callbacks for its own
module/locale. Keep observable option getters in the binding and actual
capability inventories in their owning module. Resolve all relevant keys before composing one final
locale; repeated single-key composition would discard other retained keys.

Null is distinct from an absent option and from an empty Boolean extension
keyword. In particular DateTimeFormat's hour12 option can explicitly override hc
with null, while an empty kn keyword requests true. Canonicalize option aliases
before SameValue and membership: equal or rejected options retain a supported
extension, accepted different options remove it. Operational callback errors
never become unsupported-value fallback. Preserve input key order in returned
values and canonicalize the retained extension together only once.

Copy all result strings into the output allocator, freeing identity-service
canonicalization scratch with its original allocator. Results can outlive the
identity service. The existing one-key Number/RelativeTime/Duration bindings use
a thin wrapper over this shared algorithm; no module queries another module's
capability inventory implicitly.

## Public embedding APIs

Proposed additions, to be finalized with their owning feature commits:

```c
JS_EXTERN int JS_AddIntrinsicIntl(JSContext *ctx);
JS_EXTERN int JS_SetIntlDefaultLocale(JSContext *ctx, const char *locale);
JS_EXTERN JSValue JS_GetIntlDefaultLocale(JSContext *ctx);
```

The setter canonicalizes and validates, reports errors through ctx, and preserves
the old default on failure. A null locale resets the runtime to its documented
host-default policy. The getter returns a new JS string. Reuse existing
JS_SetDefaultTimeZone/JS_GetDefaultTimeZone rather than adding Intl-only zone
settings. Feature-disabled behavior and initialization/OOM rollback are tested.
The shared default policy is available when either Temporal or Intl is compiled,
without requiring either namespace to be initialized in the requesting context.
When both features are disabled, these existing APIs report TypeError.

`timezone/identifier.[ch]` owns canonical named/fixed-minute identifiers independently
of Temporal bindings and parsing. Temporal adapts the common record/errors to
its exact epoch-nanosecond operations. Date resolves local milliseconds through
floor seconds plus an unchanged nonnegative millisecond remainder, preserving
historical second offsets and nonzero-offset recovery at TimeClip boundaries.
Date uses the earliest overlap candidate and the pre-transition gap offset, as
specified by UTC(t); do not replace Temporal's separate disambiguation algorithm
with this Date-specific operation. Host discovery, primary default IDs and the
transactional runtime snapshot remain shared for Date, Temporal and Intl.

Enabled JS_NewContext installs Intl and the corresponding locale-sensitive
integrations. JS_NewContextRaw does not; JS_AddIntrinsicIntl requires initialized
base objects and must work without Temporal. Successful repeated initialization
is idempotent and preserves intrinsic identity. Stage new objects before
publishing the namespace; failure releases partial owned state and permits
retry without exposing a half-initialized Intl object. Safe runtime class
registrations need not be undone merely because a context initialization failed.

Keep public API/ABI additions compatible, mark callable declarations JS_EXTERN,
update the embedding manual and exact shared-library export tests together.
Backend/data-version diagnostics initially belong in tooling, not new JS globals
or mandatory public APIs. Audit QuickJS/QuickJS-NG embedding APIs before naming
new entry points. Raw contexts can opt into Intl without enabling Temporal.

## Native data sources and complete locale coverage

Use released, pinned sources with download URLs, cryptographic hashes, license
receipts, transforms and coverage inventories. Source priority is standards and
official data, then ICU4X, then ICU4C for missing implementation details.

| Data | Source and use |
| --- | --- |
| Locale IDs, aliases, inheritance, likely subtags/preferences | Released CLDR core/supplemental/BCP47 data |
| Number, plural, currency and unit patterns | CLDR numbers, plurals/ranges, currency/unit data and relevant supplemental metadata |
| Date/time patterns, localized eras/months/zones | CLDR dates, day periods, intervals, metazones and zone-name data |
| Relative, duration, list and display names | Corresponding CLDR locale data and supplemental preferences |
| Case mapping, folding, normalization, character properties | Pinned Unicode UCD and applicable Unicode algorithms |
| Collation | Unicode collation data plus CLDR locale tailoring |
| Segmentation | UAX #29/property/emoji data, relevant CLDR tailoring/suppressions; dictionary/model gaps investigated in ICU4X first, then ICU4C |
| Calendar calculation and named-zone rules | Existing independently owned calendar/timezone packs and provenance |

Do not equate all CLDR locales with an imported name list. For every
locale-dependent functional module,
record canonical source locale, inherited data, aliases, default-content
relationships and successful execution. Root/parent data is valid coverage when
the locale genuinely inherits it. Account for every source field relevant to
standard Intl; document deliberate exclusions of unrelated CLDR facilities.
New CLDR fields fail or require an explicit inventory decision, not silent loss.
Do not expose nonstandard units/options simply because CLDR contains them.
Inventory calendar identifiers against the reviewed standard and actual calendar
implementations. A localized calendar name, including a DisplayNames record,
does not establish that DateTimeFormat or Temporal supports its arithmetic.
Advertised formatting calendars require complete computation and integration.

Segmentation must include appropriate language behavior where default Unicode
word boundaries are insufficient. Dictionary/model ownership, generation and
licenses are part of the module, not a runtime ICU4C/Rust dependency. Do not add
text shaping, line layout or unrelated CLDR functionality to this project implicitly.

Native uses the latest released CLDR when its source pin is established. ICU4C
uses its own bundled versions; never combine newer native alias/plural data
silently into an otherwise ICU4C profile. Record Unicode, CLDR, tzdb and model
versions individually, since their release/version numbers are not interchangeable.

For this first implementation, the user selected CLDR 49 beta to align with
Unicode 18. Pin `release-49-beta4` (published 2026-10-12), commit
`7cdeb9e7fac52c59f22fc7248b05f989ee5689c2`, reviewed 2026-10-19.
This is an explicit prerelease exception to the normal release policy. Record
the beta qualifier in generated provenance; upgrading to the final release
requires regeneration and validation rather than silently changing the inputs.
Use the complete release-tag source tree, since production ZIPs may omit
records before our module-specific collectors apply their documented selection policy.

## CLDR XML collection and resolution

Read released CLDR XML using Python's standard-library ElementTree, one input
file at a time. Module-specific collectors interpret known LDML structures and
produce typed records for numbers, dates, names, lists and other consumers.
There is no generic XML-to-binary tree serialization and no runtime XML parser.

A shared resolver handles aliases, root/parent inheritance, component-specific
parentLocales, default-content locales and the explicit inheritance/no-inheritance
markers. Resolve relative/cross-locale aliases against the supported LDML paths;
detect cycles and reject unresolved references. Do not assume every parent is
obtained by removing the final locale subtag.

Pin the applicable DTD/schema metadata and distinguish identity attributes from
metadata. ElementTree is not a validating DTD processor and does not supply all
DTD-defaulted attributes; apply required defaults explicitly. Define draft/alt
selection per consumed dataset, preserve pattern whitespace and order where
meaningful, and audit relevant fields that cannot be represented. Unsupported
relevant input fails generation instead of silently disappearing.

Validate normalized records before packing: locale/data-locale relationships,
supported identifiers, plural categories, pattern operands and cross-section
references. Produce an inventory recording direct, inherited and aliased data
for every advertised Intl API/locale. Algorithm coverage is checked separately
from successful extraction of a record.

The writer assigns deterministic IDs, pools strings/records, and emits typed
sections with explicit widths, lengths, offsets and byte order. Separate reader
tests compare decoded records with normalized inputs, exercise bounds/endian
handling, and check reproducibility and full/minimal coverage manifests. XML
collection, resolution, module-specific normalization and binary packing remain separate
testable components.

## Data pipeline and packaging

Use a deterministic pipeline:

```text
pinned source archives or vendored official records
    -> validated project schema
    -> one sectioned native Intl binary pack plus manifest
    -> generated C data owner in the build directory
```

Normal builds are offline and consume checked-in prepared data. Explicit tools
download sources over HTTPS, verify hashes and regenerate; raw XML/JSON is never
parsed at engine runtime. Keep temporary archives and normalized intermediates
in the build/cache directory. Official records that require manual transcription
use reviewable machine-readable files with source/range/hash provenance.

Check in prepared full and minimal Intl packs with their manifests, produced
from the same source lock/schema generation. Ordinary builds convert only the
selected prepared pack to C; minimal never requires downloading raw CLDR or
linking the full pack. A later subset generator may operate on prepared data
after its section-dependency contract is established.

The Intl pack contains a versioned header/directory, shared locale registry,
string pools and common numeric/plural records, followed by module-owned sections.
Calendar and timezone packs remain separate. Localized calendar/zone names belong
in Intl; numeric calendar transitions and zone rules stay with their owners.
Unicode property tables retain one generated C owner and are not copied into
each module-specific pack. Choose widths, encoding, alignment and byte order explicitly;
decode safely rather than casting untrusted byte offsets into C structs.

Generation includes the union of selected native modules and their actual
dependencies. Full-native covers all required functionality/locales; minimal changes
the locale-data selection. Shared pools and section references use stable,
validated IDs. File structure and source generators remain modular even though
the final pack is one file. Runtime file loading/replacement is outside version
one; compiled data stays immutable.

Use structural savings first: pooled strings, deduplicated records, inheritance,
sparse indexes, tries and suitable integer encodings. Do not introduce general
compression libraries. Measure full/minimal binary size and generator/compiler
memory costs before choosing more complex encodings. A single C array normally
prevents linker removal of unused subranges, so trim at generation time. If
embedding becomes a compilation bottleneck, change C chunking without changing
the logical pack or duplicating its ownership.

A shared manifest/generation ID protects cross-section compatibility. Readers
validate required sections, bounds, IDs, versions and corruption before use;
truncated or mismatched data is an error. A module never substitutes unrelated
English data merely because a required section is missing.

## Source layout

Keep bindings and engine-independent modules separate. Current module locations
are listed below; ICU4C/ICU4X source sets remain later-version additions:

```text
src/quickjs/builtins/intl/     JS bindings, slots, options and realm/GC integration
src/quickjs/                  runtime default-locale/default-zone coordinators
src/intl/common/              precise values, parts and common error contracts
src/intl/locale/              locale facade/core and native/ICU4C implementations
src/intl/number/              number-formatting contracts, native algorithms and state
src/intl/plural/              plural evaluation, also shared by formatting modules
src/intl/relative/            relative-time formatting and composition
src/intl/.../                other functional/shared modules with semantic owners
src/intl/native/              pack reader and genuinely shared native helpers
src/intl/icu4c/               genuinely shared ICU4C conversion/error helpers
src/unicode/                 Unicode facade, native implementation and ICU4C adapter
src/calendar/                existing calendar facade/core; add ICU4C adapter
src/timezone/                existing zone facade/core; add ICU4C adapter
tools/intl/                  fetch, normalize, generate, inventory and audit tools
data/intl/                   source locks, prepared pack, manifests and licenses
tests/                       JS subsystem regressions, C contract/OOM tests
```

Each module directory may contain several backend files; avoid one giant source
per backend. Do not create empty directories or one-file wrappers before a real
ownership boundary exists. Existing Unicode/Temporal paths can be migrated only
where extraction is needed, with source moves separate from behavior changes.
Future module-local ICU4X sources and shared `src/intl/icu4x/` helpers follow
the same ownership rules when implementation is authorized.

## Complete feature delivery checklist

| Feature/API group | Required delivery |
| --- | --- |
| Locale | Constructor, canonicalization, maximize/minimize, all current getters/information methods and capability integration |
| ListFormat / DisplayNames | Styles/types, parts, fallback rules and all relevant names/locales |
| NumberFormat | Decimal/BigInt precision, all options, notation, units/currencies, parts and range formatting |
| PluralRules | Cardinal/ordinal, digit/rounding/notation options and selectRange |
| RelativeTimeFormat | Numeric/auto, styles, unit normalization, negative zero, parts and number/plural dependencies |
| DurationFormat | All units/styles/display options, fractional/digital behavior, sign handling and parts |
| DateTimeFormat | Styles/components, calendar/numbering/hour cycle/zone options, parts/ranges and Date/Temporal integration |
| Collator | Sort/search, sensitivity, punctuation, numeric/case options, bound compare and String.localeCompare |
| Segmenter | Grapheme/word/sentence, iteration, containing, UTF-16 indices, word status and language-specific behavior |
| Intl object | getCanonicalLocales, supportedValuesOf, intrinsics and descriptors |
| Outside Intl | String locale case conversion; Number/BigInt/Date/Temporal locale methods; Unicode integration |

Check the reviewed draft's actual members, including recent additions, rather
than freezing this checklist as a substitute for an API inventory. Stage 4 work
still outside the draft follows its pinned PR text. Public headers and disabled
build behavior accompany each API addition, not a later unrelated patch.

## Implementation sequence and commit boundaries

Each stage may take several commits. Keep independent refactors, behavior fixes,
data imports and features reviewable; each published feature commit includes
focused tests. Future performance follow-ups remain separate unless clearly
part of the same feature change.

| Stage | Work | Completion gate |
| --- | --- | --- |
| 1 | Audit saved Intl implementations/tests, API coverage, source licenses and specification deltas | Reuse map and per-module missing-work inventory |
| 2 | Extract shared clock/default-zone dependencies; build calendar/timezone by consumers | Existing behavior passes with Temporal on/off; no Intl required |
| 3 | Add common C contracts and native build resolution; review future ICU4C/ICU4X interface feasibility | Native contract smoke tests and invalid configurations; no external backend linked |
| 4 | Establish source locks, pack schema/generator/reader and locale coverage inventory | Offline deterministic generation, endian/bounds/corruption tests |
| 5 | Locale foundation, common defaults, Locale/Intl object APIs and public initialization | Canonicalization/preferences/default/realm/OOM tests |
| 6 | Unicode facade and native case/property/normalization integration | String/RegExp/parser behavior preserved; Unicode conformance inputs |
| 7 | Native ListFormat and DisplayNames | API/parts/fallback tests and complete relevant locale data |
| 8 | Exact numeric foundation, PluralRules and NumberFormat in small independent increments | Precision/rounding/grammar/range and provider contract tests |
| 9 | Native RelativeTimeFormat and DurationFormat | Selected module composition and standard semantics tests |
| 10 | Native DateTimeFormat and shared calendar/timezone integration | Date/Temporal/Intl agree on zone/calendar behavior; Temporal-off builds |
| 11 | Collator and locale comparison | Unicode/CLDR tailoring tests and localeCompare integration |
| 12 | Segmenter, dictionary/model data and iterator integration | Boundary suites, original UTF-16 indices and language coverage |
| 13 | Full locale/data inventory, native version-one profiles, public API/export/manual audit | Complete native release acceptance matrix and recorded future-backend interface risks |

Review future backend API feasibility in stage 3 before freezing a contract
around native-only layouts. Isolated probes can inform that review without
adding ICU4C/ICU4X to version-one builds or requiring their full CI matrices.
Stage 10 delivers native calendar/zone integration; its dependents can be prepared
while unrelated modules proceed. Move dependency-free completed work forward
when another module is blocked. Stages describe dependencies, not a demand to
combine all their changes into one commit or defer an independent ready module.

### Initial Segmenter profile

The `minimal` build exposes `Intl.Segmenter` with module-specific `en`/`en-US`
availability. Its plain C contract lives in `src/intl/segment/segment.h`; the
Unicode-default backend delegates grapheme, word and sentence boundaries to the
shared Unicode 18 algorithms. It does not claim to execute CLDR break rules or
dictionary tailoring. Minimal word-like status means the segment contains a
Unicode Letter or Decimal_Number. Native exposure waits for those additional
rules and language-specific data, rather than advertising incomplete coverage.

Boundaries and segment data refer to the original UTF-16 input, including NUL,
lone surrogates and indices inside pairs. `containing()` walks known boundaries
from zero; the shared Unicode forward APIs cannot start at arbitrary code-unit
indices. Each iterator has its own cursor. Bindings retain one shared immutable
input conversion and separate Segmenter references, so dropping a Segments
wrapper does not invalidate an iterator. No eager boundary array is required.

Repository tests cover protocols, realms, coercion, GC and allocation failures.
The minimal Test262 overlay enables the whole Segmenter directory, except two
explicitly documented cases that assume locales outside this profile. Native
acceptance will retain those cases and add CLDR/dictionary coverage. The profile
distinction is observable: Unicode 18 GB9c joins a leading Indic linker with its
consonant, whereas the pinned CLDR beta root still has the older consonant-led
rule. Tests preserve that distinction instead of treating the profiles as equal.

### Native Segmenter data preparation

Native preparation must keep three inputs separate: pinned Unicode break properties,
CLDR locale rules and sentence suppressions, and separately versioned dictionary
data. A CLDR release label does not establish the version of every break
algorithm. In particular, the beta root's GB9c rule differs from the Unicode 18
default described above. Keep that difference explicit in the selected profile
and its tests; do not replace the shared Unicode default silently.

Resolve CLDR segmentation inheritance through the `segmentations` parent
component, independently of ordinary locale parents. Variable declarations are
ordered definitions, not an unordered map of recursively expanded expressions.
For example, Japanese rules redefine `$Hiragana` using its previous value, and
`en_US_POSIX` redefines word-break variables using inherited values. Preserve
the earlier environment when expanding each definition. Rejecting repeated
names or resolving such references against the final map would lose valid data
or create artificial cycles. Preserve rule order and source identities too.

Compile the relevant checked set/property/rule syntax into bounded native data;
normal builds and runtime execution must not parse XML or depend on Python's
Unicode database. Grapheme, word and sentence are Segmenter granularities.
CLDR line-break records belong to a different facility and need an explicit
inventory exclusion. Sentence suppressions are another data layer: retain their
inheritance and suffix/context semantics separately from ordinary break rules,
and document the policy selecting them. Do not invent support for `ss`, `lb`
or other Unicode extension keys: Segmenter's relevant extension-key list is empty.

Dictionary selection follows the input's script runs, not just the requested
locale; English-locale text can contain Thai or Japanese words. Pin dictionary
sources, generation versions, hashes and file-specific licenses separately from
CLDR. ICU4X's dictionary matcher is a reference for a native C implementation,
not a Rust runtime dependency or an automatic oracle for newer Unicode rules.
Check ambiguous matches, unknown text and grapheme-safe forward progress before
publishing locale capabilities. Root inheritance establishes support only when
all required rules and input-script refinements are executable.

Keep prepared bytes immutable and shared, resolved configuration in the module's
operation handle, and per-input text or boundary caches with their input owner.
Never put a mutable iterator cursor in a shared Segmenter. If an algorithm needs
allocating scratch or a cached boundary index, expose that ownership and failure
path in the private contract rather than changing an allocation-free query
silently. Reuse the binding's original UTF-16 positions and independent cursors.

References: [ECMA-402 Segmenter internal slots](https://tc39.es/ecma402/#sec-intl.segmenter-internal-slots),
[Unicode 18 text segmentation](https://www.unicode.org/reports/tr29/tr29-49.html),
and the pinned [CLDR segmentation specification](https://github.com/unicode-org/cldr/blob/7cdeb9e7fac52c59f22fc7248b05f989ee5689c2/docs/ldml/tr35-general.md#segmentations).

DateTimeFormat preparation starts with `tools/intl/date_pattern_syntax.py`, a
host-side TR35 date-pattern tokenizer. It preserves field symbols/run lengths
and decodes quoted literals independently of format selection or calendar
calculation. Raw source auditing is separate from the next typed collection
stage, which must resolve calendar aliases, retain source identities and handle
numbering overrides, names, skeletons and intervals. Compilation must distinguish
these date patterns from date-time combination templates; shared pair templates
do not use the date-pattern apostrophe rules.
The lexical compiler provides separate `compile_combination` tokens for exactly
one time and one date slot, applying apostrophe quoting to fields and braces.
Its raw-source audit covers style connectors without resolving aliases or
choosing standard/atTime/relative fallback policy. Slot tokens keep substituted
date/time patterns separate from decoded connector literals and field runs.

`tools/intl/date_patterns.py` owns the first typed date/time style facts. It
resolves length styles and their independent skeleton records across calendar
and locale aliases, retaining numbering overrides and terminal provenance.
Available formats, intervals, names and combination templates remain subsequent
collection stages. These host-side facts do not imply prepared runtime data or
advertised formatting-calendar support.

The optional style section 27 pools source records and complete locale rows,
retaining independent pattern/skeleton values and numbering overrides. Its
calendar and alternate registries describe raw source identities. Profile
projection preserves external source provenance; runtime formatters must still
establish arithmetic and formatting capabilities independently. Both prepared
profiles include the section and have corresponding C reader tests. The C style
owner is an immutable view with exact queries and temporary validation scratch;
it owns neither calendar computation nor format selection and retains no heap
state. Pattern/skeleton text and numbering overrides stay uninterpreted at this
boundary, ready for the later native date-format compiler.

`calendar_sources.py` shares structural XML traversal and alias validation
between style and connector collection. Typed key inventories, subtree ownership
and alias-path mappings remain with their collectors. `date_combinations.py`
resolves standard/atTime/relative source connectors independently at each style
width, preserving raw overrides and terminal provenance through calendar,
group and form aliases. It leaves form fallback to the consuming formatter,
and establishes host-side facts before a separate wire/C-owner stage.
Optional connector section 28 pools records and locale rows over calendar,
width, form and alternate selectors. Eight-word records preserve raw overrides
and typed terminal provenance independently of consumer fallback. Both production
profiles include this section and have corresponding C reader tests. The immutable
connector owner
checks canonical sharing and references, frees all validation scratch, and
provides exact queries that allocate nothing. Pattern and connector data share
the width selector in `datetime/style-types.h`; their records, states and query
interfaces retain distinct owners. Missing, blocked and explicit empty records
remain distinct across source, wire and runtime queries.

`calendar_names.py` collects months, weekdays, quarters, day periods, eras,
cyclic names and month templates in sparse child-first layers. Its typed group
is calendar/family/cyclic-set/context/width; leaf identities separately retain
the source identifier, month year type and alternate. Ancestor aliases map
typed prefixes, retaining requesting or explicit-source locale anchors and
terminal provenance. Missing groups, blocked leaves and explicit empty text
remain distinct. Month templates have their own numeric/all selectors and are
not parsed as date patterns. Raw CLDR era IDs remain source identities until
the calendar integration maps them through supplemental era codes. Neither
localized names nor inventory entries establish calendar arithmetic support.
This host collector precedes its separate wire and runtime-query stages.
Optional calendar-name section 29 stores a typed group registry, shared locale
rows, sparse leaf maps and source nodes. Each node retains its own terminal
locale and group. Aliases may change calendar, context, width or cyclic set
along a parent chain, while retaining the semantic name family. The wire never
flattens those chains or selects lateral fallback. Profile projection retains
reachable source nodes outside the selected requesting locales. Canonical
sharing, typed references and reachability are checked before consumption.
The immutable C owner in `datetime/name-data.[ch]` validates the section using
temporary depth, coverage and source-string scratch, released before return.
Exact group/head/node/entry queries borrow pack storage and allocate nothing.
An exact-key resolve query follows only prepared parents and retains the actual
origin group. Blocked and explicitly empty leaves stop inheritance; year,
alternate, width, context and calendar fallback remain consumer policy.
Family-level and era-string validation caches bound repeated shared-data work.
The reader has authored C fixtures, including external source locales, empty
inventories, unaligned bytes and allocation-failure retries. Both prepared
profiles include section 29 and compile its production reader, with separate
prepared-data C tests. Formatter integration follows separately.

The native `datetime/name-select.[ch]` consumer maps admitted source symbols
and widths to their exact typed name groups. It follows the prepared aliases,
retaining missing, blocked, empty and translated observations plus their origin.
Only absent weekday short names try abbreviated. Month selection exhausts the
explicit leap-year or common-year key before the ordinary key; a blocked/empty
year-specific key stops that substitution. Default era names never pick a
variant alternate automatically. Calendar arithmetic remains responsible for
mapping its months, eras and cycles to raw name codes. Day-period name selection
can separately substitute the actual local AM/PM code for an unavailable b/B
label, retaining the primary observation and never reviving its blocked parent.
These allocation-free queries do not render numbers or leap-month templates.

`date_available_formats.py` owns available date patterns and append templates,
not interval patterns, range templates or numeric separators. Its sparse groups
retain calendar, family and container alternate; exact leaf keys separately
retain skeleton ID or append request, optional plural count and leaf alternate.
An absent count is distinct from `other`. Calendar and container aliases retain
requester/source anchors and terminal provenance without flattening overrides.
Skeleton spelling is preserved; the collector does not normalize fields or
validate semantic skeleton ordering and widths. Available pattern values use
the ordinary date-pattern lexer. Append text has a distinct substitution-template
grammar: `{0}` is the existing format, `{1}` the added field and optional `{2}`
the field display name. Raw append text such as `{1} ob {0}` must not be parsed
as a date pattern or a date/time-style connector. Matching, plural/alternate
fallback and formatter capability remain consumer responsibilities.
Optional section 30 encodes these groups with separate container-alternate
references, counted leaf selectors, shared sparse maps and typed source nodes.
Canonical sharing and reachability validation preserve empty, missing and
blocked states. Head/parent compatibility is family-based, allowing source
calendar or container-alternate changes without losing provenance. Profile
projection keeps reachable origins outside the requester inventory. Prepared
profiles both include this section. The immutable C owner in `datetime/available-data.[ch]`
validates canonical source records and frees all scratch before returning. Its
exact group/head/node/leaf queries borrow pack storage and allocate nothing;
container alternates compare by bytes, independently of leaf alternates. Source
locale and code validation is cached by string/family, bounding repeated scans
of shared long skeleton IDs. Authored fixtures cover unaligned storage, empty
inventories, external origins, malformed records and allocation-failure retries.
Prepared-data C tests cover every stored selector and source layer, English
pattern provenance and validation allocation failures. The reader is compiled
in production Intl builds. These checks do not implement pattern matching or
template rendering.

Physical map-span and entry-at queries expose all exact keys for candidate
enumeration without leaking wire decoding into the builder. Enumeration does not choose inheritance,
plural or alternate fallback; a consumer must retain child-key shadowing even
when that child's pattern is blocked, empty or not renderable.

`datetime/available-source.[ch]` resolves visible exact keys along one source
chain without interpreting patterns or append templates. Nearest blocked, empty
and unusable source text still shadows the same code/plural/leaf-alt parent key.
It skips repeated physical maps, gathers at most the owner's physical entries,
then sorts and selects deterministic nearest records. The retained index owns
one allocation and borrows the unchanged pack through a copied owner view;
queries allocate nothing and preserve origin groups. Head zero needs no
allocation. This source inventory is separate from usable native candidates and
final ECMA Format Records, including duplicate-component grouping and hour-cycle
pairing. A counted `other` key is never an uncounted fallback.

Compiled patterns can be copied into independent owned storage or combined
by expanding the decoded connector's time/date slots. Token-level composition
preserves quoted slot text and source field widths without reparsing inserted
literals. Adjacent literals coalesce even across substitution boundaries.
This syntax operation does not choose styles or adapt hour cycles; those
choices remain with native format construction.

`datetime/source-candidate.[ch]` compiles the visible ordinary pattern family
for an explicitly selected leaf alternate and uncounted keys. It selects no
alternate or plural fallback. Blocked/empty leaves, unsupported projections and
literal-only/AM-PM-only patterns contribute no direct candidate; child masking
has already taken effect. Flexible day-period-only patterns remain candidates.
Eligible malformed syntax and allocation failures propagate and unwind all owned
patterns. Filtered keys are not syntax-validated by this operation. Compiled
tokens are owned, while skeletons, raw text and source provenance borrow the
unchanged pack. The input source index can be released independently. Distinct
skeletons and hour cycles remain distinct even when component vectors match;
these are source candidates, not the final ECMA format list or capability proof.

Native DateTimeFormat uses an explicit browser-compatible source preference:
try the resolved `alt="ascii"` pattern key first, then the ordinary key if it
is missing, blocked, empty, malformed or unsupported. This is cross-alternate
fallback, not revival of a masked ancestor of the same key. Allocation failure,
overflow and operational errors propagate. Each exact key already resolves
inheritance, so an inherited usable ASCII key can win over a nearer ordinary
key. Keep both variants and their provenance in the binary data.

Style patterns and datetimeSkeleton fields select their alternatives
independently, including during calendar-year recovery. Look up alternate IDs
inside each owning catalog; IDs are not shared across source modules. The exact
source APIs retain their no-fallback contracts. Existing connector and interval
data has no ASCII alternative; preserve those actual source literals. Strings
and parts use the same selected program; range text similarly agrees with its
parts, even when its own interval punctuation differs from single-value text.
This is native compatibility policy allowed by ECMA-402's locale data freedom,
not a requirement for ASCII or a numbering-system-specific digit workaround.
Do not globally normalize Unicode whitespace in final output. The policy follows
explicit CLDR variants documented by the
[ICU source conversion guide](https://github.com/unicode-org/icu/blob/ddc6b66926bfac6036144ca9902992a0d6d50af9/tools/cldr/cldr-to-icu/README.md#using-altascii-cldr-alternate-values-from-the-cldr-xml).

`datetime/format-source.[ch]` groups source candidates by all eleven public
component values while retaining every original source index. Groups follow
first input occurrence; members follow original source order, independently of
allocation addresses or sort stability. Two owned index/group blocks borrow no
pattern ownership or caller-view address. The original candidate pool remains
alive for resolving indexes. Grouping does not finalize an ECMA Format Record:
English `hm` and `Hm`, for example, have different hour widths, requiring
explicit width adaptation before pairing normal and 12-hour programs. Flexible
day periods, ranges and required-subset synthesis also remain separate steps.

Mixed date/clock preparation opts into flexible-period sources only when
explicit dayPeriod and date fields are both requested. Clock-only requests
need no mixed records; their date-only Temporal records are null. Retain each actually observed b/B width, clock
shape and zone style separately, including H-only clocks; equal field presence
alone must not hide a genuine source or intentional width override. Allocate
only combinations whose date and clock operands exist. The no-period path
preserves its plain candidate order and allocation count. Clock indexes still
refer to the original owner, so source-width evidence and range recomposition
use the actual selected operand rather than a similar pattern.

Native BestFit may adapt an explicitly requested b/B textual width after
matching, without changing the period symbol, names or literal layout. Retain
raw skeleton run-length bits for each selected source and exact-text aliases;
an already matching skeleton length preserves its differing pattern width as
an intentional TR35 override. API short/long/narrow requests use canonical
lengths 1/4/5; raw B/BB/BBB evidence remains distinct. Derived date-clock records
inherit period evidence from the clock, independently of the date's month hints.
Only widths whose reachable names pass renderer admission are considered.

Score effective metadata without allocating a candidate catalog, then adapt
only the winning owned program. Basic keeps its specified finite-record scoring.
AM/PM is not converted to a flexible period. Replay the same adaptation on
retained range clock operands before proving exact recomposition. Result metadata,
strings and parts therefore describe the same actual width; do not relabel a
short token as narrow merely to satisfy resolvedOptions.

Ordinary preparation also retains declared numeric/text month requests for
its selected source and aliases with exactly identical raw pattern text. This
preserves mappings such as Hebrew `yMd` to `d MMM y` after component grouping
has merged it with a textual skeleton. Capture both source families, including
sources selected outside the original group; derived records inherit the
month-bearing parent, which is the first/date operand of COMBINE. Date-only
matching uses normal hints regardless of the formatter's hour cycle.

Native best fit waives its additional numeric/text appearance penalty when a
declared source mapping already requested that kind. Keep Basic scores,
missing/extra-field penalties and the actual chosen width intact. This is not
a global ban on Hebrew numeric months: month-only numeric requests remain
numeric, while full dates can truthfully resolve to the source's short name.

`datetime/single-program.[ch]` prepares owned normal/12-hour programs for one
source group before range construction and renderer admission. A fixed table
of year-kind and period classes finds compatible whole-source pairs without
pair enumeration. Require identical year kinds and b/B selection semantics;
a class must occur in the group. Prefer direct hour families, exact hour widths
and then stable source indexes. Cross-family b/B copies use representative H/h,
retain all non-hour tokens and literals, and record adaptations separately from
original metadata. Re-project copies to the unchanged target components. The
renderer must apply the resolved h11/h12/h23/h24 cycle. Missing complementary
programs return unsupported without allocation; do not strip AM/PM punctuation
or revive masked sources. No-hour groups copy one source without a 12-hour
program. Copied programs survive releasing both input owners; diagnostic source
indexes require the original pool for any later provenance lookup.

Native semantic preparation includes a compatibility workaround for CLDR
calendar-date patterns that mistakenly use week-year `Y`: when a pattern has
an unquoted month (`M` or `L`) and no week fields (`w` or `W`), interpret `Y` as
calendar-year `y`, preserving its width and the pattern's punctuation. Weekday
fields and quoted letters do not change this condition. This project policy
is not required by ECMA-402 or LDML. The lexical compiler, raw pack strings and
source provenance remain unchanged. Apply it before pattern/skeleton projection
and before splitting a complete interval into endpoints; calendar arithmetic
continues to use the requested calendar.

`datetime/style-program.[ch]` compiles selected styles separately from
available-pattern matching. Pattern and independent skeleton provenance retain
raw numbering overrides; numeric rendering still uses the resolved ECMA
NumberingSystem. Date/time family mixing and semantic fields outside the
supported projection/workaround remain unsupported sources. Single-date
composition accepts a selected date and a complete, compatible time pair. With
both, the connector uses the requested date style width: usable atTime first,
then standard; missing/blocked/empty forms do not revive masked parents or try
relative/another width. Without both sources no connector is needed. Compose
owned token programs, retain source facts, and verify the final component union,
year kinds and auxiliary period semantics. This supplies no ranges, missing
hour-family synthesis, renderer admission or public formatter capability.

`datetime/style-date.[ch]` is native DATE preparation above that source
compiler. Preserve the original selected style and independent skeleton. A
remaining width-one Y with otherwise supported date-only fields may explicitly
request a calendar-year alternative. Choose a real visible whole y-only available
pattern in the supplied source catalog, retaining its punctuation and field order.
Prefer exact component widths; otherwise permit only SHORT-to-LONG month/weekday
name expansion with unchanged context and a matching source skeleton. Do not
override an intentionally different locale skeleton/pattern width. Retain the
replacement entry, origin, pool index and reconstruction flag separately from
the original style. Missing/blocked/empty and other unsupported forms do not
fallback. Generic provenance never chooses arithmetic calendar. Prepared styles
can enter the existing composer; ranges and renderer admission remain separate.

`datetime/style-hour.[ch]` constructs complete time-style single-date pairs
from the authoritative style, compatible whole available programs, or bare
clocks plus the exact visible Timezone append template. Retain specific/generic
zone distinctions and b/B; never delete AM/PM punctuation. Explicitly adapt only
hour family and permitted numeric hour/minute/second widths, using the shared
`program-adapt` kernel. Original source metadata stays unchanged, with SIZE_MAX
for the style's own program and actual pool indexes for available sources.
Record zone appends as synthesis. Literal append templates use a parsed pair
and token composition; their ASCII letters/apostrophes are not LDML syntax.
A final AM/PM tier can invert the active Timezone template, proving exact
recomposition of all original tokens and literals. Use a recovered compatible
clock, or expand only a proven hour-only h+a layout with the authoritative
zone-free clock. This preserves its actual AM/PM position and all clock units
without guessing punctuation. Unmatched templates or larger incomplete layouts
remain unsupported; extraction and layout expansion have explicit provenance
flags. Successful style pairs do not establish final range/renderer capability.

`datetime/numeric.[ch]` executes numeric/literal single-date programs against
fully specified local numeric fields. It uses selected NumberFormat instances
with grouping off and prescribed minimum integer widths, collapses each result
to one date-field part, and keeps its complete localized text. Two-digit output
retains the last two Unicode code points; supplementary digits occupy two UTF-16
units each. Calendar years at or below zero use 1-year, related years retain their
sign, resolved hour cycles override raw h/H/K/k, and fractional milliseconds are
truncated before formatting. Own output independently of programs and number
services; unwind all temporary numeric results on failure. Named/zone fields
remain explicitly unsupported by this numeric kernel, to be composed by the
full renderer with chronology/name/zone modules. The shared output vocabulary
now distinguishes date-field parts; number-bearing consumers retain their
original number-only type bound.

`datetime/text.[ch]` composes numeric fields with native localized era, month,
weekday, cyclic-year and day-period names into owned UTF-16 and typed parts.
Reuse the numeric kernel one field at a time; do not duplicate its digit,
year/hour or fractional rules. Resolve names and unary leap affixes once before
bounded two-pass emission. A complete decorated month, numeric or named, is one month part;
apostrophes in CLDR unary templates are literal, unlike LDML pattern quotes.
The grammar-only unary parser lives in `common`, shared with unit formatting.

Era codes use explicit metadata rather than arithmetic on raw CLDR IDs. ISO's
localized catalog stays distinct from its Gregorian era mapping; historical
Japanese uses Gregorian labels. Hebrew names use their independent raw keys and
year selectors. Only Chinese/Dangi's repeated nominal months receive the exact
inherited leap template. Numeric M/L fields select the numeric/all template;
apply it after ordinary localized digit formatting and two-digit clipping.
This preserves the nominal month and distinguishes it from its leap repetition,
for example English `5` versus `5bis`, without using the year's ordinal month.

This native CLDR policy follows TR35's regular M/L month-pattern handling and
ICU behavior. The living ECMA-402 numeric branch (730ca2b, reviewed 2026-10-23,
[FormatDateTimePattern](https://tc39.es/ecma402/#sec-formatdatetimepattern)) calls
FormatNumeric directly and does not explicitly include leap decoration. Record
this specification-text gap rather than claiming that its numeric pseudocode
mandates the suffix. CLDR supplies the actual text; never hardcode `bis`, invent
a textual resolved width, or trim a decorated result to its last two code points.

Flexible day periods consume resolved format rules and a separate full-precision
subsecond fact; noon plus
one nanosecond must not use an exact-noon point. The rule selector takes an
explicit midnight-support policy independently of the immutable data. Native
DateTimeFormat suppresses the exact 00:00 midnight label because its API has no
context distinguishing the start of this day from the end of the previous day.
For B, keep the general variable period; b uses AM. Noon remains an exact point.
This follows TR35's caller-choice recommendation and ICU's established display
policy, not a requirement to remove midnight from CLDR or use selection rules.
Raw kernel callers can explicitly enable the point for contexts that need it.

Missing/blocked/empty names or
required templates return unsupported here, without invented lateral fallbacks.
The final formatting module owns admission and the standard's permitted
unavailable-name fallback policy. Zones, complete formatter admission and the public constructor
remain separate work; this renderer alone does not advertise DateTimeFormat.

`datetime/range-layout.[ch]` retains proven date/clock operands for same-day
range synthesis. Capture the winning ordinary COMBINE, mixed date/clock or
style inputs before their temporary owners are released; decode the actual
LDML connector and require exact token recomposition against the final chosen
program. This preserves quoted literals, endpoint affixes, year kinds, selected
hour family and zone placement. Fractional descendants retain their exact
borrowed decimal source and replay that insertion on the clock before copying
the proved result. A relocated zone append is usable only when
that same proof succeeds. Never split a final pattern by punctuation or trim
common prefixes from formatted strings.

When no whole interval is selected and the calendar date is equal, combine one
shared date with two complete clock endpoints using the real interval fallback
and retained date/time connector. This implements TR35's date/time synthesis
path even when an hms-specific interval is absent. Date differences and records
without a proved factorization keep the complete-endpoint fallback. Preserve
whole-interval precedence and all-equal collapse; flatten date/glue and endpoint
clock parts into shared/startRange/endRange provenance without losing seconds,
fractions or zone fields. The owner copies its three programs; output is
independent and every allocation failure unwinds without partial publication.
See [TR35 Interval Format Algorithm](https://www.unicode.org/reports/tr35/tr35-79/tr35-dates.html#interval-format-algorithm).

`datetime/zone-offset.[ch]` prepares localized GMT presentation independently
of timezone rule arithmetic and named-zone selection. Copy exact inherited
hourFormat and gmtFormat sources, preserving signs, bidi marks, quoted hour
literals and separators. The short representation uses an unpadded hour and
omits zero minutes unless seconds are present; long retains two-digit hours and
minutes. Historical nonzero seconds reuse the source H/m separator. Numeric
pieces use selected decimal formatters and the whole result is one timeZoneName
part, without leaking number subparts. The owner and results have independent
lifetimes, bounded output and allocation-failure cleanup.

A supplied gmtZeroFormat remains authoritative. The pinned CLDR 49 beta4 data
omits this source; absent, blocked or empty zero labels use the positive numeric
GMT template rather than inventing a language-independent standalone label.
This follows its accompanying TR35 examples GMT+00:00 and UTC+0. Source-only
legacy zero leaves remain preserved by the binary data owner. Exact chain
resolution preserves missing/BLOCK/empty and original alias provenance; it does
not perform name-type, width, metazone or territory fallback. Those are still
required for complete native specific/generic zone presentation.

Fixed zero identifiers retain their public `+00:00` identity while name styles
try the CLDR Etc/GMT zone and GMT metazone names. Generic styles can use a
standard GMT name because a fixed zero offset is permanently standard. Missing,
blocked or empty names retain numeric fallback. Explicit shortOffset/longOffset
styles bypass name selection and preserve numeric GMT formatting, including at
zero. This separates locale names from the wrapper: French can use the GMT name
while its numeric offset wrapper remains UTC{0}. It is an implementation-defined
presentation policy consistent with CLDR name lookup, not a synthetic zero label.

`datetime/ordinary-programs.[ch]` prepares a finite owned set of ordinary
single-date programs. Retain complete useful source pairs in stable group
order; deduplicate by the full standard component vector. Add all fourteen
required component-presence subsets using actual locale/calendar evidence:
era-free M/Md programs plus the Year and Day-Of-Week append templates, exact
SHORT standard date/time connectors, and second fields followed by the selected
numbering system's decimal separator and one-to-three fractional digits.
Reproject each transformed program and verify normal/twelve fields and year
kinds agree; never relabel a G-bearing source as an era-free format.

When plain twelve-hour hm/hms evidence is absent, a whole compatible flexible-B
clock can supply the literal position of a newly constructed AM/PM a field.
This is a separate plain-clock record with explicit construction provenance;
original B-bearing source and public dayPeriod records remain unchanged. Do
not strip a source field and guess which surrounding punctuation belongs to it.

Native ordinary formats also include single B, BBBB and BBBBB field programs,
as permitted by TR35's flexible-day-period skeletons. Their metadata retains
the actual short/long/narrow width, and their construction provenance does not
claim a CLDR available-format source. Existing real records keep precedence.
These programs still use format-context names and formatting rules, including
exact midnight/noon; absence of an hour does not select stand-alone-context
names or selection rules. Name capability admission remains a later step.

Programs own token/literal storage independently of input pools. Retain source
indexes, exact append/connector provenance and backward construction links;
borrow only provenance slices from unchanged packs. Missing/BLOCK/empty evidence
is unsupported rather than a hardcoded English pattern. This closes mandatory
single-date shape construction; arithmetic/name capability admission, range
programs and the public formatter still have their own requirements.

`datetime/zone-name.[ch]` renders all six standard zone styles over borrowed
rule handles and validated name, presentation and locale metadata owners. It
keeps accepted IANA spellings/primary IDs separate from CLDR's stable first-alias
identity. Shared `locale/timezone-identity.[ch]` resolves that identity and its
associated territory using explicit region overrides before the five-character
short-ID rule. Shared source syntax belongs in `common/`, not the formatter.

Resolve exact zone translations before the instant's half-open metazone period;
do not bridge gaps by reviving an earlier period. Metazone indexes belong to the
presentation identity table, not the shared string table or timezone table.
Presentation standard/daylight offsets override raw DST designation when they
identify an unambiguous type; this handles Dublin's negative-DST rules. Missing
specific names fall back to localized GMT. Generic names use preferred-zone
qualification for the locale territory, then 001, with country/city and literal
fallback templates. Missing generic names use a primary/single-zone country or
exemplar city and regionFormat. Raw geographic city fallback alone replaces
underscores. Non-geographic POSIX zones can use the localized Etc/Unknown city;
Etc offset zones retain localized GMT. BLOCK/empty never resurrect a parent
translation, and literal apostrophes/template operand order remain intact.

Type fallback without a daylight label follows the CLDR generic-then-standard
rule. When daylight names exist, substitute standard for generic only after
proving constant offset/raw-DST information and compatible effective-standard
presentation throughout the closed +/-184-day window, including every period
and gap. This is a sufficient native fallback policy: it does not infer a
saving amount from TZif or claim exhaustive TR35 substitution for perpetual
nonzero daylight saving. Without proof, use the location/GMT alternatives.
Every output is one independently owned bounded timeZoneName part; numeric
formatters retain responsibility for localized offset digits. The public
DateTimeFormat owner will cache immutable identity/territory facts and perform
locale admission; this leaf has no host discovery, coercion or locale matching.

`datetime/chronology.[ch]` converts checked local epoch days through the shared
calendar facade, then projects native formatting fields without allocating or
borrowing localized data. Keep arithmetic/era years separate and let the numeric
renderer apply nonpositive-year adjustment only once. Modern Japanese and
Ethiopic formatting use era-relative years; historical Japanese uses Gregorian
era presentation. Mandatory Gregorian formatting uses core ISO arithmetic even
without native calendar extensions. Hebrew numeric months remain ordinal while
raw name keys follow monthCode; Chinese/Dangi numeric months are nominal, with
separate leap decoration and mathematical cyclic-year modulo. Preserve related
years independently. The calendar algorithms/data link for either Intl or
Temporal consumers, without requiring Temporal's JavaScript bindings.

The shared timezone facade also exposes an exact closed-window stability
query for total UTC offset and raw DST designation. Walk real explicit and
periodic boundaries rather than comparing endpoints: a period may contain
changes and return to its initial state. Include same-offset DST changes and
explicit/footer handoff, while ignoring abbreviation-only changes. Existing
Temporal transition queries continue to report only UTC-offset changes. This
primitive is one proof input for generic-name fallback, not proof of unchanged
CLDR presentation or a particular daylight-saving amount; presentation overrides
and metazone policy remain with the zone renderer.

`datetime/local-time.[ch]` applies either an immutable named-zone rule or a
fixed minute offset to normalized epoch seconds plus nanoseconds, then derives
all calendar/clock fields from the same local day. Negative instants use floor
division; historical named offsets retain seconds. Preserve the complete
nanosecond remainder and a separate subsecond fact while deriving truncated
milliseconds for numeric display. Return the raw rule DST designation for later
presentation policy; CLDR offset overrides belong to zone-name selection, not
this arithmetic bridge. Check addition bounds before applying an offset, and
reset output on every failure. Input coercion and Date/Temporal range policy
remain with the frontend. Shared timezone core, selected backend and generated
data link for Intl or Temporal; Temporal JS bindings remain optional.

Non-Gregorian ToLocalTime calculations remain implementation-defined in the
pinned draft. Its unconditional nonpositive-year formatting step can differ
from ICU for negative single-era years; this implementation follows the specified
formatting step rather than silently introducing another era or skipping it.

`datetime/era-data.[ch]` owns explicit CLDR canonical-era to raw name-key
metadata from supplemental calendarData, independently of localized-name and
calendar-arithmetic owners. Optional section 35 keeps raw calendar identities
and arbitrary decimal name keys; it never derives Japanese IDs from era order
or creates named eras from Chinese/Dangi sequential epoch records. Era boundaries,
inherited Gregorian presentation and canonical aliases remain consumer policy.
Opening validates each shared string once per grammar domain and checks both
mapping domains; ordinary lookup allocates nothing and borrows the unchanged pack.

`locale/type-identity.[ch]` resolves exact metadata names and complete legacy
alias tokens to a terminal preferred identity, without applying Unicode-type
syntax to raw source spellings. The scan is restricted to one extension/key.
Aliases converging on one terminal are valid; conflicting terminals are data
errors. Variable rows establish no identity. This shared operation allocates
nothing and does not infer any formatter capability.

`datetime/calendar-identity.[ch]` applies that equivalence independently to
available-pattern, style, connector, calendar-name and interval catalogs. Each
query returns the selected owner's raw index; catalogs need not have matching
contents or ordering. Ambiguous multiple raw keys for one identity are rejected.
Gregorian `gregory` maps through metadata to `gregorian`, while `iso8601` keeps
its own source inventory and `generic` stays source-only. Existing source alias
chains and origin calendars are unchanged. Arithmetic support, locale calendar
preferences and renderer admission remain separate from this projection.

`date_intervals.py` owns interval fallback templates, greatest-difference
patterns and range-example templates; numeric separators retain a separate
owner. Groups retain calendar, interval-container alternate, family, skeleton
and item alternate. Exact leaves retain the difference field or range type and
their own alternate. There is no plural selector. Typed ancestor/item/range
aliases preserve requesting/explicit-source anchors and source provenance.
Both interval and other sparse collectors use the resolver's common layer
freezing mechanism; their source grammars and selectors remain separate.

The interval pattern lexer preserves optional `earliestFirst:`/`latestFirst:`
ordering directives without splitting repeated logical fields. Fallback and
range templates use literal `{0}`/`{1}` substitution, sharing the host pair
validator with ListFormat rather than using date-pattern quoting or field
rules. Empty, missing, inherited and blocked records remain distinct. CLDR's
numeric/non-numeric/mixed `intervalFormatRange` templates are for synthesized
comparison examples, explicitly not production interval data; they must not
become production `formatRange` synthesis policy. Matching, source-root stopping,
field equivalence, greatest-difference calculation and rendering remain consumer
responsibilities.

Optional section 31 stores interval layers using shared dense calendar blocks.
The seven-word header gives version, calendar/suffix/block/map/node/entry counts.
Calendars and four-word suffixes (container alternate, family, skeleton, item
alternate) form a rectangular inventory; the product must fit a uint32 source
identity. Locale/calendar pairs reference shared suffix-head blocks. Sparse
maps use three-word leaves (code, leaf alternate, text-or-blocked), and four-word
source nodes retain origin locale, global source group, map and parent. Aliases
may change calendar and all alternate/skeleton selectors within a family.

The host codec validates canonical sharing and complete reachability without
scanning an implicit locale/calendar/suffix cube. Its decoded inventory stays
factored; source rows are expanded only for nonzero heads. Projection retains
reachable source locales outside the requester inventory. This layout is local
to interval data; existing section 29/30 formats are unchanged.

The immutable C owner in `datetime/interval-data.[ch]` checks stored calendar
blocks, suffixes, maps and nodes, including checked global group IDs. Source
locale, skeleton and per-family code checks are cached by string identity.
All validation scratch is released before return; exact calendar/group/head/
node/leaf queries borrow pack storage without allocation. Authored fixtures
cover source aliases, external origins, all difference/range selectors, empty
and blocked leaves, shared long skeletons, unaligned storage, malformed records
and allocation-failure retries. Both prepared profiles include this section,
with the C reader compiled in production Intl builds. Prepared-data tests check
all stored selectors, calendar-block indexing, English provenance, unaligned
storage and validation allocation failures. Range formatter integration follows
separately.

`timezone_names.py` owns localized timezone presentation source, independently
of the timezone engine's identifiers, offsets, DST designation and transitions.
Its finite sparse groups distinguish format templates, exemplar cities, named
zone overrides and metazone names. Entity, entity alternate, width and width
alternate remain separate selectors; each leaf retains its field, optional
region-format type and leaf alternate. Only observed source groups are stored;
there is no calendar-style Cartesian registry of unrelated zone/metazone IDs.
Name-width aliases preserve requester/explicit-source anchors and exact source
identities, including slash-bearing zone IDs. Unreviewed ancestor aliases fail
explicitly. Missing, inherited, blocked and explicit empty values stay distinct.

The shared host literal-template validator handles fixed one/two operand GMT,
region, fallback and dual-offset templates as well as ListFormat/interval pairs.
Apostrophes are literal in these substitution templates. Hour formats have their
own lexer for two quoted hour/minute subpatterns; signs, directional marks,
spacing and source field widths survive unchanged. Names, cities and unknown
GMT text are literal strings. CLDR 49 beta4 has no `gmtZeroFormat`; the collector
does not manufacture one. Runtime zero-offset and localized digit policy belong
to the renderer. Supplemental metazone periods, golden zones and primary-zone
presentation maps have a separate owner; their standard/daylight offset hints
must not redefine the timezone engine's DST designation. CLDR stable name keys
also do not redefine ECMA primary identifiers.

Optional section 32 stores these source layers with four fixed family codes and
independently shared dense head rows per family. The canonical five-selector
group registry determines each family's width; no unrelated identity products
are implied. An absent family has no rows or requester references. A present
family with no values has one all-zero row. Nonempty maps, source nodes and
four-selector leaves remain shared across families, with explicit origin locale
and source-group provenance. Zone and metazone names share a semantic class;
format and city aliases cannot cross that boundary. Requester projection keeps
reachable external source locales and groups before string pooling.

The host codec checks exact extents, selector shapes, canonical order, unique
and used physical rows/maps/nodes, marker states, compatible alias chains and
reverse reachability. It validates stored head rows once and expands only present
heads for the host source view. Raw format text remains source data: collection
validates its grammar, while token compilation and formatting policy remain
separate from structural wire admission.

The immutable C owner in `datetime/timezone-name-data.[ch]` validates physical
family blocks, selector identities, compatible source chains, canonical maps
and full reachability. Per-string source/identity/code checks are cached during
opening. All scratch is released before return; exact group/head/node/leaf
queries allocate nothing and borrow the unchanged pack. Authored C fixtures
cover both alias directions, external source locales, independent alternates,
empty/blocked values, absent families, hostile records, unaligned storage and
allocation-failure retries. The owner does not choose primary identifiers,
metazone periods, standard/daylight names, offsets or renderer fallback.
Both prepared profiles include these source layers. Their C tests query every
stored selector and the independent family-row addressing, English provenance,
unaligned storage and validation allocation failures. Runtime formatting
integration follows separately.

`timezone_presentation.py` collects locale-independent metazone periods,
preferred metazone/territory zones, dominant `primaryZones` and active metazone
ID metadata. Periods are half-open UTC second ranges with distinct unbounded
endpoints; actual Monrovia input includes seconds despite the DTD's minute-only
annotation. Gregorian parsing uses integer arithmetic without host timezone
conversion. Intentional gaps remain gaps. Standard/daylight offset overrides
remain independently optional and may classify display names without changing
the engine's DST designation. One golden zone may represent different
metazones in different periods; only the metazone/territory key is unique.

References use BCP47 first-alias CLDR stable IDs, with timezone and metazone
namespaces kept separate. Dominant zones are location-label policy, unrelated
to ECMA primary-identifier selection. The small shared `timezone_syntax.py`
keeps identifier grammar independent of either localized or supplemental
collectors. Unreviewed deprecated/preferred metazone-ID forms fail explicitly;
metadata release fields remain source facts. Binary and runtime integration of
these supplemental records follow separately.

Optional section 33 encodes these global runtime facts with separate active-ID,
zone-span, period, preferred-zone and dominant-zone tables. Four independent
presence flags distinguish finite endpoints and standard/daylight overrides
from absence, including numeric zero; absent numeric words must be zero.
Finite wire timestamps accept all signed 64-bit epoch seconds, independently
of the collector's narrower textual grammar. Periods retain their source
boundaries, including adjacent equal assignments and intentional gaps.

The codec explicitly projects release metadata out of runtime columns and
retains it in source provenance. Empty timezone spans, unused active IDs and
dominant targets without an explicit metazone row remain valid source facts.
Canonical validation checks exact physical order, namespace-specific identities,
contiguous spans, nonoverlap and historical golden-zone membership. It does not
promise BCP47 cross-section or formatter-capability admission; the collector
checks stable-ID membership, and formatter admission must check its complete
semantic dependencies. This small global section needs no locale projection.
The immutable `datetime/timezone-presentation-data.[ch]` owner validates physical
records and releases all scratch storage before return. Full signed timestamps
are decoded without out-of-range casts; historical membership uses sorted
physical zone/metazone pairs, and syntax checks are cached per string. Exact
allocation-free queries preserve gaps and do not introduce territory fallback.
The source identity grammar is shared with localized timezone-name validation.
Both prepared profiles carry these global supplemental facts without locale
projection. Generation reuses the shared locale identity result and records the
authenticated supplemental source in provenance. Profile tests exercise every
physical period's inclusive start and exclusive end, source gaps, second-precise
Monrovia data, Irish offset overrides and exact preferred/dominant queries.
These records do not install or advertise the DateTimeFormat API.

`datetime/components.h` defines the standard's DateTimeFormat option-field
records separately from DisplayNames field codes and raw CLDR pattern symbols.
Zero-initialized records have no requested components. The shared
`datetime/basic-match.[ch]` implements BasicFormatMatcher penalties, validates
field-specific value domains and retains the earliest equal-scoring candidate.
It allocates nothing and depends on neither locale data nor a selected backend.
Native candidate compilation and best-fit policy remain separate consumers;
providing this algorithm does not install a DateTimeFormat constructor.

Supplemental day-period rules are a separate local-clock data owner from
translated calendar names. Both collectors share only the day-period vocabulary
in `dayperiod_codes.py`. `dayperiod_rules.py` preserves explicit formatting
and selection groups, fixed noon/midnight points, half-open wrapping intervals
and minute precision. Fixed points may overlap the complete variable partition;
fixed AM/PM retain their standard meanings. Unknown/deprecated endpoint forms
fail explicitly rather than being silently changed to inclusive/exclusive forms.
Formatting intervals must not repeat twelve-hour hour labels, including when
minute-aligned endpoints touch more hour buckets than their duration suggests.
Rule inheritance uses simple locale truncation and selects an entire rule list,
independently of the exceptional main-data parents used by localized names.
Keep that source origin and do not merge missing period codes from a parent.
Rounding, fractional exact-point eligibility, `a`/`b`/`B` token policy and missing-label
AM/PM fallback belong to the formatter. Profile and formatter integration follow;
source collection alone does not establish DateTimeFormat capability.

Optional section 34 retains explicit day-period origins in global kind/locale
rows and interns shared immutable rule lists. The version-1 closed code enum
follows `dayperiod_codes.CODES`; forms distinguish exact points and half-open
intervals. Its header is `(version, map_count, rule_count)`; directory count is
the number of three-word kind/locale/map rows. Two-word map spans address
four-word code/form/start/end rules. Maps have contiguous physical spans, at most twelve unique types,
canonical ordering and complete reachability. Empty/point-only lists remain
source facts. A list shared by formatting and selection must satisfy both
referencing kinds, including formatting's clock-hour uniqueness constraint.
Do not expand these small tables into the prepared locale inventory or infer
Intl API support from their presence.

The immutable owner in `datetime/dayperiod-data.[ch]` validates physical bounds,
canonical row/map order, source locale syntax, exact code/form constraints and
variable-day partitions. Format restrictions apply if any formatting origin
references a shared list. Validation releases all scratch on success and failure;
exact-origin, map and rule queries allocate nothing and borrow pack storage.
`datetime/dayperiod-code.[ch]` owns the common C vocabulary used independently
by calendar-name admission and clock-rule decoding. Both prepared profiles
retain the complete global supplemental table, including explicit origins outside
their locale inventory. Each profile uses its own shared string pool and records
the consumed source in provenance. This reader supplies no locale inheritance,
period selection or formatter API.

The native consumer in `datetime/dayperiod.[ch]` resolves one whole rule list
by structural locale truncation, independently of exceptional main-data
parents. Explicit empty/point-only lists stop inheritance and formatting and
selection kinds stay distinct. Clock classification separates fixed a/b from
flexible B, prioritizes exact points over variable ranges and falls back to
AM/PM when no variable period applies. The caller supplies the specified local
record's fractional-presence flag; omitted display fields do not round it.
Translated-name availability and fallback remain renderer policy. Queries
allocate nothing and do not normalize clock values such as 24:00.

The C source compiler in `datetime/pattern.[ch]` separately decodes ordinary
LDML patterns, date/time combination slots and interval ordering directives.
It preserves recognized field symbols, full run lengths and repetitions, while
coalescing decoded literal text into one owned allocation. Input storage may be
released after compilation. Scalar text, quotes and combination slot counts are
validated before allocation. Lexical validity does not imply ECMA candidate
admission: quarter/week fields, long fractions, literal-only patterns, raw
numbering overrides and hour-family adaptation require separate consumer policy.
In particular, retain `a` versus `b`/`B`, `M` versus `L`, all weekday name widths,
all four hour symbols and `r` plus `U` until the renderer has used their meaning.

The allocation-free native projection in `datetime/pattern-info.[ch]` maps an
ordinary compiled pattern to public component widths while separately retaining
its hour cycle, period kind and calendar/related/cyclic year flags. `a` supplies
AM/PM metadata without adding flexible `dayPeriod`; `r` plus `U` share the YEAR
component while retaining distinct output semantics. Numeric `yyyy` does not
impose four-digit ECMA padding, and `rr` or `UU` do not imply two-digit output.
Recognized fields outside the current projection subset return UNSUPPORTED,
not malformed source. Literal-only and AM/PM-only metadata remains valid here;
semantic admission belongs to candidate construction. Raw tokens retain month
context and weekday width 6 for rendering. The initial z/v/O zone subset must
be extended or explicitly synthesized for the pinned Persian generic Z pattern;
Z must not be silently interpreted as z or O.

Within each module, implement the common binding/contract, native algorithms
and data coverage as coherent tested increments. Use feature-disabled
or explicitly incomplete development configurations until the claimed profile is
complete. Preserve saved regressions only when they verify an actual contract;
remove redundant tests after reviewing their coverage, not just their count.

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

ICU4X follows as version three, beginning with the feasibility assessment above;
its implementation remains outside both preceding versions' completion gates.

## Version one validation and acceptance

For implementation commits, local validation is a fresh combined ASan+UBSan
build and bounded repository tests. Prefer JS regressions, std.createRealm and
std.gc; use C for backend contracts, embedding/OOM and inaccessible state.
Full Test262 runs in CI with all available runner threads and per-case timings;
CI supplies sanitizer and platform variants. Track exact revisions and inspect
failures instead of treating queued work as evidence. Keep intentional long
tests, including the requested DST exclusions, explicitly recorded.

Required matrices:

- Intl disabled/minimal/native, each with Temporal on/off where meaningful.
- No ICU4C/ICU4X libraries or Rust toolchain required by these engine builds.
- GCC/Clang, static/shared libraries, export checks, Windows and representative
  32-bit/non-native-endian CI configurations.
- Out-of-tree/cross builds, offline data generation and exact pack reproduction.

Repository regressions cover getter/coercion order, abrupt completion, realm
identity, defaults before/after mutation, bound functions, GC/lifetime, reentrant
calls, workers and failed initialization. Numeric tests include exact large
BigInt/decimal values, negative zero, nonfinite inputs, rounding ties, visible
fraction operands and range identity. Text tests include embedded NUL, lone
surrogates, supplementary characters and original UTF-16 part/segment indexes.

Validate UAX normalization/break data, CLDR inheritance/aliases and every declared
module-locale combination. Compare native with ICU4X/ICU4C on compatible pinned
data, classifying version/policy differences; agreement is not automatic proof
or permission to copy a reference bug. Validate suspected Test262 defects against
the standard. Do not add skips or alter expected-error baselines to hide failures.
Keep repository tests fast through representative boundary cases and separate
exhaustive data audits in CI/tool targets.

Benchmark likely performance changes and otherwise the agreed 25-commit cadence,
against the parent/agreed feature baseline. Use unsanitized GCC/Clang non-LTO,
assertions disabled, fresh JS processes and alternating paired runs. Add Intl
construction/reuse, large-decimal, collation and segmentation workloads as
needed. Report individual/aggregate throughput, cold setup, allocations and
binary/data sizes. Keep ICU4C-versus-native provider cost separate from regressions
introduced within one provider; ICU4C's intrinsic algorithm costs are accepted
under the user's backend policy.

Version-one completion requires all standard APIs and the disabled, minimal
and native profiles implemented, full native locale/data inventory
reconciled, repository/CI results reviewed, no unexplained
correctness failures, reproducible licensed data, and clear provider/version
limitations. Record future-backend interface risks, without requiring ICU4C or
ICU4X adapters, library dependencies or complete CI matrices. Their delivery
plans remain in versions two and three. Runtime backend mixing/loading and
unrelated engine work remain separate follow-ups.
