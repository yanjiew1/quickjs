#!/usr/bin/env python3
"""Host proposal for KASI /life/lunc/between captured month-start JSON.

No fetching occurs. The caller supplies an acquisition manifest containing
immutable paths, hashes and exact official request URLs for all 24 queries.
Normal/leap response arrays are separate, even when leap arrays are empty.
"""
import argparse
import calendar
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import re
import sys
from urllib.parse import parse_qs, urlsplit

from prepare_pmo_months import (
    EPOCH, InvalidSource, Month, complete_years, encode_binary, encode_c,
    require, sexagenary,
)

FIELDS = frozenset(("LUNC_LEAP_MM", "LUNC_PRCN", "LUNC_ILJIN", "SOLC_LEAP_YYYY", "LUNC_EN_DD", "SOLC_JD", "SOLC_MM", "SOLC_DD", "LUNC_WLGN", "LUNC_MM", "LUNC_DD", "JULIAN_LeapYear", "JULIAN_YYYY", "SOLC_YYYY", "LUNC_YYYY", "JULIAN_MM", "JULIAN_DD", "SOLC_WEEK"))
OPTIONAL_FIELDS = frozenset(("LUNC_AGE",))
KOREAN_WEEKDAYS = "월화수목금토일"
# Captured leap queries leave the auxiliary month label empty in every row.
# These identities can only enter through read_capture after byte hash proof.
ABSENT_WLGN_LEAP_CAPTURES = {1: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=01&dd=01&isLeap=1', 'sha256': '4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945'}, 2: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=02&dd=01&isLeap=1', 'sha256': '78a73ed6a0b0b8fe1eb0b34ca563cd712a56da5727a0992a34a6e6b3223f9780'}, 3: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=03&dd=01&isLeap=1', 'sha256': '0e09f234f85e0a55756ccab9952a262f6a19542faca6bad56b926cedcdf524dc'}, 4: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=04&dd=01&isLeap=1', 'sha256': '8a8d0080914fa3d7ae217c55b57397ce1d9a795ad0b2dfb5736eb49d7d87fe46'}, 5: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=05&dd=01&isLeap=1', 'sha256': '0df1e8353b71629544f7a0feee909b2c8961087d8cac332eee849db4263daca7'}, 6: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=06&dd=01&isLeap=1', 'sha256': 'd48bde48f859f5b431e19911b44309479aab40b271d6a2501b84a830dcaa33b1'}, 7: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=07&dd=01&isLeap=1', 'sha256': '832c2e889ba6f4e8724e08563f5f704e109fbd26f621774e59f2da9ba608ca9f'}, 8: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=08&dd=01&isLeap=1', 'sha256': '5ce537e2dcb7a7fdf543118d6fc3ca1abe4d958e83555f4625295ffe1d7ef826'}, 9: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=09&dd=01&isLeap=1', 'sha256': '2d3185c7afbccaea39efe1655909a5962724b689a189835033976c820fd199f1'}, 10: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=10&dd=01&isLeap=1', 'sha256': 'd488442a8130887e53b4e7d910f0fd912752f0a80caf2125af15d5f71af7c8cf'}, 11: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=11&dd=01&isLeap=1', 'sha256': '4d48afb4b405adbae979181ad0c1ebf15f6ecc1ca440ee9614fe75e8ea18c32a'}, 12: {'url': 'https://astro.kasi.re.kr/life/lunc/between?start_yyyy=1899&end_yyyy=2051&mm=12&dd=01&isLeap=1', 'sha256': '4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945'}}


def unique_object(pairs):
    obj = {}
    for key, value in pairs:
        require(key not in obj, f"duplicate JSON key {key}")
        obj[key] = value
    return obj


def load_json(raw):
    try:
        return json.loads(raw.decode("utf-8", errors="strict"), object_pairs_hook=unique_object)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise InvalidSource(f"invalid UTF-8 JSON: {error}") from error


def integer(record, key, width):
    value = record[key]
    require(isinstance(value, str) and re.fullmatch(r"[0-9]{" + str(width) + r"}", value), f"invalid decimal field {key}")
    return int(value)


def labelled_sexagenary(text, expected=None):
    require(isinstance(text, str), "sexagenary label must be string")
    match = re.fullmatch(r"[가-힣]{2}\(([^()]{2})\)", text)
    require(match and match.group(1) in {sexagenary(n) for n in range(60)}, "malformed Korean sexagenary label")
    require(expected is None or match.group(1) == expected, "incorrect Korean sexagenary label")


def parse_record(row, month, leap, request_first, request_last, *, capture_identity=None):
    require(isinstance(row, dict) and FIELDS <= set(row) <= FIELDS | OPTIONAL_FIELDS, "unexpected KASI object schema")
    require(all(isinstance(row[key], str) for key in FIELDS), "KASI calendar field values must be strings")
    if "LUNC_AGE" in row:
        age = row["LUNC_AGE"]
        require(type(age) in (int, float) and 0 <= age < 30 and math.isfinite(age), "KASI LUNC_AGE must be finite numeric age in [0, 30)")
    year = integer(row, "LUNC_YYYY", 4)
    require(request_first <= year <= request_last, "KASI lunar year outside requested range")
    require(integer(row, "LUNC_MM", 2) == month and row["LUNC_DD"] == "01", "KASI row is not requested lunar month start")
    require(row["LUNC_LEAP_MM"] == ("윤" if leap else "평"), "KASI leap label/request disagreement")
    solar_year = integer(row, "SOLC_YYYY", 4)
    solar_month = integer(row, "SOLC_MM", 2)
    solar_day = integer(row, "SOLC_DD", 2)
    try:
        date = dt.date(solar_year, solar_month, solar_day)
    except ValueError as error:
        raise InvalidSource("invalid Gregorian KASI date") from error
    require(solar_year in (year, year + 1), "Gregorian/lunar arithmetic year mismatch")
    if month == 1 and not leap:
        require(solar_year == year and solar_month in (1, 2), "KASI M01 year mismatch")
    epoch_day = (date - EPOCH).days
    jdn = epoch_day + 2440588
    require(integer(row, "SOLC_JD", 7) == jdn, "Gregorian date/JDN disagreement")
    require(row["SOLC_WEEK"] == KOREAN_WEEKDAYS[date.weekday()], "KASI weekday disagreement")
    require(row["SOLC_LEAP_YYYY"] == ("윤" if calendar.isleap(solar_year) else "평"), "Gregorian leap-year disagreement")
    labelled_sexagenary(row["LUNC_PRCN"], sexagenary(year - 4))
    labelled_sexagenary(row["LUNC_ILJIN"], sexagenary(jdn + 49))
    if row["LUNC_WLGN"] == "":
        require(leap == 1 and (request_first, request_last) == (1899, 2051) and
                month in ABSENT_WLGN_LEAP_CAPTURES and
                capture_identity == ABSENT_WLGN_LEAP_CAPTURES[month],
                "unapproved absent KASI auxiliary leap-month label")
    else:
        labelled_sexagenary(row["LUNC_WLGN"])
    julian_year = integer(row, "JULIAN_YYYY", 4)
    julian_month = integer(row, "JULIAN_MM", 2)
    julian_day = integer(row, "JULIAN_DD", 2)
    require(1 <= julian_month <= 12, "invalid Julian month")
    julian_lengths = [31, 29 if julian_year % 4 == 0 else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    require(1 <= julian_day <= julian_lengths[julian_month - 1], "invalid Julian day")
    a = (14 - julian_month) // 12
    y = julian_year + 4800 - a
    m = julian_month + 12 * a - 3
    require(julian_day + (153 * m + 2) // 5 + 365 * y + y // 4 - 32083 == jdn, "Julian/Gregorian date disagreement")
    require(row["JULIAN_LeapYear"] == ("윤" if julian_year % 4 == 0 else "평"), "Julian leap-year disagreement")
    length = integer(row, "LUNC_EN_DD", 2)
    require(length in (29, 30), "KASI month length must be 29 or 30")
    return Month(epoch_day, year, month, bool(leap), length)


def read_capture(capture, directory):
    require(isinstance(capture, dict) and set(capture) == {"path", "sha256", "url"}, "capture manifest schema mismatch")
    require(all(isinstance(v, str) for v in capture.values()), "capture manifest values must be strings")
    require(re.fullmatch(r"[a-f0-9]{64}", capture["sha256"]), "capture SHA256 malformed")
    url = urlsplit(capture["url"])
    require(url.scheme == "https" and url.netloc == "astro.kasi.re.kr" and url.path == "/life/lunc/between" and not url.fragment, "capture URL is not KASI endpoint")
    query = parse_qs(url.query, strict_parsing=True)
    require(set(query) == {"start_yyyy", "end_yyyy", "mm", "dd", "isLeap"} and all(len(v) == 1 for v in query.values()), "KASI query schema mismatch")
    require(query["dd"] == ["01"] and query["isLeap"][0] in ("0", "1") and re.fullmatch(r"(?:0[1-9]|1[0-2])", query["mm"][0]), "KASI query is not month-start request")
    require(re.fullmatch(r"[0-9]{4}", query["start_yyyy"][0]) and re.fullmatch(r"[0-9]{4}", query["end_yyyy"][0]), "KASI query year malformed")
    first, last = int(query["start_yyyy"][0]), int(query["end_yyyy"][0])
    require(first <= last, "reversed KASI query year range")
    month, leap = int(query["mm"][0]), int(query["isLeap"][0])
    path = Path(capture["path"])
    if not path.is_absolute():
        path = directory / path
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == capture["sha256"], "KASI capture hash mismatch")
    value = load_json(raw)
    require(isinstance(value, list), "KASI capture must be JSON array")
    records = [parse_record(row, month, leap, first, last,
                            capture_identity={"url": capture["url"], "sha256": capture["sha256"]})
               for row in value]
    require([r.lunar_year for r in records] == sorted({r.lunar_year for r in records}), "KASI array out of order or duplicate lunar year")
    return (month, leap), first, last, records


def assemble(manifest, directory):
    require(isinstance(manifest, dict) and set(manifest) == {"calendar", "first_lunar_year", "last_lunar_year", "captures"}, "KASI acquisition manifest schema mismatch")
    require(manifest["calendar"] == "dangi", "KASI manifest must identify Dangi")
    first, last = manifest["first_lunar_year"], manifest["last_lunar_year"]
    require(type(first) is int and type(last) is int and 1 <= first <= last <= 9998, "KASI target year bounds invalid")
    require(isinstance(manifest["captures"], list) and len(manifest["captures"]) == 24, "require all 12 normal and 12 leap captures")
    captures, records, capture_coverage = {}, [], []
    for capture in manifest["captures"]:
        key, begin, end, parsed = read_capture(capture, directory)
        require(key not in captures, "duplicate KASI month/leap capture")
        require(begin <= first and end >= last + 1, "KASI request must include next-year boundary")
        captures[key] = parsed
        capture_coverage.append({
            "month": key[0], "is_leap": bool(key[1]),
            "requested_first_lunar_year": begin, "requested_last_lunar_year": end,
            "row_count": len(parsed),
            "first_observed_lunar_year": parsed[0].lunar_year if parsed else None,
            "last_observed_lunar_year": parsed[-1].lunar_year if parsed else None,
        })
        records.extend(r for r in parsed if first <= r.lunar_year <= last or (r.lunar_year == last + 1 and r.month == 1 and not r.is_leap))
    for month in range(1, 13):
        require({r.lunar_year for r in captures[(month, 0)]} >= set(range(first, last + 1)), f"normal lunar month {month} incomplete across target years")
    require(any(r.lunar_year == last + 1 and r.month == 1 and not r.is_leap for r in records), "next-year normal M01 sentinel missing")
    records.sort(key=lambda r: r.epoch_day)
    require(len({(r.lunar_year, r.month, r.is_leap) for r in records}) == len(records), "duplicate KASI month identity")
    for year in range(first, last + 1):
        group = [r for r in records if r.lunar_year == year]
        require([r.month for r in group if not r.is_leap] == list(range(1, 13)), f"normal months reordered in KASI year {year}")
        require(sum(r.is_leap for r in group) <= 1, f"multiple leap months in KASI year {year}")
    for previous, current in zip(records, records[1:]):
        require(current.epoch_day - previous.epoch_day == previous.length, "KASI published month length/successor disagreement")
        if current.is_leap:
            require(not previous.is_leap and current.lunar_year == previous.lunar_year and current.month == previous.month, "KASI leap month does not repeat preceding normal month")
        elif previous.month == 12:
            require(current.month == 1 and current.lunar_year == previous.lunar_year + 1, "KASI lunar year rollover malformed")
        else:
            require(current.month == previous.month + 1 and current.lunar_year == previous.lunar_year, "KASI normal month progression malformed")
    sentinel = records.pop()
    require(sentinel.lunar_year == last + 1 and sentinel.month == 1 and not sentinel.is_leap, "KASI terminal sentinel malformed")
    bounds = {
        "observed_first_day": records[0].epoch_day,
        "observed_end_day": sentinel.epoch_day,
        "proven_first_day": records[0].epoch_day,
        "proven_end_day": sentinel.epoch_day,
        "capture_coverage": sorted(capture_coverage, key=lambda row: (row["month"], row["is_leap"])),
        "following_lunar_year": {
            "lunar_year": last + 1,
            "observed_normal_months": [m for m in range(1, 13) if any(r.lunar_year == last + 1 for r in captures[(m, 0)])],
            "observed_leap_months": [m for m in range(1, 13) if any(r.lunar_year == last + 1 for r in captures[(m, 1)])],
            "missing_normal_months": [m for m in range(1, 13) if not any(r.lunar_year == last + 1 for r in captures[(m, 0)])],
            "complete_year_certified": False,
            "role": "normal M01 is target end sentinel; other following-year captures are retained source evidence outside this table",
        },
    }
    # All target years are complete. The last boundary is retained explicitly
    # rather than manufacturing a length for the next-year sentinel month.
    return records, bounds, sentinel.epoch_day


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    manifest_raw = args.manifest.read_bytes()
    manifest = load_json(manifest_raw)
    records, bounds, sentinel = assemble(manifest, args.manifest.parent)
    wire = encode_binary(records, bounds)
    c_source = encode_c(records, bounds, authority="kasi").encode("utf-8")
    audit = {
        "status": "validated KASI month starts and published lengths; no runtime activation",
        "calendar": "dangi",
        "arithmetic_year": "KASI LUNC_YYYY; Gregorian lunar-M01 year; no +2333",
        "auxiliary_label_schema": {"leap_capture_pins": ABSENT_WLGN_LEAP_CAPTURES,
            "meaning": "empty WLGN in pinned leap queries is absent auxiliary metadata; primary fields remain strict"},
        "manifest_sha256": hashlib.sha256(manifest_raw).hexdigest(),
        "captures": manifest["captures"],
        "month_count": len(records),
        "bounds": bounds,
        "complete_lunar_years": list(range(manifest["first_lunar_year"], manifest["last_lunar_year"] + 1)),
        "next_year_m01_sentinel": sentinel,
        "binary_sha256": hashlib.sha256(wire).hexdigest(),
        "c_source_sha256": hashlib.sha256(c_source).hexdigest(),
    }
    args.output_dir.mkdir(parents=True, exist_ok=False)
    (args.output_dir / "kasi-months.bin").write_bytes(wire)
    (args.output_dir / "kasi-months.inc").write_bytes(c_source)
    (args.output_dir / "provenance.json").write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except (InvalidSource, OSError, UnicodeError, ValueError, KeyError) as error:
        print(f"FAIL CLOSED: {error}", file=sys.stderr)
        sys.exit(1)
