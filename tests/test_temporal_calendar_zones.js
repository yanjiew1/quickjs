/* Native Temporal calendar/zone conversion and observable field coercion. */
function assert(value, expected, message) {
    if (arguments.length < 2) expected = true;
    if (!Object.is(value, expected)) throw Error(message || `${value} !== ${expected}`);
}
function throws(type, callback) {
    try { callback(); } catch (error) { assert(error instanceof type); return; }
    throw Error(`expected ${type.name}`);
}

const day = Temporal.PlainDate.from({ year: 2024, monthCode: "M02", day: 29 });
assert(day.dayOfWeek, 4);
assert(day.dayOfYear, 60);
assert(day.weekOfYear, 9);
assert(day.daysInMonth, 29);
assert(day.inLeapYear, true);
assert(day.add({ years: 1 }).toString(), "2025-02-28");
throws(RangeError, () => day.add({ years: 1 }, { overflow: "reject" }));
assert(Temporal.PlainDate.from("2023-01-31").until("2023-02-28", {
    largestUnit: "month"
}).toString(), "P28D");
assert(Temporal.PlainMonthDay.from({ month: 2, day: 29 }).day, 29);
assert(Temporal.PlainMonthDay.from({ month: 2, day: 29, year: 2023 }).day, 28);
assert(Temporal.PlainMonthDay.from({ monthCode: "M02", day: 29, year: 2023 }).day, 28);
assert(Temporal.PlainMonthDay.from({ monthCode: "M02", day: 29, year: 1e300 }).day, 29);
throws(RangeError, () => Temporal.PlainDate.from({
    year: 2024, month: 3, monthCode: "M02", day: 1
}));
throws(RangeError, () => Temporal.PlainDate.from("2024-01-01[u-ca=unknown]"));
throws(RangeError, () => Temporal.PlainDate.from("2024-01-01[u-ca=islamic]"));
throws(RangeError, () => Temporal.PlainDate.from("2024-01-01[u-ca=islamic-rgsa]"));

const gets = [];
const fields = {};
for (const [name, value] of [
    ["calendar", "iso8601"], ["day", 1], ["month", 1],
    ["monthCode", undefined], ["year", 1e300]
]) Object.defineProperty(fields, name, { get() { gets.push(name); return value; } });
throws(RangeError, () => Temporal.PlainDate.from(fields));
assert(gets.join(","), "calendar,day,month,monthCode,year");
const codeGets = [];
throws(TypeError, () => Temporal.PlainDate.from({
    get day() { codeGets.push("day"); return 1; },
    get month() { codeGets.push("month"); return 1; },
    get monthCode() { codeGets.push("monthCode"); return 1; },
    get year() { codeGets.push("year"); return 2024; }
}));
assert(codeGets.join(","), "day,month,monthCode");

const brandedYearMonth = Temporal.PlainYearMonth.from("2024-01");
Object.defineProperty(brandedYearMonth, "calendar", {
    get() { throw Error("the native calendar slot must take precedence"); }
});
throws(TypeError, () => Temporal.Duration.from({ days: 1 }).total({
    unit: "days", relativeTo: brandedYearMonth
}));

const z = Temporal.ZonedDateTime.from("2024-02-29T12:34:56.123456789+05:30[+05:30]");
assert(z.offset, "+05:30");
assert(z.offsetNanoseconds, 19800000000000);
assert(z.toPlainDateTime().toString(), "2024-02-29T12:34:56.123456789");
assert(z.withTimeZone("UTC").hour, 7);
assert(z.withTimeZone("2024-01-01T00:00Z").timeZoneId, "UTC");
assert(z.withTimeZone("UTC").minute, 4);
assert(z.toInstant().epochNanoseconds, z.epochNanoseconds);
throws(TypeError, () => z.withTimeZone({ toString() { return "UTC"; } }));
throws(RangeError, () => z.withTimeZone("+00:00:01"));
const offsetObject = {
    [Symbol.toPrimitive](hint) { assert(hint, "string"); return "+05:30"; }
};
assert(Temporal.ZonedDateTime.from({
    year: 2024, month: 2, day: 29, hour: 12, timeZone: "+05:30", offset: offsetObject
}).hour, 12);
throws(TypeError, () => Temporal.ZonedDateTime.from({
    year: 2024, month: 2, day: 29, timeZone: "UTC", offset: 0
}));
assert(Temporal.Duration.from({ hours: 25 }).total({
    unit: "days", relativeTo: "2024-01-01T00:00[UTC]"
}), 25 / 24);

/* Temporal uses the low-level ICU backend even when Intl is disabled. */
const calendarDate = Temporal.PlainDate.from("2024-01-01[u-ca=gregory]");
assert(calendarDate.era, "ce");
assert(calendarDate.eraYear, 2024);
assert(Temporal.PlainDate.from("2024-01-01[u-ca=buddhist]").year, 2567);
assert(Temporal.PlainDate.from("2024-01-01[u-ca=roc]").year, 113);
assert(Temporal.PlainDate.from("2024-01-01[u-ca=ethioaa]").year, 7516);
assert(calendarDate.weekOfYear, undefined);
assert(calendarDate.yearOfWeek, undefined);
assert(Temporal.PlainDate.from({
    calendar: "gregory", era: "bc", eraYear: 1, month: 1, day: 1
}).year, 0);
throws(RangeError, () => Temporal.PlainDate.from({
    calendar: "gregory", era: "CE", eraYear: 1, month: 1, day: 1
}));
assert(Temporal.PlainDate.from("1872-12-31[u-ca=japanese]").era, "ce");
assert(Temporal.PlainDate.from("1873-01-01[u-ca=japanese]").era, "meiji");
assert(Temporal.PlainDate.from("2019-04-30[u-ca=japanese]").era, "heisei");
assert(Temporal.PlainDate.from("2019-05-01[u-ca=japanese]").era, "reiwa");
assert(Temporal.PlainDate.from("2024-01-01[u-ca=islamicc]").calendarId, "islamic-civil");
assert(Temporal.PlainDate.from("2024-01-01[u-ca=ethiopic-amete-alem]").calendarId, "ethioaa");
const eraGets = [];
throws(RangeError, () => Temporal.PlainDate.from({
    calendar: "gregory",
    get day() { eraGets.push("day"); return 1; },
    get era() { eraGets.push("era"); return "x".repeat(100); },
    get eraYear() { eraGets.push("eraYear"); return 1; },
    get month() { eraGets.push("month"); return 1; },
    get monthCode() { eraGets.push("monthCode"); return undefined; },
    get year() { eraGets.push("year"); return undefined; }
}));
assert(eraGets.join(","), "day,era,eraYear,month,monthCode,year");
const overlap = "2024-11-03T01:30[America/New_York]";
const earlier = Temporal.ZonedDateTime.from(overlap, { disambiguation: "earlier" });
const later = Temporal.ZonedDateTime.from(overlap, { disambiguation: "later" });
assert(later.epochNanoseconds - earlier.epochNanoseconds, 3600000000000n);
assert(earlier.offset, "-04:00"); assert(later.offset, "-05:00");
throws(RangeError, () => Temporal.ZonedDateTime.from(overlap, { disambiguation: "reject" }));
const lisbonBefore = Temporal.ZonedDateTime.from("1992-09-01T00:00[Europe/Lisbon]");
assert(lisbonBefore.getTimeZoneTransition("next").toInstant().toString(),
       "1993-03-28T01:00:00Z");
const lisbonAfter = Temporal.ZonedDateTime.from("1992-10-01T00:00[Europe/Lisbon]");
assert(lisbonAfter.getTimeZoneTransition("previous").toInstant().toString(),
       "1992-03-29T01:00:00Z");
const gap = "2024-03-10T02:30[America/New_York]";
assert(Temporal.ZonedDateTime.from(gap, { disambiguation: "earlier" }).hour, 1);
assert(Temporal.ZonedDateTime.from(gap, { disambiguation: "later" }).hour, 3);
const spring = Temporal.ZonedDateTime.from("2024-03-09T12:00[America/New_York]");
assert(spring.add({ days: 1 }).epochNanoseconds - spring.epochNanoseconds, 23n * 3600000000000n);
assert(spring.add({ hours: 24 }).hour, 13);
assert(Temporal.Duration.from({ days: 1 }).total({ unit: "hours", relativeTo: spring }), 23);
/* A midnight-straddling gap starts the date at the transition itself. */
const brazil = Temporal.ZonedDateTime.from("2018-11-04[America/Sao_Paulo]");
assert(brazil.hour, 1);
assert(brazil.startOfDay().epochNanoseconds, brazil.epochNanoseconds);

/* CalendarDateAdd is shared by plain, relative-duration and zoned callers. */
const january = Temporal.PlainDate.from("2024-01-31");
assert(january.add({ months: 1, days: 1 }).toString(), "2024-03-01");
assert(january.subtract({ months: 1 }).toString(), "2023-12-31");
assert(january.add({ weeks: 1 }).toString(), "2024-02-07");
throws(RangeError, () => january.add({ months: 1 }, { overflow: "reject" }));

const januaryTime = Temporal.PlainDateTime.from("2024-01-31T12:34:56.123456789");
assert(januaryTime.add({ months: 1 }).toString(),
       "2024-02-29T12:34:56.123456789");
assert(januaryTime.add({ months: 1, hours: 24 }).toString(),
       "2024-03-01T12:34:56.123456789");
throws(RangeError, () => januaryTime.add({ months: 1 }, { overflow: "reject" }));

assert(Temporal.Duration.compare({ months: 1 }, { days: 29 }, {
    relativeTo: january
}), 0);
assert(Temporal.Duration.compare({ months: 1 }, { days: 28 }, {
    relativeTo: january
}), 1);
assert(Temporal.Duration.from({ months: 1, days: 1 }).total({
    relativeTo: january, unit: "days"
}), 30);
assert(Temporal.Duration.from({ months: 1, days: 1 }).round({
    relativeTo: january, largestUnit: "months", smallestUnit: "days"
}).toString(), "P1M1D");

const januaryZoned = Temporal.ZonedDateTime.from(
    "2024-01-31T12:34:56.123456789+05:30[+05:30]"
);
assert(januaryZoned.add({ months: 1 }).toPlainDateTime().toString(),
       "2024-02-29T12:34:56.123456789");
throws(RangeError, () => januaryZoned.add({ months: 1 }, { overflow: "reject" }));
throws(RangeError, () => Temporal.PlainDate.from("+275760-09-13").add({ days: 1 }));
assert(january.add({ months: 1 }).toString(), "2024-02-29");
