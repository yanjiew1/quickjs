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
SHA-256 of a small data-input recipe: actual CLDR inventory, actual dictionary
file hashes (including the license), selected locales and supported Unicode /
semantic profile. Declared release provenance is recorded separately.
Generator source comments, compiler binaries, paths and timestamps are not
inputs to this identity. A producer code change affects artifacts only when
its output changes; archive and section hashes identify that output.
Successful Unicode property-query facts remain in the preparation manifest.

`source_inputs.Inputs` tracks reads for this invocation independently of a
reader's previous activity. The manifest records consumed CLDR/dictionary
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
and data-input recipe instead of copying arbitrary prepared sections.

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

For a production native pack, pass `--compiled-segmentation` and its existing
`--segmentation-manifest` to `tools/intl/generate.py`, together with compiled
collation. Final generation verifies the sidecar's archive size/hash, recomputes
its input fingerprint and matches the current CLDR inventory and locale
registry before transferring sections 39/40/41. This binds stages without
requiring final generation to reopen dictionary text. The enclosing manifest records the segmentation recipe and
the final generation identity includes every transferred payload hash.
Minimal generation accepts no segmentation archive and retains no dictionaries.

The pinned-source CI job reproduces the archive, preserved license/notices and
both final profiles. Normal builds still consume only `data/intl/native.bin`
or `minimal.bin`; `embed.py` emits one C owner and attributes dictionary-derived
bytes to the full dictionary license and per-file notice paths.

For the full Git source variant, pass `--source-pin data/intl/pin-git.json`
and the `cldr-49-git` source root to preparation and final generation. Each
archive retains that exact source recipe; common/Git archives are not
interchangeable even when their compiled rule languages agree.
