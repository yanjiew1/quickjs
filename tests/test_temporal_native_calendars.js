/* Native calendar frontend regressions. Requires Temporal, including builds
 * with Intl disabled. Every advertised calendar must execute these operations.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, project LICENSE. */
function same(actual, expected, label) {
    if (!Object.is(actual, expected))
        throw Error(label + ": " + String(actual) + " != " + String(expected));
}
function throws(type, callback) {
    try { callback(); } catch (error) {
        if (error instanceof type) return;
        throw error;
    }
    throw Error("expected " + type.name);
}
const calendars = ["iso8601", "buddhist", "chinese", "coptic", "dangi", "ethioaa",
    "ethiopic", "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla",
    "islamic-umalqura", "japanese", "persian", "roc"];
for (const calendar of calendars) {
    const date = new Temporal.PlainDate(2024, 2, 29, calendar);
    const fields = { calendar, year: date.year, monthCode: date.monthCode, day: date.day };
    same(Temporal.PlainDate.from(fields, { overflow: "reject" }).equals(date), true,
         calendar + " construction");
    same(date.dayOfWeek, 4, calendar + " weekday");
    same(date.daysInWeek, 7, calendar + " week length");
    same(date.daysInMonth >= date.day, true, calendar + " day limit");
    same(date.daysInYear >= date.dayOfYear, true, calendar + " year limit");
    same(date.monthsInYear >= date.month, true, calendar + " ordinal limit");
    same(date.weekOfYear === undefined, calendar !== "iso8601", calendar + " week");
    same(date.yearOfWeek === undefined, calendar !== "iso8601", calendar + " week year");
    const next = date.add({ months: 1 });
    same(date.until(next, { largestUnit: "month" }).months, date.day <= next.daysInMonth ? 1 : 0,
         calendar + " month difference preserves the original day");
    same(date.add({ weeks: 2, days: 1 }).since(date, { largestUnit: "day" }).days, 15,
         calendar + " weeks and days");
    const ym = Temporal.PlainYearMonth.from({ calendar, year: date.year,
                                            monthCode: date.monthCode });
    same(ym.monthCode, date.monthCode, calendar + " year-month");
    const md = Temporal.PlainMonthDay.from(fields);
    same(md.monthCode, date.monthCode, calendar + " month-day");
    same(md.day, date.day, calendar + " reference day");
    throws(TypeError, () => Temporal.PlainDate.from({ calendar, monthCode: "M01", day: 1 }));
    throws(RangeError, () => Temporal.PlainDate.from({ ...fields, monthCode: "M00" }));
    throws(RangeError, () => Temporal.PlainDate.from({ ...fields, month: date.month + 1 }));
    if (calendar !== "iso8601") {
        throws(TypeError, () => Temporal.PlainMonthDay.from({ calendar, month: 1, day: 1 }));
        for (const year of [-999999, 999999])
            throws(RangeError, () => Temporal.PlainMonthDay.from({ calendar, year,
                                                               monthCode: "M01", day: 1 }));
    }
}
for (const [alias, canonical] of [["ETHIOPIC-AMETE-ALEM", "ethioaa"], ["ISLAMICC", "islamic-civil"]])
    same(new Temporal.PlainDate(2024, 1, 1, alias).calendarId, canonical, alias);
throws(RangeError, () => new Temporal.PlainDate(2024, 1, 1, "islamic"));

// The primary Persian authority records 1403 as leap and 1404 as common.
let persian = Temporal.PlainDate.from({ calendar: "persian", year: 1403, month: 12, day: 30 },
                                     { overflow: "reject" });
same(persian.withCalendar("iso8601").toString(), "2025-03-20", "Persian authority leap day");
same(persian.add({ days: 1 }).year, 1404, "Persian authority year boundary");
same(persian.add({ years: 1 }).day, 29, "Persian constrain common year");
throws(RangeError, () => persian.add({ years: 1 }, { overflow: "reject" }));

let japanese = Temporal.PlainDate.from({ calendar: "japanese", era: "heisei", eraYear: 31,
                                       monthCode: "M05", day: 1 });
same(japanese.era, "reiwa", "lenient Japanese era resolution");
same(japanese.eraYear, 1, "Japanese era metadata");
same(japanese.with({ month: 4, day: 30 }).era, "heisei", "Japanese field invalidation");
same(japanese.with({ year: 1989 }).eraYear, 1, "Japanese year invalidation");
throws(RangeError, () => Temporal.PlainDate.from({ calendar: "japanese", year: 2018,
    era: "reiwa", eraYear: 1, monthCode: "M05", day: 1 }));
let bce = Temporal.PlainDate.from({ calendar: "gregory", era: "bc", eraYear: 1,
                                  month: 1, day: 1 });
same(bce.year, 0, "Gregorian era alias");
same(bce.era, "bce", "Gregorian canonical era");

// M05L regulates to M06 when adding a year without the Hebrew leap month.
let hebrew = Temporal.PlainDate.from({ calendar: "hebrew", year: 5784,
                                     monthCode: "M05L", day: 30 });
same(hebrew.month, 6, "Hebrew leap ordinal");
same(hebrew.add({ years: 1 }).monthCode, "M06", "Hebrew leap fallback");
same(hebrew.add({ years: 1 }).day, 29, "Hebrew fallback day");
throws(RangeError, () => hebrew.add({ years: 1 }, { overflow: "reject" }));
for (const calendar of ["chinese", "dangi"]) {
    let lunar = Temporal.PlainDate.from({ calendar, year: 2020, monthCode: "M04", day: 30 });
    let next = lunar.add({ months: 1 });
    same(next.monthCode, "M04L", calendar + " inserted leap month");
    same(next.day, 29, calendar + " short leap month");
    same(next.monthsInYear, 13, calendar + " leap year month count");
    throws(RangeError, () => lunar.add({ months: 1 }, { overflow: "reject" }));
    same(next.add({ years: 1 }).monthCode, "M04", calendar + " leap fallback");
    const md = Temporal.PlainMonthDay.from({ calendar, monthCode: "M11L", day: 20 });
    same(md.toString().slice(0, 4), "2034", calendar + " normative leap reference");
    throws(RangeError, () => Temporal.PlainMonthDay.from({ calendar, monthCode: "M01L", day: 1 },
                                                       { overflow: "reject" }));
    same(Temporal.PlainMonthDay.from({ calendar, monthCode: "M01L", day: 1 }).monthCode,
         "M01", calendar + " impossible leap reference constrain");
}

// MonthDay only requires its supplied year to contain some representable date.
same(Temporal.PlainMonthDay.from({ calendar: "gregory", year: -271821,
                                  monthCode: "M01", day: 1 }).day, 1, "edge MonthDay year");
same(Temporal.PlainYearMonth.from({ calendar: "gregory", year: -271821,
                                   monthCode: "M04" }).month, 4, "edge reference month");
const minimum = new Temporal.PlainDate(-271821, 4, 19, "gregory");
throws(RangeError, () => minimum.subtract({ days: 1 }));
const maximum = new Temporal.PlainDate(275760, 9, 13, "gregory");
throws(RangeError, () => maximum.add({ days: 1 }));
