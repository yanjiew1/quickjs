# Native IANA time zones

The pristine IANA release `2026e` is supplied by the `third_party/tz`
submodule, pinned to `039ef27cc5f062a2055cb67435d6d71adbefd27d`.
Initialize it with `git submodule update --init --recursive` before builds.
`pin.json` binds every consumed source. The target library uses original
C code in `src/timezone`; no tzcode runtime sources or headers are linked.

`host-zic.c` compiles the pinned `zic.c` for the build host only. The host
compiler, Python and awk must be available. Generation performs no network
or Git operations and never executes target programs. Geographic metadata
uses pinned released CLDR XML at build time, covered by `cldr/LICENSE`.

The generator runs upstream `ziguard.awk` with `PACKRATDATA=backzone` and
`PACKRATLIST=zone.tab`. Host zic compiles complete fat TZif without leap
corrections or a range restriction. Per-zone compiler intermediates live
in a temporary directory. The final `timezone-data.bin` is one QJTZ binary
containing names, primary relationships and deduplicated payloads.

`bundle.py` defines the version 1 container: big-endian header/index offsets
point into its strings and TZif payloads. Alias records share the primary
payload extent. `embed.py` validates that binary and derives byte and index
views whose names point into the same embedded strings. The JSON manifest
is an audit sidecar; runtime uses no XML, JSON or installed zoneinfo tree.

`make test-timezone-generator` runs metadata, container-validation and exact
embedding fixtures without executing zic or the JS engine. The build rules
for generating the binary are introduced before provider activation.

## Immutable native provider

`make libqjstimezone.a` builds the decoder and provider without QuickJS or
ICU. System files are preferred per primary name; absent, malformed,
leap-aware or coverage-limited data selects the compiled fallback. Accepted
system bytes are copied with the caller's allocator. Embedded snapshots
borrow compiled bytes. Every successful source read has exactly one release,
and allocation errors propagate rather than becoming cached fallback success.

One serialized embedding agent owns a `QJSTzProvider`. Its lazy cache keeps
the first accepted immutable primary snapshot; aliases share that snapshot.
Provider-open results are borrowed until provider destruction. Standalone
`qjs_tz_open` results are owned and closed explicitly. No global `TZ` mutation,
libc calendar calculation or tzcode runtime symbols are emitted.

Unix discovery recognizes compiled identifiers from `TZ`, `/etc/localtime`
or `/etc/timezone`; Windows uses the compiled CLDR key/region table. Runtime
Date and Temporal policy integration is introduced separately.
