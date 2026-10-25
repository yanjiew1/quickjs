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

# Intl architecture and ownership

Intl has native C implementations with `minimal` and `native` data profiles.
Bindings expose ECMAScript APIs; independent C modules own locale algorithms,
formatting and data. Meson selects direct implementations at build time. There
is no runtime backend registry, replacement mechanism or provider vtable.

This guide describes the current structure, not an implementation journal or
proof that a particular revision has passed CI. Use the latest applicable
ECMA-262/ECMA-402 living drafts; unmerged Stage 4 features use their Stage 4 PR
text. Exact reviewed revisions, source versions and deliberate interpretation
choices belong with the affected code, manifests and
[native implementation notes](intl-native-notes.md).
[ICU4C v2 and prospective ICU4X v3](intl-future-backends.md) remain separate plans.

## Architecture and ownership

A **functional module** owns a coherent responsibility, such as number formatting
or collation. A **shared module** supplies operations actually used by multiple
consumers; one module can serve both roles, as plural evaluation does. A
`*Service` C type is initialized state, not a separately loaded code module.
An operation handle, such as a Collator or number formatter, owns resolved
configuration. Stateless helpers need no artificial service instance.

The layers are:

1. `src/quickjs/builtins/intl/` owns JS coercion/getter order, slots, realms,
   bound functions, exceptions, GC and result objects.
2. `src/intl/` owns engine-independent C contracts, fixed standard algorithms,
   module composition and native computation. It receives already-coerced values.
3. Owning data readers admit immutable typed views; native implementations
   interpret them. Extraction and source resolution run in host tools, not JS.

| Owner | Responsibility and actual reuse |
| --- | --- |
| `intl/locale` | Identity, canonicalization, matching and preference queries; no implicit dependency on formatter availability |
| `intl/number` | Exact rounding/notation, localized numeric text, currency/unit composition and number ranges |
| `intl/plural` | Exact cardinal/ordinal/range selection, also used by other formatting modules |
| `intl/list` | Resolved list templates and contextual conjunction/disjunction rules |
| `intl/relative`, `intl/duration` | Relative composes Number/Plural; Duration composes Number/List |
| `intl/datetime` | Pattern selection, native format programs, chronology bridge, localized names and date ranges |
| `intl/display` | Display-label selection/composition; metadata presence does not imply calendar arithmetic support |
| `intl/segment` | Native CLDR/dictionary boundaries or minimal Unicode-default boundaries, retaining original UTF-16 indexes |
| `intl/common` | Shared exact values, text/parts builders, template grammars and errors; no generic module dispatcher |
| `intl/native` | QJIN envelope and shared immutable string/locale views |
| `src/calendar`, `src/timezone` | Calendar arithmetic and timezone identity/rules, independently used by Date, Temporal and Intl |
| `src/unicode` | Normalization, properties, case operations and default boundaries, also available with Intl disabled |

API names do not establish module dependencies. A binding can coordinate several
modules without moving those dependencies into the locale foundation. For
example, Locale preference queries return source facts; the binding filters
calendars through DateTimeFormat capabilities or resolves territory zones through
the timezone facade. Each formatter establishes its own available locales and
options. A localized name or an entry in a source registry is not capability
proof, and catalog indexes from different owners are not interchangeable.

Fixed standard algorithms and common policies have shared owners; a selected
implementation may execute a whole operation when its observable semantics are
equivalent. This applies equally to future native/ICU adapters. Avoid artificial
one-file wrappers and common directories containing unrelated responsibilities.

## Private contracts and lifetime

Owning headers expose plain C options, length-bearing strings, exact mathematical
values and typed results. They do not expose JSContext/JSValue or ICU object
layouts. Finite decimal strings and BigInts are not rounded through double merely
for convenience. Negative zero, nonfinite values, embedded NUL, unpaired
surrogates and UTF-16 index units retain their individual contracts.

- Runtime-owned service instances initialize lazily and publish only on success.
  One checked QJSIntlData view per runtime admits shared strings/locales; each
  module still admits its own typed sections, profile and compiled fingerprints.
- Private constructors can copy an unchanged checked native view, or receive
  NULL for standalone admission. Only the immutable backing bytes must survive;
  the caller's view variable need not. Minimal Segmenter opens no data pack.
- Formatter handles own configuration and can outlive their factory where the
  owning header promises it. Borrowed pack slices retain the backing-data
  lifetime; independently owned results use their original allocator.
- Output/reset/close rules belong to each interface. Publish complete objects,
  clear partial results on failure, and keep initialization retryable. Allocation
  errors and malformed data must not become unsupported-value fallback.
- Immutable service configuration does not make mutable cursors or result
  scratch safe to share. Segmenter iterators have independent traversal state.

See the actual headers for per-operation allocator, buffer and output-aliasing
requirements. Generated data has one C owner; checked views need no GC marker or
heap wrapper. No global validation registry or resource-provider framework is
needed for these compiled immutable bytes.

## Defaults, realms and embedding

Default locale and timezone policy belongs to JSRuntime. Contexts share that
host policy; JS constructors, bound functions, exceptions and result objects
belong to the correct realm. Existing formatters retain their snapshots when a
default changes. Default locale selection and embedder overrides must work for
the enabled constructors' capability intersection, not merely a metadata list.

`JS_NewContext()` installs compiled Intl support. Raw contexts first initialize
base objects, then call `JS_AddIntrinsicIntl()`; repeated successful calls retain
intrinsic identity. Construction stages objects before publishing the namespace,
so failed initialization leaves no half-published object and allows retry.
`JS_SetIntlDefaultLocale()` / `JS_GetIntlDefaultLocale()` coordinate the runtime
locale. Date, Temporal and Intl share the existing default-timezone APIs and
coordinator, with one host-discovery/fallback policy. See
[`include/quickjs.h`](../include/quickjs.h) and the
[embedding manual](quickjs.texi) for public contracts.

## Build profiles and data

`intl=disabled|minimal|native` selects one coherent build profile. Minimal uses
reduced native formatting data for `en`/`en-US`, retaining required numeric
systems and calendar/zone semantics. Segmenter is the explicit algorithm
exception: minimal uses Unicode-default boundaries; native executes compiled
CLDR rules and dictionary refinements. Internal root data is not a public locale. `timezone_backend=minimal`
is a different option and does not provide full Intl's named-zone coverage.

Intl can run with Temporal constructors disabled. Shared calendar/timezone and
native duration support follow their actual consumers; Intl need not enable the
Temporal JS namespace. Keep each compiler/configuration in a separate build
directory, including cross builds; host tools run on the build machine.
[BUILDING.md](../BUILDING.md) owns configuration and test commands.

The production pack follows its pinned official CLDR common ZIP. That
publication intentionally filters low-coverage locales. The matching complete
Git snapshot is an explicit alternative; do not silently backfill production
records from it. Both profiles keep their own deterministic inventories and
fingerprints. `intl_data_file` selects a compatible prepared alternative pack.

## CLDR XML collection and resolution

Host Python collectors use ElementTree one file at a time. They interpret known
LDML structures into typed records; there is no generic XML-tree serialization
or runtime XML parser. The checked DTD inventory distinguishes identity and
metadata attributes and supplies required defaults that ElementTree does not
apply automatically.

Shared source resolution owns alias cycles, root/parent inheritance, component
parentLocales and inheritance/blocking markers. Collectors own their group/key
inventories and supported alias paths. Calendar, numbering, main-data and
supplemental components need not use identical parent rules. Preserve source
origin, ordering, significant whitespace, explicit empty values and blocks;
consumer fallback must not resurrect a masked parent key.

Collection, semantic normalization and binary encoding have separate contracts.
Validate relevant fields rather than silently discard unsupported structures.
Pool deterministic strings/records, preserve external alias origins during
profile projection, and admit typed references before use. Independent source
oracles, malformed-reader fixtures and ownership/OOM tests complement JS tests.

[Data maintenance](data-updates.md) owns acquisition, generation and source-pin
updates. Acquisition authenticates accepted sources; generators accept compatible
supplied inputs, check semantics and record actual-byte receipts. See the
[tool map](../tools/intl/README.md), [QJIN format](intl-data-format.md) and
[section reference](intl-data-sections.md) for the corresponding owners. Algorithm
rationale belongs in [native notes](intl-native-notes.md); future backend
requirements belong in [their plan](intl-future-backends.md).
