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
