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

const zero = new Temporal.Duration();
const maximum = new Temporal.ZonedDateTime(8640000000000000000000n, "UTC");
const previous = Temporal.ZonedDateTime.from("+275760-09-12T00:00Z[UTC]");
const previousSecond = previous.add({ seconds: 1 });
assertRangeError(() => zero.round({ largestUnit: "day", smallestUnit: "minute",
                 relativeTo: maximum }), "round needs following day boundary");
assertRangeError(() => zero.total({ unit: "day", relativeTo: maximum }),
                 "total needs following day boundary");
assertRangeError(() => zero.total({ unit: "day", relativeTo: previousSecond }),
                 "total following day preserves wall time");
assertEqual(zero.total({ unit: "day", relativeTo: previous }), 0,
            "total following day exactly at maximum");
assertEqual(zero.total({ unit: "hour", relativeTo: maximum }), 0,
            "time total does not need next date");
assertEqual(zero.round({ largestUnit: "day", smallestUnit: "nanosecond",
            relativeTo: maximum }).toString(), "PT0S", "exact difference");
assertEqual(zero.round({ largestUnit: "minute", smallestUnit: "minute",
            relativeTo: maximum }).toString(), "PT0S", "time difference");
assertEqual(maximum.until(maximum, { largestUnit: "day", smallestUnit: "minute" })
            .toString(), "PT0S", "equal ZonedDateTime returns zero");
const minimum = new Temporal.ZonedDateTime(-8640000000000000000000n, "UTC");
assertEqual(zero.total({ unit: "day", relativeTo: minimum }), 0,
            "minimum forward day boundary");
assertEqual(zero.round({ largestUnit: "day", smallestUnit: "minute",
            relativeTo: minimum }).toString(), "PT0S", "minimum zero rounding");
