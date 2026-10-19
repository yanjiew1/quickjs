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

This is an engine-independent formatter component, not yet the JavaScript
ListFormat service. Its tests cover output partitions, unusual UTF-16, ownership,
long lists, malformed templates, overflow and allocation failure.

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
