# Updating and replacing generated data

Normal builds consume prepared data. They do not download CLDR, Unicode or
timezone publications, and the runtime does not parse XML, JSON or source
documents. Data maintenance is an explicit operation in a working branch.

The intended sequence is **select sources → generate candidate pins → review
provenance → regenerate → validate → commit a coherent update**. A new source
version, a generated binary format and a generator's compatibility policy are
different things; changing one does not automatically change the others.

## Maintenance commands

| Operation | Entry point | Families |
| --- | --- | --- |
| Acquire and authenticate pinned sources | `python3 tools/data/acquire.py FAMILY` | `cldr`, `dictionaries`, `calendar`, `timezone` |
| Generate and verify prepared candidate data offline | `python3 tools/data/generate.py FAMILY` | `intl`, `calendar`, `timezone` |
| Calculate a candidate source lock | `python3 tools/data/update_pin.py` | CLDR, dictionaries, calendar, timezone |

Put `--help` after a family for its input options. Family collectors and
validators remain in their owning tool directories; orchestration and output
helpers are private in `tools/data/_support`. Unicode keeps its existing C
generator and download/checksum procedure listed below.

Acquisition verifies pin hashes. Generators accept compatible supplied inputs
without requiring those hashes to match; they validate structure/semantics and
record the bytes consumed. A source lock can accompany custom input as metadata.

## What a pin means

Pins are reproducibility locks, not a requirement to calculate hashes by hand.
Use [`update_pin.py`](../tools/data/update_pin.py) to generate them. The tool
hashes the actual inputs, computes CLDR's complete logical inventory and
dictionary file sizes, and writes a separate candidate. It refuses to
overwrite an input pin or raw input file.

An HTTPS download plus a calculated checksum is not independent verification of
the publisher or the data's meaning. Review the official release/tag, checksums
or signatures when available, licenses and publication provenance. Supply
`--expected-sha256` when an independently obtained archive checksum is available.
The tool does not manufacture semantic approval. Changes relative to the supplied
template clear its review date, and the report says no semantic review was
performed. A manually edited `--pin` is the new comparison baseline: clear its
stale review dates and supply the inputs matching any edited provenance yourself.
Fill review/publication dates only after checking them.

| Owner | Accepted source lock | Prepared outputs / detailed instructions |
| --- | --- | --- |
| Intl locale, collation and boundary rules | [`data/intl/pin.json`](../data/intl/pin.json); alternate [`pin-git.json`](../data/intl/pin-git.json) | `data/intl`; [data tools](../tools/intl/README.md) |
| Segmentation dictionaries | [`segment-dictionary-pin.json`](../data/intl/segment-dictionary-pin.json) | Included in native Intl; full dictionary license and notices |
| IANA rules and CLDR Windows/alias metadata | [`data/timezone/pin.json`](../data/timezone/pin.json) | `data/timezone`; [timezone tools](../tools/timezone/README.md) |
| Calendar publications and Japanese eras | [`data/calendar/pin.json`](../data/calendar/pin.json) | `data/calendar`; [calendar tools](../tools/calendar/README.md) |
| Unicode properties and normalization | [`unicode_download.sh`](../tools/unicode_download.sh), [`unicode-data.sha256`](../tools/unicode-data.sha256), version macros in `src/unicode/libunicode.h` | `libunicode-table.c/.h`; [build instructions](../BUILDING.md#external-suites-and-developer-tools) and Unicode CI |

Use a build/work directory outside tracked data for candidates, caches and logs.
The paths below are examples to replace for your machine. Python 3, a build-host
C toolchain, Meson and Ninja are needed for the full generation pipeline.

## CLDR: official common release or complete Git data

The default pin selects the official **common release ZIP**, whose production
filtering intentionally omits lower-coverage locales. The separate Git pin
selects the complete `common/` tree of an immutable release commit. Neither
distribution is silently supplemented from the other.

Generate a candidate for a selected release without manually computing hashes:

```sh
python3 tools/data/update_pin.py \
  --pin data/intl/pin.json --release 49 --resolve-tag --download \
  --output /path/to/work/common-pin.json \
  --manifest-output /path/to/work/common-inventory.json \
  --report /path/to/work/common-pin-report.json
```

Replace `49` with the release being reviewed. `--resolve-tag` explicitly resolves
the official GitHub tag to a commit; no mutable `latest` lookup occurs during
normal builds. For an offline operation, use `--commit FULL_SHA --archive FILE`
instead of tag resolution/download. To reproduce the current accepted pin from
an existing archive, omit `--release` and `--commit`.

The source archive is inspected with the same bounded reader used by normal
acquisition. Unsafe paths, links, duplicate logical files, oversized inputs and
unsupported layouts are rejected. The generated manifest covers every selected
`common/` file and `LICENSE`; it is not a list of only currently consumed files.

For the corresponding full Git distribution:

```sh
python3 tools/data/update_pin.py \
  --pin data/intl/pin-git.json --release 49 --resolve-tag --download \
  --output /path/to/work/git-pin.json \
  --manifest-output /path/to/work/git-inventory.json
```

Alternatively, `--distribution git` derives the Git archive URL from a common
template; `--distribution common` selects the official common URL. For custom
URLs, copy a pin to a working JSON template and edit its
`cldr.archive` and provenance fields directly. Pass that template with `--pin`
and the actual archive with `--archive`. The supported archive layout and CLDR
schema still apply; no checksum needs calculating by hand.
For Git archives the root is `cldr-COMMIT/`; common ZIPs contain `common/` and
`LICENSE`. An arbitrary JSON export is not an interchangeable XML source.

Acquire each authenticated candidate into its own cache:

```sh
python3 tools/data/acquire.py cldr --source-pin /path/to/work/common-pin.json \
  --cache /path/to/common-cache
python3 tools/data/acquire.py cldr --source-pin /path/to/work/git-pin.json \
  --cache /path/to/git-cache
```

Cache roots are `cldr-RELEASE` and `cldr-RELEASE-git`. `--offline` repairs/verifies
them using an already cached, matching archive. Compare inventories and resolved
locale graphs; fewer production files do not translate directly into the same
number of unsupported locales because parents and aliases also matter.
[`check_git.py`](../tools/intl/check_git.py) separately compares a full Git
snapshot against an actual clean checkout, including blob identities.

## Regenerate and select an Intl pack

Compile the **build-host** tools once; do not pass cross-target executables:

```sh
meson setup /path/to/host-tools -Dtests=false -Dexamples=false -Dtemporal=false
meson compile -C /path/to/host-tools intl-collation-compile intl-segment-property
python3 tools/data/acquire.py dictionaries --cache /path/to/dictionary-cache
```

Run the scripted offline pipeline for either selected source distribution:

```sh
python3 tools/data/generate.py intl \
  --sources /path/to/common-cache/cldr-49 \
  --source-pin /path/to/work/common-pin.json \
  --dictionaries /path/to/dictionary-cache/icu-dictionaries-79.1 \
  --collation-compiler /path/to/host-tools/intl-collation-compile \
  --property-tool /path/to/host-tools/intl-segment-property \
  --output-dir /path/to/work/common-packs
```

The command compiles collation, prepares native segmentation, generates each
requested pack and runs the source/manifest verifier. It retains per-stage logs
and `regeneration-report.json`, with finite stage timeouts and child cleanup.
Output/log aliases into source trees are rejected before execution; existing
logs must be regular files. Use `--profiles minimal` to omit native
dictionaries/property preparation, or `--profiles native` for native only. A failed prerequisite stops dependent work;
partial outputs remain candidates. Repeat using the Git pin and Git source root
into a different output directory.

Use the resulting pack without replacing the bundled distribution:

```sh
meson setup /path/to/custom-build -Dintl=native \
  -Dintl_data_file=/path/to/work/common-packs/native.bin
```

The selected pack must match the minimal/native profile. Current builds embed
it; they do not load or switch data at runtime. Dictionary preparation accepts
supplied compatible text without matching pin hashes. Pass an optional
`--dictionary-pin /path/to/candidate.json` for declared source metadata; no
repository pin needs replacing merely to generate a candidate.

## Dictionaries, IANA rules and calendar publications

The same pin command supports the other existing pin schemas. For dictionaries,
it computes SHA-256 and byte lengths for all six inputs, including
the complete license. A release change requires the complete source set:

```sh
python3 tools/data/update_pin.py \
  --pin data/intl/segment-dictionary-pin.json \
  --release 79.1 --resolve-tag --download \
  --output /path/to/work/dictionary-pin.json
```

Use `--source-tree /path/to/icu-checkout` instead of `--download` for an offline
ICU tree, or `--files-root DIRECTORY` for a flat directory of named inputs.
Select a clean checkout at the declared commit; a local tree is not proved to
match a remote commit just because the command hashes its files. Preserve all
third-party notices, not only the generic Unicode license. No ICU library or
Rust toolchain is introduced into the engine by using their data receipts.

For an IANA release, the tool hashes the archive and checks the same required
source set used by generation, without maintaining member hashes. The accepted
layout is the `eggert/tz` Git release archive:

```sh
python3 tools/data/update_pin.py \
  --pin data/timezone/pin.json --release 2026e --resolve-tag --download \
  --output /path/to/work/timezone-pin.json
```

CLDR timezone metadata has an independent version. Derive its candidate from
the same selected CLDR snapshot instead of copying hashes manually:

```sh
python3 tools/data/update_pin.py --pin data/timezone/pin.json \
  --cldr-pin /path/to/work/common-pin.json \
  --cldr-sources /path/to/common-cache/cldr-49 \
  --output /path/to/work/timezone-cldr-pin.json
```

This copies release/commit/archive identity and hashes the supplied
`timezone.xml`, `windowsZones.xml` and `LICENSE` members. It leaves the IANA pin
unchanged. After review, stage those raw members and pin, then use the existing
timezone acquisition/generation scripts. Check primary identities, renaming
policy, territory mappings, historical rules and transitions.

Japanese eras and their CLDR license can use the same propagation command with
`--pin data/calendar/pin.json`; unrelated calendar sources remain unchanged.
Calendar publications otherwise have independent publishers and versions;
there is no single calendar release to substitute. Copy its pin to a working
JSON template and edit the selected source's revision, URL and descriptive
metadata. Then hash the replacement raw input and maintained factual JSON:

```sh
python3 tools/data/update_pin.py --pin /path/to/work/calendar-template.json \
  --file kabise.txt=/path/to/new/kabise.txt \
  --data-dir /path/to/updated-calendar-facts \
  --output /path/to/work/calendar-pin.json
```

Also replace/update the corresponding license and source URL when changing
that publication. Review descriptive notes/ranges for the new source;
propagation does not rewrite factual interpretation. Metadata changed by the
updater requires its actual input bytes; manual template edits follow the
review responsibility described above. Untouched receipts remain unchanged.
`--data-dir` rehashes the
normalized JSON names listed in the template. It does not import raw data or
certify the calendar algorithms; generation performs the semantic checks.
Calendar normalization/generation remain
separate scripted stages described in the calendar README. Required published
ranges, independently checked transcriptions and approximation boundaries must
remain distinguishable. Do not extend a claimed official range by relabeling an
approximation or an ICU precomputation.

## Unicode and compatibility review

For Unicode, update the downloader version, complete source checksums and the
library version macros together. Download into a work directory; verify the
release's UCD/emoji inputs, regenerate `.c` and `.h` with
`-Dunicode_data=/path/to/ucd`, and
run the exhaustive CI normalization, property and boundary oracles. New property
values or encoding assumptions may require generator/algorithm changes.

The pin tool deliberately does not rewrite compatibility code. In particular,
[`segment_compat.py`](../tools/intl/segment_compat.py) applies a reviewed,
byte-guarded Japanese FE correction. A new source may fix or alter that defect;
inspect the new declarations and remove/adapt the correction with regression
coverage. Never just refresh guard hashes to make an unexplained failure pass.
The dictionary grammar, schema/DTD contracts, Unicode-property version and source
publication checks likewise remain active after candidate generation.

Regenerate when input data or compiler output changes. Segmentation identifies
actual data inputs, while archive and section hashes identify compiled output;
producer comment or checkout changes alone do not require data churn. Never
edit generated receipts to disguise changed inputs or output. Source version changes do
not by themselves justify changing binary wire versions; see the
[format guide](intl-data-format.md) and component format documents.

## Acceptance and commit scope

Review source/license differences, inventory additions/removals, generated
capabilities and representative semantic differences. Validate both production
common and full Git variants after a CLDR upgrade. Run repository tests in the
selected configurations and complete the matching CI source-reproduction,
Test262, sanitizer and platform gates. Measure likely performance changes.

Stage reviewed pins/raw receipts, necessary compatibility changes, generated
packs/manifests and focused regressions together. Keep candidate caches, logs,
downloaded non-vendored publications and machine-specific paths out of commits.
Record exact source versions and reasons; do not claim arbitrary data versions
work merely because their hashes can be generated.

## Reference projects

Reviewed 2026-10-24. These are design references, not dependencies:

- ICU4X separates source providers from export selection and supports explicit
  source versions/local roots. Its CLI's `latest` means the generator's tested
  source constant, not unrestricted upstream head. It recommends reproducible
  generated data and CI freshness checks: [data management](https://icu4x.unicode.org/2_4/tutorials/data-management/),
  [2.4 source/CLI](https://github.com/unicode-org/icu4x/blob/031d9e89f6a3fd26a14491e090729d46094328fd/provider/icu4x-datagen/src/main.rs).
- ICU4C has an explicit CLDR conversion/integration procedure and separate data
  build/filter/packaging tools. Replacing source data involves regeneration and
  tests, not merely renaming a binary package: [CLDR integration](https://unicode-org.github.io/icu/processes/cldr-icu.html),
  [data build tool](https://unicode-org.github.io/icu/userguide/icu_data/buildtool.html).

This project adopts that source-selection/generation/acceptance separation while
retaining its own complete file hashes, binary contracts and semantic guards.
