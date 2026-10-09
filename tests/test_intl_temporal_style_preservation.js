function assert(value, message) {
    if (!value) throw Error(message || "assertion failed");
}
function same(actual, expected, message) {
    assert(Object.is(actual, expected), message ||
           "expected " + expected + ", got " + actual);
}
function partsSignature(parts) {
    return JSON.stringify(parts.map(({ type, value }) => ({ type, value })));
}
const epoch = new Date(0);
const date = new Temporal.PlainDate(1970, 1, 1);
const datetime = new Temporal.PlainDateTime(1970, 1, 1);
const time = new Temporal.PlainTime();
const instant = new Temporal.Instant(0n);

// AdjustDateTimeStyleFormat must retain the base pattern when all its
// fields are applicable. Compare APIs sharing locale data, not spellings.
for (const locale of ["ja", "en", "fr"]) {
    for (const formatMatcher of ["basic", "best fit"]) {
        for (const dateStyle of ["full", "long", "medium", "short"]) {
            const options = { dateStyle, timeZone: "UTC", formatMatcher };
            const f = new Intl.DateTimeFormat(locale, options);
            const expected = f.format(epoch);
            for (const value of [date, datetime, instant]) {
                same(f.format(value), expected);
                same(partsSignature(f.formatToParts(value)),
                     partsSignature(f.formatToParts(epoch)));
                same(f.formatRange(value, value), expected);
                const parts = f.formatRangeToParts(value, value);
                assert(parts.every(part => part.source === "shared"));
                same(parts.map(part => part.value).join(""), expected);
                same(value.toLocaleString(locale, options),
                     epoch.toLocaleString(locale, options));
            }
        }
        // Short time styles have no time zone name to remove.
        const options = { timeStyle: "short", timeZone: "UTC", formatMatcher };
        const f = new Intl.DateTimeFormat(locale, options);
        for (const value of [time, datetime, instant]) {
            same(f.format(value), f.format(epoch));
            same(value.toLocaleString(locale, options),
                 epoch.toLocaleString(locale, options));
        }
        const both = { dateStyle: "full", timeStyle: "short",
                       timeZone: "UTC", formatMatcher };
        const combined = new Intl.DateTimeFormat(locale, both);
        same(combined.format(datetime), combined.format(epoch));
        same(datetime.toLocaleString(locale, both),
             epoch.toLocaleString(locale, both));
    }
}

// Styles with inapplicable fields still need a reduced format.
const style = new Intl.DateTimeFormat("ja", {
    calendar: "iso8601", dateStyle: "full", timeStyle: "long",
    timeZone: "UTC"
});
for (const [value, allowed] of [
    [date, ["weekday", "era", "year", "month", "day", "literal"]],
    [new Temporal.PlainYearMonth(1970, 1),
     ["era", "year", "month", "literal"]],
    [new Temporal.PlainMonthDay(1, 1), ["month", "day", "literal"]],
    [time, ["dayPeriod", "hour", "minute", "second", "literal"]]
]) {
    const parts = style.formatToParts(value);
    assert(parts.length > 0);
    assert(parts.every(part => allowed.includes(part.type)));
}
const datetimeParts = style.formatToParts(datetime);
assert(!datetimeParts.some(part => part.type === "timeZoneName"));
assert(style.formatToParts(instant).some(part =>
    part.type === "timeZoneName"));
