# Windows time-zone mapping

`windowsZones.xml` and `LICENSE` are exact Unicode CLDR source files pinned in
`pin.json`. The XML SHA-256 and source commit/date are embedded in the generated
`src/timezone/windows-zone-data.inc`. The XML is data covered by UNICODE LICENSE
V3; the separate LDML publication terms in the upstream LICENSE are retained
verbatim. The generated derivative retains its Unicode attribution and the
complete upstream copyright and permission notice is distributed here.

Regenerate explicitly, from the repository root, using the backend's exact
pinned tz source directory:

```
python3 tools/gen-windows-zones.py \
  --xml tools/timezone/cldr/windowsZones.xml \
  --cldr-pin tools/timezone/cldr/pin.json \
  --tz-source "$TZ_SOURCE_DIR" --tz-pin tools/timezone/pin.json \
  --output src/timezone/windows-zone-data.inc
python3 tests/test_windows_zones_generator.py \
  --tz-source "$TZ_SOURCE_DIR" --tz-pin tools/timezone/pin.json
```

The generator validates the CLDR XML hash and every relevant pinned tz source
hash. Every target must name a Zone or Link in those sources, including backward
and backzone aliases. It rejects duplicate Windows key/territory pairs, key case
collisions, malformed names/territories/lists, and keys without a `001` mapping.
Generation is deterministic and uses neither a compiler nor an engine.

The Windows-only production module contains an immutable byte array. Normal
builds and runtime discovery need no XML parser, Python interpreter, ICU, or
external mapping files. The Windows object depends on the committed `.inc`.

Windows discovery calls `GetDynamicTimeZoneInformation` and uses its
`TimeZoneKeyName`. `StandardName` is localized and unsuitable for mapping.
Current CLDR Windows keys are printable ASCII; the wrapper validates every
UTF-16 code unit and rejects non-ASCII or unterminated keys without lossy
conversion. Windows keys compare with ASCII case folding, independent of locale.
Empty/unknown keys and Windows API errors return failure. Output capacities
include the terminating NUL; an available output buffer is cleared on failure.

Native discovery calls `GetUserGeoID(GEOCLASS_NATION)` and then
`GetGeoInfoW(geo, GEO_ISO2, buffer, capacity, 0)` to obtain the user-selected
country. `GEOID_NOT_AVAILABLE` means no geographic location is configured;
`GetGeoInfoW` returns zero on failure. `LangId` must be zero for `GEO_ISO2`.
The buffer capacity is counted in WCHAR units. The wrapper accepts only two
uppercase ASCII letters followed by a verified NUL. It fills the buffer with
nonzero sentinels before the call and accepts only returned counts of two or
three, accommodating whether the copied-word count includes the terminator.
It rejects absent, partially written, nonterminated, numeric, non-ASCII,
lowercase, oversized, or otherwise malformed results without lossy conversion.

The lookup prefers the exact territory row for that Windows key. A missing,
invalid, or unmapped territory selects `001`; a geographic API error also
selects `001`. This policy combines the configured time-zone key with the
user's selected country, which can differ from the country of that zone. A
valid country does not guarantee a territory-specific row. Both geographic
APIs are supported since Windows XP/Server 2003; the module's existing
`GetDynamicTimeZoneInformation` call still sets a Windows Vista baseline.
There is no `GetUserDefaultGeoName` dependency or optional-function import.
Microsoft documents the geographic APIs at
[GetUserGeoID](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getusergeoid)
and [GetGeoInfoW](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getgeoinfow).

A Windows key can cover several IANA zones, and a territory can still contain
an ordered list of targets. This is not a unique conversion. The compiled table
retains the complete CLDR list and returns its first target, as the recommended
default. For example, Pacific Standard Time maps to America/Vancouver for CA
and America/Los_Angeles for 001; Central Standard Time's CA list begins with
America/Winnipeg. The selected user country supplies this distinction; it does not identify a
unique zone within a country. The
backend validates and canonicalizes the returned IANA identifier; the lookup
intentionally returns CLDR aliases such as Asia/Calcutta. Backend policy owns
explicit TZ/POSIX override precedence.

`DynamicDaylightTimeDisabled` disables Windows' dynamic year-specific rules;
it does not by itself disable all daylight saving. Windows can use fixed
transition dates instead. Completely disabling daylight saving also requires
clearing its standard/daylight transition dates. `TIME_ZONE_ID_UNKNOWN` can
indicate a zone without transitions, including an ordinary named zone; it is
not sufficient evidence of a manually configured fixed offset either.

Discovery treats a recognized nonempty Windows key as the named-zone setting
and preserves its CLDR mapping regardless of that flag or the current API
status. It does not infer a fixed offset from the flag, replace a recognized
key with an offset, or reinterpret custom Windows transition dates. IANA rules
for the selected name govern the backend's calculations. Those calculations
can differ from Windows local clock conversion when Windows uses fixed
year-specific transition dates, disabled daylight saving, or custom rules.
This named-key mapping provides no claim of OS-clock agreement under such
settings. Unknown/missing keys and API errors return failure for the backend
to apply its established fallback policy.

The binary format `WTZ1` has a 16-byte little-endian header: magic[4], total
length u32, key count u16, row count u16, and pool offset u32. Sorted key records
are eight bytes: pool offset u16, first row u16, row count u16, reserved zero
u16. Territory rows are eight bytes: three ASCII bytes (`001` or two letters
plus NUL), reserved zero u8, list pool offset u16, and list byte length u16.
The pool contains interned NUL-terminated ASCII keys and ordered target lists
separated by one space. Offsets are relative to the pool. No structure casts,
native-endian reads, or alignment assumptions are used. The decoder checks the
header/table bounds, every consulted key string, selected row range, each
selected-key row's list bounds/terminator, and the chosen list's characters.

`tests/test_windows_zones.c` is a standalone portable C fixture: compile that
file alone, because it includes the module to exercise malformed/truncated
binary inputs. On Windows it also mocks the time-zone and both geographic APIs, checking
that the wrapper actually calls them with `GEOCLASS_NATION`, `GEO_ISO2`, the
WCHAR capacity, and zero language. It covers Canada, US, unmapped country,
missing GEOID, geographic failures/counts, partial/full/nonterminated buffers,
non-ASCII and malformed countries, time-zone API errors, missing/unknown keys,
UTF-16 rejection, output buffer sizes, and disabled dynamic rules. These mocked
checks do not establish live Windows API behavior. The separate
`tests/test_windows_zones_system.c` fixture independently queries both actual
geographic APIs to obtain its expected territory mapping. Native discovery and
integration require separate root-run Windows/MinGW/Wine gates.
