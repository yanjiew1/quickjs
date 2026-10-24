# Native timezone source inputs

[`data/timezone/pin.json`](../../data/timezone/pin.json) selects released
**IANA tzdata 2026e** and **CLDR 49 common release XML**.
The pin records IANA release commit
`039ef27cc5f062a2055cb67435d6d71adbefd27d` as provenance. Acquisition verifies the release archive
SHA-256 before placing it in the cache.
CLDR is pinned to `96e54b273d2da4ec6fbad350ba19ff23fa24a588`.
The vendored BCP47 aliases and Windows mappings are exact members of the
authenticated official common archive; its URL/hash and member paths are recorded
in the timezone pin. The IANA rule release remains independently selected.

## Acquisition

Choose a cache on a filesystem with sufficient space:

```sh
python3 tools/timezone/acquire.py --cache /path/to/timezone-cache
python3 tools/timezone/acquire.py --cache /path/to/timezone-cache --offline
```

Acquisition uses HTTPS, verifies the release archive's SHA-256, limits download
size, and uses finite request timeouts and retries. It replaces the archive
atomically. Existing verified inputs need no network. Updating a release
requires an explicit pin review.

Only `tz-2026e.tar.gz` is needed in the cache. Generation reads supplied archive
bytes and materializes required regular files into a fresh private directory;
it does not check them against pin hashes.
It validates the complete set before writing any source, rejects duplicate or
linked required members, and never uses archive paths as output paths. Old
extracted `tz-2026e/` cache directories are ignored, so modifying a cached
`zic.c` cannot change the compiler used by generation. Per-member IANA hashes
are unnecessary: acquisition authenticates the archive as one source.

Raw CLDR 49 common-release metadata remains under
`data/timezone/sources/cldr`, with the Unicode license. Acquisition verifies
these separately vendored files without downloading duplicate cache copies.
`timezone.xml` supplies geographic alias groups; `windowsZones.xml` supplies
Windows-key/territory mappings. These are generator inputs, never runtime XML
dependencies. The native backend's final data is a single binary pack.

Normal builds do not download data, read installed timezone files, or run
this acquisition script. Pinned zic sources are available only for build-host
data generation; target runtime code does not link tzcode.

Sources reviewed on 2026-10-23:

- [IANA release archive](https://www.iana.org/time-zones/releases/2026e)
- [CLDR 49 release notes](https://cldr.unicode.org/downloads/cldr-49)
- [Official CLDR common archive](https://unicode.org/Public/cldr/49/cldr-common-49.zip)

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

The generator compiles the supplied zic using `HOST_CC` or `cc`; use
`--host-cc` to select another **build-host** C compiler. It does not use a
cross compiler from `CC`, invoke target binaries, download inputs, or depend
on the installed zic. `awk` and a host C toolchain are needed for regeneration.
Temporary headers, executables and per-zone files stay under the selected
cache and are removed after generation. Subprocesses have finite timeouts.

The input tailoring retains `backzone` histories and `zone.tab` country
identities. It applies no leap-second table or date-range truncation.
The manifest records declared source revisions and actual archive/CLDR input
hashes, without host paths or changing timestamps. CI regenerates and compares both outputs.

## Runtime embedding

Meson builds `timezone-describe` for the build host, even when cross compiling.
`embed.py` uses that checked C reader to validate every distinct TZif payload
and emits one C data owner plus immutable rule descriptors. Runtime queries
borrow those views; they do not allocate, reparse the pack or load files.
Canonical UTC belongs to the common timezone layer. The native backend retains
UTC aliases as distinct identifiers, with their primary identity set to UTC.

The metadata follows ECMA-402's AvailableNamedTimeZoneIdentifiers and
GetAvailableNamedTimeZoneIdentifier contracts. Names retain canonical spelling,
lookup ignores ASCII case, and all mappings/rules remain stable for the process
lifetime. The IANA release review must check new Zone renames and their
recommended waiting period before changing primary identities. No active
rename waiting override is needed for the pinned 2026e snapshot.

Common consumers can filter `qjs_timezone_at()` with `qjs_timezone_is_primary()`
to obtain AvailablePrimaryTimeZoneIdentifiers, preserving sorted order.
`qjs_timezone_info()` supplies both the UTC offset and actual DST designation
for ECMA-402 ToLocalTime. Negative DST and equal-offset designation changes
are retained; DST is never inferred by comparing numeric offsets.

`tests/test_native_timezone.c` reparses every compiled payload and compares its
view with the generated descriptor. It also checks all identifiers, aliases,
ordering and case variants, plus real historical offsets, negative DST,
date-line gaps and distant future recurrence. The test uses only embedded data.

## Windows host mappings

`windows.py` converts the released `windowsZones.xml` into a compact WTZ1
binary table with interned strings and 16-bit indices. Meson emits that binary
as its own C translation unit; runtime code never parses XML. This host
identity table is separate from the single QJTZ rule pack and requires no
additional runtime data file. Every IANA target is checked against the pack.

The lookup uses the stable Windows timezone key and the user's ISO country,
preferring an exact territory row and then `001`. It preserves CLDR's target
order and selects the first target. Windows discovery uses
`GetDynamicTimeZoneInformation`, `GetUserGeoID` and `GetGeoInfoW(GEO_ISO2)`;
localized display names are not used as keys. The discovery API requires Vista
or newer. Custom Windows rules or disabled dynamic daylight settings can differ
from the selected IANA zone's rules.

## Supplied inputs

Use `generate.py --archive /path/to/tz-release.tar.gz --cldr-dir /path/to/cldr`
to generate from custom sources; the CLDR directory needs `timezone.xml`.
`--pin /path/to/pin.json` supplies declared release names and provenance when
using another release. Archive structure, required files, primary identities
and timezone rules are still validated. The generator records actual input
hashes without requiring them to match the pin. Run acquire separately when
authentication against official source identities is wanted.
