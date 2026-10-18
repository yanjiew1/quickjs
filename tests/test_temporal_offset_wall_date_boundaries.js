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

const below = "-271821-04-19T23:00-01:00[-01:00]";
const minimumNs = -8640000000000000000000n;
for (const offset of ["prefer", "reject"])
    assertRangeError(() => Temporal.ZonedDateTime.from(below, { offset }),
                     "local date before CheckISODaysRange");
for (const offset of ["use", "ignore"])
    assertEqual(Temporal.ZonedDateTime.from(below, { offset }).epochNanoseconds,
                minimumNs, "offset branch accepts exact minimum");
for (const offset of ["use", "ignore", "prefer", "reject"]) {
    assertEqual(Temporal.ZonedDateTime.from("-271821-04-20T00:00Z[UTC]",
                { offset }).epochNanoseconds, minimumNs, "exact minimum");
    assertEqual(Temporal.ZonedDateTime.from(
                "+275760-09-13T23:59+23:59[+23:59]", { offset }).epochNanoseconds,
                -minimumNs, "upper local time after maximum instant");
}
const fiveMinutes = Temporal.Duration.from({ minutes: 5 });
const zero = new Temporal.Duration();
assertRangeError(() => Temporal.Duration.compare(fiveMinutes, zero,
                 { relativeTo: below }), "relativeTo local date range");
assertRangeError(() => zero.round({ smallestUnit: "minute", relativeTo: below }),
                 "round relativeTo local date range");
assertRangeError(() => zero.total({ unit: "hour", relativeTo: below }),
                 "total relativeTo local date range");
assertEqual(zero.total({ unit: "hour", relativeTo: "-271821-04-19" }), 0,
            "plain relativeTo retains additional minimum day");
