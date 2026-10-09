/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
function assertEqual(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw new Error(message + ": " + actual + " != " + expected);
}
function assertRangeError(callback, message) {
    try {
        callback();
    } catch (error) {
        if (error instanceof RangeError)
            return;
        throw error;
    }
    throw new Error(message + ": missing RangeError");
}

const calendarNames = ["1997-12-04[u-ca=iso8601]", "11111111", "1111-11-11"];
const constructors = [
    calendar => new Temporal.PlainDate(2000, 5, 2, calendar),
    calendar => new Temporal.PlainDateTime(2000, 5, 2, 0, 0, 0, 0, 0, 0, calendar),
    calendar => new Temporal.PlainYearMonth(2000, 5, calendar),
    calendar => new Temporal.PlainMonthDay(5, 2, calendar),
    calendar => new Temporal.ZonedDateTime(0n, "UTC", calendar),
];
for (const create of constructors) {
    for (const calendar of calendarNames)
        assertRangeError(() => create(calendar), "constructor calendar identifier");
    assertEqual(create("ISO8601").calendarId, "iso8601", "case folding");
}
for (const Type of [Temporal.PlainDate, Temporal.PlainDateTime,
                   Temporal.PlainYearMonth, Temporal.PlainMonthDay]) {
    for (const calendar of ["11111111", "1111-11-11"])
        assertRangeError(() => Type.from("2000-05-02[u-ca=" + calendar + "]"),
                         "calendar annotation identifier");
}
const date = Temporal.PlainDate.from({
    year: 2000, month: 5, day: 2, calendar: "1997-12-04[u-ca=iso8601]"
});
assertEqual(date.calendarId, "iso8601", "property bag calendar conversion");
assertEqual(date.withCalendar("1997-12-04").calendarId, "iso8601",
            "withCalendar conversion");
