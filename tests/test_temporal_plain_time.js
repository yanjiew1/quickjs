/*
 * Temporal.PlainTime tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import { assert, assertThrows, assertArrayEquals } from "./assert.js";

const PlainTime = Temporal.PlainTime;
const names = ["hour", "minute", "second", "millisecond", "microsecond", "nanosecond"];

function fields(time) {
    return names.map(name => time[name]);
}

function test_time_creation() {
    assert(PlainTime.name, "PlainTime");
    assert(PlainTime.length, 0);
    assert(PlainTime.from.length, 1);
    assert(PlainTime.compare.length, 2);
    assertThrows(TypeError, () => PlainTime());
    assertThrows(TypeError, () => +new PlainTime());
    assert(new PlainTime().toString(), "00:00:00");
    assert(new PlainTime(1.9, 2.9, 3.9, 4.9, 5.9, 6.9).toString(), "01:02:03.004005006");
    assertArrayEquals(fields(new PlainTime(-0, -0, -0, -0, -0, -0)), Array(6).fill(0));
    assert(Object.prototype.toString.call(new PlainTime()), "[object Temporal.PlainTime]");
    for (const name of names) {
        const descriptor = Object.getOwnPropertyDescriptor(PlainTime.prototype, name);
        assert(descriptor.get.name, "get " + name);
        assert(descriptor.get.length, 0);
        assert(descriptor.set, undefined);
        assert(descriptor.enumerable, false);
        assert(descriptor.configurable, true);
        assertThrows(TypeError, () => descriptor.get.call({}));
    }
    for (const values of [[24], [-1], [1, 60], [1, 0, 60], [0, 0, 0, 1000],
                         [0, 0, 0, 0, 1000], [0, 0, 0, 0, 0, 1000], [NaN], [Infinity]])
        assertThrows(RangeError, () => new PlainTime(...values));
    assertThrows(TypeError, () => new PlainTime(1n));
    const order = [];
    const values = names.map(name => ({ valueOf() { order.push(name); return 1; } }));
    new PlainTime(...values);
    assertArrayEquals(order, names);
    const conversionError = {};
    let actualError;
    try {
        new PlainTime(24, { valueOf() { throw conversionError; } });
    } catch (error) {
        actualError = error;
    }
    assert(actualError, conversionError);
    class Derived extends PlainTime {}
    const derived = new Derived(12);
    assert(Object.getPrototypeOf(derived), Derived.prototype);
    assert(Object.getPrototypeOf(PlainTime.from(derived)), PlainTime.prototype);
}

function test_time_conversion() {
    const order = [];
    const fieldOrder = ["hour", "microsecond", "millisecond", "minute", "nanosecond", "second"];
    const bag = new Proxy(Object.fromEntries(names.map(name => [name, {
        valueOf() { order.push("value " + name); return 1.9; }
    }])), { get(target, key) { order.push("get " + key); return target[key]; } });
    const options = new Proxy({ overflow: {
        toString() { order.push("value overflow"); return "constrain"; }
    } }, { get(target, key) { order.push("get " + key); return target[key]; } });
    assertArrayEquals(fields(PlainTime.from(bag, options)), Array(6).fill(1));
    assertArrayEquals(order, fieldOrder.flatMap(name => ["get " + name, "value " + name])
                     .concat(["get overflow", "value overflow"]));
    order.length = 0;
    const native = new PlainTime(12, 34);
    Object.defineProperty(native, "hour", { get() { throw new Error("unobservable"); } });
    assert(PlainTime.from(native, options).toString(), "12:34:00");
    assertArrayEquals(order, ["get overflow", "value overflow"]);
    assert(PlainTime.from({ hour: 90, minute: -1, second: 90,
                           millisecond: 1001, microsecond: -2, nanosecond: 1001 }).toString(),
           "23:00:59.999000999");
    assertThrows(RangeError, () => PlainTime.from({ hour: 24 }, { overflow: "reject" }));
    assertThrows(TypeError, () => PlainTime.from({}));
    assertThrows(TypeError, () => PlainTime.from(12));
    assertThrows(TypeError, () => PlainTime.from(null));
    assertThrows(TypeError, () => PlainTime.from({ toString() { return "12:00"; } }));
    assertThrows(TypeError, () => PlainTime.from("12:00", null));
    assertThrows(RangeError, () => PlainTime.from("12:00", { overflow: "bad" }));
    const datetime = new Temporal.PlainDateTime(2024, 1, 1, 12, 34, 56, 789, 123, 456);
    assert(PlainTime.from(datetime).toString(), "12:34:56.789123456");
    assert(PlainTime.from(new Temporal.ZonedDateTime(0n, "+05:30")).toString(), "05:30:00");
}

function test_time_strings() {
    const valid = [
        ["T12", "12:00:00"],
        ["t1234", "12:34:00"],
        ["12:34:56.123456789", "12:34:56.123456789"],
        ["12:34:56,123456789", "12:34:56.123456789"],
        ["1970-01-01T12:34:56+05:30", "12:34:56"],
        ["12:34[+05:30][u-ca=iso8601]", "12:34:00"],
        ["12:34[America/New_York][u-ca=unknown]", "12:34:00"],
        ["12:34:60", "12:34:59"],
    ];
    for (const [text, expected] of valid)
        assert(PlainTime.from(text).toString(), expected);
    for (const text of ["", "1231", "12-31", "2024-01", "12:00Z", "12:00z",
                        "1970-01-01T12:00Z", "24:00", "12:60", "12:00:61",
                        "12:00:00.1234567890", "12:3456", "1234:56", "12:00.1",
                        "12:00[!unknown=value]", "12:00[u-ca=iso8601][!u-ca=iso8601]",
                        "12:00[UTC][UTC]", "12:00[]", "12:00garbage"])
        assertThrows(RangeError, () => PlainTime.from(text));
}

function test_time_arithmetic() {
    const time = PlainTime.from("23:59:59.999999999");
    assert(time.add({ nanoseconds: 1 }).toString(), "00:00:00");
    assert(new PlainTime().subtract({ nanoseconds: 1 }).toString(), "23:59:59.999999999");
    assert(new PlainTime(12).add({ years: 1, months: 2, weeks: 3, days: 4 }).toString(), "12:00:00");
    const huge = 18446744073709551616;
    const remainder = BigInt(huge) % 86400000000000n;
    const hour = Number(remainder / 3600000000000n);
    const minute = Number(remainder / 60000000000n % 60n);
    const second = Number(remainder / 1000000000n % 60n);
    const ms = Number(remainder / 1000000n % 1000n);
    const us = Number(remainder / 1000n % 1000n);
    const ns = Number(remainder % 1000n);
    assertArrayEquals(fields(new PlainTime().add({ nanoseconds: huge })),
                      [hour, minute, second, ms, us, ns]);
    assert(new PlainTime(12).until("14:30").toString(), "PT2H30M");
    assert(new PlainTime(12).since("14:30").toString(), "-PT2H30M");
    assert(new PlainTime(12).until("14:30", { largestUnit: "minute" }).minutes, 150);
    assert(PlainTime.from("00:00:00.000000001").until("23:59:59.999999999",
           { largestUnit: "nanosecond" }).nanoseconds, 86399999999998);
    assert(new PlainTime(12).since("14:30", { smallestUnit: "hour", roundingMode: "ceil" }).hours, -2);
    assert(new PlainTime(12).until("14:30", { smallestUnit: "hour", roundingMode: "ceil" }).hours, 3);
    assertThrows(RangeError, () => time.until("12:00", { largestUnit: "day" }));
    assertThrows(RangeError, () => time.until("12:00", { smallestUnit: "minute", roundingIncrement: 7 }));
    assert(time.equals("23:59:59.999999999"), true);
    assert(time.equals("23:59:59.999999998"), false);
    assert(PlainTime.compare("12:00", "12:01"), -1);
    assert(PlainTime.compare("12:00", "12:00"), 0);
    assert(PlainTime.compare("12:01", "12:00"), 1);
}

function test_time_with_round_and_format() {
    const time = PlainTime.from("12:34:56.123456789");
    assert(time.with({ minute: 60 }).toString(), "12:59:56.123456789");
    assertThrows(RangeError, () => time.with({ minute: 60 }, { overflow: "reject" }));
    for (const input of [{}, { calendar: "iso8601", minute: 1 }, { timeZone: "UTC", minute: 1 },
                         time, new Temporal.PlainDate(2024, 1, 1), "12:00"])
        assertThrows(TypeError, () => time.with(input));
    const log = [];
    time.with(new Proxy({ minute: 1 }, { get(target, key) { log.push(key); return target[key]; } }));
    assertArrayEquals(log, ["calendar", "timeZone", "hour", "microsecond", "millisecond",
                           "minute", "nanosecond", "second"]);
    assert(time.round("minute").toString(), "12:35:00");
    assert(PlainTime.from("23:59:59.999999999").round("second").toString(), "00:00:00");
    assert(PlainTime.from("12:30").round({ smallestUnit: "hour", roundingMode: "halfEven" }).hour, 12);
    assert(PlainTime.from("13:30").round({ smallestUnit: "hour", roundingMode: "halfEven" }).hour, 14);
    assertThrows(TypeError, () => time.round());
    assertThrows(RangeError, () => time.round("day"));
    assertThrows(RangeError, () => time.round({ smallestUnit: "hour", roundingIncrement: 24 }));
    assertThrows(RangeError, () => time.round({ smallestUnit: "minute", roundingIncrement: 7 }));
    assert(time.toString({ fractionalSecondDigits: 4 }), "12:34:56.1234");
    assert(time.toString({ smallestUnit: "minute", roundingMode: "ceil" }), "12:35");
    assert(time.toString({ fractionalSecondDigits: "auto" }), "12:34:56.123456789");
    assertThrows(RangeError, () => time.toString({ smallestUnit: "hour" }));
    assertThrows(RangeError, () => time.toString({ fractionalSecondDigits: 10 }));
    assert(time.toJSON({ smallestUnit: "minute" }), time.toString());
    if (typeof Intl === "undefined")
        assert(time.toLocaleString(Symbol(), null), time.toString());
    else {
        Object.defineProperty(time, "hour", {
            get() { throw new Error("native PlainTime slots must be used"); }
        });
        const formatted = time.toLocaleString("en", { hour: "numeric" });
        assert(typeof formatted, "string");
        assert(formatted.length > 0, true);
    }
    const optionsLog = [];
    time.round(new Proxy({ smallestUnit: "minute" }, {
        get(target, key) { optionsLog.push(key); return target[key]; }
    }));
    assertArrayEquals(optionsLog, ["roundingIncrement", "roundingMode", "smallestUnit"]);
}

test_time_creation();
test_time_conversion();
test_time_strings();
test_time_arithmetic();
test_time_with_round_and_format();
