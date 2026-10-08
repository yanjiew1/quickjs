/* Enabled Intl backend: real calendar and IANA transition semantics. */
"use strict";
function assert(actual, expected, message = "ZonedDateTime Intl") {
    if (!Object.is(actual, expected)) throw new Error(message + ": " + String(actual) + " !== " + String(expected));
}
function assert_throws(type, callback) {
    try { callback(); } catch (error) {if (error instanceof type) return; throw error;}
    throw new Error("Expected " + type.name);
}
const ZDT = Temporal.ZonedDateTime;
const nsHour = 3600000000000n;

/* Overlap choices, explicit offset validation, and offset preservation. */
const overlap = "2020-11-01T01:30[America/New_York]";
const early = ZDT.from(overlap, {disambiguation: "earlier"});
const late = ZDT.from(overlap, {disambiguation: "later"});
assert(late.epochNanoseconds - early.epochNanoseconds, nsHour);
assert(early.offset, "-04:00");
assert(late.offset, "-05:00");
assert(ZDT.from(overlap).equals(early), true);
assert_throws(RangeError, () => ZDT.from(overlap, {disambiguation: "reject"}));
assert(late.with({minute: 45}).offset, "-05:00");
assert(late.round({smallestUnit: "hour", roundingMode: "floor"}).offset, "-05:00");
assert(early.round({smallestUnit: "hour", roundingMode: "floor"}).offset, "-04:00");
assert(ZDT.from("2020-11-01T01:30-05:00[America/New_York]").equals(late), true);
assert(ZDT.from("2020-11-01T01:30-05:00[America/New_York]", {offset: "ignore"}).equals(early), true);
assert_throws(RangeError, () => ZDT.from("2020-11-01T01:30-03:00[America/New_York]"));

/* Gap disambiguation and exact hours versus calendar days. */
const gap = "2020-03-08T02:30[America/New_York]";
assert(ZDT.from(gap, {disambiguation: "earlier"}).hour, 1);
assert(ZDT.from(gap, {disambiguation: "later"}).hour, 3);
assert(ZDT.from(gap).hour, 3);
assert_throws(RangeError, () => ZDT.from(gap, {disambiguation: "reject"}));
const spring = ZDT.from("2020-03-07T12:00[America/New_York]");
assert(spring.add({days: 1}).epochNanoseconds - spring.epochNanoseconds, 23n * nsHour);
assert(spring.add({hours: 24}).hour, 13);
assert(spring.add({days: 1}).hour, 12);
assert(spring.until(spring.add({days: 1}), {largestUnit: "day"}).days, 1);
assert(spring.until(spring.add({days: 1})).hours, 23);
const shortDay = ZDT.from("2020-03-08T12:00[America/New_York]");
const longDay = ZDT.from("2020-11-01T12:00[America/New_York]");
assert(shortDay.hoursInDay, 23);
assert(longDay.hoursInDay, 25);
assert(shortDay.round("day").day, 8);  // 11 elapsed of 23 hours
assert(longDay.round("day").day, 2);   // 13 elapsed of 25 hours
assert(ZDT.from("2020-03-08T12:30[America/New_York]").round("day").day, 9);
assert(shortDay.startOfDay().hour, 0);

/* A skipped midnight and a half-hour transition exercise non-hour shifts. */
const saoPaulo = ZDT.from("2018-11-04[America/Sao_Paulo]");
assert(saoPaulo.hour, 1);
assert(saoPaulo.startOfDay().equals(saoPaulo), true);
assert(saoPaulo.withPlainTime().equals(saoPaulo), true);
const lordHowe = ZDT.from("2020-10-04T12:00[Australia/Lord_Howe]");
assert(lordHowe.hoursInDay, 23.5);
assert(lordHowe.offset, "+11:00");

/* Transitions are strict, including the nanosecond on either side. */
const next = spring.getTimeZoneTransition("next");
assert(next.toInstant().toString(), "2020-03-08T07:00:00Z");
assert(next.offset, "-04:00");
assert(next.subtract({nanoseconds: 1}).getTimeZoneTransition("next").epochNanoseconds,
       next.epochNanoseconds);
assert(next.getTimeZoneTransition("next").epochNanoseconds > next.epochNanoseconds, true);
assert(next.add({nanoseconds: 1}).getTimeZoneTransition("previous").epochNanoseconds,
       next.epochNanoseconds);
assert(next.getTimeZoneTransition("previous").epochNanoseconds < next.epochNanoseconds, true);
assert(new ZDT(0n, "US/Eastern").timeZoneId, "US/Eastern");
assert(new ZDT(0n, "US/Eastern").equals(new ZDT(0n, "America/New_York")), true);

/* Minute matching accepts historical seconds only for minute-only text. */
const paris = ZDT.from("1900-01-01T00:00+00:09[Europe/Paris]");
assert(paris.offset, "+00:09:21");
assert_throws(RangeError, () => ZDT.from("1900-01-01T00:00+00:09:00[Europe/Paris]"));
assert(ZDT.from("1900-01-01T00:00+00:09:21[Europe/Paris]").equals(paris), true);
assert(paris.toString().includes("+00:09[Europe/Paris]"), true);

const gregory = shortDay.withCalendar("gregory");
assert(gregory.calendarId, "gregory");
assert(gregory.year, 2020);
assert(gregory.era, "ce");
assert(gregory.eraYear, 2020);
assert(gregory.weekOfYear, undefined);
assert(gregory.toString().endsWith("[u-ca=gregory]"), true);
assert_throws(RangeError, () => shortDay.until(gregory));
const japanese = ZDT.from({calendar: "japanese", era: "reiwa", eraYear: 2,
                          monthCode: "M03", day: 8, timeZone: "Asia/Tokyo"});
assert(japanese.year, 2020);
assert(japanese.era, "reiwa");
assert(japanese.eraYear, 2);
const hebrew = ZDT.from("2024-03-01T12:00[Asia/Jerusalem][u-ca=hebrew]");
assert(hebrew.inLeapYear, true);
assert(hebrew.monthsInYear, 13);
assert(hebrew.monthCode, "M05L");
assert(hebrew.with({monthCode: "M06"}).monthCode, "M06");
assert(gregory.toPlainDate().calendarId, "gregory");
assert(gregory.toPlainDateTime().calendarId, "gregory");
assert(typeof gregory.toLocaleString("en-US", {year: "numeric", timeZoneName: "short"}), "string");

/* The formatter uses slots, rejects an override before string coercion,
   and rejects non-ISO calendar mismatch. Direct DTF input remains invalid. */
const formatted = ZDT.from("2020-03-08T12:00[America/New_York]");
Object.defineProperty(formatted, "timeZoneId", {get() {throw new Error("zone getter");}});
Object.defineProperty(formatted, "calendarId", {get() {throw new Error("calendar getter");}});
assert(formatted.toLocaleString("en-US", {hour: "2-digit", hourCycle: "h23"}), "12");
assert_throws(TypeError, () => formatted.toLocaleString("en-US", {
    timeZone: {toString() {throw new Error("override coercion");}}
}));
assert_throws(RangeError, () => gregory.toLocaleString("en-US", {calendar: "buddhist"}));
assert_throws(TypeError, () => new Intl.DateTimeFormat("en-US").format(gregory));
