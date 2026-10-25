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

# Calendar JSON facts

The files in [`data/calendar`](../data/calendar) are the maintained, readable
inputs to calendar data generation. They contain parsed facts; ordinary
regeneration does not need the original Rust, XML, PDF or website responses.
The runtime reads the generated QJCA binary, not JSON.

This document describes the current input contract. The validators and generator
must change together if that contract or its supported coverage changes; the
`schema` field is not a promise to support older development formats.

## Files and consumers

| File | Contents | Validation / transformation |
| --- | --- | --- |
| `persian.json` | Published year starts and leap markers | `normalize.persian_data` |
| `islamic-umalqura.json` | Year starts and twelve month-length bits | `normalize.ummalqura` |
| `chinese.json` | Composite lunisolar years and source coverage | `generate.datasets`, then `bundle.pack` |
| `dangi.json` | Composite Korean years and identified future extension | `generate.datasets`, `lunisolar.korea_precomputed`, then `bundle.pack` |
| `icu4x-korea-data.json` | Parsed precomputed Korean years, including padding | `lunisolar.korea_precomputed` |
| `japanese.json` | Modern era identifiers, starts and year offsets | `normalize.japanese_data` |
| `pin.json` | Source provenance, accepted hashes and boundary policy | Acquisition and maintenance tooling |
| `calendar-data-manifest.json` | Generated input receipts and binary summary | Generator output, not a factual input |

The named modules are in [`tools/calendar`](../tools/calendar). Source history,
licenses and publication discrepancies are explained in its
[README](../tools/calendar/README.md); QJCA's binary layout is documented there
under [Single binary pack](../tools/calendar/README.md#single-binary-pack).

## Common conventions

- Each facts file is a JSON object with integer `schema: 1` and its exact
  `calendar` identifier. Duplicate object keys are rejected.
- Dates encoded as `epoch_day` are signed whole days relative to proleptic
  Gregorian **1970-01-01 = 0**. They are civil date coordinates, independent of
  timezone, DST or milliseconds. For example, `-25567` is 1900-01-01.
- Spans use **[inclusive start, exclusive end)** epoch days.
  `end_epoch_day` is the first day after the final stored year.
- `first_year` agrees with the first row. Years are ordered, consecutive and
  complete; a following year starts at the previous start plus its month lengths.
- Numeric facts are integers, not quoted strings, booleans or fractional values.
  Month-length arrays store days, not month numbers.
- Keep one year per line where practical. Whitespace and JSON key order do not
  affect calendar meaning, but byte hashes change when serialization changes.

The row excerpts below illustrate field positions; they are not complete input
files. The generator currently requires the coverage described for each dataset,
including the terminal boundary. Shortening an example into a real dataset or
editing coverage labels alone does not extend supported publication data.

## Persian years

```json
{
  "schema": 1,
  "calendar": "persian",
  "first_year": 1206,
  "end_epoch_day": 54866,
  "rows": [[1206, -52150, 0], [1207, -51785, 0]]
}
```

Each row is `[persian_year, first_epoch_day, star_count]`. `star_count` preserves
0, 1 or 2 stars from the reviewed transcription: zero means a common year;
either nonzero value means a leap year, not two extra days. The first six months
have 31 days, the next five 30, and the final month 29 or 30 accordingly.

The current validator requires all **1206–1498** years, starting at epoch day
`-52150`. The terminal boundary is derived from the final year's leap marker;
it is not an additional published year row. Do not reconstruct these facts from
the inconsistent Gregorian column of the original publication; see the source
notes in the tools README.

## Umm al-Qura years

```json
{
  "schema": 1,
  "calendar": "islamic-umalqura",
  "first_year": 1300,
  "end_epoch_day": 74839,
  "rows": [[1300, -31826, 2730], [1301, -31472, 3412]]
}
```

Each row is `[hijri_year, first_epoch_day, month_mask]`. The twelve-bit mask is
stored as a JSON integer from 0 through 4095. **Bit 11 describes month 1; bit 0
month 12.** A set bit means 30 days; a clear bit means 29. Thus month `m`, numbered
1–12, has `29 + ((month_mask >> (12 - m)) & 1)` days.

The current contract requires **1300–1600 AH**, first epoch day `-31826`, terminal
boundary `74839`, and years of 354 or 355 days. The parsed data is derived from
ICU4X's precomputed source; it is not labelled a direct capture from KACST.

## Chinese and Dangi composite years

Both use rows of the form:

```json
[1900, -25537, [29, 30, 29, 29, 30, 29, 30, 30, 29, 30, 30, 29, 30], 8]
```

The fields are `[year, first_epoch_day, month_lengths, leap_month]`. `year` is
the related Gregorian year in which the lunisolar year starts, not a cyclical
era year or a Dangi epoch offset. `month_lengths` lists actual months in
chronological order. A common year has twelve entries and `leap_month: 0`.
A leap year has thirteen entries; `leap_month` is the **nominal month number**
1–12 repeated by the inserted month. In the example, the ninth array entry is
leap month 8 (`M08L`), following ordinary month 8; it is not leap month 9.
Month lengths must be 29 or 30; total year lengths are 353–355 or 383–385 days.

These objects also carry the following fields:

| Field | Meaning |
| --- | --- |
| `first_year`, `end_epoch_day`, `rows` | Complete stored years and terminal boundary; currently 1899–2100 |
| `primary_span` | Continuous date coverage of the primary publication composite |
| `required_span` | Gregorian date interval requiring primary published coverage |
| `approximate_months` | Exact months supplied by the boundary model outside required dates |
| `boundary_model` | Provenance of that model: `algorithm_version`, `role`, `source_revision` |
| `precomputed_source`, `precomputed_span` | Dangi-only identification and coverage of its separately sourced extension |

An `approximate_months` row is
`[year, nominal_month, is_leap, first_epoch_day, length]`, where `is_leap` is the
integer 0 or 1. For example, `[1899, 1, 0, -25892, 30]` describes an ordinary
30-day month. Each such month must lie entirely outside `required_span`.
The rows identify approximation explicitly; they must not be relabelled as
published months merely to satisfy coverage checks.

Current coverage is:

| Dataset | `primary_span` | `required_span` | Additional facts |
| --- | --- | --- | --- |
| Chinese | `[-25567, 47875]` | `[-25567, 47847]` (1900-01-01 through 2100-12-31) | Eleven approximate months complete lunisolar year 1899 before the mandatory interval |
| Dangi | `[-25892, 29597]` | `[-25567, 29585]` (1900-01-01 through 2050-12-31) | Precomputed extension `[29597, 47875]`; no approximate months remain |

Both stored tables end at epoch day `47875`. Dangi's primary KASI span and
required span stay unchanged when adding the precomputed extension: extending
the table does not turn ICU4X-derived facts into KASI publications. The generator
checks Dangi's saved rows against `icu4x-korea-data.json`, including overlaps.
`boundary_model` remains provenance even though its current Dangi approximation
list is empty. Its `source_revision` identifies the project commit containing
the approximation model used for boundary generation; it is not a PMO/KASI
publication revision or this JSON's hash. It neither selects runtime algorithms
nor acts as a generator hash gate. The pin and generated manifest retain this
provenance separately from the packed year records. The
[model source](../src/calendar/lunisolar.c) records its ICU provenance and license.

### Parsed Korean source facts

`icu4x-korea-data.json` has `schema: 1`, `calendar: "dangi"`, `first_year: 1912`,
`end_epoch_day: 48614`, and the same four-field year-row shape above. It retains
all **1912–2102** rows, including the two padding years beyond 2100. The validator
checks the complete source table, continuity, leap-month shape, terminal boundary
and a January/February start for every year. Only rows through 2100 are copied
into the composite pack. This JSON is the maintained source input; the original
Rust is needed only for an explicit reimport/update, not normal generation.

## Japanese eras

```json
{
  "schema": 1,
  "calendar": "japanese",
  "eras": [["meiji", "1873-01-01", 1867], ["taisho", "1912-07-30", 1911]]
}
```

Each row is `[era_code, first_gregorian_date, year_offset]`. Dates use canonical
`YYYY-MM-DD` spelling and strictly increase. Codes are unique ASCII identifiers
matching `[a-z][a-z0-9-]{0,30}`; `ce`, `bce`, `ad` and `bc` are reserved.
For a date in the era, `eraYear = gregorianYear - year_offset`.

The first row starts at **1873-01-01**, the Gregorian-adoption boundary. Its
lookup date is clamped, but its offset retains the original era's start year
minus one: Meiji therefore uses 1867, so 1873 is year 6. Each later offset must
be its actual start year minus one. Earlier dates use the engine's Gregorian-era
rules. The binary writer permits at most 1,024 era records; another modern era
can be added through data without a hardcoded count or C-name list.

## Provenance, generation and updates

`pin.json` records declared sources under `sources`, factual filenames and
accepted hashes under `normalized`, the reviewed spec revision, and boundary
policy. It is distinct from a facts file. Keep original publication coverage,
precomputed extensions, derived approximations and licenses distinguishable.
The generated manifest records actual input hashes, source metadata and QJCA
summary/hash; neither it nor a newly calculated pin proves semantic correctness.

Acquisition verifies accepted hashes. Generation accepts compatible custom JSON,
validates structure and calendar boundaries, and records the bytes it actually
consumes; it does not require them to match the pin's old hashes. Use a separate
candidate directory and follow the
[supplied-data procedure](../tools/calendar/README.md#supplied-data). Run the
focused validation with `python3 tests/test_calendar_generator.py`.

When extending published ranges, update the corresponding explicit coverage
checks and regression witnesses as well as the facts and provenance. Preserve
join checks against adjacent years and distinguish source corrections from
algorithm changes. Normal builds consume the prepared binary and need neither
a Rust toolchain nor the original publications.
