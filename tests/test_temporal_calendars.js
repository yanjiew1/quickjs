/* Native calendar behavior without Intl or external calendar libraries. */
import { assert, assertThrows } from "./assert.js";

const calendars = ["iso8601", "buddhist", "chinese", "coptic", "dangi", "ethioaa",
    "ethiopic", "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla",
    "islamic-umalqura", "japanese", "persian", "roc"];
const Date = Temporal.PlainDate;

function test_profiles() {
    assert(new Date(2024, 2, 10).calendarId, "iso8601");
    if (scriptArgs[1] === "iso") {
        for (const id of calendars.slice(1))
            assertThrows(RangeError, () => new Date(2024, 2, 10, id));
        return false;
    }
    for (const id of calendars) {
        const date = new Date(2024, 2, 10, id);
        const copy = Date.from({calendar: id, year: date.year,
            monthCode: date.monthCode, day: date.day});
        assert(date.equals(copy), true, id);
        assert(date.toPlainYearMonth().toPlainDate({day: date.day}).equals(date), true, id);
        assert(date.toPlainMonthDay().toPlainDate({year: date.year}).equals(date), true, id);
        assert(date.daysInWeek, 7);
        if (id !== "iso8601") {
            assert(date.weekOfYear, undefined);
            assert(date.yearOfWeek, undefined);
        }
        const end = date.add({months: 3, days: 2});
        assert(date.add(date.until(end, {largestUnit: "month"})).equals(end), true, id);
    }
    assert(new Date(2024, 2, 10, "HeBrEw").calendarId, "hebrew");
    assert(new Date(2024, 2, 10, "ethiopic-amete-alem").calendarId, "ethioaa");
    assert(new Date(2024, 2, 10, "islamicc").calendarId, "islamic-civil");
    for (const id of ["islamic", "islamic-rgsa", "gregorian"])
        assertThrows(RangeError, () => new Date(2024, 2, 10, id));
    return true;
}

function test_publications_and_eras() {
    const persian = Date.from({calendar: "persian", year: 1404, month: 1, day: 1});
    assert(persian.withCalendar("iso8601").toString(), "2025-03-21");
    for (const calendar of ["chinese", "dangi"]) {
        const date = new Date(2024, 2, 10, calendar);
        assert(date.year, 2024);
        assert(date.monthCode, "M01");
        assert(date.day, 1);
        assert(date.era, undefined);
        assert(date.eraYear, undefined);
    }
    const umm = Date.from({calendar: "islamic-umalqura", year: 1446, month: 1, day: 1});
    const civil = Date.from({calendar: "islamic-civil", year: 1446, month: 1, day: 1});
    assert(umm.withCalendar("iso8601").toString(), "2024-07-07");
    assert(civil.withCalendar("iso8601").toString(), "2024-07-08");
    const heisei = Date.from({calendar: "japanese", era: "heisei", eraYear: 32,
        month: 1, day: 1});
    assert(heisei.year, 2020);
    assert(heisei.era, "reiwa");
    assert(heisei.eraYear, 2);
    const zero = Date.from({calendar: "gregory", era: "ad", eraYear: 0, month: 1, day: 1});
    assert(zero.year, 0);
    assert(zero.era, "bce");
    assert(zero.eraYear, 1);
}

function test_leap_months_and_reference_dates() {
    const leap = Date.from({calendar: "hebrew", year: 5784, monthCode: "M05L", day: 30});
    assert(leap.month, 6);
    const next = leap.add({years: 1});
    assert(next.monthCode, "M06");
    assert(next.day, 29);
    assertThrows(RangeError, () => leap.add({years: 1}, {overflow: "reject"}));
    const md = (calendar, monthCode, day) => Temporal.PlainMonthDay.from({calendar, monthCode, day});
    assert(md("chinese", "M01", 30).toString(), "1970-03-07[u-ca=chinese]");
    assert(md("hebrew", "M04", 26).toString(), "1972-12-31[u-ca=hebrew]");
    assert(md("chinese", "M11L", 10).toString().slice(0, 4), "2033");
    assert(md("chinese", "M11L", 11).toString().slice(0, 4), "2034");
    assertThrows(RangeError, () => Temporal.PlainMonthDay.from(
        {calendar: "chinese", monthCode: "M11L", day: 30}, {overflow: "reject"}));
    assertThrows(TypeError, () => Temporal.PlainMonthDay.from(
        {calendar: "hebrew", month: 1, monthCode: "M13", day: 1}));
    for (const [year, month, day] of [[275760, 12, 31], [-271821, 4, 18]])
        assertThrows(RangeError, () => Temporal.PlainMonthDay.from(
            {calendar: "gregory", year, month, day}));
}

function test_field_order() {
    const log = [];
    const values = {calendar: "hebrew", day: 1, era: "am", eraYear: 5784,
        month: 6, monthCode: "M05L", year: 5784};
    const fields = {};
    for (const key of Object.keys(values))
        Object.defineProperty(fields, key, {get() { log.push(key); return values[key]; }});
    Date.from(fields, {get overflow() { log.push("overflow"); return "reject"; }});
    assert(log.join(), "calendar,day,era,eraYear,month,monthCode,year,overflow");
}

if (test_profiles()) {
    test_publications_and_eras();
    test_leap_months_and_reference_dates();
    test_field_order();
}
