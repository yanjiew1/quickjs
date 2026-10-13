/* Day rounding across a backward shift that repeats the previous date. */
"use strict";

function assert(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw new Error(message + ": " + String(actual) + " !== " + String(expected));
}

const ZDT = Temporal.ZonedDateTime;
const inputs = [
    "2010-03-04T23:10:00+11:00[Antarctica/Casey]",
    "2010-03-05T00:45:00+11:00[Antarctica/Casey]",
    "2010-03-04T23:10:00+08:00[Antarctica/Casey]",
    "2010-03-05T00:45:00+08:00[Antarctica/Casey]",
].map(value => ZDT.from(value));
const starts = [
    "2010-03-04T00:00:00+11:00[Antarctica/Casey]",
    "2010-03-05T00:00:00+11:00[Antarctica/Casey]",
    "2010-03-06T00:00:00+08:00[Antarctica/Casey]",
].map(value => ZDT.from(value));

/* The ICU transition provider must retain both real offset choices. */
assert(inputs[2].epochNanoseconds - inputs[0].epochNanoseconds,
       10800000000000n, "three hour overlap on March 4");
assert(inputs[3].epochNanoseconds - inputs[1].epochNanoseconds,
       10800000000000n, "three hour overlap on March 5");
assert(inputs[2].epochNanoseconds > starts[1].epochNanoseconds,
       true, "March 4 repeats after March 5 first begins");

const modes = [
    ["floor", [0, 1, 0, 1]],
    ["trunc", [0, 1, 0, 1]],
    ["halfCeil", [1, 1, 1, 1]],
    ["halfEven", [1, 1, 1, 1]],
    ["halfExpand", [1, 1, 1, 1]],
    ["halfFloor", [1, 1, 1, 1]],
    ["halfTrunc", [1, 1, 1, 1]],
    ["ceil", [1, 2, 1, 2]],
    ["expand", [1, 2, 1, 2]],
];
for (const [roundingMode, expected] of modes) {
    for (let i = 0; i < inputs.length; i++) {
        const result = inputs[i].round({smallestUnit: "day", roundingMode});
        const label = "Casey input " + i + " " + roundingMode;
        assert(result.epochNanoseconds, starts[expected[i]].epochNanoseconds,
               label + " first midnight");
        assert(result.timeZoneId, "Antarctica/Casey", label + " time zone");
        assert(result.calendarId, "iso8601", label + " calendar");
    }
}

/* Exactly midnight remains unchanged, including all nearest tie modes. */
for (const [roundingMode] of modes) {
    assert(starts[1].round({smallestUnit: "day", roundingMode}).epochNanoseconds,
           starts[1].epochNanoseconds, "first March 5 midnight " + roundingMode);
}

/* Ordinary day progress and skipped midnight retain exact tie handling. */
const beforeGap = ZDT.from("1919-03-30T11:45[America/Toronto]");
const gapStart = ZDT.from("1919-03-31T00:30[America/Toronto]");
assert(beforeGap.round("day").epochNanoseconds, gapStart.epochNanoseconds,
       "halfExpand to skipped midnight");
assert(beforeGap.round({smallestUnit: "day", roundingMode: "halfTrunc"})
       .epochNanoseconds, beforeGap.startOfDay().epochNanoseconds,
       "halfTrunc before skipped midnight");
