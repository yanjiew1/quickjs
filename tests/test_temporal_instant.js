/* Native Temporal.Instant formatting, arithmetic, and option ordering. */
"use strict";

function assert(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("Temporal.Instant: " + actual + " !== " + expected);
}

function assert_throws(constructor, callback) {
    try {
        callback();
    } catch (error) {
        if (error instanceof constructor)
            return;
        throw error;
    }
    throw new Error("Temporal.Instant did not throw " + constructor.name);
}

function test_format() {
    const Instant = Temporal.Instant;
    const cases = [
        [0n, "1970-01-01T00:00:00Z"],
        [1n, "1970-01-01T00:00:00.000000001Z"],
        [-1n, "1969-12-31T23:59:59.999999999Z"],
        [1234000000n, "1970-01-01T00:00:01.234Z"],
        [-8640000000000000000000n, "-271821-04-20T00:00:00Z"],
        [8640000000000000000000n, "+275760-09-13T00:00:00Z"],
    ];
    for (const [ns, expected] of cases) {
        const value = new Instant(ns);
        assert(value.toString(), expected);
        assert(value.toJSON(), expected);
        assert(Instant.from(value.toString()).epochNanoseconds, ns);
        assert(JSON.stringify(value), JSON.stringify(expected));
    }
    const value = new Instant(123456789n);
    assert(value.toString({fractionalSecondDigits: 9}),
           "1970-01-01T00:00:00.123456789Z");
    assert(value.toString({fractionalSecondDigits: 2.9}),
           "1970-01-01T00:00:00.12Z");
    assert(value.toString({fractionalSecondDigits: {toString() {return "auto";}}}),
           "1970-01-01T00:00:00.123456789Z");
    assert(value.toString({smallestUnit: "minute"}), "1970-01-01T00:00Z");
    assert(value.toString({timeZone: "+05:30"}),
           "1970-01-01T05:30:00.123456789+05:30");
    assert(value.toString({timeZone: "UTC", fractionalSecondDigits: 0}),
           "1970-01-01T00:00:00+00:00");
    assert(value.toString({smallestUnit: "microseconds", roundingMode: "ceil"}),
           "1970-01-01T00:00:00.123457Z");
    assert(new Instant(-1n).toString({fractionalSecondDigits: 0}),
           "1969-12-31T23:59:59Z");
    for (const precision of [-1, 10, NaN, Infinity, "9", null])
        assert_throws(RangeError, () => value.toString({fractionalSecondDigits: precision}));
    for (const smallestUnit of ["year", "day", "hour", "auto", "second\0junk"])
        assert_throws(RangeError, () => value.toString({smallestUnit}));
    assert_throws(TypeError, () => value.toString(null));
    assert_throws(TypeError, () => value.toString(1));
    assert_throws(TypeError, () => Instant.prototype.toString.call(Instant.prototype));
    assert_throws(TypeError, () => Instant.from(Instant.prototype));
    assert_throws(TypeError, () => Instant.compare(value, Instant.prototype));
    assert_throws(TypeError, () => value.equals(Instant.prototype));
    const custom = Object.create(Instant.prototype);
    custom.toString = () => "1970-01-01T00Z";
    assert(Instant.from(custom).epochNanoseconds, 0n);
}

function test_rounding() {
    const Instant = Temporal.Instant;
    const modeCases = [
        ["ceil", 10n], ["floor", 0n], ["expand", 10n], ["trunc", 0n],
        ["halfCeil", 10n], ["halfFloor", 0n], ["halfExpand", 10n],
        ["halfTrunc", 0n], ["halfEven", 0n],
    ];
    for (const [roundingMode, expected] of modeCases) {
        const options = {smallestUnit: "nanosecond", roundingIncrement: 10,
                         roundingMode};
        assert(new Instant(5n).round(options).epochNanoseconds, expected);
        /* Half-even also depends on the parity of the full increment. */
        assert(new Instant(-5n).round(options).epochNanoseconds,
               roundingMode === "halfEven" ? 0n : expected - 10n);
    }
    assert(new Instant(60000000001n).round("minute").epochNanoseconds, 60000000000n);
    assert(new Instant(0n).round({smallestUnit: "hour", roundingIncrement: 24})
           .epochNanoseconds, 0n);
    for (const roundingIncrement of [0, -1, NaN, Infinity, 1000000001, 7])
        assert_throws(RangeError, () => new Instant(0n).round({
            smallestUnit: "hour", roundingIncrement,
        }));
    assert_throws(TypeError, () => new Instant(0n).round());
    assert_throws(TypeError, () => new Instant(0n).round(null));
    assert_throws(RangeError, () => new Instant(0n).round({}));
}

function test_arithmetic_and_difference() {
    const Instant = Temporal.Instant;
    const value = new Instant(1n);
    assert(value.add("PT0.000000002S").epochNanoseconds, 3n);
    assert(value.subtract({nanoseconds: 2}).epochNanoseconds, -1n);
    assert(value.add({hours: 24}).epochNanoseconds, 86400000000001n);
    for (const unit of ["years", "months", "weeks", "days"])
        assert_throws(RangeError, () => value.add({[unit]: 1}));
    const limit = new Instant(8640000000000000000000n);
    assert_throws(RangeError, () => limit.add({nanoseconds: 1}));
    assert(value.epochNanoseconds, 1n);
    const end = new Instant(3723004005006n);
    const duration = new Instant(0n).until(end, {largestUnit: "hours"});
    assert(duration.years, 0);
    assert(duration.days, 0);
    assert(duration.hours, 1);
    assert(duration.minutes, 2);
    assert(duration.seconds, 3);
    assert(duration.milliseconds, 4);
    assert(duration.microseconds, 5);
    assert(duration.nanoseconds, 6);
    assert(end.since(new Instant(0n), {largestUnit: "hour"}).toString(),
           duration.toString());
    assert(new Instant(0n).since(end, {largestUnit: "hour"}).hours, -1);
    assert(new Instant(0n).until(new Instant(-5n), {
        smallestUnit: "nanosecond", roundingIncrement: 10,
        roundingMode: "halfExpand",
    }).nanoseconds, -10);
    for (const largestUnit of ["year", "week", "day"])
        assert_throws(RangeError, () => value.until(end, {largestUnit}));
    assert_throws(RangeError, () => value.until(end, {
        largestUnit: "second", smallestUnit: "hour",
    }));
    assert(value.until(value).toString(), "PT0S");
}

function test_observable_order() {
    const value = new Temporal.Instant(0n);
    const log = [];
    const stringOption = key => ({toString() {log.push("string " + key); return {
        roundingMode: "trunc", smallestUnit: "second", timeZone: "UTC",
    }[key];}});
    const options = new Proxy({}, {get(_, key) {
        log.push("get " + key);
        if (key === "fractionalSecondDigits")
            return 0;
        if (key === "timeZone")
            return "UTC";
        return stringOption(key);
    }});
    assert(value.toString(options), "1970-01-01T00:00:00+00:00");
    assert(log.join(), "get fractionalSecondDigits,get roundingMode,string roundingMode," +
           "get smallestUnit,string smallestUnit,get timeZone");
    log.length = 0;
    assert_throws(RangeError, () => value.toString(new Proxy({}, {get(_, key) {
        log.push(key);
        return key === "smallestUnit" ? "hour" : undefined;
    }})));
    assert(log.join(), "fractionalSecondDigits,roundingMode,smallestUnit,timeZone");
    log.length = 0;
    value.until({[Symbol.toPrimitive](hint) {
        assert(hint, "string"); log.push("other"); return "1970-01-01T00Z";
    }}, new Proxy({}, {get(_, key) {log.push(key); return undefined;}}));
    assert(log.join(), "other,largestUnit,roundingIncrement,roundingMode,smallestUnit");
    for (const name of ["add", "subtract", "until", "since", "round", "toString",
                        "toJSON", "toLocaleString", "toZonedDateTimeISO"]) {
        log.length = 0;
        const other = new Proxy({}, {get() {log.push("unexpected"); throw 1;}});
        assert_throws(TypeError, () => Temporal.Instant.prototype[name].call({}, other));
        assert(log.length, 0);
    }
}

function test_zoned_and_date_bridges() {
    const value = new Temporal.Instant(-1n);
    const zoned = value.toZonedDateTimeISO("+01:00");
    assert(zoned.epochNanoseconds, -1n);
    assert(zoned.timeZoneId, "+01:00");
    assert(zoned.calendarId, "iso8601");
    Object.defineProperty(zoned, Symbol.toPrimitive, {get() {throw 1;}});
    assert(Temporal.Instant.from(zoned).epochNanoseconds, -1n);
    const date = new Date(-1);
    Object.defineProperty(date, "valueOf", {get() {throw 1;}});
    assert(date.toTemporalInstant().epochNanoseconds, -1000000n);
    assert_throws(TypeError, () => Date.prototype.toTemporalInstant.call({}));
    assert_throws(RangeError, () => new Date(NaN).toTemporalInstant());
}

test_format();
test_rounding();
test_arithmetic_and_difference();
test_observable_order();
test_zoned_and_date_bridges();
