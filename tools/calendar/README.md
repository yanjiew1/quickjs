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

## Acquisition and offline normalization

Download the hash-pinned official PDF into a selected cache:

```sh
python3 tools/calendar/acquire.py --cache /path/to/calendar-cache
python3 tools/calendar/acquire.py --cache /path/to/calendar-cache --offline
```

Reproduce normalized records offline, then compare them with the repository:

```sh
python3 tools/calendar/normalize.py --output-dir /path/to/build/calendar
cmp data/calendar/persian.json /path/to/build/calendar/persian.json
cmp data/calendar/islamic-umalqura.json /path/to/build/calendar/islamic-umalqura.json
```

To independently compare all official PDF leap markers, install `pdftotext`
and add `--verify-persian-pdf /path/to/calendar-cache/persian-official.pdf`.
The converter has a timeout, runs only for this explicit check, and requires
the exact pinned PDF. Downloaded publications are never runtime dependencies.

Run the fast offline generator tests with
`python3 tests/test_calendar_generator.py`.

## Single binary pack

`generate.py` checks normalized records against their source receipts and emits
one `calendar-data.bin` plus a provenance manifest. QJCA version 1 uses big-endian
integers: a 32-byte header, 32-byte dataset indices, source-version strings and
12-byte year records. Each year stores its first epoch day, two bits per month
for lengths 29 through 31, the month count and the nominal month repeated by
the inserted leap month (zero when absent). Indices
store contiguous year coverage and the separately derived terminal boundary.
Calendar kinds are stable format identifiers, independent of C enum values.

Both the Python generator and C reader validate bounds, reserved bits, ordering,
calendar-specific month shapes and every adjacent/terminal boundary. Year lookup
indexes directly into immutable records, avoiding repeated cumulative scans.
`embed.py` validates the pack and creates one C data owner in the build directory.
The C reader also accepts an immutable byte span for a future external loader;
current builds do not load calendar files at runtime. No compression library is
required. Persian and Umm data together currently occupy 7288 bytes.

```sh
python3 tools/calendar/generate.py \
  --output /path/to/build/calendar-data.bin \
  --manifest /path/to/build/calendar-data-manifest.json
cmp data/calendar/calendar-data.bin /path/to/build/calendar-data.bin
cmp data/calendar/calendar-data-manifest.json /path/to/build/calendar-data-manifest.json
```
