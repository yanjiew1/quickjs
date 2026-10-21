# Native Intl data tools

These Python 3.6 or newer host tools prepare immutable source data for native
Intl generation. Normal builds use prepared binary data and do not download
or parse XML.

See [the binary format guide](../../doc/intl-data-format.md) and
[section reference](../../doc/intl-data-sections.md) for the envelope, exact wire
layouts, size reductions and format maintenance rules.

Canonical-era presentation metadata is collected by `calendar_eras.py` and
encoded by `calendar_era_tables.py` in optional section 35. It retains explicit
CLDR `calendarData` code/type pairs independently of locale inventories and
calendar arithmetic. Japanese raw IDs are source values, not array positions;
cyclic calendars without canonical era codes do not acquire a fictitious era.

## Acquire the pinned inputs

```sh
python3 tools/intl/acquire.py --cache /path/to/intl-cache
python3 tools/intl/acquire.py --cache /path/to/intl-cache --offline
```

`pin.json` identifies CLDR 49 beta 4 by release tag, commit, archive SHA-256 and
the SHA-256 of its complete source manifest. This prerelease was deliberately
selected for Unicode 18 support. An upgrade must replace the pins and regenerate
and validate the data; the tool never follows a development branch or `latest`.

The archive comes from the complete release source tree. Its `common/` files
and license are materialized under `cldr-49-beta4` in the cache. The generator
can therefore apply explicit draft/alternate selection instead of inheriting
the production ZIP's filtering. Java source and other programs in the archive
are not executed or installed.

Acquisition verifies the archive and the complete file inventory before writing
source files. Existing correct files are retained; missing or modified cache
files are repaired from the authenticated archive, including in offline mode.
Downloads, member counts, expanded sizes and individual files have bounds.
Archive links, unsafe paths and source-cache symlinks are rejected.

## Consume verified source files

`collation_root.py` validates the authenticated `common/uca/allkeys_CLDR.txt`
root table. It retains contractions, expansions, all weight levels and variable
flags, then orders mappings by scalar sequence for deterministic lookup-table
generation. Its audit reports observed sizes without making those sizes runtime
limits. `collation_tables.py` encodes the optional section 36 and audits a full
source roundtrip. Neither prerequisite executes locale tailorings or advertises
Collator support. Both prepared profiles carry the same root mapping section;
the allocation-free C reader is `src/intl/collation/root-data.c`. Full locale
tailorings and the comparator remain separate runtime work.

`collation_groups.py` collects source group identities for the host semantic
compiler. It joins FractionalUCA's synthetic boundaries with pinned CLDR script
samples and explicit reordering-token metadata, without interpreting comments or
equating scripts just because they share a lead byte. It retains the first real
mapping key after each boundary, skipping numeric/reserved markers. Hiragana/
Katakana and the two Meroitic scripts share groups; Han aliases are retained.
Composite sample metadata alone does not create a reordering alias.

These are build-time facts, not a new executable pack section. Implicit anchor
keys are deliberately unresolved: the semantic compiler must use the same
root/implicit-weight implementation as execution, rather than zip fractional
CEs with allkeys CEs. The generic-unassigned boundary has no real key or
separately selectable script identity. Fixed U+FFFE/trailing behavior remains
outside the reorderable group list. Authored tests cover aliases, missing real
anchors, version/ordering errors and coarse lead-byte collisions; CI audits the
complete pinned catalog.

### Host collation compiler input

`collation_ir.py` expands the ordered program catalog into a counted build-time
artifact. `intl-collation-compile --audit-ir input.qjci` reads it using explicit
little-endian loads, runs text operands through the engine's pinned Unicode NFD
implementation, and requires every scalar inside starred ranges to be NFD-inert
before normalization. Compatibility decompositions do not disqualify a scalar;
canonical decompositions, Hangul syllables and nonzero combining classes do.
Starred range order remains significant and is not sorted by the serializer.

This is the compiler input/normalization stage, not completed semantic weight
allocation. It does not execute settings, prove prefix normalization boundaries,
or emit executable locale tailorings. Python retains ownership of XML, syntax,
parents, type aliases and import expansion; it never substitutes its host Unicode
version for the C algorithms. Normal builds still consume prepared runtime packs.
The tool runs on the build machine in cross builds; native local sanitizer builds
also instrument its C reader and normalization paths.

The private `QJCI` version 1 layout is independent of the runtime `QJIN` format.
All integers are unsigned little-endian 32-bit values. Record sizes include their
headers, labels are counted UTF-8 without C0/DEL characters, and there is no
alignment padding:

| Record | Fields |
| --- | --- |
| File header | `QJCI` magic, version, Unicode version (`18 << 16`), total bytes, program count |
| Program | record bytes, locale bytes, collation bytes, alternate bytes, instruction count; then the three labels and instructions |
| Instruction | record bytes, kind, auxiliary, rule-text line, rule-text column, source-label bytes; then the source label and three scalar vectors |
| Scalar vector | element count, followed by that many Unicode scalar values |

Instruction kinds are reset (1), relation (2), starred relation (3), setting (4),
suppression (5) and optimization hint (6). Reset vectors are text/special-name/
empty, with auxiliary `before` strength; relation vectors are prefix/text/
extension, with auxiliary relation strength. Range instructions carry inclusive
endpoint pairs in the first vector; only starred relations have a strength.
Setting vectors contain name/NUL-terminated values/empty, with zero auxiliary.
Imports must already be expanded. Source positions refer to original rule text,
not invented XML line coordinates. Artifacts and normalization allocations are
bounded; starred expansion has per-program and artifact-wide work budgets,
checked in a non-allocating preflight before normalization. A format change must
update the Python writer, C reader, regression fixtures and this description.

`collation-weights.c` provides the compiler's sparse symbolic ordering arena.
Root IDs denote virtual ordinal coordinates without allocating root nodes.
Tailored IDs remain stable while adjacent insertions accumulate in hashed root
gaps. Freezing sorts only touched gaps, assigns final ranks, and records sparse
prefix offsets for resolving every unchanged root. A root precedes insertions
in its own gap, preserving the zero sentinel and common root aliases. Node
provenance records the stronger parents; copied mappings can retain IDs even
when another mapping is replaced.

This generic arena does not decide relation strength, ignorable bands, logical
reset extrema, case policy or implicit-primary coalescing. The semantic compiler
must supply those rules. IDs and final ranks are different domains: bit 31
marks a tailored ID, but an ordinary root's final rank may also cross that bit.
Allocation failures preserve the ordering and permit retry. A bounded chain
test matches the pinned stroke tailoring's 102,038 insertions without creating
millions of per-program root nodes.

The shared `src/intl/collation/logical.c` root adapter coalesces each implicit
head/continuation pair before assigning a logical primary coordinate. A tail
that numerically resembles another head is still consumed once. Head secondary/
tertiary weights and explicit variable/implicit metadata survive conversion.
Malformed pairs and standalone WF1 violations are rejected.

Ordinary `P << 16` and implicit `(head << 16) | tail` keys map arithmetically to
7,471,103 virtual nonzero root coordinates, with zero remaining ignorable. The
domain includes unused anchors but preserves real root order; it needs no
per-codepoint table. Exhaustive ordinal adjacency tests and both prepared
profiles' actual NFD/lookup streams validate this bridge. Current production
comparison still uses raw root weights; compiler/ranked comparison integration
is subsequent work.

Collectors use `acquire.Sources` with the cache's `cldr-49-beta4` directory.
The reader authenticates the manifest before use, verifies each consumed file,
and tracks logical input paths for generated provenance. Cache paths and file
timestamps are not provenance. The XML interpretation, inheritance resolution
and binary encoding are separate layers described in
[the Intl design](../../doc/intl-design.md#cldr-xml-collection-and-resolution).

Tests in `tests/test_intl_sources.py` use small authored archives and perform no
network access. CI also acquires the real pinned archive. These checks establish
source reproducibility; they do not establish formatter or locale coverage.

## XML and attribute schema audit

```sh
python3 tools/intl/cldr_xml.py --sources /path/to/intl-cache/cldr-49-beta4
```

The shared XML layer accepts UTF-8, preserves element text and child order,
does not fetch external DTDs, and rejects internal entity declarations. Its
restricted DTD reader supplies declared attribute defaults and distinguishes
identity, value and metadata attributes. Consumed nodes reject unknown,
missing required, changed fixed and invalid enumerated attributes.

This is not general DTD validation: each typed collector checks its consumed
child structure, identifier/number grammar, draft/alternate policy, aliases
and inheritance. CDATA whitespace is preserved; token attributes collapse
U+0020 after XML parsing and preserve character-reference whitespace.
Collation rules and other ordered children must retain
their semantic order instead of being sorted by an unordered record key.

The audit checks the three pinned attribute schemas and well-formedness of
main, BCP47 and supplemental XML. It reports counts separately from service
coverage. `tests/test_intl_xml.py` exercises authored schema/XML fixtures.

## Locale inheritance graph

```sh
python3 tools/intl/parents.py --sources /path/to/intl-cache/cldr-49-beta4
```

The shared graph retains source locale identities, default-content locales,
likely subtags and main/component parent overrides. Component maps stand alone,
including empty maps. The `nonlikelyScript` rule applies to every matching
language/script pair, rather than just its migration examples. Explicit parent
exceptions take precedence. Parent chains must terminate at root without cycles.

Graph nodes and source XML filenames are not advertised service availability.
Each collector still establishes its own complete records and capabilities.
`tests/test_intl_parents.py` checks authored graphs, including exceptions,
component independence and rules outside the listed migration examples.

## Typed fields and list templates

```sh
python3 tools/intl/list_patterns.py --sources /path/to/intl-cache/cldr-49-beta4
```

`resolve.Records` resolves a finite inventory of typed fields, including explicit
inheritance and empty-override markers. An inherited `source="locale"` alias
restarts lookup at the requesting locale; an explicit locale alias changes
that lookup anchor. Cycles, unknown keys and mixed records fail explicitly.
Collectors translate supported XPath forms into field mappings rather than
retaining XML trees or expanding arbitrary paths during resolution.

The list collector reads all source locales and resolves each of the nine
standard type/style combinations and four template parts. It selects standard
records without `alt`, admits all published draft states, and reports omitted
alternates. Text is preserved exactly and both placeholders must occur once.
Unknown relevant structures, mixed aliases, duplicate fields and missing or
blocked required templates are errors. These templates do not alone implement
context-sensitive list behavior or install a JavaScript formatter.

## Date/time pattern syntax

```sh
python3 tools/intl/date_pattern_syntax.py --pattern "yyyy-MM-dd 'at' HH:mm"
python3 tools/intl/date_pattern_syntax.py --combination "{1} 'at' {0}"
python3 tools/intl/date_pattern_syntax.py --sources /path/to/intl-cache/cldr-49-beta4
python3 tools/intl/date_pattern_syntax.py --sources /path/to/intl-cache/cldr-49-beta4 --combinations
```

The DateTimeFormat data compiler's lexical prerequisite decodes TR35 apostrophe
quoting into immutable literal and field tokens. Fields retain their exact source
symbol and run length; literals retain scalar text, whitespace and bidi controls.
Repeated fields remain separate for interval patterns. Unassigned ASCII letters,
input-skeleton-only `j`/`J`/`C`, unclosed quotes, NUL and non-scalar text fail.

The source command audits all raw date/time length patterns, available-format
values and greatest-difference interval values, across calendars and alternates.
It reports inheritance/block markers separately and does not resolve aliases.
Combination/append templates have their own grammar and are excluded. This audit
does not validate field widths, interpret `numbers` overrides, select formats,
establish locale coverage or install DateTimeFormat. Those operations belong to
the subsequent typed collector/compiler and native service. Authored tests run
in the repository suite; CI also audits the pinned CLDR source patterns.

`compile_combination` handles date/time style connectors separately. It requires
one `{0}` time slot and one `{1}` date slot, preserves their order, and applies
date-pattern apostrophe quoting to the surrounding text and braces. Slots are
tokens with symbol `0` or `1`, zero width and empty text; fields and decoded
literals retain the ordinary token representation. Quoted markers remain literal
text. The `--combinations` audit checks raw style connectors across calendars,
forms and alternates, distinguishing inheritance/block markers from patterns.
Alias resolution, style selection and placeholder substitution have later owners.

## Date/time style source records

```sh
python3 tools/intl/date_patterns.py --sources /path/to/intl-cache/cldr-49-beta4
```

This typed collector resolves full/long/medium/short date and time styles for
every source calendar. Pattern and `datetimeSkeleton` records inherit separately;
each retains its own `numbers` override and alternate identity. Overrides remain
source text for the subsequent data compiler to interpret. Metadata attributes
do not create extra records, and all published draft states are admitted.

Whole-calendar and date/time family aliases map typed keys, with inherited
`source="locale"` aliases restarting at the requesting locale. Explicit-source
aliases change that anchor. Missing, blocked, inherited and explicitly empty
values remain distinct; terminal locale/calendar/family/width/field/alternate
origins accompany resolved values. Duplicate fields, mixed aliases, cycles and
unsupported consumed structures fail. Calendar names, available-format maps,
intervals and combination templates have separate owners.

The collector produces host-side resolved facts for the prepared style section;
these facts do not establish formatter capability. Raw CLDR calendar IDs include
data-only `generic`; they must not be advertised as supported arithmetic calendars.
Authored repository tests cover inheritance, aliases, provenance, overrides,
markers and invalid data. CI additionally resolves the pinned production source.

Section 27 reserves the optional style wire schema. Its little-endian words begin
with version, calendar count, alternate count, shared-row count and shared-record
count. Calendar/alternate string references precede one row index per locale.
Shared rows use calendar, date/time family, full/long/medium/short width,
pattern/skeleton field and alternate as nested column selectors. Nine-word
records contain state, text, numbering override, source locale, and source
calendar/family/width/field/alternate indices. Missing records are all zero;
blocked records retain provenance with empty text and numbering override.

`date_style_tables.py` pools identical records and locale rows canonically and
validates exact coverage, ordering, reachability, dimensions and references.
Profile projection retains source identities outside the selected locale list.
The generator emits this section in both prepared production profiles. Pattern
text and numbering overrides remain source facts for the later date-format
compiler to interpret.

`src/intl/datetime/style-data.c` validates the optional section against an opened
shared data owner. It checks bounded dimensions, calendar/alternate registries,
canonical row/record sharing, reachability, states and source identities. Queries
borrow immutable text, raw numbering overrides and provenance without allocating;
opening retains no allocation and requires no teardown. Missing, blocked and
empty values remain distinct. Authored C tests exercise every query selector,
unaligned input, corrupt records/dimensions and allocation failure with retry.

## Date/time combination source records

```sh
python3 tools/intl/date_combinations.py --sources /path/to/intl-cache/cldr-49-beta4
```

The typed connector collector retains standard, atTime and relative forms at
each calendar/style width, including alternate identities, raw numbering
overrides and terminal provenance. It maps whole-calendar, dateTimeFormats and
per-form aliases to typed keys, preserving requesting-locale and explicit-source
resolution. Missing, blocked, inherited and explicitly empty values stay distinct.
Unsupported structures, mixed aliases, duplicate records and cycles fail.

`calendar_sources.py` owns structural calendar traversal and alias/container
validation shared with the style collector; individual collectors own their
subtrees, typed keys and XPath mappings. Combination patterns use date-pattern
apostrophe rules, not the shared unit/list pair-template grammar. Selection of
atTime/relative versus standard is consumer policy after inheritance, not a
collector fallback. Both prepared profiles include these source records; they
do not install DateTimeFormat or establish calendar capability.

Section 28 is the optional canonical connector schema. Its little-endian header
contains version, calendar count, alternate count, shared-row count and shared-
record count. Calendar/alternate string references precede per-locale row indices.
Rows use calendar, full/long/medium/short width, standard/atTime/relative form and
alternate as nested selectors. Eight-word records retain state, text, numbering
override, source locale and source calendar/width/form/alternate indices. Missing
is all zero; blocked retains provenance with empty text and numbering override.
`date_combination_tables.py` validates exact coverage, canonical pooling, all
references and reachability; projection retains origins outside selected locales.
`src/intl/datetime/combination-data.c` validates these records through a borrowed
immutable owner, using temporary scratch with no retained allocation. Queries
return exact borrowed text, overrides and source metadata without allocation or
form fallback. Authored C tests cover every selector, corrupt tables, unaligned
input and validation allocation failure with retry. Prepared-profile C tests
exercise every selector and representative English connector forms.

## Relative-time source records

```sh
python3 tools/intl/relative_patterns.py --sources /path/to/intl-cache/cldr-49-beta4
```

The collector resolves the eight RelativeTimeFormat units in long, short and
narrow widths. It retains terminal locale/unit/width origins, restarts inherited
field aliases in the requesting locale, and distinguishes missing, blocked and
explicitly empty values. Default containers are selected before duplicate checks;
unsupported scoped records and whole-container aliases fail explicitly.

Counted past/future patterns may contain zero or one `{0}` placeholder. Auto
relative literals preserve braces as text, and retain every discovered canonical
integer key rather than assuming only yesterday/today/tomorrow. Authored tests
cover alias inheritance, container selection, markers, malformed source and key
boundaries. These records are source facts; collecting them does not install RelativeTimeFormat.

Section 23 reserves the resolved relative-time wire schema. It uses little-endian
words: version, literal-key count, locale-row count, map count and entry count;
then pooled literal keys, per-locale row indexes, shared 24-column rows, contiguous
map start/count pairs, and four-word entries. Columns use the collector's unit
order, then long/short/narrow width. Selectors 0–11 are past/future plural counts;
12 and above index the sorted literal keys. Entries retain selector, text string
(or `0xfffffffe` for blocked), source locale string and source unit/width column.
Missing selectors are absent; pooled empty text remains a value.

The codec checks exact coverage, canonical sharing/order, contiguous extents,
source identities, integer keys and required long `other` forms. Profile
projection prunes unused maps/keys but keeps external source identities. The production generator emits this section in both prepared profiles. It is
not advertised as a RelativeTimeFormat capability until a complete service exists.

`src/intl/relative/data.c` validates the schema in C before exposing immutable
maps and allocation-free exact pattern/literal queries. It borrows the shared
pack, releases validation scratch on success and failure, and retains no heap
state. Source identities may lie outside the selected locale registry. Authored
and both prepared-profile tests cover queries, unaligned storage, malformed
records and every reached validation allocation failure.

The relative renderer in `src/intl/relative/format.c` implements `MakePartsList`
without choosing locale, tense or plural category. Its dedicated part records
attach a unit to every inserted number span, including number-origin literal
spans; surrounding template text has no unit. Automatic literals bypass template
parsing and always have one part, even when empty. Counted patterns may omit the
number entirely. Output owns its UTF-16 text and parts, with explicit bounds,
allocation-failure cleanup and no retained input storage.

`src/intl/relative/native.c` composes the Number and Plural service interfaces.
It maps canonical locale tags across their inventories explicitly, validates
all unit/style groups, and delegates numeric-system capabilities to Number.
Formatters own their dependency handles and survive service closure. Auto
literal lookup precedes rounding; negative zero keeps the past tense for numeric
output. Plural-category misses use the CLDR `other` form, while an absent entire
style uses long data. Operational and malformed-data errors are not fallback.

There are two relevant inconsistencies in ECMA-402 revision
`730ca2b88c305862db7be2f83c510dcca33ab48c` (reviewed 2026-10-19):

- `PartitionRelativeTimePattern` passes signed `real(value)` to number formatting
  without taking its magnitude, despite past/future expressing the direction.
  Native output inserts unsigned magnitude, matching established RelativeTimeFormat
  behavior and the corresponding Test262 negative-value cases.
- Its `ResolvePlural` call still passes a Number, although that operation now
  takes an Intl mathematical value. [The PluralRules change](https://redirect.github.com/tc39/ecma402/pull/1026),
  merged as `7344f4219a54dad6ecfe261d121f7463e016726c` on 2025-12-05,
  removed the old internal `real(n)` conversion and added a different conversion
  at public `select`/`selectRange` boundaries, without updating this caller.
  Relative's internal number and plural formatting therefore share the exact
  real magnitude, preserving the pre-change algorithm. This is a conservative
  interpretation of the stale call, not an explicit editor clarification.
  Converting only the plural input through public `select`'s shortest decimal
  can make Number `1.0005` display `1` with the plural word `days`.

The source, renderer and service layers do not implement observable JavaScript
coercion, options or branding. Those belong to the engine binding described below.

## Region and script preferences

```sh
python3 tools/intl/preferences.py --sources /path/to/intl-cache/cldr-49-beta4
```

The preference collector reads regional week boundaries, calendar orders,
language/region hour-cycle overrides, and script direction metadata. World
week defaults fill missing fields; wraparound weekends become sorted ISO
weekday lists. Standard first-day records take precedence over alternate
variants. Hour cycles retain the preferred cycle first and deduplicate the
remaining allowed patterns without losing their preference order. Unknown
script direction remains distinct from left-to-right.

These source records do not establish formatter availability. Consumers still
apply locale region/subdivision precedence, calendar aliases and supported
calendar/formatter capabilities. Tests use authored fixtures and need no
network access.

## Numbering metadata

```sh
python3 tools/intl/numbering.py --sources /path/to/intl-cache/cldr-49-beta4
```

The numbering collector checks numeric systems as ten distinct Unicode scalar
values, including supplementary-plane digits. Algorithmic systems retain
their rule references; collecting a reference does not implement that rule.
The locale graph resolves default, native, traditional and financial numbering
preferences without substituting an arbitrary system for missing data.
Optional preferences distinguish absent records (`null`) from explicit empty
override markers (`""`). Required defaults must resolve. Standard entries are
selected without alternates, with all published draft states admitted.

These records prepare number-service data. They do not advertise NumberFormat
available locales or supported numbering systems. Authored tests cover scalar
repertoires, inheritance, explicit markers and malformed/conflicting records.

Prepared section 13 stores sorted three-word definition records: pooled system
identifier, kind (0 numeric / 1 algorithmic), and pooled repertoire/rule string.
Section 14 stores sorted five-word records: pooled source locale followed by
definition indexes for default, native, traditional and financial preferences.
For the last two fields, `0xffffffff` means absent and `0xfffffffe` means
explicitly blocked. Required fields cannot use either sentinel. All words are
little-endian. The decoder checks both sections together, including canonical
order, numeric scalar uniqueness, rule syntax and mandatory Latin/root data.
Both profiles retain complete numbering metadata, including synthetic regional
fallbacks. Rule references are not formatter support declarations.

`src/intl/locale/numbering-data.c` owns the C schema validation and borrowed
definition/preference queries. It checks exact-key ordering, Unicode digit
repertoires and rule syntax before publishing a view. Source locale grammar
and casing are checked without alias inference; duplicate variant detection
uses checked temporary storage and sorting for long keys. Accessors allocate
nothing and apply no implicit locale fallback. Real-profile and authored C
tests cover astral digits, unaligned storage, optional states, allocation
failure/retry, long keys, malformed fields and section shapes. These queries
are metadata infrastructure; future number services select actual capabilities.

`number_symbols.py` collects the 13 common numeric symbols and two optional
currency separators for every numeric system and source locale. It checks
whole-group aliases, restarts `source="locale"` aliases at the requesting
locale, and preserves exact text, explicit empty values and inheritance.
Optional absence and explicit blocks remain distinct for downstream LDML
fallback rules. Standard containers/fields are selected before duplicate checks;
unsupported alias paths or numbering scopes fail explicitly. The standalone
audit runs against authenticated CLDR sources in CI. The collected records are
packed for each prepared profile; they are not yet consumed by NumberFormat.

Section 15 stores these symbol records as little-endian words. Its directory
count is the shared locale count. Three header words give the numeric-system,
symbol-vector and locale-row counts. Next come sorted numeric definition
indexes, a row index for each shared locale, fixed-width locale rows (one
vector index per numeric system), and 15-word symbol vectors in collector field
order. Unique rows and vectors are sorted and all must be referenced. This
shares both whole locale maps and symbol sets across numbering systems without
compression. Optional currency fields use `0xffffffff` for absent and
`0xfffffffe` for blocked; all other entries are pooled string indexes. Empty
text remains different from absence. The decoder checks extents, complete
numeric-system coverage, references, canonical order and resolved text.

`src/intl/number/symbol-data.c` owns validation and borrowed C queries for
section 15. It opens against validated numbering metadata, checks all shared
maps and vectors, then releases its temporary coverage bitmap. Queries use
shared-locale and numbering-definition indexes, allocate nothing, and preserve
the three symbol states. Algorithmic definitions return unsupported; this
owner executes no numbering rule and applies no locale or currency fallback.
Standalone authored and real-profile tests cover exact text, empty values,
optional states, unaligned storage, allocation failure/retry, invalid references,
noncanonical records and malformed section dimensions.

## Standard number patterns

`number_patterns.py` resolves standard decimal, scientific, percent and currency
patterns for every numeric system and source locale. Currency accounting and
the optional `alphaNextToNumber`/`noCurrency` variants retain separate keys.
Typed aliases follow the requesting-locale rules; alternate aliases cannot
replace baseline records. Required patterns must be present and nonempty;
optional absent, blocked and explicitly empty values stay distinct. Minimum
grouping digits resolve independently as positive unsigned integers.

The collector preserves the exact pattern language, quoting and scientific
affixes. `number_pattern_syntax.py` compiles standard fixed/scientific skeletons
into grouping and digit metadata plus typed positive, negative and explicit-plus
token sequences. Literal braces/apostrophes remain literal text; currency widths
and localized signs stay semantic tokens. Negative subpatterns supply affixes
only, without overriding positive digit settings. When negative affixes contain
no minus token, LDML leaves the explicit-plus variant equal to positive;
inserting any further sign for ECMA-402 signDisplay belongs to the service.
This is not an arbitrary
DecimalFormat pattern API: unsupported padding, significant-digit and rounding
increment skeletons fail explicitly. ECMA-402 options govern actual rounding.

Compact patterns, currency names/spacing, unit patterns and range patterns have
separate owners. These prepared records do not advertise a NumberFormat
capability. CI compiles all distinct patterns from authenticated CLDR sources;
authored tests cover aliases, inheritance, alternate selection, quoted text,
scientific affixes, suffix signs and malformed records.

Section 17 stores compiled patterns as little-endian words. Its directory count
is the shared locale count. Eight header words contain schema version 2 and
the numeric-system, vector, row, pattern, affix and token counts. Sorted numeric
definition indexes follow, then one (minimum grouping digits, row index) pair
per locale. Shared locale rows contain one vector index per numeric system;
each vector contains nine pattern references in collector field order. Optional
references use `0xffffffff` for absent, `0xfffffffe` for blocked and `0xfffffffd`
for explicitly empty. The five required patterns accept none of these markers.

The eighth header word counts legacy per-currency overrides. After the vectors,
sorted pairs map raw pooled pattern strings to compiled pattern indexes. These
overrides come from the profile's currency metadata, use the same data compiler,
and share typed numeric metadata/affixes with ordinary locale patterns. They do
not require a runtime LDML parser. The raw currency record remains available;
the consuming formatter decides precedence and supplies the locale's grouping
preference. This lookup does not choose a currency display or rounding policy.

Each pattern has nine numeric words: primary/secondary grouping, minimum
integer digits, integer placeholder count, minimum/maximum fraction digits,
decimal-point presence, minimum exponent digits and exponent-plus presence.
Three affix indexes follow for positive, negative and explicit-plus forms.
An affix descriptor gives its offset/count in the final token array. Tokens
are (kind, pooled string index) pairs; nonliteral tokens require string zero,
literal tokens require nonempty text, and each affix contains exactly one
number token. Adjacent literals merge. The decoder checks all extents,
references, canonical ordering, numeric constraints and complete record use.
Sharing whole locale maps, pattern vectors and typed affixes avoids duplicating
patterns across numeric systems without a compression dependency.

`src/intl/number/pattern-data.c` validates section 17 against the unchanged
numbering view, including complete record use and typed affix canonicality.
It borrows immutable storage, releases validation scratch on every path and
provides allocation-free exact-index pattern/token queries. Absent, blocked
and empty optional patterns remain distinct; algorithmic systems are explicitly
unsupported by this numeric owner. Authored and real-profile C tests cover
scientific affixes, quotes, suffix signs, grouping, unaligned data, corruption,
allocation failure/retry and output reset.

## Compact number source maps

`compact_patterns.py` resolves decimal short/long and currency short patterns.
Sparse maps retain magnitude, plural/exact selectors and currency alternates;
missing selectors do not silently become `other`. Explicit block, inheritance
and empty text remain distinct. A typed, iterative map resolver follows whole
numbering-group and length aliases with the requesting locale intact, including
explicit-source context changes. Empty overlays and aliases share immutable
resolved maps instead of expanding every plural leaf for every numbering system.
Final maps are deduplicated and canonically ordered.

Decimal maps are required. An absent currency compact map remains empty:
CLDR root Arabic currency data has standard patterns but no compact patterns.
Any specified formatter composition/fallback belongs to the consuming engine,
not XML resolution. `compact_pattern_syntax.py` compiles consecutive zero
placeholders into exact scale exponents and typed sign forms. Literal-only
plural/exact forms borrow their variant family's `other` divisor; the exact
pattern `0` retains its ordinary-format fallback mode. Each numeric variant keeps its own divisor,
including inherited alternates that differ from the baseline. The consuming
formatter must choose a coherent notation policy; data compilation does not
silently change source patterns or conflate fallback with a scaled placeholder.
Optional blocks and empty values remain distinct. These maps do not advertise
NumberFormat support. Authored tests and the authenticated
source audit cover resolution and syntax independently of formatter execution.

Section 18 stores compact maps as little-endian words. Its directory count is
the shared locale count. Eight header words contain schema version 1 and counts
for numeric systems, locale rows, maps, entries, patterns, affixes and tokens.
Sorted numeric definition indexes precede one row index per locale. Each shared
row contains three map references per numeric system: decimal short, decimal
long and currency short. Decimal maps must be nonempty; currency maps may be
empty. Profile projection prunes unused maps before packing.

Map descriptors contain contiguous entry offsets/counts. An entry contains
magnitude, count selector, variant selector and pattern reference. Count order is
`0`, `1`, `zero`, `one`, `two`, `few`, `many`, `other`; variant order is
`standard`, `alphaNextToNumber`, `noCurrency`. Entries sort by these three
numeric keys. Blocked and explicitly empty patterns use `0xfffffffe` and
`0xfffffffd`; a missing selector has no entry. Each magnitude requires a usable
standard `other` pattern. A literal form requires a usable `other` in its own
variant family, with the same divisor.

Patterns contain mode (scaled, literal or ordinary fallback), divisor exponent,
zero-placeholder count and three affix indexes. Scaled exponents plus zero count
equal magnitude plus one. Literal forms have no number token. Ordinary fallback
has exponent zero, one zero placeholder and unadorned number/sign forms. Affix
descriptors and typed token pairs follow section 17's representation, excluding
percent/per-mille scaling tokens. Rows, maps, patterns and affixes are shared,
canonically ordered and fully referenced; decoders validate those invariants
and all record extents before returning semantic data.

`src/intl/number/compact-data.c` validates and borrows section 18 against the
unchanged numbering view. Temporary coverage/placeholder memory is released
before return, including errors. Queries allocate nothing: exact locale/system/
kind map lookup, indexed or exact magnitude/count/variant lookup, and typed
affix-token access. Missing selectors return unsupported; blocked/empty entries
retain their states. The consumer chooses currency families, plural/exact-count
fallback and rounding policy. Authored and real-profile C tests cover literal
forms, distinct alternate divisors, empty currency maps, sparse selectors,
unaligned storage, corruption, output reset and allocation failure/retry.

## Currency source records

`src/intl/currency/boundary.c` classifies the currency-symbol boundary for
`alphaNextToNumber`: a prefix must end in a letter followed by zero or more
combining marks, while a suffix must begin with a letter. These are Unicode
General_Category `L` and `M`, not the broader `Alphabetic` property. Formatting
characters and spaces are significant boundaries. Allocation-free predicates
in the Unicode library use its existing category table; no duplicate Unicode
table is compiled into the currency service.

`currencies.py` collects exact plain/plural names, standard/narrow/formal/variant
symbols and legacy per-currency decimal/group/pattern overrides. Sparse maps
inherit by the main locale graph and share unchanged parents. Explicit
inheritance follows the parent; absent leaves, blocked text and empty text
stay distinct. It does not
invent currency-code text or plural fallback. Unsupported aliases/selectors
fail explicitly instead of discarding a selector and conflating records.

Supplemental fraction records retain regular digits/rounding and cash metadata.
LDML's regular defaults are 2/0; absent cash fields inherit their corresponding
regular fields. These are source facts, not instructions to enable cash rounding
in NumberFormat. The collected code inventory also includes historical codes;
it is not the available-currency inventory for supportedValuesOf. Formatting,
DisplayNames fallback and currency capability selection have separate consumers.
The authored tests and authenticated CI audit exercise this source-only slice;
collecting or packing these facts does not publish a NumberFormat constructor.

Section 19 stores little-endian currency facts. Its directory count is the
shared locale count. Nine header words contain schema version 2, code count,
fraction-row count, vector count, map count and four default fraction values
(regular digits/rounding, cash digits/rounding). Sorted code string references
follow; the inventory includes supplemental-only codes as well as display data.
Fraction rows contain a code index and four values, sorted by code index.

One shared-map index per locale precedes fixed-width maps with one vector
reference per currency code. `0xffffffff` means no fields for that code.
Sorted shared vectors contain sixteen fields: nine name-count forms in
collector count order, then standard/narrow/formal/variant symbol, decimal,
group and pattern, followed by one root-origin mask word. Only counted-name
bits 1 through 8 are valid; a set bit requires a present field, including an
explicit block or empty value. References distinguish absent (`0xffffffff`), blocked
(`0xfffffffe`), explicitly empty (`0xfffffffd`) and nonempty pooled text.
Inheritance/block markers cannot be encoded as ordinary text; all-absent
vectors are redundant and rejected. Maps/vectors are canonical and completely
referenced. Projection prunes unused maps; global fraction facts remain intact.
The decoder checks defaults, ordering, extents and references before resolving
semantic fields. No code-name text or plural-name fallback is synthesized.
Origin participates in vector/map sharing and survives locale projection.
Explicit child text clears root origin even when identical to inherited text;
inheritance markers preserve it. This lets consumers exclude root for counted
names while retaining root as a source for uncounted-name fallback.

`src/intl/currency/data.c` validates that schema against an unchanged open
data view and exposes borrowed exact code/field queries. It also returns the
stored default fraction facts for well-formed codes without a specific row,
including unknown display codes. Queries allocate no memory. Temporary open
validation memory is released on every path. The data owner does not select
plural fallback, currency availability, display style or formatter behavior.

`src/intl/currency/display.c` consumes those facts without allocation. Regular
symbols fall back to the canonical supplied code; narrow symbols try the regular
symbol first. Explicit empty strings succeed, while blocks allow the next field
fallback without resurrecting the same field from a parent. After rounding,
names try an applicable exact 0/1 selector, the supplied cardinal category,
`other`, an uncounted name and finally the code. Counted names exclude terminal
root sources; uncounted names may include them. Template selection is a separate
consumer: a name's fallback does not change the template's category. Returned
text borrows the immutable data or the caller's code; no currency availability
decision or NumberFormat registration is implied by this helper.

## Number range and currency layouts

`resolve.SparseMaps` handles typed whole-group aliases over sparse field maps.
It keeps the requesting locale separate from the current parent/source state,
resolves iteratively and shares immutable inherited maps. Collectors own their
group/field inventories and XPath interpretation; the resolver does not inspect
XML. Compact patterns and unit data share the same alias semantics.

`overlays` exposes an alternative persistent chain of local field snapshots,
ordered before their parent/source dependencies. This retains the context
needed to try lateral plural/case alternatives locally before moving to a
parent. Inherited group aliases restart in their requesting or explicit-source
locale, after any genuine original-child overrides. Empty and inheritance-only
layers are omitted; blocked and explicitly empty fields remain. Walk linked
nodes iteratively and treat them as immutable. Use separate `resolve` and
`overlays` caches, with unchanged records/parent graphs. This operation performs
no selector fallback and does not change the existing flattened maps or packs.

`number_layout.py` resolves miscellaneous range/approximation/at-least/at-most
templates, currency-name unit templates, currency-spacing match sets/insertion
text and legacy ISO append patterns. They are distinct from numeric skeletons
and per-currency display fields. Alias interpretation is typed and limited to
the reviewed group/spacing paths, with requesting-locale and explicit-source
semantics. Text and UnicodeSet expressions remain exact source facts.

Miscellaneous templates and currency boundaries are required for each numeric
system. Currency-name selectors and the ISO append pattern may be absent,
blocked or explicitly empty. In particular, root `arab` has spacing but lacks
currency-name templates. Raw rows preserve that absence; the separate locale-level
projection below follows default-system inheritance.
Selecting plural/currency-name fallback and interpreting spacing predicates
belong to later consumers. CI audits the authenticated full source inventory;
this collector or its packed records establish no formatter capability.

Section 20 has the shared locale count in its directory and five little-endian
header words: schema version 3, numeric-system count, shared vector count,
shared row count and projected currency-vector count. Exact numeric
numbering-definition indexes follow, then one shared-row index per locale.
Each sorted unique row has one vector index per
numeric system. Each sorted unique vector has nineteen string references in
collector field order, followed by one root-origin mask word. Only currency-unit
field bits 4 through 11 are valid, and absent fields cannot carry origin. Alias
resolution records the terminal source locale; equal child overrides clear root
origin. Selected-count template fallback can exclude root, while the `other`
fallback can include it. Absent and blocked optional fields use `0xffffffff` and
`0xfffffffe`; explicitly empty text uses string index zero. Required templates
and match predicates cannot be absent, blocked or empty; required insertion
text can be empty. Ordinary pooled references cannot contain inheritance or
block markers. Validation checks complete shared-row/vector coverage and exact
locale/numeric-system coverage before exposing semantic fields.

The raw vectors are followed by one projected currency-vector index per locale,
then sorted unique vectors containing eight selector references (exact zero/one,
zero/one/two/few/many/other) and an eight-bit terminal-root mask in one word.
All projected vectors must be referenced. Their absent/blocked/empty states
use the same representation as raw layout fields.

Currency-name templates are locale-level data: only local leaves under the
locale's resolved default numbering system enter this projection. Missing leaves
and inheritance markers follow the parent's projection, which used that parent's
own default. Selected-group aliases retain their raw target paths and requesting
locale/explicit-source behavior. This can differ from fully resolved raw data
even when the raw text and root masks are identical. Projection happens before
profile filtering; it introduces no runtime XML parser or numbering-system guess.

`src/intl/number/layout-data.c` validates section 20 against the shared numbering
inventory and exposes allocation-free exact locale/system queries. Strings
borrow immutable pack storage; empty VALUE text differs from absent/blocked
fields. Open releases its validation scratch on success and failure. Parsing
templates, evaluating UnicodeSet predicates and selecting formatter fallback
are separate consumer responsibilities.

`qjs_intl_currency_unit_layouts_at` exposes the locale-level projection without a
numbering-system argument. The currency display consumer independently tries
exact/category templates excluding root, then `other` including root, without
changing the name's selector. Requested numeric `nu` still controls digits,
numeric symbols and decimal patterns.

## Sanctioned measurement-unit identifiers

`src/intl/unit/identifiers.def` is the common inventory of the forty-five
ECMA-402 sanctioned single units and their CLDR data identifiers. The private
C identifier helper performs exact case-sensitive lookup and accepts either
one sanctioned unit or precisely two joined by `-per-`. It does not accept
arbitrary CLDR units, aliases, multiplication or localized names. The helper
allocates nothing; source collection and formatting remain separate work.
An exact adapter maps the five sanctioned divisions with precomposed CLDR names
to their long data identifiers. Other sanctioned pairs require composition;
they remain valid when no precomposed name exists. Never derive the grouping
prefix from the numerator's category: `liter-per-kilometer`, for example, uses
`consumption-liter-per-kilometer`, not a `volume-` prefix. Both prepared-profile
tests check every sanctioned pair against the full source group inventory and
reject missing, unexpected or ambiguous mappings.

`unit_patterns.py` collects long/short/narrow display names, unit patterns with
their exact plural/case selectors, grammatical gender, per-unit patterns and
division compound layouts. It preserves all source single-unit data identifiers;
that inventory is broader than the sanctioned API list. Sparse shared maps
resolve unit/width aliases in requesting-locale or explicit-source context.
Absent fields, blocked fields, explicit empty text and inheritance remain
distinct. No case/plural/width formatting fallback is invented by collection.

The collector also retains ordered source overlays beside the exact resolved
maps. Each row points to a persistent chain of nonempty local maps, with source
locale and width/group identity retained on every node. Parent node IDs precede
their children; zero ends a chain. Identical fields can share a map, but different
source identities do not collapse into one node. This lets future consumers try
local plural/case alternatives before inherited exact leaves without losing
alias context. These chains preserve source facts without selecting a formatter
fallback policy.

Digital duration layouts, coordinate formatting and arbitrary product/power
composition have separate consumers. CI audits all authenticated relevant unit
records; collecting those source facts does not publish unit formatter support.

`unit_grammar.py` separately collects the supplemental `deriveComponent` rules
for division-unit plural and case. Resolve each feature along the separate
`grammaticalFeatures` parent chain independently, retaining the rule's source
locale and both component values. `compound` passes through the requested
feature; fixed plural categories and case names remain distinct data tokens.
Thus a French plural override does
not erase the root case rule, while German denominator case comes from its own
rule. Require complete root defaults, reject duplicate/unsupported records and
validate the source DTD attributes. Other known structures and compound gender
rules are checked but not included in division output. This collector does not
enable compound formatting by itself.

Section 22 stores these derivations independently of the unit-text section.
Two little-endian header words contain schema version 1 and shared-row count.
One shared-row index per locale precedes strictly sorted, unique six-word rows:
the plural origin and its two component tokens, then the case origin and its
two tokens. All six words reference the shared string pool. Every shared row
must be referenced. Profile projection retains required origin/token strings,
including origins outside the selected locale registry. Grammar origins are
independent of localized unit-text origins and cardinal-rule origins.

`unit/grammar-data` validates this section over the generic immutable data view,
borrowing its storage without retaining the opener. Scratch bytes cache source
and symbolic-token checks per string role; nested source-variant scratch is also
released on failure. Queries return borrowed rules and allocate nothing. The
owner validates symbolic case names, not membership in a grammatical inventory;
the formatter applies the selected rules and owns any fallback decisions.

Section 21 stores those sparse unit forms. Its directory count is the shared
locale count. Eight little-endian header words contain schema version 2, group,
case, raw shared-row, shared-map, entry, overlay shared-row and node counts.
Group records contain kind
(`compound=0`, `unit=1`) and a name string reference; only division `per` is a
compound group. Sorted case string references start with the empty default case.
One row index per locale precedes sorted unique rows, each containing one map
index for every width (long/short/narrow) and group combination. A second locale
index table and sorted unique row table hold overlay heads for the same columns.
Zero means no local layers; other heads are one-based node IDs.

Four-word nodes contain origin locale string, origin width/group column,
local-map index and parent ID. Alias origins may name locales outside the
selected profile. Parents precede children, and nodes are strictly sorted by
`(parent, origin locale string index, origin column, map index)`. This permits
linear duplicate and reachability checks without repeatedly walking every
chain. Local maps are nonempty; raw exact maps may be empty. Chains preserve
unit-versus-compound kind even when aliases change width or unit name.

Sorted unique maps have contiguous `(offset, count)` descriptors into two-word
entries. An entry holds a selector and text reference. The selector's high eight
bits identify the collector field, the next eight identify absent count (0) or
one of its eight count selectors (1–8), and the low sixteen hold a case index.
Entries are strictly sorted by selector. Absence means no entry; `0xfffffffe`
is blocked text, and string index zero is explicitly empty. Inheritance/block
markers cannot be ordinary pooled text. Unit and compound fields cannot mix
or be assigned to the wrong group family. Rows/maps and non-default cases must
all be referenced by raw rows or reachable overlay nodes. Both map families
share one pool. Projection removes unused maps/cases/nodes while preserving the
global source group inventory; that inventory is not API availability.

`src/intl/unit/data.c` validates section 21 against the unchanged shared data
view, including selector identities, map families, complete ownership and
canonical ordering. Indexed and exact queries borrow immutable storage and
allocate nothing. They preserve missing, blocked and explicitly empty forms;
grammatical/plural/width fallback and formatter capability belong to consumers.
Overlay queries additionally expose each borrowed local map and its source
locale, width/group and next ID, without allocating or performing fallback.

`src/intl/unit/pattern.c` adapts these chains to number-bearing single-unit
patterns. Within one layer, exact numeric 0/1 precedes the source's cardinal
category and OTHER; each count tries requested case, nominative and absent
case. Empty case requests mean grammatical nominative. Numeric callers retain
their rounded visible operands in a source-rule callback; fixed category mode
serves grammatical component counts and nonfinite OTHER. Missing source rules
propagate failure instead of silently substituting requester rules.

The native adapter skips valid literal-only and explicitly empty forms while
preserving them as raw data. BLOCK and ineligible values suppress their exact
parent path, not unrelated alternatives. A bounded shadow mask also remembers
locally present categories that a later source might select differently. A
consulted malformed template is a data error. The prefix/suffix parser supports
one `{0}` with literal apostrophes and possibly empty affixes; it does not import
decimal-pattern quoting. Selection allocates nothing and returns borrowed
source/selector provenance. Rendering and the NumberFormat binding have separate
owners.

The division-template parser retains literal prefix/separator/suffix slices and
which operand appears first. Both `{0}` and `{1}` must occur once; reversed
patterns such as `每{1}{0}` are supported. The noun adapter accepts literal-only
unit forms or strips one numeric placeholder at a nonspace edge, removing
outer Unicode White_Space while preserving internal text. A middle placeholder
with noun text on both sides is ineligible for compound-name extraction, as in
TR35's composition algorithm; it is not silently joined. These helpers borrow
their inputs and do not select locale/plural/case fallback.
The noun selector reuses the numeric selector's ordered layer/case traversal and
path shadows, with its own noun eligibility rules. A fixed grammatical count
bypasses numeric rule evaluation; a pass-through `compound` derivation can use
the rounded-quantity callback and each source's cardinal rules. Neither mode
tries exact numeric `0`/`1` selectors. It returns
the noun and source/form provenance without allocating, including alias origins
outside the prepared locale registry.
Uncounted template selection separately handles a denominator's `perUnitPattern`
and generic division layouts. It applies local case alternatives before parent
layers, honors blocked/empty/missing-operand path shadows, and returns parsed
template fragments with their actual source. Per-unit templates have no case
selector; generic division layouts support case alternatives. Neither lookup
performs plural selection or invents a width fallback.

`src/intl/unit/format.c` composes the selected prefix/suffix around a complete
numeric body, preserving its sign, exponent and compact parts. Nonspace affixes
become unit parts, with outer Unicode White_Space separated as literal parts;
internal spaces stay within a unit part. A bare or whitespace-only template
retains an empty unit part. The complete UTF-16 result is independently owned,
with exact output bounds and allocation-failure cleanup. The compact and unit
renderers share the private bounded UTF-16 builder; unit/plural/sign selection
remains outside that writer. Borrowed prefix/suffix fragment lists can compose
compound affixes without intermediate text allocations. Part boundaries are
assigned after measuring all fragments: internal spaces remain in one unit part
per affix, and only outer whitespace becomes literal. Every fragment must end
on a UTF-8 scalar boundary. The single-pattern entry point delegates to this
renderer. This does not register JavaScript NumberFormat.

## Localized numeric bodies

`src/intl/number/body.c` renders an already rounded unsigned plain decimal as
UTF-16 integer, group, decimal and fraction parts. It substitutes scalar digit
repertoires, preserves integer padding and trailing fraction zeros, and applies
primary/secondary grouping with auto/always/min2/disabled policy. NaN and
infinities use the supplied localized symbols. Parts partition the owned text,
including supplementary digits and multi-code-unit separators.

The caller selects the locale, symbols and grouping preferences. Rounding,
percent scaling, sign patterns and style/notation affixes remain separate
NumberFormat steps; the body operation does not advertise a JS constructor.
It sizes the complete result before allocating, enforces an explicit output
bound, and frees partial results on allocation failure.

`src/intl/number/round.c` implements NumberFormat's scale-before-round order:
percent scaling, an unsigned ComputeExponent probe, and rounding the original
signed value at the final notation exponent. Scientific and engineering use
mathematical magnitudes; compact uses a caller-selected ordered threshold
vector, including explicit exponent-zero fallback entries. A carry can choose
the next notation scale before the final rounding. Zero and nonfinite inputs
retain their distinct kinds, and negative rounded zero keeps its sign.

This differs from PluralRules, which rounds the unscaled value once and uses
notation only for plural exponent metadata. The compact formatter must select
its pattern family before supplying thresholds; this helper does not perform
plural lookup or choose currency/unit affixes.

`src/intl/number/affix.c` composes a decoded typed pattern around a numeric
body, retaining its text and part boundaries. Literal affixes and the selected
sign, percent and currency text are decoded from scalar UTF-8. Currency display
selection and boundary spacing belong to the caller; every currency placeholder
uses that resolved text. Sign selection follows GetNumberFormatPattern over
the rounded value, treating NaN as positive zero and preserving negative zero.
If positive sign display requires a sign but the LDML
variant contains no plus token, the operation can insert one before the pattern.
Empty replacements remain distinct parts. Complete UTF-16 sizing precedes
allocation and the output bound covers affixes and body together.

`src/intl/number/native.c` assembles those operations behind the compile-time
number-service contract. The internal formatter supports standard,
scientific and engineering notation with decimal/percent styles, numeric digit
repertoires, resolved
grouping/sign settings and exact rounding options. It validates the selected
profile once per service, excludes root from locale matching inventory, and
caches decoded body symbols and typed pattern tokens in each formatter.
Explicit numbering-system selection is exact; negotiation belongs to bindings.
A formatter may outlive its service, and results may outlive both.

Standard/scientific/engineering currency-name formatting uses the decimal
pattern's complete signed number as the numeric argument of the selected
locale-level unit template.
It selects currency names and placement templates independently after rounding,
including exact zero/one and root-cutoff rules. NaN and infinities use `other`.
Scientific/engineering names use the final visible rounded mantissa, with c=e=0:
ECMA-402 passes this scaled value into currency-name selection, while CLDR
notation operands c/e are compact-only. Thus a zero-fraction-digit English
`1E3 US dollar` follows this native policy, unlike ICU's `1E3 US dollars`.
Visible fraction zeros and trailingZeroDisplay still affect plural selection.
No expansion of a large scientific exponent is needed for name selection.
Compact names instead use the decimal-short/long notation body and choose the
outer name/template from the expanded rounded compact quantity, with c/e set
to its final compact exponent. This is this backend's CLDR locale-data policy
for ECMA-402's implementation-dependent currency text. The shifted plural
operation avoids constructing an expanded string and retains fraction zeros
left after shifting: `1.20000c3` supplies `1200.00`, while `1.20c3` supplies
`1200`. Exact zero/one selectors test that expanded numeric value. The inner
compact affix still uses the visible mantissa with c/e=0; scientific and
engineering names retain their separate mantissa policy described above.
Monetary decimal/group symbols apply, with per-currency overrides taking
precedence; currency symbol/accounting patterns do not replace decimal affixes.
The `CurrencyDigits` service query exposes regular digits for frontend defaults,
without enabling cash rounding. Currency codes are copied into formatter state.

`currency-format.c` substitutes `{0}` and `{1}` once each, preserving the signed
number's parts and emitting the complete name as one `currency` part. Prefix
templates therefore place the name before the numeric sign. Apostrophes remain
literal. Invalid templates fail before result allocation; the complete output
bound includes names, separators and numeric text.

Currency symbol, narrow-symbol and code display are implemented for standard,
scientific and engineering notation. Standard currency signs honor compiled
per-currency pattern overrides; accounting selects its own generic family.
Within a generic family, `alphaNextToNumber` is selected from the closest
currency scalar's General_Category L. Compact prefix selection has a distinct
L-followed-by-marks rule. An alpha-capable family replaces legacy spacing even
when the ordinary form is selected, avoiding an unintended hybrid policy.

Families without an alpha variant, including legacy per-currency overrides,
use the pinned `currencySpacing` predicates `[[:^S:]&[:^Z:]]` and `[:digit:]`.
Other predicate syntax returns unsupported instead of silently approximating
UnicodeSet semantics. Insertions require direct currency/number tokens and
an actual adjacent Nd scalar: supplementary digits qualify, hanidec characters,
NaN/infinity and notation literals do not. Inserted text is a literal part;
empty currency replacements never create a gap. Formatter state owns fallback
codes and otherwise borrows only immutable compiled metadata.

Scientific/engineering output applies the exact scale-before-round operation,
then emits the localized exponent separator/sign and uses the selected digit
repertoire for both mantissa and exponent, including zero. This follows ICU/CLDR
presentation rather than the pinned PartitionNotationSubPattern's ASCII
exponent text. The open [ECMA-402 issue 627](https://redirect.github.com/tc39/ecma402/issues/627)
tracks the missing exponent transliteration; it is not an adopted spec correction.
Part spans and output bounds count UTF-16 units, including supplementary digits.
Positive scientific-pattern literals wrap the complete notation body; the
style's sign is outside it. Nonfinite values omit the exponent and notation
wrappers. Source fraction defaults, exponent plus signs and zero-padding do
not override the resolved ECMA-402 options and exponent rules.

Compact decimal notation uses the selected standard-`other` divisor before
rounding, then chooses an exact `0`/`1` or plural affix from the visible rounded
mantissa. This plural context has `c = e = 0`; it is distinct from the expanded
quantity used by an outer currency or unit name. Retain the final magnitude
lookup across a carry or signed-zero result instead of recovering it from the
divisor. An incompatible inherited plural divisor falls back to `other` without
rescaling, and a raw `0` baseline omits compact notation entirely.

Literal-only compact forms such as French `mille` omit the numeric body,
including its integer padding and localized digit allocation. Compact names
remain one `compact` part across internal spaces and localized sign tokens;
only leading/trailing Unicode White_Space becomes `literal` parts. This includes
hyphens inside the pinned yrl long names. Sign display remains an outer-style
operation, not a selection of the compact pattern's negative form.

The positive compact renderer also emits resolved currency tokens as separate
`currency` parts, including explicit empty text. It preserves compact-name
boundaries and emits caller-filtered currency gaps only at directly adjacent
number/currency tokens. Currency selection, scalar predicates and alpha-family
policy belong to the native service, not this renderer.

`sign-affix.c` provides allocation-free sign-envelope projection for wrapping
that complete compact currency body. It compares each signed affix against the
positive form on each side of the number and removes maximal common prefix
and suffix context. Literal comparisons operate on Unicode scalars, so partial
UTF-8 code-point matches cannot corrupt the retained slices. Accounting
parentheses, signs and signed-only controls remain; unsigned spaces and bidi
controls are not emitted twice. Residual currency/percent tokens are rejected.
Projection is an explicit native composition policy, not an LDML pattern
rewrite. Its input token arrays are disjoint; the selected array is compacted
in place and still borrows immutable literal storage.

Compact symbol, narrow-symbol and code display uses the currency-short map,
regardless of `compactDisplay`. `compact-family.c` selects the divisor before
rounding: raw `0` ends compact selection before considering alpha variants;
otherwise direct currency/number adjacency and the compact L M* / initial-L
boundary facts choose a usable alpha `other` when available. Sparse alpha
count leaves may still be selected without an alpha `other`, but every selected
count retains the chosen divisor. A threshold with any usable alpha leaf
suppresses legacy spacing even when its ordinary form is selected. This
per-threshold modern/legacy compatibility policy is native formatter policy;
TR35 does not specify that fallback granularity.

The service caches those families and separate sign envelopes alongside its
ordinary currency forms. A complete compact currency body receives only the
projected sign envelope, so the currency and unsigned decoration occur once.
Forms without a currency placeholder retain the ordinary outer currency
pattern. Empty currency maps, raw `0`, small values and nonfinite inputs use
ordinary currency formatting. Caller-filtered legacy gaps test the actual
numeric-body edge, and literal-only forms omit unused numeric allocation.

Unit style accepts every sanctioned single/division identifier and all three
display widths in standard, scientific, engineering and compact notation.
`number/unit.c` resolves each source layer's cardinal rules, selects its
number-bearing form and renders it around the signed notation body. Compact
outer quantities use the same virtual expansion as currency names; scientific
and engineering units use their final visible mantissa. Nonfinite inputs use
OTHER without exact selectors. A unit named `percent` does not apply percent
style's multiplication. Formatter state borrows immutable source/plural views,
does not retain the caller's identifier and survives service closure.

Division formatting prefers a complete precomposed unit pattern, followed by
the denominator's `perUnitPattern`, then a generic division layout and derived
component nouns. Supplemental plural/case derivations retain independent source
provenance; each numeric component still uses its candidate text source's own
cardinal rules. A fixed grammatical count never selects exact numeric `0`/`1`.
Per-unit phrases do not additionally select denominator nouns. Generic layouts
retain their actual operand order, including denominator-first languages.

If a localized expression cannot be composed (for example, a component's
number placeholder lies inside its noun), retry the entire unit expression
using root data while retaining the localized numeric body. This follows
TR35's component extraction restriction without fabricating a noun or dropping
part of it. Malformed consulted templates remain errors, and selection never
hides unavailable source cardinal rules with a root retry. Cached local/root
heads borrow immutable owners; rendering needs at most four fragments per
affix and allocates only the final text and parts arrays.

The engine binding exposes Intl.NumberFormat on this service. Repository
tests exercise every prepared locale's default numeric system and allocation
failure at every service/open/format allocation. JavaScript tests cover the
observable constructor, exact inputs, options, bound functions, parts and ranges.

### Number range rendering

`number/range.c` composes complete endpoint results without numeric comparison
or JS coercion. Its strict native layout parser extracts the nonempty scalar
separator from `{0}<separator>{1}`; other layouts are rejected explicitly.
Unequal UTF-16 strings retain every endpoint part, with `startRange`/`endRange`
sources and a shared literal separator. The initial collapse policy returns
that sequence unchanged, as explicitly permitted by ECMA-402.

Equal strings retain the first endpoint's part structure and receive the whole
approximation sign as a prefix, with every source shared. An empty sign adds no
part. This is the native adapter's initial ILND placement policy, not a claim
to implement TR35's sign-slot placement. Modern approximation uses the selected
numeric symbols' `approximatelySign`, not the obsolete miscellaneous
approximately pattern. Endpoint formatting and NaN rejection belong to the
service; the engine binding owns JavaScript API registration.

The native service caches the selected locale/numbering system's borrowed range
separator and modern approximation symbol in the formatter. It rejects either
NaN endpoint before formatting, then formats both endpoints independently and
clears both intermediate results on every path. Descending ranges are allowed.
Identical inputs, equal infinities and distinct values with equal displayed
text all use approximation. Combined output bounds include the separator/sign.
Cached text survives service closure; final text/parts survive formatter closure.

Range parts have their own source-bearing record; ordinary result parts remain
unchanged. The shared scalar text writer supplies UTF-16 encoding and bounds,
while the number range owner assigns types/sources. Results own two final arrays,
retain empty endpoint parts and survive input lifetimes. Both allocation errors
reset the result and release partial storage.

## Numeric options and exact plural relations

`src/quickjs/builtins/intl/number-options.c` implements the shared binding-side
SetNumberFormatDigitOptions operation. It reads the four raw digit properties
before rounding options, defers their conversions until precision selection,
and keeps absent fraction/significant slots distinct from effective defaults.
Compact defaults and increment/priority restrictions follow ECMA-402. Results
publish only on success; owned temporary JS values are released on every path.
The focused repository JS suite uses a private C driver to test getter/coercion
order without advertising a constructor before its backend is complete.

`src/quickjs/builtins/intl/number-value.c` owns ToIntlMathematicalValue coercion.
It performs one number-hint ToPrimitive, preserves exact finite decimal/radix
strings and BigInts, and uses shortest decimal text for Numbers. The engine's
StringNumericLiteral conversion validates string grammar and identifies the
specified RoundMVResult infinity/zero boundaries; a finite nonzero result does
not replace the original exact text. BigInts bypass those string boundaries.
Owned decimal/text results use the runtime allocator and reset on exceptions.
Focused JS cases cover precision, coercion order, whitespace, radix grammar,
special values and overflow/underflow boundaries; the C driver checks allocation
failure and retry with buffers larger than allocator pools.

`src/intl/plural/rules.c` evaluates one CLDR relation over an unsigned plain
decimal without allocation or binary floating point. It retains visible
fractional zeros and evaluates integer operands, fractional operands, modulo,
integer/continuous ranges, lists and logical precedence exactly. Callers pass
the decimal after expanding any notation exponent; the separate exponent
supplies the `c` and `e` operands. Source sample annotations are handled by the
collector, not the evaluator. Pattern whitespace and adjacent grammar tokens
are accepted; malformed arms are rejected even when their truth value cannot
change the result. Repository witnesses include values beyond 64-bit integers,
fractional modulo, exponent boundaries, malformed input and output reset.

The separate `qjs_intl_plural_rule_evaluate_shifted` entry point accepts a
visible mantissa and a nonnegative decimal shift, independently of its `c/e`
metadata. For `1.20050c3`, a shift of three gives the operands of `1200.50`,
including `v=2`, `w=1`, `f=50` and `t=5`. Two borrowed integer spans and a
virtual zero tail avoid exponent-sized storage. Modulo uses exponentiation
by squaring for that tail, while comparisons inspect significant length and
at most ten digits. An all-zero coefficient stays zero even at `UINT32_MAX`.
The native category selector exposes the same shifted operation. Both paths
share one rule grammar; compile-time specialization keeps virtual-digit
classification out of ordinary relation handlers. Repository tests compare
bounded explicit expansions and cover huge shifts, fractional remainders,
visible zeros and independent signed `c/e` metadata. Compact currency-name
formatting uses this operation for its outer name and placement template.

`src/intl/plural/plural.h` is the engine-independent plural service contract;
`native.c` implements it with the selected compiled pack. Available locales
exclude root, and formatter handles can outlive their service. ResolvePlural
rounds the original signed mathematical value once, retaining its unsigned
plain text for plural operands and range equality. It does not invoke
NumberFormat's scale/round pipeline. Nonfinite values select `other`; a NaN
range is an error. Equal rounded strings return the individual category before
looking up CLDR range rules.

Native notation policy attaches exponent metadata without changing that rounded
text: zero for standard/zero/ordinary fallback, rounded decimal magnitude for
scientific, its mathematical floor to a multiple of three for engineering,
and the selected locale's standard `other` compact divisor for compact. Literal
display-affix selection is distinct from public PluralRules category selection.
Tests cover unscaled Polish rounding, French compact exponents, negative
engineering exponents, signed rounding, range equality, every locale's compact
formatter, bounded-output errors and allocation failures throughout opening,
individual selection and range selection. This C facade alone does not publish
a JavaScript constructor.

The binding in `src/quickjs/builtins/intl/plural-rules.c` publishes PluralRules
with constructor/subclass realms, exact mathematical coercion, category arrays,
digit options and select/selectRange. Formatter ownership belongs to the JS
instance; the native service is initialized lazily and belongs to its runtime.
Repository JS tests cover observable behavior and C allocation-failure tests
cover partial construction, resolved objects and selection retries. CI runs the
whole PluralRules Test262 directory for native. English-only minimal excludes
two unconditional multilingual-data expectations; its API behavior remains
covered in the repository suite. Optional microbenchmarks measure construction,
Number selection, exact decimal selection and ranges.

`plural_rules.py` reads cardinal, ordinal and range supplemental data through
the checked DTD and source reader. Conditions exclude sample annotations and
normalize Pattern_White_Space; runtime tokens and integer bounds are checked
without changing their logical structure. It resolves the stand-alone
`plurals` component parent chain, independently of main-component overrides,
and retains source origins. Category/range records are unique and sorted;
root rules are required. Explicit empty range maps stop inheritance, while
missing range maps and pairs remain absent. The consumer applies the pinned
LDML range policy (CLDR 49 specifies the end category for absent pairs).
Endpoint-omitting range records require a reviewed mapping before admission;
the pinned release contains explicit endpoints. These collected records are
audited in CI and packed for both profiles without advertising PluralRules capability.

Section 16 packs these records for each prepared profile. Its directory count
is the shared locale count. Two little-endian header words give the condition
and range vector counts. Each locale then has two pairs of word indexes
(cardinal then ordinal), selecting a condition vector and a range vector.
Condition vectors contain six pooled string indexes in category order, with
`0xffffffff` marking absent categories; `other` is present and empty. Range
vectors contain 36 bytes in start/end category order, with `0xff` marking
absent pairs and values 0 through 5 naming the result category. Unique vectors
are sorted and all must be referenced. The typed decoder validates coverage,
extents, references, grammar and canonical records. Missing pairs stay explicit
until the consumer applies its range policy. The packed data establishes no
PluralRules capability by itself.

`src/intl/plural/data.c` owns the C view for section 16. It validates all maps,
category and range vectors, and each distinct condition string before opening.
Temporary coverage and grammar-cache storage is released on every path. Queries
return borrowed condition slices, a category-presence mask and the sparse range
map without allocation or locale fallback. Authored and real-profile tests
cover unaligned input, exact category data, missing/empty distinctions, grammar
and schema corruption, output reset and allocation failure/retry.

The native engine selects the first matching category from an unchanged
validated rule view, retaining exact operands and visible zeros. It allocates
nothing and holds no state. Range selection uses an explicit pair result or
CLDR 49's end-category default; the service remains responsible for equal
formatted strings, mathematical-value coercion, rounding and notation expansion.
Authored and real-profile witnesses cover cardinal/ordinal selection, Arabic
categories, large coefficients, all sparse fallback pairs and error outputs.

## Locale lookup

`src/intl/locale/matching.c` implements the fixed prefix lookup over a service's
validated available-locale inventory and a canonical language priority list.
It preserves request order, removes only the Unicode extension, and drops
singleton extension boundaries during prefix truncation. A borrowed two-slice
view avoids allocating a temporary tag. The result identifies the requested
and available entries and retains the original Unicode extension.

An identity or source-locale registry is not automatically an available-locale
inventory. Each service establishes its own supported records before calling
this helper. Default-locale policy, best-fit matching and option/extension
resolution are separate operations.

`availability.py` completes the formatter registry with the scriptless
language-region fallbacks required for language-script-region entries. These
generated identities reuse complete source records, preferring the region's
likely script, then the language's likely script and a stable source identity.
Existing source entries retain precedence. Source filenames and parent graphs
are unchanged; generated aliases and this selection policy are recorded in the
prepared manifest. The registry still establishes no service capability until
the corresponding patterns and algorithms are validated by their owner.

## Native list pattern engine

`src/intl/list/native-engine.c` expands checked pair/start/middle/end templates
into exact UTF-16 text and element/literal spans. It preserves empty elements,
embedded NUL and unpaired surrogates. An iterative right-tail traversal avoids
recursive expansion and repeated copies of already formatted tails.

Context templates support Spanish conjunction/disjunction and Hebrew Script
rules. The Spanish numeric-prefix scanner covers grouped thousands and decimal
fractions from the CLDR rules, cross-checked against ICU4X. Context predicates
adapted from ICU retain their attribution and the shipped `src/intl/LICENSE-ICU`.
Data selection must enable replacements only for the matching pair/end template.
The collector derives replacements after inheritance resolution and only when
each individual pair or end template exactly matches the CLDR default. A
different pair template does not suppress a matching end replacement, or the
reverse. Hebrew uses the ASCII hyphen shown in the pinned CLDR rules.

The engine-independent component's tests cover output partitions, unusual UTF-16, ownership,
long lists, malformed templates, overflow and allocation failure.

The JavaScript binding in `src/quickjs/builtins/intl/list-format.c` supplies
constructor/option ordering, locale resolution, branding and result realms.
`format()` and `formatToParts()` consume string iterables, retaining exact
UTF-16 values. Iterator stepping failures propagate directly; non-string
elements trigger iterator closing with the original TypeError preserved.
Ropes flatten after iteration, and narrow strings widen into one temporary
buffer. Part objects and their result array use own data properties.
Repository tests cover reentrancy, GC, cross-realm calls and failures during
flattening, widening, formatting and partial result creation. Optional
`intl_list_*` microbenchmarks cover construction, ASCII/wide formatting and parts.

`src/intl/list/list.h` is the backend-neutral list service contract. Its native
implementation admits the compiled profile's complete template/context records,
excludes `root` from the available-locale inventory, and maps inventory entries
back to their checked data rows. A formatter copies its resolved patterns, so it
can outlive the service. Results likewise outlive either handle and are cleared
with the original allocator. Hebrew contexts query the existing Unicode Script
owner; they do not duplicate tables or approximate scripts by block boundaries.
Tests cover all available locales and nine variants, canonical inventory IDs,
post-root indexing, contextual output and service/formatter allocation failures.

## Binary envelope

`bundle.py` and `src/intl/native/pack.c` define the same QJIN 1.0 envelope.
Integers are little-endian and decoded bytewise, including on unaligned inputs.
The 64-byte header contains the total size, section count, 32-byte generation
identity, minimal/native profile and zeroed reserved fields. Each sorted
16-byte directory entry contains an ID, offset, byte length and record count.
Sections are contiguous except for zero padding to four-byte boundaries.
The envelope is bounded to 256 MiB and 128 sections.

The allocation-free reader borrows immutable storage; opening it validates only
the envelope. Section owners must check their own record schemas, references
and required dependencies before use. Unknown optional sections establish no
service capabilities. A generation identity is not a cryptographic check of
the contents; source acquisition and prepared-pack manifests provide hashes.

## Prepare typed records

```sh
python3 tools/intl/generate.py --sources /path/to/intl-cache/cldr-49-beta4 \
  --profile native --output /path/to/native.bin --manifest /path/to/native.json
```

The sections contain sorted UTF-8 strings, a checked locale-parent registry,
locale identity metadata, regional/script preferences, and deduplicated sets
of resolved list templates. Both profiles retain all identity and preference
metadata. `--profile minimal` selects English and its parents for formatter
records; `native` retains the full graph. Section IDs
and string IDs are stable within a generation, not across source upgrades.
The manifest records exact source reads, policy, pin and section hashes. These
inputs determine the generation identity without local paths or timestamps.
Its `complete_services` remains empty until full service behavior is delivered.
Normal engine builds do not invoke these collectors or parse source XML.

Prepared files are in `data/intl/{minimal,native}.bin` with matching manifests
and the CLDR license. The repository suite checks their hashes and typed tables
offline. CI additionally rebuilds both packs from the authenticated archive and
compares every byte of the binaries and manifests. These initial files contain
only the sections documented above, not yet the complete Intl data inventory.

`embed.py` validates a prepared pack and emits one immutable C array in the
build directory. The shared C reader validates UTF-8/string ordering, shared
record extents, references and the parent graph. Each service data owner then
validates its own sections before exposing borrowed records. Temporary
validation memory is freed before return, including on errors. Separate C
repository tests consume both real prepared profiles, so
the Python encoder and C reader are checked together on every test platform.
The reader does not authenticate data or provide BCP47 canonicalization.

Preference sections 7, 8, 9 and 11 hold regional week records, calendar orders,
hour-cycle orders and script directions. Tables use sorted pooled keys; week
records contain first weekday, minimum week days and a seven-bit weekend set.
Ordered calendar/hour lists share pooled strings with checked token counts.
Script directions use a distinct unknown value. The preference owner validates
these schemas separately from the common envelope and locale identity tables.
The C owner in `src/intl/locale/preference-data.c` checks all four required
tables before publishing a borrowed view. Shared lists are validated once per
table, including token uniqueness; exact-key lookups use binary search and
distinguish absent data from malformed input and allocation failure. Tests for
both prepared profiles cover corruption, unaligned input and every temporary
allocation failure.

List context section 12 shares the locale order of section 2 and requires the
base list templates in section 10. Its little-endian words contain a set count,
one set index per locale, and sorted unique sets of nine triples in list variant
order. Each triple contains a context kind and pooled pair/end template IDs.
Kinds are 0 (disabled), 1 (Spanish conjunction), 2 (Spanish disjunction), and
3 (Hebrew conjunction). Disabled triples are all zero; enabled triples retain
the unchanged base template when only their other template needs replacement.
Every set is referenced. The decoder checks this structure and template syntax;
selection of the appropriate language, variant and exact-default replacement
belongs to the authenticated-source collector. A structurally valid pack alone
does not prove these linguistic rules or advertise a complete ListFormat service.
The ListFormat data owner additionally checks every locale/variant against its base templates
and these exact-default rules before exposing the contextual records. Unknown
optional sections remain distinct from validated contextual capability.

`src/intl/locale/preferences.c` applies the fixed ECMA-402 rules above the
selected backend's exact data queries. Region selection preserves explicit
regions, subdivision regions and likely-subtag fallback, with an independent
`rg` override. Unknown scripts retain an undefined direction. The JavaScript
binding supplies observable internal keyword slots and creates fresh result
objects in the method's realm; it does not expose native wire views.

## Locale metadata audit

```sh
python3 tools/intl/locale_metadata.py --sources /path/to/intl-cache/cldr-49-beta4
```

Collect language, script, territory, variant and subdivision aliases, preserving
regional replacement priority; named-zone aliases stay with the timezone owner.
Likely subtags and BCP47 key/type records are separate typed collections. Apply
LDML's semantic defaults for omitted extension (`u`) and value type (`single`),
which the DTD itself does not provide. Preserve preferred/deprecated metadata
and detect cycles or missing preferred targets within the appropriate key.
Indirect schema names such as `REORDER_CODE` stay distinct from literal types.
IANA-name and region labels on timezone types are retained as metadata; actual
zone rules and primary-identifier policy remain with the timezone service.
Unsupported root-level additions fail explicitly rather than disappearing.
Legacy alias spellings remain data; they do not widen runtime locale syntax.
Registry membership does not establish installed service capabilities.

Both prepared profiles retain the same complete locale identity metadata, even
though their formatter locale selections differ. Aliases, likely subtags,
extension keys and extension types are an atomic group of sections (3–6).
Python checks their schemas, references, ordering and preferred-name chains.
The shared C table reader handles only strings/locales in sections 1 and 2.
`list/data` owns sections 10 and 12, validating templates and their exact
contextual admission rules before list consumers use them. Other services do
not validate or depend on list sections when opening the shared data view.
The locale owner uses
`qjs_intl_locale_metadata_open()` to check sections 3–6, including scoped
preferred-name graphs, before a runtime locale service can consume them.

The same owner exposes allocation-free typed extension-type queries and exact
extension/key/name lookup. Returned aliases, preferred identity, flags and
timezone labels remain borrowed source facts. Queries neither substitute aliases
nor advertise service capabilities. This lets native consumers share metadata
without decoding locale-owned wire columns themselves. Authored tests cover
legacy aliases, preferred/deprecated records, indirect types, timezone labels,
missing lookups and invalid-argument resets.

## Native locale identity operations

`src/intl/locale/native-id.c` consumes a successfully opened metadata view.
It canonicalizes Unicode locale IDs and extension values and implements Add
Likely Subtags and Remove Likely Subtags (favorRegion). Syntax validation and
canonicalization preserve allocator failures; inputs are length-bearing slices
and successful outputs are separately owned strings. These operations establish
identity only, not the locales installed for a formatter service.

The parser grows lists geometrically and sorts variant/attribute lists instead
of scanning an unbounded prefix for every item. It accepts long valid tags
without an arbitrary subtag limit. Alias replacement components are validated
before bounded language/script/region storage is used. Tests cover both prepared
profiles, nonterminated and embedded-NUL input, competing aliases, extension
ordering, unknown likely subtags, malformed replacement data and failure at every
allocation in canonicalize/maximize/minimize.

## NumberFormat JavaScript binding

`src/quickjs/builtins/intl/number-format.c` owns constructor/prototype behavior,
legacy chaining, cached bound functions, result conversion and JS object state.
The runtime owns a lazily opened number service; formatters have independent
native handles. The realm retains its intrinsic constructor and fallback symbol
so overwriting the public namespace does not alter intrinsic operations.

`number-format-options.c` reads non-locale options in specification order and
retains absent digit slots for resolvedOptions. Shared locale-key resolution
retains accepted `nu` extensions and canonicalizes explicit option values before
service capability checks. Numbering metadata alone never advertises support.

The cached `format` function captures its formatter and the getter realm's
intrinsic constructor. Both captures are marked, and the formatter class marks
its cached function so their cycle can be collected. Data callbacks do not
otherwise enter their creation realm; the constructor capture anchors that
realm for conversion errors, including borrowed getters. Ordinary native
methods retain their normal function-realm behavior.

Number/BigInt `toLocaleString` validate their receiver before reading locale or
option arguments, then use an intrinsic formatter without observing public
constructor/format replacements. Intl-off behavior remains the ECMA-262 path.

The focused Test262 profile now includes NumberFormat and its normative optional
constructor mode. Minimal excludes exact strings for unadvertised locales.
Three exact range-output tests prescribe collapse/spacing choices beyond the
current draft's ILND/no-collapse contract; their explicit exclusions cite that
policy. Repository tests retain coercion, rounding, equality, parts and sources.

## RelativeTimeFormat JavaScript binding

`src/quickjs/builtins/intl/relative-time-format.c` owns constructor/prototype
behavior and result conversion. It resolves `nu` with the shared locale-key
helper, reads style/numeric options in order, and checks receiver branding
before `ToNumber(value)` and `ToString(unit)`. Unit coercion still occurs for
nonfinite values; conversion errors precede later range checks.

The runtime owns the lazy relative service. Each object owns its formatter and
resolved strings, with no retained JS references requiring a mark callback.
The ordinary format methods use their own function realm for results/errors;
they are not cached bound functions. Parts have `type`, `value` and, for inserted
number spans, `unit` in that property order.

Focused repository tests cover descriptors, options/getter order, singular and
plural units, signed zero, exact internal rounding, auto lookup, numbering-system
precedence, subclasses, cross-realm behavior, reentrant coercion and GC. C tests
inject allocation failure in constructor, format, formatToParts, resolvedOptions
and supportedLocalesOf with fresh realm objects and warmed immutable services.
The focused Test262 configuration includes RelativeTimeFormat; the English-only
profile explicitly excludes seven cases requiring unavailable German/Polish data.

## Digital-duration source patterns

`duration_patterns.py` collects `units/durationUnit` patterns for `hm`, `hms`
and `ms`. It uses the pinned DTD attribute roles and main-locale inheritance,
selects default records before checking duplicates, and resolves typed group
aliases in the requesting locale or an explicit source locale. Each result
retains its terminal source locale and skeleton. Missing, blocked and explicit
empty values remain distinct; unsupported scopes or structural aliases fail.

The collector preserves raw LDML text. It does not yet select DurationFormat
locales or project patterns into the service's two separator fields and hour
padding preference. CLDR 49 beta 4 includes `h.mm:ss`, `hh:mm:ss`, quoted
prefixes and unquoted text around fields; requiring one identical separator or
discarding those distinctions during collection would lose source information.
Textual unit forms remain owned by `unit_patterns.py`.

Inspect a verified source cache without changing prepared packs:

```sh
python3 tools/intl/duration_patterns.py --sources path/to/cldr-49-beta4
```

Section 24, schema 1, stores these raw resolved records. Its three little-endian
header words are version, shared-row count and source-record count. Each shared
locale selects a three-word row (`hm`, `hms`, `ms` record indexes). Sorted unique
four-word records contain state (`0` missing, `1` value, `2` blocked), pooled
text, pooled terminal source locale and source skeleton (`0` hm, `1` hms, `2` ms).
Missing is exactly four zero words. Blocked records have empty text but retain
their source identity; a present empty pattern has state 1.

Rows and records are canonically shared and all must be reachable. Decoding
checks dimensions, complete extents, references, states and provenance before
returning borrowed source facts. Profile projection preserves source identities
outside the selected locale registry. The optional section is recognized by the
Python shared-pack validator and emitted by the production generator in both
prepared profiles. Its presence does not establish DurationFormat capability.

`src/intl/duration/data.c` validates the C schema before exposing borrowed views.
It checks every row and record, complete reachability, states, terminal source
locale spelling and skeleton identities. All temporary validation memory is
released before return, including multi-variant source-name scratch and failure
paths. Exact locale/skeleton queries allocate nothing and perform no display or
locale fallback. Raw LDML interpretation remains a separate service concern.

Authored C fixtures cover missing, blocked, empty and unequal-separator patterns,
external alias origins, malformed wire fields, every opening allocation failure
and retry. Both prepared profiles are opened and queried independently, including
unaligned input. The composed generator fixture also includes digital patterns.

`src/quickjs/builtins/duration-value.[ch]` provides the shared JavaScript-to-record
conversion needed by DurationFormat and Temporal. It copies branded duration
slots when available, or reads object fields in alphabetical order, or parses
a duration string. It preserves exact integral binary64 fields, including time
components outside the signed 64-bit range, and validates their combined bound.
Failures leave the caller's output record unchanged.

This conversion does not create a Temporal object, initialize its intrinsics,
or require Temporal constructors to be enabled. Temporal's existing conversion
then creates its result object from the validated record. The native duration,
epoch and unit arithmetic sources are shared when either feature is enabled;
calendar and time-zone implementations are not pulled into Intl-only builds.

`src/intl/duration/format.[ch]` implements the pure `ListFormatParts` composition
step. It replaces each ListFormat element with the next component's typed parts,
preserving every number-origin unit, including literal parts. List and digital
separators have no unit. Complete partitions and exact element text are checked
before allocating the owned text and parts; identical text and empty elements
remain distinct by position. Borrowed UTF-16 may contain NUL or lone surrogates.
The unit enum comes from the shared native duration record definitions, without
linking Temporal constructors or backend implementations.

`duration/native-digital.[ch]` projects the native backend's inherited `hms`
pattern into two separate inter-field literals and its raw hour width. It
validates scalar UTF-8, ordered `h`/`m`/`s` runs and apostrophe quoting, and owns
the dequoted separators independently of source storage. Outer literal text
is retained in the raw data but cannot be represented by DigitalFormat's two
separator fields. `hm`/`ms` padding does not override this `hms` projection.
Malformed data is an error, rather than an implicit colon fallback.

Do not replace colon characters with numeric `timeSeparator`: the
[TR35 number-symbol text](https://unicode.org/reports/tr35/tr35-numbers.html#timeseparator)
withdraws that pattern-character interpretation. The initial native projection
retains the same source separators across numbering systems; the Number
service localizes the digits. Authored syntax/OOM fixtures and every prepared
locale's `hms` pattern exercise this projection.

The raw hour width is deliberately separate from an effective
`TwoDigitHours` option policy. Both ECMA-402 revision
`730ca2b88c305862db7be2f83c510dcca33ab48c` and PR 1044 revision
`d026068ed653130a37d79a95c0122ecf5319f1a4` describe that flag as numeric-hour
padding, yet their `GetDurationUnitOptions` step unconditionally overrides the
hour style. [Proposal issue 161](https://github.com/tc39/proposal-intl-duration-format/issues/161)
discusses digital preferences and unresolved outer prefixes; the
[PR 188 review](https://redirect.github.com/tc39/proposal-intl-duration-format/pull/188#discussion_r1570697492)
explicitly objects to the style override. Its merge
`835300f1ee7a0a739ee2205dea2bae2657e67983` did not add a guard. The projection
preserves source facts without silently resolving that observable option-policy
contradiction. References reviewed 2026-10-19.

`duration/decimal.[ch]` converts a validated duration component into an exact
mathematical decimal for NumberFormat. It combines fractional second fields
using checked portable 128-bit integers before inserting the decimal point;
it never adds or divides them as binary64 values. This preserves nanoseconds
next to large seconds and integral field magnitudes beyond `int64_t`.
The helper allocates nothing and leaves rounding, visible fractional precision
and displayed-negative-zero policy to its consumer. Its shared native duration
arithmetic does not require Temporal JavaScript constructors.

`duration/options.[ch]` owns the pure duration-unit option algorithm over
already-coerced requests. The binding retains observable property access and
coercion order; it resolves each unit before reading the next unit's options.
The helper handles digital defaults, numeric/fractional transitions, display
defaults and forced minute/second widths. A separate resolved-record validator
protects the C formatter boundary. Effective `TwoDigitHours` is an explicit
input, rather than being inferred from raw pattern width inside this algorithm.

`duration/duration.h` is the compile-time service boundary. The native owner
maps its locale inventory into Number and List by canonical tag, rather than
assuming their indexes agree. It validates inherited digital syntax without
allocating per locale. Each formatter owns its separators, a unit List formatter
and Number formatters for displayed/suppressed signs, and may outlive its factory.
It retains no caller string or options storage. Allocation callbacks, including
the `realloc` required by List, outlive all corresponding handles and results.

Native duration composition uses the common Number and List interfaces:
mathematical fractional values remain exact, rounding truncates at the requested
fractional precision, only the first displayed component gets a sign, and zero
minutes bridge displayed hours and seconds. Digital separators carry no unit;
all Number-origin records, including literals, carry their component's unit.
The outer List parts preserve that metadata by element position. Intermediate
results are bounded and released on every failure; final results own their text
and parts independently of all formatter and factory handles.

The initial native effective `TwoDigitHours` policy is `false`, independently
of the raw CLDR hour width. This keeps the normative option algorithm and
explicit text-hour requests intact while the inconsistency described above
remains unresolved. Explicit `hours: "2-digit"` still pads. The policy does not
claim that raw `hh` preferences were absent: those facts remain in the data and
projection. Formatter ownership, all reached allocation failures, large exact
fractions, numeric/text grouping, negative zero, parts and representative digital
locales have focused C tests. The observable JavaScript surface is described
below.

## DurationFormat JavaScript binding

The enabled profiles now expose `Intl.DurationFormat`, its locale query,
resolved options, ordinary `format` and `formatToParts` methods. Constructor
options use `GetOptionsObject`, so nonobject primitives are rejected. Unit
style and display properties are read/coerced in specification order; transition
errors occur after the current display option and before the next unit.
Formatting accepts duration-like objects, strings and branded Temporal.Duration
slots through the shared converter, without allocating an intermediate Temporal
object. The Intl-only profile does not enable Temporal constructors.

With both features enabled, `Temporal.Duration.prototype.toLocaleString` uses
its realm's captured intrinsic DurationFormat constructor and native formatting,
independently of mutable global properties or prototype methods. The context
marks/releases that constructor; partial initialization resets its class
prototype and state. Formatter objects own only native handles and strings.
Runtime service caches close after object finalizers have released those handles.

Focused JavaScript tests cover options/input getter and coercion order, abrupt
completion, branding, parts, exact fractions, negative zero, numbering systems,
subclasses, realms and GC. Native tests cover reached binding allocation failures
and successful retries. The focused CI Test262 configuration admits DurationFormat
and Temporal.Duration locale formatting (`Temporal=auto`). Two tests also call
not-yet-delivered `supportedValuesOf` or DateTimeFormat APIs and remain deferred
until those dependencies land; repository checks cover Duration's numeric-system
options and method descriptors directly. The minimal profile
excludes only two Duration cases that require unsupported German/Serbian/Chinese
locales; English array-like locale arguments are covered in the repository tests.
The native profile keeps both cases.

## Locale numbering-system information

`Locale.prototype.getNumberingSystems()` returns the explicit numbering-system
slot, including an unknown or empty value. Otherwise it uses prefix matching
against NumberFormat's available locales and returns that locale's default system
as a singleton. An unmatched locale returns `latn`, independently of the host
default. This follows the pinned NumberingSystemsOfLocale algorithm, which does
not enumerate all supported digit repertoires. Binding-level orchestration shares
the runtime's Number service; the locale identity backend has no NumberFormat
dependency. Repository checks include a Unix Bengali host-default environment,
branding, foreign-realm arrays, fresh results and inherited index setters. The
corresponding Locale Test262 group is enabled in CI.

## DisplayNames JavaScript binding

Both enabled profiles expose `Intl.DisplayNames`, `supportedLocalesOf`, `of`
and `resolvedOptions` for all six standard types. Constructor options are
required objects and the `type` option is mandatory. Read style, type, fallback
and languageDisplay in specification order after locale resolution; validate
languageDisplay even for nonlanguage types, but report it only for language.

The binding owns CanonicalCodeForDisplayNames: canonicalize language aliases,
reject extensions/private use and duplicate variants, and apply only the specified
case regularization for other types. Preserve that API code independently of
backend lookup aliases. A missing field returns its canonical code or undefined
according to fallback; present empty/code-identical labels remain successful.
Fixed date-field identities and exact code parsing now live in `datetime/field`,
shared by the binding and native adapter without relying on binary table layout.

Formatter objects retain native handles and owned locale text, with runtime
service teardown and partial-intrinsic cleanup. JavaScript tests cover option
and code coercion order, abrupt completion, canonical fallback, descriptors,
subclasses, cross-realm behavior and GC reentrancy. Binding OOM tests reuse warmed
runtime services with fresh realm objects and retry every reached failure point.
CI admits both DisplayNames Test262 feature flags. One branding case also
constructs the undelivered DateTimeFormat and remains deferred until that
constructor lands; repository checks cover every currently delivered Intl receiver.
Microbenchmarks cover construction, composed language names, calendar aliases
and currency-name queries.

## Display-name source facts

`display_names.py` collects language, script, territory, variant and calendar
names, locale composition patterns and nested-bracket replacements. It retains
all source `alt`, `menu` and `scope` selectors independently; CLDR's `alt="long"`
is not an ECMA-402 style selection. Calendar `scope="core"` names coexist with
unscoped names, and script stand-alone names remain distinct from names used
inside a language description. Currency names already have a shared owner; date
field names have separate typed fields.

The collector returns `groups`, deduplicated `maps`, child-first `nodes` and
locale/group `rows`. A map entry is `(code, alt, menu, scope, text)`, keeping LDML
code spelling. A node is `(source_locale, group, map_index, parent_node)`. Node
references are one-based, with zero denoting no layer. Empty strings and explicit
blocking markers remain values; inheritance markers do not create a local
override. Whole-group aliases retain source identity and child overlays.

Retaining layers lets a consumer choose lateral alternatives at one source
locale before walking to its parent. It also distinguishes an explicit child
name from an inherited identical string. API fallback, language composition,
code canonicalization and locale availability belong to later consumers.
Authored tests cover all groups, exact text, alternative identities, metadata,
markers, source aliases, cycles and rejected ambiguous structures.

`display_tables.py` encodes these facts in section 26. Six header words contain
schema version 1 and the counts of shared locale rows, maps, nodes, entries and
lookup keys. One row index per locale selects seven group heads. A zero head
means no source layer; other heads and parent references are one-based. Maps
have contiguous entry spans. Nodes contain pooled source locale, group, map and
parent, ordered by depth and then by those four fields. Identical suffixes and
locale rows are shared; every row, map and node must be reachable.

Each six-word entry contains normalized code, `alt`, `menu`, `scope`, original
source code and text. The first four fields determine lookup order and uniqueness.
Identifier groups use lowercase ASCII and hyphens in the lookup column; pattern
and bracket keys retain exact spelling. Original spelling remains available for
provenance. Normalization collisions within one source layer are rejected;
overrides across child and parent layers remain valid. Empty text is a present
value; `0xfffffffe` encodes an explicit block, and missing keys have no entry.

A sorted group/code inventory covers all reachable entries, including blocked
and alternate-only facts. It supports later bounded compound-name lookup and
does not advertise service availability. Projection prunes unused layers while
retaining alias origins outside the selected locale profile. Codec tests cover
canonical roundtrips, shared raw maps with different group normalization,
projection, normalization collisions, empty profiles and malformed sections.
Both prepared profiles include the section. Minimal limits display locales to
English and its fallback chain while retaining every code in those layers;
native retains all CLDR display locales. Neither profile's source inventory
alone establishes DisplayNames service availability.

`src/intl/display/data.c` validates section 26 and exposes borrowed layer heads,
node identity, exact entry lookup and the sorted group/code inventory. Validation
checks canonical depth order, group-consistent heads and parents, complete
reachability, normalized/raw agreement and exact inventory coverage. Temporary
depth, coverage and source-locale scratch is freed on all paths. Queries allocate
nothing and do not walk parents, normalize caller codes or apply style/fallback
policy. An absent entry returns `UNSUPPORTED`; blocked and empty values return
distinct successful entries. Authored and empty-profile C tests cover provenance,
corruption, unaligned packs, argument resets and allocation failure/retry.
Prepared-profile C tests open both complete bundles, traverse every locale/group
chain and query the exact English language name. Generator tests verify English
facts, profile projection and deterministic authored output; manifests identify
the names section and authenticated inputs alongside the other shared tables.

`src/intl/display/select.c` applies native label policy separately from raw data
validation. For language, region, calendar and embedded script names, short and
narrow try `alt=short` then the ordinary label within each layer. Long uses the
ordinary label. Standalone scripts instead try stand-alone then ordinary for
long, and short, stand-alone, ordinary for short/narrow; that three-way priority
is an explicit native policy. Variants, patterns and bracket replacements use
their ordinary fields. Menu/scope variants and `alt=long` are not selected.

Only after trying a layer's alternatives does lookup follow its parent. An
explicit block masks that exact alternative in later layers, leaving the other
alternatives available. Empty and code-identical labels remain present. Selection
returns borrowed text and source locale without allocating or applying API code
fallback. Authored tests distinguish child ordinary/standalone names from parent
short names and cover independent blocks, empty values and equal child provenance.

`src/intl/display/calendar-names.c` derives calendar display identities from the
existing BCP47 metadata and normalized calendar-name inventory. It follows
preferred chains and source aliases, retaining original API identities such as
`islamicc` while selecting the `islamic-civil` source field. `gregory` and
`ethioaa` similarly reach their CLDR source names without a second alias dataset.
Well-formed source identities and alias API keys remain available independently
of arithmetic-calendar support. Invalid legacy spellings are source candidates,
not additional accepted API syntax.

The projection owns one sorted key/record allocation. Metadata is needed only
during construction; source lookup keys borrow the names blob. Exact lookups
allocate nothing and apply no alias substitution to API input. Genuinely distinct
source candidates or conflicting API-key mappings fail rather than choose an
arbitrary name; case/separator variants of one normalized source do not conflict.
Authored and prepared-profile tests cover preferred chains, alias agreement,
uppercase aliases, unknown source names, missing fields, ambiguity, empty data,
allocation failures and lookup after releasing the metadata blob.

`src/intl/display/language-match.c` selects the longest available dialect name
from actual source candidates for the input's primary language. Candidates must
contain ordered input subtags; ties prefer earlier input positions, then the
alphabetical source key. Each candidate resolves its style and inherited label
before ranking, so an inherited compound can beat a local generic name. Standard
mode selects only the primary language. Input is a validated canonical Unicode
Language ID without extensions or private use; canonicalization belongs to the
caller.

Matching allocates nothing and scans existing candidates rather than enumerating
variant subsets. Its result borrows the selected label, source locale and
normalized matched key. Empty and code-identical labels remain present. Authored
tests cover partial variant compounds, specificity/ties, primary boundaries,
blocked/menu-only candidates, inherited short-only compounds and long variant
lists. Prepared-profile tests check actual names rather than assuming that every
conditional example in TR35 exists in the pinned CLDR source. Composition of
remaining qualifiers and API fallback remain separate operations.

`src/intl/display/language-format.c` composes the matched base with unconsumed
script, region and variant labels, using canonical input subtags for missing
qualifiers. It preserves present empty names and requires an actual localized
base independently of API fallback. The result owns bounded UTF-16 text and
retains no source or input pointers.

The two-operand literal parser now belongs to `common/pair-pattern`, shared with
unit division templates. Language composition applies the source's outer pattern
and left-folded separator in either operand order. Iterative emission measures
then writes the exact result without recursion or repeatedly copied intermediate
strings. Only consulted templates are parsed. Native bracket substitution affects
qualifier labels, leaves the base name and pattern literals untouched, and uses
exact scalar replacement facts when the same bracket occurs in outer literals.
This follows native locale-pattern conventions rather than implementing a general
Unicode bracket-nesting algorithm. Tests distinguish ASCII/fullwidth brackets,
literal apostrophes, both fold orders, empty labels, malformed/absent templates,
supplementary characters, output bounds, allocation failures and result lifetime.

`display/display.h` is the backend-neutral DisplayNames service contract. The
native implementation composes the shared names, currency, date-field and locale
metadata readers; it does not open other Intl formatting services. Its available
locale list contains prepared non-root locale records and stays independent of
the raw code inventories. Admitted locales have usable inherited language-name
composition templates. A valid code need not have a localized field.

Formatters copy resolved options and immutable compiled-data views. Calendar
formatters additionally own their derived calendar-name projection, so all
formatters can outlive the creating service. Native queries return owned UTF-16
plus a distinct presence flag. Core code canonicalization and API code/none
fallback remain outside the backend. Currency uses its uncounted name at every
width, including present empty/code-identical names. Date-field queries use the
exact requested width and ordinary alternate. Simple label conversion and result
cleanup belong to `display/text`, shared with language composition. Repository
tests cover both profiles, all six kinds, all twelve date fields at three widths,
standard/dialect dispatch, numeric regions, limits, allocator failures and lifetimes.

## Shared date-field labels

`date_field_names.py` owns the twelve ECMA-402 date-field labels across long,
short and narrow widths. It maps `weekOfYear`, `dayPeriod` and `timeZoneName`
to LDML `week`, `dayperiod` and `zone`, retaining default and alternate labels
separately. Relative literals and counted relative-time patterns remain with
`relative_patterns.py`; this collector supplies data for consumers such as
DisplayNames and DateTimeFormat.

Rows contain requesting locale, field, width, alternate, state, exact text and
terminal source locale/field/width/alternate. Missing, blocked and explicitly
empty labels remain distinguishable. Typed field aliases restart at the
requesting locale for `source="locale"`, so an inherited narrow-to-short-to-long
alias can resolve the child's long label. There is no automatic width or
alternate substitution beyond source aliases. Authored tests cover every code,
widths, alternate labels, aliases, provenance, markers and structural rejection.

Section 25 stores these shared labels as little-endian words. Four header words
contain schema version 1, alternate count, row count and record count, followed
by pooled alternate identifiers and one row index per shared locale. A row has
twelve fields × three widths × alternate-count record references in that order.
Rows and six-word records are sorted and deduplicated, and every encoded row
and record must be used.

A record contains state, pooled text, pooled terminal locale, terminal field,
width and alternate indexes. The missing record is all zeroes. Present empty
text and blocked records retain their terminal identity. Projection retains
source locales outside the selected display-locale profile. Codec tests cover
roundtrips through the generic pack validator, projection, bad dimensions,
references, ordering, unused entries and noncanonical missing records. Both prepared
profiles include the section.

`src/intl/datetime/field-data.c` owns validation and borrowed queries for section
25. It checks bounded spans, canonical sharing, alternate spelling, source
identities and all references. Temporary validation and source-variant scratch
is released before return, including allocation failures. Label and alternate
queries allocate nothing, retaining exact state and terminal provenance.

Authored C tests cover requesting-context width aliases, alternate labels, empty
and blocked values, external multi-variant origins, corruption, output reset,
unaligned input and every reached allocation failure/retry. Prepared-profile
tests query all field/width/alternate combinations; generator tests check English
labels and the alternate AM/PM form. The shared owner establishes no service
locale availability and applies no consumer formatting fallback.
