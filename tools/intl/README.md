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
is the shared locale count. Seven header words contain schema version 1 and
the numeric-system, vector, row, pattern, affix and token counts. Sorted numeric
definition indexes follow, then one (minimum grouping digits, row index) pair
per locale. Shared locale rows contain one vector index per numeric system;
each vector contains nine pattern references in collector field order. Optional
references use `0xffffffff` for absent, `0xfffffffe` for blocked and `0xfffffffd`
for explicitly empty. The five required patterns accept none of these markers.

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

## Exact plural relations

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
