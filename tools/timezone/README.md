# Native timezone source inputs

`pin.json` selects released **IANA tzdata 2026e** and **CLDR JSON 48.2.3**.
The IANA Git tag resolves to `039ef27cc5f062a2055cb67435d6d71adbefd27d`.
CLDR is pinned to `91c267402229a59e3ef2774544f001bf959e8809`.
The CLDR JSON patch release includes timezone changes for tzdata 2026e;
the older CLDR XML 48.2 release does not include all those changes.

## Acquisition

Choose a cache on a filesystem with sufficient space:

```sh
python3 tools/timezone/acquire.py --cache /path/to/timezone-cache
python3 tools/timezone/acquire.py --cache /path/to/timezone-cache --offline
```

Acquisition uses HTTPS, checks the archive and every consumed source against
SHA-256 pins, limits download size, and uses finite request timeouts and
retries. It replaces verified files atomically. Archive extraction accepts
only the named regular files, validates the complete source set before writing,
and never follows archive paths or symlinks. Existing verified inputs need no
network. Updating a release requires an explicit pin review.

The cache contains `tz-2026e/` and its archive. Raw CLDR timezone metadata is
versioned under `data/timezone/sources/cldr`, with the Unicode license. Acquisition verifies those
vendored files without downloading duplicate cache copies. `timezone.json`
supplies geographic alias groups; `windowsZones.json` supplies
Windows-key/territory mappings. These are generator inputs, never runtime
JSON dependencies. The native backend's final data is a single binary pack.

Normal builds do not download data, read installed timezone files, or run
this acquisition script. Pinned zic sources are available only for build-host
data generation; target runtime code does not link tzcode.

Sources reviewed on 2026-10-19:

- [IANA release archive](https://www.iana.org/time-zones/releases/2026e)
- [CLDR 48.2.3 release notes](https://cldr.unicode.org/downloads/cldr-48)
- [Released CLDR JSON sources](https://github.com/unicode-org/cldr-json/tree/48.2.3)

Run the fast offline acquisition contracts with:

```sh
python3 tests/test_timezone_generator.py
```

## Primary identities and the binary format

`primary.py` applies ECMA-402's primary-identifier rules. It preserves
`zone.tab` country-zone exceptions and cross-country historical links rather
than treating every alias as the final linked Zone. CLDR supplies geographic
alias groups, not an alternative primary-identifier policy.

`bundle.py` writes one endian-independent QJTZ version 2 container. Its
48-byte header and 16-byte index records use big-endian 32-bit integers.
The header records total size, section offsets, source-version string offsets
and algorithm compatibility version 1. Reserved fields must be zero.
Each index record contains identifier and primary string offsets plus a TZif
payload extent. Names are ordered by canonical ASCII spelling; the payload
pool deduplicates identical bytes while preserving separate identities.

The validator checks canonical strings, bounds, ordering, primary relationships
and exact payload coverage. It recognizes TZif framing; the C rule reader
separately validates complete TZif contents before embedding or use.

## Offline regeneration

After acquisition, generate the checked-in pack and its audit manifest:

```sh
python3 tools/timezone/generate.py \
  --cache /path/to/timezone-cache \
  --output /path/to/build/timezone-data.bin \
  --manifest /path/to/build/timezone-data-manifest.json
cmp data/timezone/timezone-data.bin /path/to/build/timezone-data.bin
cmp data/timezone/timezone-data-manifest.json /path/to/build/timezone-data-manifest.json
```

The generator compiles the pinned zic using `HOST_CC` or `cc`; use
`--host-cc` to select another **build-host** C compiler. It does not use a
cross compiler from `CC`, invoke target binaries, download inputs, or depend
on the installed zic. `awk` and a host C toolchain are needed for regeneration.
Temporary headers, executables and per-zone files stay under the selected
cache and are removed after generation. Subprocesses have finite timeouts.

The input tailoring retains `backzone` histories and `zone.tab` country
identities. It applies no leap-second table or date-range truncation.
The manifest contains fixed source identities and hashes, without host
paths or changing timestamps. CI regenerates and compares both outputs.
