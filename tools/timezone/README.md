# Native IANA time zones

The data source is the pristine IANA tz release `2026e`, pinned to
`039ef27cc5f062a2055cb67435d6d71adbefd27d`. `pin.json` binds all data and
generation sources that are consumed. The target library compiles original
C code in `src/timezone`; it includes no tzcode source or tzcode headers.
IANA's public-domain `tzfile.5` and `localtime.c` were consulted for format
and future-rule semantics, without importing their runtime implementation.

`host-zic.c` builds the pinned `zic.c` only for the build host. The generator
runs `ziguard.awk` with `PACKRATDATA=backzone PACKRATLIST=zone.tab`, then uses
host-zic to compile complete, fat TZif data without leap corrections or a
range restriction. The per-zone files are temporary compiler intermediates
inside a temporary directory. The generator's final binary output is ONE
`timezone-data.bin` containing identifiers, primary relationships and
deduplicated complete payloads. It does not install or ship a zoneinfo tree.

`bundle.py` defines the QJTZ version 1 format. Its 32-byte header and 16-byte
index records contain big-endian unsigned offsets; all strings and payloads
live in this one file. Aliases refer to their primary's exact payload extent.
`embed.py` validates the entire binary and derives `timezone-data.inc` from
its exact bytes. The C index uses pointers into those embedded binary strings,
so there is no second hand-maintained identifier source. A JSON manifest is
an audit sidecar, not another runtime data file. It records the release, the
complete binary hash, every record extent and each TZif payload hash.

`tzif.c` validates both TZif blocks and decodes the 64-bit timeline with byte
reads. It rejects unknown versions, leap-aware data, invalid counts, indices,
offsets, abbreviations, flags, ordering and framing. It reads POSIX future
rules directly: fixed offsets, Julian/zero-based/month-week-weekday dates,
signed transition times in versions 3/4, southern seasons and perpetual
daylight time. Gregorian recurrence is calculated with integer arithmetic
throughout the supported Temporal range; no libc calendar or global TZ state
is used. Designation-only and DST-flag-only changes are excluded from the
offset-transition API. The API's strict/inclusive second boundary semantics
remain unchanged; the Temporal caller handles subsecond cutoffs.

One `QJSTzProvider` belongs to one serialized embedding agent or JSRuntime.
Its lazy cache preserves the first accepted immutable snapshot for each
primary until provider destruction. Aliases share the snapshot. A new runtime
can observe changed system data. Metadata always comes from the compiled
binary; a system file cannot introduce names or alter primary relationships.

System files are preferred per primary. Missing, unreadable, malformed,
version-1, leap-aware, over-65536-byte or coverage-limited files select the
embedded reference. Legitimate historical `-00` intervals and Factory remain
usable when those intervals do not expand beyond the pinned reference.
Structurally valid system databases can differ from the pin's political rules.
Allocation failures propagate and do not become cached failures or fallback
success. Every successful source read receives exactly one release, including
failure to allocate the retained copy. Accepted system bytes are copied with
the caller's allocator; embedded snapshots borrow the compiled bytes. Both
allocators and sources retain their documented opaque-pointer lifetimes.

Date and Temporal retain the same host/provider selection when Temporal is
enabled. ICU Date and Temporal continue through the existing ICU backend.
The optional native Intl frontend uses the runtime's borrowed native provider
only when ICU is disabled. ICU-enabled builds select ICU data exclusively;
ambiguous CONFIG_INTL_NATIVE plus CONFIG_ICU is rejected. Native Intl's
per-realm default timezone is a snapshot
of native host discovery; the native Intl provider reports the same canonical
identifier. Native Intl configuration identifiers are validated against QJTZ
metadata and retained as immutable compiled strings, not caller buffers.

Unix discovery uses known TZ identifiers, an `/etc/localtime` zoneinfo symlink
or `/etc/timezone`. The existing Windows module retains its separate compiled
CLDR binary mapping and `GetUserGeoID`/`GetGeoInfoW` region discovery, with
`001` as the geographic fallback. This change does not replace or regenerate
that mapping. The CLDR/IANA primary algorithm and pinned CLDR XML inputs are
unchanged. Unicode licensing remains in `tools/timezone/cldr/LICENSE`.

All source-defined external functions and data use the `qjs_` namespace.
`namespace-inventory.json` lists the original decoder exports. No libc time
API, `tz_*` symbol, tzcode global state or tzcode runtime object is emitted.

Build entry points:

```sh
make CONFIG_INTL=n libqjstimezone.a
make CONFIG_INTL=n CONFIG_TEMPORAL=y
make test-timezone-generator
make CONFIG_INTL=n tests/test_tzif_decoder tests/test_timezone_system_reader tests/test_timezone
```

Cross builds use a host-executable pinned `HOST_ZIC` and never execute target
programs during generation. The submodule, host compiler, awk and Python must
already be available. Generation performs no network or Git operation.

The preparation is source-only. Root validation must regenerate one exact
binary, check embedding equality, run all decoder/provider/Temporal/Date and
Windows witnesses, repeat the four feature matrix rows and related official
groups, and audit GCC/Clang/32-bit/MinGW namespace and no-ICU dependencies.
Earlier accepted tzcode receipts are historical evidence and do not establish
acceptance of this replacement implementation.
