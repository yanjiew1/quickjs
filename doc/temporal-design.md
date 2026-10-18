# Temporal and native calendar and timezone design

Temporal is implemented in C, with independent calendar and timezone libraries.
The native configuration supplies its own data and algorithms without ICU or
installed system timezone databases. This design records the agreed target;
implementation progress and validation belong in working notes.

## Specification and implementation sources

Use these sources in order:

1. The latest ECMA-262 and ECMA-402 living drafts, including the latest
   specification text in relevant unmerged Stage 4 pull requests.
2. Standards and authoritative publications referenced by ECMA, then relevant
   Unicode, CLDR and calendar specifications for details ECMA leaves open.
3. ICU4X implementations and their cited algorithm sources where the preceding
   sources do not give sufficient algorithms.
4. ICU4C implementations and documentation for comparison and remaining details.

For Temporal while Stage 4 remains unmerged, the PR specification is primary.
Resolve apparent specification defects using its PR discussion first, then the
Temporal proposal specification. If these still do not settle a question,
choose a reasonable interpretation and record the reasoning and focused tests.
Do not silently treat a reference implementation as normative.

An implementation's newer release does not establish conformance. Check the
chosen model, published data, negative years, division, range and overflow
semantics against the specification. Review reusable QuickJS and QuickJS-NG
work under the same rules. Credit adapted code, retain applicable licenses and
copyright notices, and identify the actual source revision in its commit.

Reviewed specification baselines on 2026-10-18:

- [Temporal Stage 4 PR](https://redirect.github.com/tc39/ecma262/pull/3966),
  head `3d4a6e7124a6878cb5af3132af7e01e01a88317f`.
- [Temporal and Intl era and monthCode Stage 4 PR](https://redirect.github.com/tc39/ecma402/pull/1044),
  head `d026068ed653130a37d79a95c0122ecf5319f1a4`.
- [ECMA-262 Date and time algorithms](https://tc39.es/ecma262/multipage/numbers-and-dates.html).
- [ECMA-402 IANA database requirements](https://tc39.es/ecma402/#sec-use-of-iana-time-zone-database).
- [Unicode LDML dates](https://unicode.org/reports/tr35/tr35-dates.html).

Recheck these revisions when implementation starts and record changes. Algorithm
notes identify stable specification anchors, reference revisions, review dates,
exact publication coverage and any permitted approximation.

### Reviewed draft discrepancy

The pinned Temporal PR contains a known off-by-one guard in
ISODateTimeWithinLimits. Its description and the published proposal retain
PlainDate's lower date of -271821-04-19, and PlainDateTime starts one
nanosecond after that date's midnight. Keep those bounds, including when
converting the minimum Instant with a negative offset. The
[PR review identifying the lost day](https://redirect.github.com/tc39/ecma262/pull/3966#discussion_r4283470645)
was checked on 2026-10-19. Test262's PlainDate and PlainDateTime limit tests
agree with the wider range; they do not need a local patch.

The same pinned PR's GetAvailableNamedTimeZoneIdentifier reads
[[PrimaryIdentifier]] from an Available Named Time Zone Identifier Return
Record, whose fields are only [[Identifier]] and [[Result]]. Primary identity
belongs to a nonempty result's Time Zone Identifier Record. Keep immutable
identifier/primary mappings directly; do not reproduce that invalid field
access or dereference an empty cached result. Source: temporal/timezone.emu
at the pinned PR head, reviewed 2026-10-19.

GetTemporalUnitValuedOption in the pinned PR also spells millisecond and
milliseconds with three consecutive l characters. Retain the correctly
spelled unit names used throughout the rest of Temporal and by Test262.
Source: temporal/abstractops.emu, reviewed 2026-10-19.

## Code ownership and backend selection

- JavaScript builtins own coercion and property access order, realms, exceptions,
  JS values and GC integration.
- A pure C Temporal library owns parsing, formatting, exact epoch arithmetic,
  durations, rounding and common calendar and timezone operations.
- A pure C ISO core owns Gregorian civil-date primitives. Date and timezone
  recurrence algorithms can reuse it without linking non-ISO calendars.
- A calendar facade routes ISO operations to the ISO core and other calendar
  operations to the selected extension backend.
- The common timezone library owns mandatory UTC; Temporal also handles
  numeric offsets in common code. Backends supply additional named-zone rules.
- Host discovery obtains the default zone identifier or fallback offset.

Common private headers define backend functions and opaque state. Build-time
source selection chooses one implementation of each backend contract. A backend
may span many translation units with its own private header. There is no runtime
backend registration, loading, replacement or dispatch table. Runtime selection
of a calendar or timezone identifier remains available within that backend.

Proposed build profiles:

| Option | Meaning |
| --- | --- |
| `temporal` | Include Temporal bindings and algorithms |
| `calendars=iso` | ISO core, without a non-ISO backend or its data |
| `calendars=native` | ISO core plus every standard native calendar |
| `timezone_backend=native` | Native named zones from embedded IANA data |
| `timezone_backend=minimal` | No additional zones; UTC and numeric offsets remain in core |
| `system_timezone_policy=utc` | UTC fallback for unresolved default discovery |
| `system_timezone_policy=host-offset` | Current host offset fallback, then UTC |

These names are design targets, not a statement that existing Meson options
already implement them. Future ICU backends implement the same private
contracts and supply their own data and algorithms. They are not required for
the native implementation. Disabling Temporal does not remove Date's timezone
requirements; build dependencies must remain explicit.

## Native timezone contract

The common layer owns UTC lookup and behavior for every backend. A minimal
backend has no names or rules. It remains a complete implementation choice
because UTC and numeric offsets are always available in common code.

Backends supply additional identifiers and immutable opaque rule handles.
They exclude canonical UTC from their enumeration; the common layer inserts
it in sorted order. A named alias whose primary identity is UTC preserves
its alias spelling but uses the core UTC rules. A political zone whose offset
is currently zero still retains its own rules. Named UTC remains distinct
from numeric +00:00.

The backend supports identifier lookup, primary identifiers, enumeration,
offset-at-instant, local-time candidates and gap boundaries, strict next and
previous offset transitions, and data-version queries. Common Temporal code
applies earlier, later, compatible and reject disambiguation policies.

Preserve historical second-level offsets, behavior before the first transition,
future recurrence rules, negative fractional instants and the entire Temporal
domain. Exact instant arithmetic must not narrow epoch nanoseconds to int64.
Use checked portable wide arithmetic, or normalized seconds and a nanosecond
remainder at the backend boundary. Do not assume local times universally have
at most two candidates. Preserve requested identifiers where required instead
of replacing every alias with its primary identifier in observable values.

Use a pinned released IANA dataset, including geographic Rule/Zone sources,
backward links, backzone and zone.tab information needed for complete names and
primary mappings. Retain pre-1970 data where supplied, while documenting IANA's
historical accuracy limits. Do not apply leap-second corrections. Deduplicate
identical payloads without merging distinct public identities incorrectly.

Native runtime code does not link tzcode. A pinned build-host zic may be used
as a generation tool; its per-zone intermediates stay in the build directory.
The final timezone pack is a single file. Building that pack must not execute
target binaries or depend on the build machine's installed zic or zoneinfo.

## Default timezone policy and Date integration

Native discovery first tries a supported identifier. On Unix, parse TZ names,
optional leading colons and supported fixed-offset forms with POSIX sign rules.
Do not guess a geographic zone from a general POSIX DST expression. Do not
read /etc/localtime, /etc/timezone or installed zoneinfo for native rule lookup.

On Windows, obtain the stable TimeZoneKeyName with
GetDynamicTimeZoneInformation. Obtain the territory using GetUserGeoID and
GetGeoInfoW with GEO_ISO2. Use the CLDR Windows-key/territory mapping, falling
back to territory 001, and the first listed mapping when several are supplied.
Localized display names are not identifiers. Document how custom or disabled
Windows DST settings interact with IANA rules.

The selected system_timezone_policy applies when native discovery cannot map
the host setting; minimal mode uses it directly. Host-offset mode samples the
current offset and exposes a valid fixed-offset identifier. It provides no
historical or future DST rules. On Unix, obtaining that offset through libc may
depend on installed system data; this is an explicit opt-in discovery fallback,
not a dependency of native timezone calculations. If a valid minute-aligned
identifier cannot be represented, use UTC rather than rounding the offset.

Date and Temporal use the same service and a consistent zone snapshot during
each operation. Existing ZonedDateTime values retain their stored identifiers.
The system clock is separate and shared with Date.now; obtaining an instant
does not require a timezone backend. Explicit invalid or unsupported timezone
arguments raise the specified error. OOM and corrupt data are errors, not UTC
fallback. Default host discovery failures are handled separately.

## Calendar scope and algorithms

The standard native set is buddhist, chinese, coptic, dangi, ethioaa, ethiopic,
gregory, hebrew, indian, islamic-civil, islamic-tbla, islamic-umalqura, iso8601,
japanese, persian and roc, with specified aliases. Do not infer the supported
set from all historical CLDR registrations: islamic and islamic-rgsa are not
in the reviewed Stage 4 calendar table.

Group implementations by shared algorithms and state ownership: Gregorian
derivatives, fixed-cycle solar calendars, Hebrew, Islamic, Persian and East
Asian lunisolar algorithms. Share astronomy helpers only where their models
are genuinely common. Calendar algorithms do not inherit the user's current
timezone or locale. A native calendar needs no Intl formatting data.

The contract includes epoch-day conversion, arithmetic years, eras and era
years, ordinal months and monthCode, leap-month handling, year/month metadata,
field resolution and primitives for calendar-specific add and difference.
Temporal's observable coercion and overflow processing retain their specified
order. Keep intermediate bounds distinct from final Temporal range checks.
Advertise only fully implemented and validated capabilities.

| Calendar | Authoritative data and range | Outside that range |
| --- | --- | --- |
| chinese | PMO publication, Gregorian 1900 through 2100 | Documented approximation |
| dangi | KASI publication, Gregorian 1900 through 2050 | Documented approximation |
| persian | Iranian calendar authority, 1206 through 1498 AP | Documented approximation |
| islamic-umalqura | KACST month data, 1300 through 1600 AH | islamic-civil |
| japanese | Government era dates with ECMA's date/era semantics | Follow the specified proleptic model |
| Other standard calendars | Specified arithmetic, epochs and era rules | Specified proleptic model |

Chinese and Dangi publication coverage must include calendar-year boundary
information needed for complete fields and arithmetic. Label derived successor
boundaries as derived. Do not substitute a small modern table for calculations
over the rest of Temporal's domain. Avoid huge per-year expansion when an
appropriate algorithm and bounded cache suffice.

Additional authorities, including Taiwan's CWA and Hong Kong's HKO, provide
cross-checks. Keep their datasets distinct: different calendars, models,
publication versions and near-midnight predictions can disagree. Differences
are investigated against the specification and designated primary source;
never resolve them by majority vote or silent table splicing. Agreement between
ICU4X and ICU4C is not independent publication evidence if they share a source.

## Data acquisition and reproducible generation

Use three layers:

1. Official source archives, documents or captured publications.
2. Reviewed normalized text or JSON with a stable project-owned schema.
3. Deterministically generated calendar and timezone binary packs.

Store normalized calendar publications and source manifests under data/calendar
and data/timezone. Small redistributable source documents can be kept there;
large downloaded archives and parsing intermediates use a caller-selected
cache or build directory. Use relative repository paths. Normalized source
data and final packs are versioned; generated embedding C lives in the build
directory and is compiled once. A header contains declarations only.

Each dataset records source identity, official URL, publication/release,
SHA-256, coverage and units, source page/table locations, conversion method,
license and attribution, and applicable specification revision. Identify
transcriptions, mirrors, generated approximations and independent validation
sources accurately. Hosting a third party's table does not establish authorship
or a license for its code.

Manual transcription is acceptable for unstable HTML or PDFs. Commit the
normalized input and enough source evidence to review it. Rebuilding the
binary must work offline without private captures or an unpublished parser.
Hash verification proves identity, not transcription accuracy; verify the
actual values and complete coverage independently.

Provide explicit acquisition, verification and generation tools. Acquisition
uses HTTPS, pinned versions/hashes, finite retries and timeouts, atomic output
and a caller-selected cache. Validate content instead of accepting an HTML
error page as a PDF. Updating to a new release is an explicit operation, not a
normal build side effect. Normal builds need no network.

Generators validate ordering, gaps, duplicates, coverage, valid month lengths,
leap-month placement, year boundaries, alias resolution and checked integer
arithmetic. Emit deterministic ordering and no changing timestamps or host
paths. Preserve upstream raw sources rather than overwriting them with
normalized representations.

## Binary data and memory ownership

Use one timezone-data.bin and one calendar-data.bin. They have explicit byte
order, checked offsets and lengths, a format version, source versions and
algorithm compatibility information. Do not serialize native structs or
pointers. Share strings and repeated records; use bitsets and indexed deltas
where appropriate without introducing compression-library dependencies.

Embedded data is the initial default. Readers accept a const byte span and
length, allowing a later external-file loader to reuse the same algorithms.
External loading is not part of the initial implementation and must not turn
into per-query filesystem I/O or runtime backend switching.

Compiled data is immutable and may be shared process-wide. Mutable caches have
an explicit runtime owner and bounded lifetime. Avoid common-query allocation
and needless copies. Keep the data snapshot stable for a runtime's lifetime.
Provider handles and borrowed records must not outlive their owner. Keep
backend errors distinct from unsupported identifiers and allocation failures.

## Validation and staged delivery

Add fast repository tests for each algorithm, publication adapter and feature.
Verify source data separately from the binary writer. Include malformed data,
complete mandatory publication ranges, range-boundary neighbors, leap months,
eras, negative years, round trips, add/difference behavior and exact transition
boundaries. Differential checks against ICU supplement authoritative fixtures;
they do not override specification requirements.

CI regenerates packs and compares them exactly with committed results, using
pinned inputs. Upstream-release discovery is a separate check from reproducible
generation. Check native 32/64-bit builds, endian handling, Windows mapping,
cross builds and exported symbols. Full Test262 runs in CI using available
threads; local implementation gates use combined ASan+UBSan repository tests.
Remove skips made obsolete by completed features. Benchmark likely Date and
Temporal performance changes against the parent with assertions disabled.

Implementation order is mandatory:

1. Complete the minimum conforming Temporal implementation first: all standard
   Temporal types and operations, ISO calendar, UTC and explicit fixed-offset
   timezones, without IANA data, non-ISO calendars or ICU. Integrate Date's shared
   timezone policy and validate the minimal configuration independently.
2. Add native IANA timezones, their reproducible acquisition and binary tools,
   complete named-zone semantics, and Unix/Windows host discovery.
3. Add the other standard calendars, their authoritative publications, source
   normalization and binary generation. Start with simple arithmetic families,
   then deliver each publication-backed calendar separately.
4. Complete conformance, independent review and documentation for the full
   configuration while retaining coverage of the minimal configuration.

Make small coherent commits within each stage. Public APIs accompany the feature
that needs them. Prefer mechanical reviewed reuse from temporal-bak over
duplication, while replacing its system-first provider and incomplete source
acquisition paths. Keep main unchanged.
