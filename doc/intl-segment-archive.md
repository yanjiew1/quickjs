# Prepared native segmentation archive

`tools/intl/segment_prepare.py` compiles authenticated CLDR sources, the build
machine's Unicode property tool and pinned ICU dictionary text into a standalone
QJIN archive. It contains exactly:

| Section | Purpose |
| --- | --- |
| 1, 2 | Local UTF-8 string pool and the final selected shared locale inventory |
| 39 | All five scalar dictionary families |
| 40 | Compiled rules and every locale/granularity route |
| 41 | Complete sentence suppression policies and every locale route |

The existing [QJIN envelope](intl-data-format.md) and individual
[dictionary](intl-segment-dictionary-format.md),
[program](intl-segment-program-format.md) and
[suppression](intl-segment-suppression-format.md) schemas apply unchanged.
This is an intermediate host artifact. The final Intl pack receives only
sections 39/40/41, with locale indexes already matched; it does not receive a
second string pool or a second runtime data file. Native provider admission
remains a separate runtime check.

## Preparation and provenance

Use the completed formatter inventory, including synthetic language-region
identities, before assigning indexes. Inheritance remains segmentation-specific.
The [catalog](intl-segment-execution.md#shared-locale-preparation) compiles distinct
captured streams once and shares complete execution profiles while retaining
source origins. Dictionary and suppression graph construction remain independent
of word/sentence matching policy.

The archive generation identity and both rule/list fingerprints contain the
SHA-256 of a reproducible JSON recipe. The recipe includes the exact CLDR and
dictionary pins, selected locales, Unicode/semantic versions and hashes of
deterministic producer C/Python sources and Unicode tables. Only source CRLF
newlines normalize to LF; other producer bytes and whitespace remain significant.
Executable paths, compiler binaries, timestamps and local cache history are
excluded. Successful Unicode property-query range counts and hashes are recorded
separately in the preparation manifest, using only facts requested by this
invocation even when the underlying resolver already has cached queries.

`source_inputs.Inputs` tracks reads for this invocation independently of a
reader's previous activity. The manifest records authenticated CLDR/dictionary
file hashes, explicit source corrections, captured streams/routes, raw ordered
suppression origins, section payload hashes and license/notice hashes.
These identities support pairing and reproduction; they are not signatures or
proof that compiled transitions implement their source languages. Independent
execution oracles, reproduction checks and C validation remain necessary.

## Pairing checks

`tools/intl/segment_bundle.py` checks exact framing/profile/section inventory,
shared locale order and parents, recipe identity and complete coverage before
transferring native sections. Every locale, including source root, must have
all three granularity routes and a suppression route; explicit empty is valid.
All dictionary families are required. Preflight exact shared-table framing and
the archive-local `('',) + locales` string pool before decoding; parent IDs are
exact uint32 integers. Check inner fingerprints before typed graph copies, and
cheap identity/route failures before large dictionary graphs. Typed codecs validate
every retained graph/program and their own resource limits.

The local archive string IDs may differ from the final formatter pool. Locale
IDs must not differ: silently renumbering just one section would select another
locale's program or list. Final generation must pair the exact completed registry
and source/compiler recipe instead of copying arbitrary prepared sections.

## Licenses and explicit host command

The full pinned ICU release license accompanies dictionary derivatives.
The producer also emits all original per-file comment notices, preserving
additional Apple, Google and other attributions beside logical source paths.
The generic Unicode notice alone is insufficient.

```sh
python3 tools/intl/segment_prepare.py \
  --sources /path/to/cldr-49 \
  --dictionaries /path/to/icu-dictionaries-79.1 \
  --property-tool /path/to/build/intl-segment-property \
  --output /path/to/native-segmentation.qjin \
  --manifest /path/to/native-segmentation.json \
  --license /path/to/LICENSE-SEGMENT-DICTIONARIES \
  --notices /path/to/NOTICE-SEGMENT-DICTIONARIES
```

The property tool is a build-machine executable, including during cross builds.
Preparation downloads nothing; acquisition owns authentication and networking.
Normal engine builds consume the prepared final binary offline.

## Final pack integration

For a production native pack, pass `--compiled-segmentation` to
`tools/intl/generate.py` together with its compiled collation archive. The
producer recomputes the completed registry and recipe before transferring
sections 39/40/41. The enclosing manifest records the segmentation recipe and
the final generation identity includes every transferred payload hash.
Minimal generation accepts no segmentation archive and retains no dictionaries.

The pinned-source CI job reproduces the archive, preserved license/notices and
both final profiles. Normal builds still consume only `data/intl/native.bin`
or `minimal.bin`; `embed.py` emits one C owner and attributes dictionary-derived
bytes to the full dictionary license and per-file notice paths.

For the full Git source variant, pass `--source-pin tools/intl/pin-git.json`
and the `cldr-49-git` source root to preparation and final generation. Each
archive retains that exact source recipe; common/Git archives are not
interchangeable even when their compiled rule languages agree.
