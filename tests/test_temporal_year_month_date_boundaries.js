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

const minimum = new Temporal.PlainYearMonth(-271821, 4);
const next = new Temporal.PlainYearMonth(-271821, 5);
const maximum = new Temporal.PlainYearMonth(275760, 9);
for (const method of ["add", "subtract"]) {
    const calls = [];
    const options = { get overflow() {
        calls.push("get overflow");
        return { toString() { calls.push("toString"); return "constrain"; } };
    }};
    assertRangeError(() => minimum[method]({ months: 1 }, options),
                     "year month converted to day one");
    assertEqual(calls.join(","), "get overflow,toString", "options read first");
    assertRangeError(() => minimum[method]({ months: 0 }), "zero addition converts date");
    assertEqual(maximum[method]({ months: 0 }).toString(), "+275760-09", "upper month");
}
for (const method of ["until", "since"]) {
    assertEqual(minimum[method](minimum).toString(), "PT0S", "equal minimum");
    assertRangeError(() => minimum[method](next), "minimum receiver conversion");
    assertRangeError(() => next[method](minimum), "minimum argument conversion");
    assertRangeError(() => minimum[method](maximum), "minimum to maximum");
    assertEqual(next[method](next).toString(), "PT0S", "equal valid month");
}
assertEqual(next.add({ months: 1 }).toString(), "-271821-06", "valid addition");
assertRangeError(() => next.subtract({ months: 1 }), "added date outside range");
assertEqual(minimum.toPlainDate({ day: 19 }).toString(), "-271821-04-19",
            "minimum permitted plain date");
