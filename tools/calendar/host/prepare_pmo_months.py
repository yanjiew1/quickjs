#!/usr/bin/env python3
"""Host source proposal. Never fetches or invokes an extraction program.

Input is the pinned PMO 1900–2025 PDF and its pdftotext -layout snapshot.
The 2026 grid is a different grammar and deliberately unsupported here.
"""
import argparse
import calendar
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
from dataclasses import dataclass

PDF_SHA256 = "dc7ed4cb5dae55c1de5a0bdc97f09256a7d6b6d7fe3c553a542e9f9f7758aba4"
TEXT_SHA256 = "45ffd7baf6838126c7f0c7a6e123df156469e1f75d352caca740df36f3e5bd6d"
PDF_URL = "https://pmo.cas.cn/xwdt2019/kpdt2019/202203/P020250414456381274062.pdf"
EPOCH = dt.date(1970, 1, 1)
STEMS = "甲乙丙丁戊己庚辛壬癸"
BRANCHES = "子丑寅卯辰巳午未申酉戌亥"
WEEKDAYS = "一二三四五六日"
MONTH_NAMES = ("正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "十一", "十二")
DAY_NAMES = tuple("初" + x for x in "一二三四五六七八九十") + tuple("十" + x for x in "一二三四五六七八九") + ("二十",) + tuple("廿" + x for x in "一二三四五六七八九") + ("三十",)
MONTH_BY_NAME = {name + "月": i + 1 for i, name in enumerate(MONTH_NAMES)}
DAY_BY_NAME = {name: i + 1 for i, name in enumerate(DAY_NAMES)}
CELL = re.compile(r"\s*(?:(闰?(?:十二|十一|正|十|二|三|四|五|六|七|八|九)月)\s*)?(" + "|".join(DAY_NAMES) + r")\s*([" + STEMS + r"][" + BRANCHES + r"])\s*([一二三四五六日])")
YEAR_NOTE = re.compile(r"([" + STEMS + r"][" + BRANCHES + r"]年(?:\s*~\s*[" + STEMS + r"][" + BRANCHES + r"]年)?)")
# These three February headings repeat the same label around a printed tilde.
# Daily rows place normal M01 on February 1; the printed text stays unchanged.
# Exceptions are restricted to the pinned source's exact date/label pairs.
PRINTED_YEAR_NOTE_DUPLICATES = {
    (1919, 2): ("己未年", "己未年"),
    (2003, 2): ("癸未年", "癸未年"),
    (2022, 2): ("壬寅年", "壬寅年"),
}
TERMS = ("小寒", "大寒", "立春", "雨水", "惊蛰", "春分", "清明", "谷雨", "立夏", "小满", "芒种", "夏至", "小暑", "大暑", "立秋", "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪", "冬至")
TERM_CELL = re.compile(r"\s*(" + "|".join(TERMS) + r")(1\)?)?\s+([0-9]+)\s+([0-9]+)\s+([0-9]+)\s+([0-9]+)")


class InvalidSource(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InvalidSource(message)


def sexagenary(index):
    return STEMS[index % 10] + BRANCHES[index % 12]


def year_name(year):
    return sexagenary(year - 4) + "年"


@dataclass(frozen=True)
class Daily:
    date: dt.date
    month: int
    leap: bool
    day: int
    year_note: tuple


@dataclass(frozen=True)
class Month:
    epoch_day: int
    lunar_year: int
    month: int
    is_leap: bool
    length: int


def parse_page(page, expected_year, expected_months, page_number):
    """Grammar: one four-month page, 31 ordered daily rows, solar-term footer.

    Empty Gregorian cells are inferred only from Gregorian month bounds. This
    avoids treating text character offsets as PDF visual column coordinates.
    """
    lines = [line for line in page.splitlines() if line.strip()]
    prefix = f"page {page_number}: "
    require(lines and re.match(r"\s*公元\s+" + str(expected_year) + r"\s+年", lines[0]), prefix + "year header mismatch")
    require(len(lines) > 8, prefix + "truncated page")
    months = tuple(int(x) for x in re.findall(r"([0-9]+)\s+月", lines[1]))
    require(months == expected_months, prefix + "month header mismatch")
    require(re.sub(r"\s+", "", lines[1]) == "".join(f"{x}月" for x in months), prefix + "unexpected month header text")
    require(lines[2].strip() == "公历" and lines[3].strip() == "日期", prefix + "Gregorian column heading missing")
    require(re.sub(r"\s+", "", lines[4]) == "农历日期星期" * 4, prefix + "daily column headings missing")
    require(lines[5].lstrip().startswith("日"), prefix + "lunar year notes missing")
    note_text = lines[5].lstrip()[1:]
    note_matches = list(YEAR_NOTE.finditer(note_text))
    require(len(note_matches) == 4, prefix + "expected four lunar year notes")
    require(not YEAR_NOTE.sub("", note_text).strip(), prefix + "unknown lunar year note text")
    notes = [tuple(re.split(r"\s*~\s*", m.group(1))) for m in note_matches]
    result = []
    active_months = [None] * 4
    for row_day in range(1, 32):
        index = 5 + row_day
        require(index < len(lines), prefix + "missing daily rows")
        row = lines[index]
        row_match = re.match(r"\s*([0-9]+)\s+", row)
        require(row_match and int(row_match.group(1)) == row_day, prefix + f"row {row_day} missing or out of order")
        rest = row[row_match.end():]
        active_columns = [i for i, month in enumerate(months) if row_day <= calendar.monthrange(expected_year, month)[1]]
        position = 0
        for column in active_columns:
            match = CELL.match(rest, position)
            require(match is not None, prefix + f"row {row_day} malformed cell for month {months[column]}")
            position = match.end()
            label, lunar_day, day_stem, weekday = match.groups()
            day = DAY_BY_NAME[lunar_day]
            if label is not None:
                leap = label.startswith("闰")
                month = MONTH_BY_NAME[label.removeprefix("闰")]
                if row_day != 1:
                    require(day == 1, prefix + "month label repeated away from Gregorian/lunar first day")
                active_months[column] = (month, leap)
            else:
                require(row_day != 1 and active_months[column] is not None, prefix + "initial Gregorian cell needs lunar month label")
                require(day != 1, prefix + "lunar month reset missing month label")
            date = dt.date(expected_year, months[column], row_day)
            require(weekday == WEEKDAYS[date.weekday()], prefix + f"weekday mismatch at {date}")
            epoch_day = (date - EPOCH).days
            require(day_stem == sexagenary(epoch_day + 2440588 + 49), prefix + f"daily sexagenary mismatch at {date}")
            month, leap = active_months[column]
            result.append(Daily(date, month, leap, day, notes[column]))
        require(not rest[position:].strip(), prefix + f"extra or invalid Gregorian cell at row {row_day}")
    require(len(lines) > 38 and re.sub(r"\s+", "", lines[37]) == "节气", prefix + "solar-term footer missing")
    # Footer is not a daily table and cannot create dates. Require its fixed
    # header/row structure so daily data cannot silently spill into the footer.
    require(re.sub(r"\s+", "", lines[38]) == "月日时分" * 4, prefix + "solar-term column headings malformed")
    footer_rows, notes, detached = [], [], []
    for line in lines[39:]:
        if line.strip() in (")", "1)"):
            detached.append(line.strip())
        elif line.strip().startswith("1)旧历书中"):
            notes.append(line.strip())
        else:
            footer_rows.append(line)
    require(len(footer_rows) == 2 and len(notes) <= 1 and len(detached) <= 1, prefix + "unexpected footer structure")
    annotations = 0
    for term_row, footer in enumerate(footer_rows):
        position = 0
        for month in months:
            match = TERM_CELL.match(footer, position)
            require(match is not None, prefix + "malformed solar-term cell")
            name, annotation, solar_month, solar_day, hour, minute = match.groups()
            require(name == TERMS[(month - 1) * 2 + term_row], prefix + "solar terms reordered")
            require(int(solar_month) == month and 1 <= int(solar_day) <= calendar.monthrange(expected_year, month)[1], prefix + "invalid solar-term date")
            require(0 <= int(hour) <= 23 and 0 <= int(minute) <= 59, prefix + "invalid solar-term time")
            annotations += bool(annotation)
            position = match.end()
        require(not footer[position:].strip(), prefix + "unrecognized solar-term text")
    if notes:
        require(re.fullmatch(r"1\)旧历书中(?:" + "|".join(TERMS) + r")时间为\s+[0-9]+\s+月\s+[0-9]+\s+日。", notes[0]), prefix + "unknown historical note")
        require((annotations == 1 and detached in ([], [")"])) or (annotations == 0 and detached == ["1)"]), prefix + "historical note marker missing or duplicated")
    else:
        require(annotations == 0 and not detached, prefix + "unexplained historical note marker")
    return result


def parse_document(text, first_year=1900, last_year=2025, require_intro=True):
    pages = text.split("\f")
    if pages[-1].strip() == "":
        pages.pop()
    if require_intro:
        require(len(pages) >= 2 and "1900—2025 年日历" in pages[0] and "编制说明" in pages[1], "PMO cover or introduction missing")
        require("1900-1911" in pages[1] and "1912-" in pages[1] and "1948" in pages[1] and "不予" in pages[1], "normative historical calendar statement missing")
        pages = pages[2:]
    expected = [(year, tuple(range(month, month + 4))) for year in range(first_year, last_year + 1) for month in (1, 5, 9)]
    require(len(pages) == len(expected), "daily page count mismatch")
    days = []
    for number, (page, (year, months)) in enumerate(zip(pages, expected), 1):
        days.extend(parse_page(page, year, months, number))
    days.sort(key=lambda row: row.date)
    require(days[0].date == dt.date(first_year, 1, 1) and days[-1].date == dt.date(last_year, 12, 31), "observed Gregorian range mismatch")
    require(len(days) == (days[-1].date - days[0].date).days + 1, "daily coverage length mismatch")
    return days


def next_month(previous, current, year):
    require(current.day == 1 and previous.day in (29, 30), f"illegal lunar reset at {current.date}")
    if current.leap:
        require(not previous.leap and current.month == previous.month, f"illegal leap month at {current.date}")
    elif previous.month == 12:
        require(current.month == 1, f"illegal lunar year rollover at {current.date}")
        year += 1
    else:
        require(current.month == previous.month + 1, f"lunar month skipped or duplicated at {current.date}")
    if current.month == 1 and not current.leap:
        require(year == current.date.year and current.date.month in (1, 2), f"lunar M01 year mismatch at {current.date}")
    return year


def validate_days(days):
    require(bool(days), "empty daily input")
    first = days[0]
    require(first.month in (11, 12) and not first.leap, "January boundary must identify previous lunar year")
    year = first.date.year - 1
    records = []
    started = first if first.day == 1 else None
    started_year = year
    leap_years = set()
    month_note_observed = {}
    month_first_day = {}
    for i, current in enumerate(days):
        if i:
            previous = days[i - 1]
            require(current.date == previous.date + dt.timedelta(days=1), f"duplicate or missing Gregorian day at {current.date}")
            same_month = (current.month, current.leap) == (previous.month, previous.leap)
            if same_month:
                require(current.day == previous.day + 1 and current.day <= 30, f"lunar day discontinuity at {current.date}")
            else:
                new_year = next_month(previous, current, year)
                if started is not None:
                    length = (current.date - started.date).days
                    require(length in (29, 30) and length == previous.day, f"invalid proven month length at {current.date}")
                    records.append(Month((started.date - EPOCH).days, started_year, started.month, started.leap, length))
                year = new_year
                started, started_year = current, year
                if current.leap:
                    require(year not in leap_years, f"multiple leap months in lunar year {year}")
                    leap_years.add(year)
        require(year_name(year) in current.year_note, f"lunar year heading mismatch at {current.date}")
        key = (current.date.year, current.date.month)
        month_first_day.setdefault(key, current)
        observed, declared = month_note_observed.setdefault(key, ([], current.year_note))
        require(declared == current.year_note, f"inconsistent lunar year note for {key}")
        if not observed or observed[-1] != year_name(year):
            observed.append(year_name(year))
    for key, (observed, declared) in month_note_observed.items():
        if tuple(observed) != declared:
            first_day = month_first_day[key]
            require(
                PRINTED_YEAR_NOTE_DUPLICATES.get(key) == declared
                and tuple(observed) == declared[:1]
                and first_day.date.day == 1 and first_day.month == 1
                and first_day.day == 1 and not first_day.leap,
                f"unobserved or reversed lunar year note for {key}",
            )
    require(bool(records), "no complete lunar months proven")
    return records, {
        "observed_first_day": (first.date - EPOCH).days,
        "observed_end_day": (days[-1].date - EPOCH).days + 1,
        "proven_first_day": records[0].epoch_day,
        "proven_end_day": records[-1].epoch_day + records[-1].length,
        "incomplete_prefix": first.day != 1,
        "incomplete_terminal_month": {"lunar_year": year, "month": days[-1].month, "is_leap": days[-1].leap, "last_observed_day": days[-1].day},
    }


def complete_years(records):
    result = []
    groups = {}
    for record in records:
        groups.setdefault(record.lunar_year, []).append(record)
    for year, group in groups.items():
        normal = [r.month for r in group if not r.is_leap]
        if normal != list(range(1, 13)):
            continue
        require(sum(r.is_leap for r in group) <= 1, f"multiple leap months in complete year {year}")
        require(all(b.epoch_day == a.epoch_day + a.length for a, b in zip(group, group[1:])), f"discontinuous complete year {year}")
        next_group = groups.get(year + 1, [])
        if next_group and next_group[0].month == 1 and not next_group[0].is_leap:
            require(group[-1].epoch_day + group[-1].length == next_group[0].epoch_day, f"year end mismatch {year}")
            result.append(year)
    return result


def encode_binary(records, bounds):
    # Explicit little endian wire format; C struct padding is never serialized.
    header = struct.pack("<8sIIiiii", b"QJSCALM1", 1, len(records), bounds["observed_first_day"], bounds["observed_end_day"], bounds["proven_first_day"], bounds["proven_end_day"])
    return header + b"".join(struct.pack("<iiBBBB", r.epoch_day, r.lunar_year, r.month, int(r.is_leap), r.length, 0) for r in records)


def encode_c(records, bounds, authority="pmo"):
    require(authority in ("pmo", "kasi"), "unknown C table authority")
    rows = ["/* Generated only from validated published calendar observations. */", '#include "calendar_month_record.h"', f"static const QJSPublishedCalendarMonth qjs_{authority}_months[] = {{"]
    rows.extend(f"    {{ {r.epoch_day}, {r.lunar_year}, {r.month}, {int(r.is_leap)}, {r.length} }}," for r in records)
    rows += ["};", f"static const QJSPublishedCalendarTable qjs_{authority}_table = {{", f"    qjs_{authority}_months, sizeof(qjs_{authority}_months) / sizeof(qjs_{authority}_months[0]),", f'    {bounds["observed_first_day"]}, {bounds["observed_end_day"]},', f'    {bounds["proven_first_day"]}, {bounds["proven_end_day"]}', "};", ""]
    return "\n".join(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pdf", type=Path, required=True)
    parser.add_argument("--text", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    pdf = args.pdf.read_bytes()
    raw = args.text.read_bytes()
    require(pdf.startswith(b"%PDF-"), "source is not PDF")
    require(hashlib.sha256(pdf).hexdigest() == PDF_SHA256, "pinned PDF hash mismatch")
    require(hashlib.sha256(raw).hexdigest() == TEXT_SHA256, "pinned pdftotext snapshot hash mismatch")
    days = parse_document(raw.decode("utf-8", errors="strict"))
    records, bounds = validate_days(days)
    wire = encode_binary(records, bounds)
    c_source = encode_c(records, bounds).encode("utf-8")
    audit = {
        "status": "validated published observations; incomplete boundary years remain unavailable",
        "source_url": PDF_URL,
        "source_sha256": PDF_SHA256,
        "text_sha256": TEXT_SHA256,
        "extraction_command": ["pdftotext", "-layout", "SOURCE.pdf", "SOURCE.txt"],
        "calendar": "chinese",
        "historical_rule": "PMO published historical calendar is normative; never corrected to modern astronomical approximation",
        "printed_year_note_duplicates": [
            {"gregorian_year": year, "gregorian_month": month,
             "printed_labels": list(labels), "observed_label": labels[0],
             "normal_m01_date": f"{year}-02-01"}
            for (year, month), labels in PRINTED_YEAR_NOTE_DUPLICATES.items()
        ],
        "daily_count": len(days),
        "month_count": len(records),
        "bounds": bounds,
        "complete_lunar_years": complete_years(records),
        "binary_sha256": hashlib.sha256(wire).hexdigest(),
        "c_source_sha256": hashlib.sha256(c_source).hexdigest(),
        "unresolved": ["enclosing lunar year 1899 before 1900-01-01", "terminal lunar year 2025 and PMO 2026 alternate layout", "PMO published years 2027-2100", "no runtime activation or support claim"],
    }
    # Create a fresh artifact directory only after all parsing/validation passes.
    # Existing output is rejected, so a failed attempt cannot replace evidence.
    args.output_dir.mkdir(parents=True, exist_ok=False)
    (args.output_dir / "pmo-months.bin").write_bytes(wire)
    (args.output_dir / "pmo-months.inc").write_bytes(c_source)
    (args.output_dir / "provenance.json").write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except (InvalidSource, OSError, UnicodeError) as error:
        print(f"FAIL CLOSED: {error}", file=sys.stderr)
        sys.exit(1)
