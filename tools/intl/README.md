# Native Intl data tools

These Python 3.6 or newer host tools prepare immutable source data for native
Intl generation. Normal builds use prepared binary data and do not download
or parse XML.

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

`number_layout.py` resolves miscellaneous range/approximation/at-least/at-most
templates, currency-name unit templates, currency-spacing match sets/insertion
text and legacy ISO append patterns. They are distinct from numeric skeletons
and per-currency display fields. Alias interpretation is typed and limited to
the reviewed group/spacing paths, with requesting-locale and explicit-source
semantics. Text and UnicodeSet expressions remain exact source facts.

Miscellaneous templates and currency boundaries are required for each numeric
system. Currency-name selectors and the ISO append pattern may be absent,
blocked or explicitly empty. In particular, root `arab` has spacing but lacks
currency-name templates; this collector does not invent a cross-system fallback.
Selecting plural/currency-name fallback and interpreting spacing predicates
belong to later consumers. CI audits the authenticated full source inventory;
this collector or its packed records establish no formatter capability.

Section 20 has the shared locale count in its directory and four little-endian
header words: schema version 2, numeric-system count, shared vector count and
shared row count. Exact numeric numbering-definition indexes follow, then one
shared-row index per locale. Each sorted unique row has one vector index per
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

`src/intl/number/layout-data.c` validates section 20 against the shared numbering
inventory and exposes allocation-free exact locale/system queries. Strings
borrow immutable pack storage; empty VALUE text differs from absent/blocked
fields. Open releases its validation scratch on success and failure. Parsing
templates, evaluating UnicodeSet predicates and selecting formatter fallback
are separate consumer responsibilities.

## Sanctioned measurement-unit identifiers

`src/intl/unit/identifiers.def` is the common inventory of the forty-five
ECMA-402 sanctioned single units and their CLDR data identifiers. The private
C identifier helper performs exact case-sensitive lookup and accepts either
one sanctioned unit or precisely two joined by `-per-`. It does not accept
arbitrary CLDR units, aliases, multiplication or localized names. The helper
allocates nothing; source collection and formatting remain separate work.

`unit_patterns.py` collects long/short/narrow display names, unit patterns with
their exact plural/case selectors, grammatical gender, per-unit patterns and
division compound layouts. It preserves all source single-unit data identifiers;
that inventory is broader than the sanctioned API list. Sparse shared maps
resolve unit/width aliases in requesting-locale or explicit-source context.
Absent fields, blocked fields, explicit empty text and inheritance remain
distinct. No case/plural/width formatting fallback is invented by collection.

Digital duration layouts, coordinate formatting and arbitrary product/power
composition have separate consumers. CI audits all authenticated relevant unit
records; collecting those source facts does not publish unit formatter support.

Section 21 stores those sparse unit forms. Its directory count is the shared
locale count. Six little-endian header words contain schema version 1, group,
case, shared-row, shared-map and entry counts. Group records contain kind
(`compound=0`, `unit=1`) and a name string reference; only division `per` is a
compound group. Sorted case string references start with the empty default case.
One row index per locale precedes sorted unique rows, each containing one map
index for every width (long/short/narrow) and group combination.

Sorted unique maps have contiguous `(offset, count)` descriptors into two-word
entries. An entry holds a selector and text reference. The selector's high eight
bits identify the collector field, the next eight identify absent count (0) or
one of its eight count selectors (1–8), and the low sixteen hold a case index.
Entries are strictly sorted by selector. Absence means no entry; `0xfffffffe`
is blocked text, and string index zero is explicitly empty. Inheritance/block
markers cannot be ordinary pooled text. Unit and compound fields cannot mix
or be assigned to the wrong group family. Rows/maps and non-default cases must
all be referenced. Projection removes unused maps/cases while preserving the
global source group inventory; that inventory is not API availability.

`src/intl/unit/data.c` validates section 21 against the unchanged shared data
view, including selector identities, map families, complete ownership and
canonical ordering. Indexed and exact queries borrow immutable storage and
allocate nothing. They preserve missing, blocked and explicitly empty forms;
grammatical/plural/width fallback and formatter capability belong to consumers.

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

Scientific/engineering output applies the exact scale-before-round operation,
then emits localized exponent separator/sign and ASCII decimal exponent digits
with explicit part types, following the pinned PartitionNotationSubPattern.
Its explicit `"0"` exponent and missing transliteration differ from established
ICU behavior; the open [ECMA-402 issue 627](https://redirect.github.com/tc39/ecma402/issues/627)
tracks that specification gap. Mantissa digits still use the selected repertoire.
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

This is a staged internal implementation, not complete Intl.NumberFormat.
Currency/unit styles, compact notation, ranges and the JS binding remain required
before registration. Repository tests exercise every prepared locale's default
numeric system and allocation failure at every service/open/format allocation.

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
build directory. The typed C reader validates UTF-8/string ordering, record
extents, references, the parent graph and list templates before exposing borrowed
text slices. Temporary graph-validation memory is freed before return, including
on errors. Separate C repository tests consume both real prepared profiles, so
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
The C reader additionally checks every locale/variant against its base templates
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
The shared C table reader handles sections 1, 2 and 10. The locale owner uses
`qjs_intl_locale_metadata_open()` to check sections 3–6, including scoped
preferred-name graphs, before a runtime locale service can consume them.


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
