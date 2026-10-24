# Native Intl data tools

Use the [data maintenance guide](../../doc/data-updates.md) for the three ordinary
operations: **acquire**, **generate**, and **update pin**. This page maps the
internal tools for people changing collectors, algorithms or data formats.
Normal builds consume prepared binary data; they neither download inputs nor
parse XML. Host Python tools support Python 3.6 or newer.

## Source and generation contract

Pins live beside data in `data/intl`. The official common CLDR release ZIP is the
production source; the complete Git snapshot has a separate explicit pin and
inventory. Do not silently supplement one from the other. Segmentation dictionary
sources and licenses have their own independent pin.

Acquisition authenticates archive/file identities before materialization, bounds
input sizes and rejects unsafe paths. Generators accept compatible supplied
sources without requiring old pin hashes. They validate structure/semantics,
record actual consumed bytes and reject source mutation within a generation
session. A declared source revision or a calculated hash is not semantic approval.
Keep a generation's source snapshot unchanged until all dependent stages finish.

Raw input, typed source resolution, encoded sections and executable capability
are distinct. Collectors own their supported LDML keys and alias paths; the
shared resolver owns reusable inheritance mechanics. Preserve explicit empty
values, blocking markers, significant whitespace and original source provenance.
Unsupported relevant input must fail rather than disappear silently. A source
inventory or successful binary read alone does not establish API availability.

## Component map

All names below are local to this directory unless a path is explicit. Collector
modules have corresponding `*_tables.py` codecs where their sections need them.
Read the actual consumer and [section reference](../../doc/intl-data-sections.md)
together before changing a field or selector.

| Component | Owners | Input → output |
| --- | --- | --- |
| Source access and schema | `acquire.py`, `source_inputs.py`, `cldr_xml.py` | Acquired/supplied CLDR files → bounded byte access, snapshots and checked attribute inventory |
| Locale inheritance | `parents.py`, `resolve.py`, `availability.py` | Component parents, aliases and markers → typed resolved layers and projected requesting/source identities |
| Identity and preferences | `locale_metadata.py`, `preferences.py`, `numbering.py` | Supplemental/BCP47/main facts → identity, preference and numbering records |
| Numeric sources | `plural_rules.py`, `number_patterns.py`, `number_symbols.py`, `compact_patterns.py`, `currencies.py`, `number_layout.py` | CLDR numeric facts → exact grammar, notation, symbol, currency and range records |
| Units | `unit_patterns.py`, `unit_grammar.py` | Sanctioned unit patterns and derivations → source-preserving component grammar |
| Lists, relative and duration | `list_patterns.py`, `relative_patterns.py`, `duration_patterns.py` | Resolved patterns → typed templates/context rules and digital separators |
| Date sources | `calendar_sources.py`, `date_patterns.py`, `date_available_formats.py`, `date_combinations.py`, `date_intervals.py` | Calendar/locale aliases and date text → typed source catalogs, not already-selected ECMA formats |
| Date names and presentation | `calendar_names.py`, `calendar_eras.py`, `dayperiod_rules.py`, `timezone_names.py`, `timezone_presentation.py`, `date_field_names.py` | Names/rules/metazones → independent source and presentation tables; timezone arithmetic remains elsewhere |
| Display names | `display_names.py` | Names, alternatives and composition templates → layered labels, independently of arithmetic support |
| Collation preparation | `collation_root_input.py`, `collation_ir.py`, `collation_bindings.py`, `collation_prepare.py` | Logical root plus import-expanded tailoring and profile target closure → paired host inputs and compiled archive |
| Collation C compiler | `collation-compile.c` and owning `collation-*.c` helpers | Counted source programs, pinned Unicode normalization and symbolic ordering → ranked tries/elements and QJCL readback |
| Boundary-rule compiler | `segment_sources.py`, `segment_inheritance.py`, `segment_expression.py`, `segment_sets.py`, `segment_nfa.py`, `segment_dfa.py`, `segment_lower.py`, `segment_prepare.py` | CLDR rules and Unicode properties → executable boundary programs, source bindings and fingerprints |
| Dictionaries/suppressions | `segment_dictionary*.py`, `segment_suppressions.py` and their codecs | Supplied dictionary text/costs and exception lists → compact immutable tries and suppression maps |
| Final pack | `generate.py`, `bundle.py`, `tables.py` | Admitted typed records plus compiled collation/segmentation → minimal/native QJIN and actual-input manifest |
| Readback and embedding | `check_data.py`, `embed.py` | Prepared pack/manifest → semantic reproduction checks or one generated C data owner |

`scalar_trie.py` supplies genuine shared scalar-graph construction. Typed
collectors retain their own grammars and selector meaning. Do not turn this map
into a generic schema framework or infer that all modules need the same parent
chain, locale inventory or fallback policy.

## Host C tools and diagnostics

`intl-collation-compile` and `intl-segment-property` are build-host executables,
including during cross builds. Python owns source XML, syntax/import expansion
and collection; the C helpers use the engine's pinned Unicode implementation.
Host Python's Unicode version does not replace the C normalization/property
algorithms. Local sanitizer builds also instrument these native reader paths.

Build the tools in the configured build directory before end-to-end generation:

```sh
meson compile -C /path/to/build intl-collation-compile intl-segment-property
```

Useful focused diagnostics (paths are examples):

```sh
python3 tools/intl/cldr_xml.py --sources /path/to/cache/cldr-49
python3 tools/intl/segment_sources.py --sources /path/to/cache/cldr-49
python3 tools/intl/segment_dictionary.py \
  --sources /path/to/cache/icu-dictionaries-79.1
python3 tools/intl/check_git.py --repository /path/to/cldr-checkout \
  --sources /path/to/cache/cldr-49-git
```

For a complete Git-source snapshot, pass the corresponding `--source-pin` where
the command accepts it; the maintenance guide shows the coherent pipeline.
Dictionary data reuse does not add an ICU backend or runtime ICU dependency.
Retain the full dictionary license/notices, including their third-party terms.

The collation compiler separates nonallocating structural preflight, normalization
and execution. Its audit commands can check existing host artifacts without
publishing executable locale data:

```sh
/path/to/build/intl-collation-compile --audit-ir /path/to/input.qjci
/path/to/build/intl-collation-compile --audit-pair /path/to/root.qjcr /path/to/input.qjci
```

Compiled source fingerprints pair actual root/tailoring inputs. They are not
cryptographic authenticity claims. The ranked runtime archive is separate from
raw root diagnostics: production omits QJIN sections 36/37, while
`tests/data/intl-collation-oracle.bin` retains the independent raw source oracle.
The ordinary generation operation reproduces it when requested; do not rebuild
it by decoding ranked output.

## Format and algorithm references

| Reference | Owner |
| --- | --- |
| [QJIN envelope](../../doc/intl-data-format.md) and [section layouts](../../doc/intl-data-sections.md) | Runtime pack and all typed sections, including test-only raw diagnostics |
| [QJCR](../../doc/intl-collation-root-format.md), [QJCI](../../doc/intl-collation-ir-format.md), [QJCL](../../doc/intl-collation-compiled-format.md) | Logical root input, counted host program input, ranked compiled archive |
| [Segmentation expressions](../../doc/intl-segment-expressions.md), [program format](../../doc/intl-segment-program-format.md), [archive](../../doc/intl-segment-archive.md) | Rule syntax, executable programs and source binding |
| [Dictionary format](../../doc/intl-segment-dictionary-format.md) and [suppression format](../../doc/intl-segment-suppression-format.md) | Word dictionaries and sentence exceptions |
| [Native decisions](../../doc/intl-native-notes.md) | Interpretation choices, formatting policies and reviewed specification discrepancies |
| [Architecture](../../doc/intl-design.md) | Functional/shared modules, contracts, defaults and ownership |

Repository tests cover authored source grammar, malformed binary reads, independent
oracles, allocation failure/retry and actual prepared profiles. CI reproduces
selected source/data artifacts. Follow [BUILDING.md](../../BUILDING.md) and the
active workflow for test selection; do not copy a frozen target inventory or
validation cadence into this tool map.
