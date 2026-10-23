# Native calendar data

Calendar algorithms follow the ECMA-402 Stage 4 era/monthCode PR, reviewed
on 2026-10-19 at `d026068ed653130a37d79a95c0122ecf5319f1a4`.
Its required publication ranges take precedence over arithmetic approximations.

`pin.json` records exact source identities. Raw redistributable receipts live
in `data/calendar/sources`; normalized factual records live in `data/calendar`.
Normal builds are offline and do not parse source documents or run source code.

## Persian leap years

The University of Tehran calendar authority publishes leap markers for
**1206 through 1498 AP**. The pinned official PDF has 144 Gregorian start
dates inconsistent with its own 365/366-day sequence. Use its leap markers,
not those inconsistent columns. The pinned CC0 transcription by Roozbeh
Pournader has the same 293 leap markers and consistent Gregorian anchors.
The normalized records preserve each single/double-star marker and verify
every adjacent year. The 1499 boundary is derived from the final leap marker;
it is not a published 1499 row.

Sources:

- [Official publication](https://calendar.ut.ac.ir/documents/2139738/7092644/Kabise+Shamsi+1206-1498.pdf)
- [Pinned transcription](https://github.com/roozbehp/persiancalendar/blob/bc1a519a68b5b0cc78c1d74b6fdb41825c677a65/kabise.txt)

## Umm al-Qura month lengths

The normalized **1300 through 1600 AH** records come from pinned ICU4X data,
which explicitly derives them from ICU4C. This is not a direct capture from
KACST and is not independent confirmation by two implementations. Preserve
the Unicode License V3 receipt with the source and derived data.

Every month is 29 or 30 days. Normalization checks each supplied Gregorian
start against the preceding month's lengths, rejects missing/duplicate years,
and derives the exclusive 1601 boundary. Both publication boundaries join
the required Islamic civil fallback continuously.

[Pinned ICU4X source and its ICU4C provenance](https://github.com/unicode-org/icu4x/blob/e078031999870d4f263635c319a79cb19543ccfc/components/calendar/src/cal/hijri/ummalqura_data.rs)

## Japanese eras

`sources/cldr-calendarData.xml` is raw supplemental data from the official
CLDR 49 common archive, pinned to release commit
`96e54b273d2da4ec6fbad350ba19ff23fa24a588`. Its modern era codes and
Gregorian start dates agree with ECMA-402's Japanese government era rules.
The Unicode License V3 receipt is `sources/LICENSE-CLDR`.

Normalization selects the era active on 1873-01-01 and every later era. It
does not require a fixed count or a C list of era names. ECMAScript uses
Gregorian eras before that date, so Meiji's lookup start is clamped to
1873-01-01, while its year offset still comes from its original start year:
1868 minus one. Thus 1873 remains Meiji 6. Earlier CLDR dates may be in the
old Japanese calendar and are not interpreted as Gregorian dates.

To update eras, review a new released CLDR source against the government
announcement and specification, replace the vendored source/license, and
update their revision and byte hashes in `pin.json`. Then run normalization
and generation below, and commit the source, `japanese.json`, binary and
manifest together. No C lookup change is needed for another era. CI compares
the regenerated JSON and binary; a synthetic sixth-era test verifies lookup
and year resolution with additional data. Updates require rebuilding the
embedded snapshot; runtime data loading is not introduced.

## Acquisition and offline normalization

Download the hash-pinned publications into a selected cache:

```sh
python3 tools/calendar/acquire.py --cache /path/to/calendar-cache
python3 tools/calendar/acquire.py --cache /path/to/calendar-cache --offline
```

Reproduce normalized records offline, then compare them with the repository:

```sh
python3 tools/calendar/normalize.py --output-dir /path/to/build/calendar
cmp data/calendar/persian.json /path/to/build/calendar/persian.json
cmp data/calendar/islamic-umalqura.json /path/to/build/calendar/islamic-umalqura.json
cmp data/calendar/japanese.json /path/to/build/calendar/japanese.json
```

To independently compare all official PDF leap markers, install `pdftotext`
and add `--verify-persian-pdf /path/to/calendar-cache/persian-official.pdf`.
The converter has a timeout, runs only for this explicit check, and requires
the exact pinned PDF. Downloaded publications are never runtime dependencies.

Run the fast offline generator tests with
`python3 tests/test_calendar_generator.py`.

## Chinese and Dangi publications

The PMO source is the `lunarInfo` numeric table hosted and actively used by
the specification-linked PMO page. Its third-party widget author is Jea Yang.
The tool extracts factual numeric records only; it never executes or
redistributes the widget code. These records agree with all 45,991 overlapping
daily records in the separately reviewed PMO 1900–2025 PDF. That comparison
does not independently certify the widget's future dates through 2100.
HKO differs on 30 dates in 2057; HKO values are not substituted for PMO.

The table starts at lunar New Year 1900. Its preceding M12 is manually
transcribed from PDF page 3: January 1 is day 1, January 30 is day 30, and
January 31 starts the next year. `pin.json` records the source PDF and page.
Earlier months of lunar 1899 use the explicitly documented approximation.

Dangi publication records come from 24 pinned official KASI responses, one normal/leap
month-start query per month. The response's Gregorian date, JDN, weekday,
Gregorian leap flag, request identity and stated length are checked before
merging. The actual response coverage, rather than its requested end year,
controls the proven interval. Lunar 2050 M11 ends on January 13, 2051.

From that boundary through the end of lunar 2100 (January 28, 2101), Dangi
uses the pinned ICU4X 2.3.0 `korea_data.rs` precomputed records. These were
generated by ICU4X's `calendrical_calculations` Korean rules; they are not
additional KASI publications. Normalization checks every overlapping year
against the existing KASI composite before appending the extension and checks
every precomputed year join. Native and JavaScript regression tests check the
return to our approximation in 2101.
`primary_span` and `required_span` retain their original coverage, while
`precomputed_source` and `precomputed_span` identify the extension separately.
The vendored Rust source is only parsed as data by Python; no Rust toolchain
or ICU library is needed to build or run QuickJS. Its Unicode License V3
receipt is `data/calendar/sources/LICENSE-ICU4X`.

[Pinned Korean precomputed source](https://github.com/unicode-org/icu4x/blob/e078031999870d4f263635c319a79cb19543ccfc/components/calendar/src/cal/east_asian_traditional/korea_data.rs)

The normalizer checks complete mandatory date coverage, normal/leap month
order and every join. It accepts approximate months only entirely outside
the mandatory Gregorian interval. Normalized records retain primary spans,
required spans and the exact approximate months; approximation is never
labelled a published fact. These are factual publication transcriptions,
without a claim that PMO/KASI publish their website code under our license.

After acquisition, build the small exporter for the build host and normalize:

```sh
meson compile -C /path/to/build calendar-boundary
python3 tools/calendar/lunisolar.py \
  --cache /path/to/calendar-cache \
  --boundary-tool /path/to/build/calendar-boundary \
  --output-dir /path/to/build/calendar
cmp data/calendar/chinese.json /path/to/build/calendar/chinese.json
cmp data/calendar/dangi.json /path/to/build/calendar/dangi.json
```

No target executable, ICU installation or system timezone database is used.
Normal builds do not fetch these publications. Offline pack regeneration
checks the normalized records against their recorded byte hashes; the command
above separately reproduces those records from the acquired source receipts.

## Single binary pack

`generate.py` checks normalized records and their source identities, then emits
one `calendar-data.bin` plus a provenance manifest. QJCA version 2 uses big-endian
integers: a 32-byte header, 32-byte dataset indices, source-version strings and
12-byte year records. Each year stores its first epoch day, two bits per month
for lengths 29 through 31, the month count and the nominal month repeated by
the inserted leap month (zero when absent). Indices
store contiguous year coverage and the separately derived terminal boundary.
Calendar kinds are stable format identifiers, independent of C enum values.

Japanese eras use kind 50 in the same pack. Its index has zero first-year and
terminal-day fields, an era count, and a payload extent of 40 bytes per era:
signed 32-bit first epoch day, signed 32-bit year offset, and a 32-byte
NUL-terminated canonical ASCII name with a zero-filled tail. Names are unique,
dates strictly increase, and records are bounded to 1,024 eras. The reader
exposes borrowed immutable names and rejects malformed encodings. Other
calendar kinds retain their 12-byte year records; v1 packs are rejected.

Both the Python generator and C reader validate bounds, reserved bits, ordering,
calendar-specific month shapes and every adjacent/terminal boundary. Year lookup
indexes directly into immutable records, avoiding repeated cumulative scans.
`embed.py` validates the pack and creates one C data owner in the build directory.
The C reader also accepts an immutable byte span for a future external loader;
current builds do not load calendar files at runtime. No compression library is
required. The four year datasets and Japanese era dataset share this single
pack; the manifest records its exact size and hash.

```sh
python3 tools/calendar/generate.py \
  --output /path/to/build/calendar-data.bin \
  --manifest /path/to/build/calendar-data-manifest.json
cmp data/calendar/calendar-data.bin /path/to/build/calendar-data.bin
cmp data/calendar/calendar-data-manifest.json /path/to/build/calendar-data-manifest.json
```
