/*
 * Temporal.Duration tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import { assert, assertThrows, assertArrayEquals } from "./assert.js";

const Duration = Temporal.Duration;
const names = ["years", "months", "weeks", "days", "hours", "minutes",
               "seconds", "milliseconds", "microseconds", "nanoseconds"];

function fields(duration) {
    return names.map(name => duration[name]);
}

function check(duration, expected) {
    assertArrayEquals(fields(duration), expected);
}

function test_metadata_and_creation() {
    assert(Duration.length, 0);
    assert(Duration.name, "Duration");
    assert(Duration.from.length, 1);
    assert(Duration.compare.length, 2);
    assertThrows(TypeError, () => Duration());
    assertThrows(TypeError, () => +new Duration());
    check(new Duration(), Array(10).fill(0));
    check(new Duration(-0, -0, -0, -0, -0, -0, -0, -0, -0, -0),
          Array(10).fill(0));
    assert(new Duration().blank, true);
    assert(new Duration().sign, 0);
    assert(new Duration(1).sign, 1);
    assert(new Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, -1).sign, -1);
    assert(Object.prototype.toString.call(new Duration()), "[object Temporal.Duration]");
    for (const name of names.concat(["sign", "blank"])) {
        const descriptor = Object.getOwnPropertyDescriptor(Duration.prototype, name);
        assert(descriptor.get.length, 0);
        assert(descriptor.get.name, "get " + name);
        assert(descriptor.set, undefined);
        assert(descriptor.enumerable, false);
        assert(descriptor.configurable, true);
        assertThrows(TypeError, () => descriptor.get.call({}));
    }
    class Derived extends Duration {}
    assert(Object.getPrototypeOf(new Derived(1)), Derived.prototype);
    const derived = new Derived(1);
    assert(Object.getPrototypeOf(Duration.from(derived)), Duration.prototype);
    assertThrows(RangeError, () => new Duration(1.5));
    assertThrows(RangeError, () => new Duration(Infinity));
    assertThrows(RangeError, () => new Duration(NaN));
    assertThrows(TypeError, () => new Duration(1n));
    assertThrows(RangeError, () => new Duration(1, -1));
    assertThrows(RangeError, () => new Duration(4294967296));
    assertThrows(RangeError, () => new Duration(0, 0, 0, 0, 0, 0, 2 ** 53));
    assertThrows(RangeError, () => new Duration(0, 0, 0, 0, 0, 0, -(2 ** 53)));
    const maximum = new Duration(0, 0, 0, 104249991374, 7, 36, 31, 999, 999, 999);
    assert(maximum.total("seconds"), 2 ** 53);
    assertThrows(RangeError, () => maximum.add({ nanoseconds: 1 }));
    const order = [];
    const values = names.map(name => ({ valueOf() { order.push(name); return 1; } }));
    new Duration(...values);
    assertArrayEquals(order, names);
}

function test_duration_conversion() {
    const expected = ["days", "hours", "microseconds", "milliseconds",
                      "minutes", "months", "nanoseconds", "seconds", "weeks", "years"];
    const log = [];
    const bag = new Proxy(Object.fromEntries(names.map(name => [name, {
        valueOf() { log.push("value " + name); return 1; }
    }])), { get(target, name, receiver) { log.push("get " + name); return Reflect.get(target, name, receiver); } });
    check(Duration.from(bag), Array(10).fill(1));
    assertArrayEquals(log, expected.flatMap(name => ["get " + name, "value " + name]));
    const native = new Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    Object.defineProperty(native, "years", { get() { throw new Error("unobservable"); } });
    check(Duration.from(native), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    assertThrows(TypeError, () => Duration.from({}));
    assertThrows(TypeError, () => Duration.from(null));
    assertThrows(TypeError, () => Duration.from(1));
    assertThrows(TypeError, () => Duration.from({ toString() { return "PT1S"; } }));
    check(new Duration(1, 2).with({ years: 3 }), [3, 2, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertThrows(TypeError, () => native.with({}));
    assertThrows(RangeError, () => new Duration(1).with({ months: -1 }));
    check(new Duration(-1, -2).abs(), [1, 2, 0, 0, 0, 0, 0, 0, 0, 0]);
    check(new Duration(-1, -2).negated(), [1, 2, 0, 0, 0, 0, 0, 0, 0, 0]);
}

function test_duration_strings() {
    check(Duration.from("PT0.999999999H"), [0, 0, 0, 0, 0, 59, 59, 999, 996, 400]);
    check(Duration.from("PT0.999999999M"), [0, 0, 0, 0, 0, 0, 59, 999, 999, 940]);
    check(Duration.from("PT46H66M71.50040904S"), [0, 0, 0, 0, 46, 66, 71, 500, 409, 40]);
    assert(Duration.from("-P1Y2M3W4DT5H6M7.008009010S").toString(),
           "-P1Y2M3W4DT5H6M7.00800901S");
    assert(Duration.from("p1dt2h").toString(), "P1DT2H");
    assert(Duration.from("PT0S").toString(), "PT0S");
    assert(Duration.from("-PT0S").toString(), "PT0S");
    assert(Duration.from("PT1,123456789S").toString(), "PT1.123456789S");
    for (const text of ["", "P", "PT", "P1DT", "P1.1D", "PT1.5H1M",
                        "PT1.5M1S", "PT1.1234567890S", "P1Y1Y", "P1D1Y",
                        "PT1S1M", " PT1S", "PT1S ", "−PT1S", "PT1e3S"])
        assertThrows(RangeError, () => Duration.from(text));
    const duration = Duration.from("P1DT23H59M59.999999999S");
    assert(duration.toString({ smallestUnit: "second", roundingMode: "ceil" }),
           "P2DT0S");
    assert(Duration.from("PT1.123456789S").toString({ fractionalSecondDigits: 4 }),
           "PT1.1234S");
    assert(new Duration(0, 0, 0, 0, 1).toString({ fractionalSecondDigits: 3 }),
           "PT1H0.000S");
    assertThrows(RangeError, () => duration.toString({ smallestUnit: "minute" }));
    assertThrows(RangeError, () => duration.toString({ smallestUnit: "auto" }));
    assert(duration.toJSON({ bad: 1 }), duration.toString());
    if (typeof Intl === "undefined")
        assert(duration.toLocaleString(Symbol(), null), duration.toString());
}

/* The divisor has only factors 2, 3, and 5 and is below 10^14. Eighty
   decimal places distinguish adjacent binary64 values throughout the
   duration range and represent every exactly terminating tie. */
function exactRatioNumber(numerator, divisor) {
    if (numerator === 0n)
        return 0;
    const negative = numerator < 0n;
    if (negative)
        numerator = -numerator;
    const integer = numerator / divisor;
    const fraction = ((numerator % divisor) * 10n ** 80n / divisor).toString().padStart(80, "0");
    return Number((negative ? "-" : "") + integer + "." + fraction);
}

function test_duration_exact_totals() {
    const units = ["day", "hour", "minute", "second", "millisecond", "microsecond", "nanosecond"];
    const lengths = [86400000000000n, 3600000000000n, 60000000000n,
                     1000000000n, 1000000n, 1000n, 1n];
    const cases = [
        [9007199, 254, 740, 993],
        [9007199254740991, 999, 999, 999],
        [18014398, 508, 481, 987],
        [1, 0, 0, 1],
        [0, 0, 0, 1],
    ];
    for (const [seconds, ms, us, ns] of cases) {
        for (const sign of [1, -1]) {
            const d = new Duration(0, 0, 0, 0, 0, 0, sign * seconds,
                                   sign * ms, sign * us, sign * ns);
            const exact = BigInt(sign) * (BigInt(seconds) * 1000000000n +
                          BigInt(ms) * 1000000n + BigInt(us) * 1000n + BigInt(ns));
            for (let i = 0; i < units.length; i++)
                assert(d.total(units[i]), exactRatioNumber(exact, lengths[i]));
        }
    }
    const wide = Duration.from({ nanoseconds: 18446744073709551616 });
    assert(wide.total("nanoseconds"), 18446744073709551616);
    assert(wide.total("seconds"), exactRatioNumber(18446744073709551616n, 1000000000n));
    assert(new Duration().total("seconds"), 0);
}

function test_duration_rounding() {
    const positive = Duration.from("PT2.5S");
    const expectations = { ceil: 3, floor: 2, expand: 3, trunc: 2,
                           halfCeil: 3, halfFloor: 2, halfExpand: 3,
                           halfTrunc: 2, halfEven: 2 };
    for (const [roundingMode, seconds] of Object.entries(expectations)) {
        assert(positive.round({ smallestUnit: "second", roundingMode }).seconds, seconds);
        const negative = roundingMode === "ceil" || roundingMode === "halfCeil" ? -2 :
                         roundingMode === "floor" || roundingMode === "halfFloor" ? -3 : -seconds;
        assert(positive.negated().round({ smallestUnit: "second", roundingMode }).seconds, negative);
    }
    assert(Duration.from("PT3.5S").round({ smallestUnit: "second", roundingMode: "halfEven" }).seconds, 4);
    check(Duration.from("PT25H").round({ largestUnit: "day" }),
          [0, 0, 0, 1, 1, 0, 0, 0, 0, 0]);
    assert(Duration.from("P500000000D").round({ largestUnit: "day", smallestUnit: "day",
           roundingIncrement: 1000000000 }).days, 1000000000);
    assertThrows(RangeError, () => positive.round({}));
    assertThrows(TypeError, () => positive.round());
    assertThrows(RangeError, () => positive.round({ smallestUnit: "minute", roundingIncrement: 7 }));
    assertThrows(RangeError, () => positive.round({ smallestUnit: "second", largestUnit: "nanosecond" }));
    assertThrows(RangeError, () => Duration.from("P1Y").total("days"));
    assertThrows(RangeError, () => Duration.from("P1Y").round("days"));
    assertThrows(RangeError, () => Duration.from("P1Y").add("P1Y"));
    check(Duration.from("PT1H").subtract("PT90M"), [0, 0, 0, 0, 0, -30, 0, 0, 0, 0]);
    assert(Duration.compare("PT60S", "PT1M"), 0);
    assert(Duration.compare("PT59S", "PT1M"), -1);
    assertThrows(TypeError, () => Duration.compare("PT1S", "PT1S", { relativeTo: 1 }));
    const log = [];
    const options = new Proxy({ largestUnit: "auto", smallestUnit: "second" },
        { get(target, key) { log.push(key); return target[key]; } });
    positive.round(options);
    assertArrayEquals(log, ["largestUnit", "relativeTo", "roundingIncrement",
                           "roundingMode", "smallestUnit"]);
}

function test_relative_duration() {
    const month = Duration.from("P1M");
    assert(month.total({ unit: "day", relativeTo: "2024-02-01" }), 29);
    assert(month.total({ unit: "day", relativeTo: "2023-02-01" }), 28);
    assert(Duration.compare("P1M", "P29D", { relativeTo: "2024-02-01" }), 0);
    assert(Duration.compare("P1M", "P29D", { relativeTo: "2023-02-01" }), -1);
    assert(Duration.from("P1Y").round({ largestUnit: "day", smallestUnit: "day",
           relativeTo: "2024-01-01" }).days, 366);
    const zdt = new Temporal.ZonedDateTime(0n, "+05:30");
    assert(Duration.from("P1D").total({ unit: "hour", relativeTo: zdt }), 24);
    assert(Duration.from("P1DT1H").round({ smallestUnit: "hour", relativeTo: zdt }).toString(), "P1DT1H");
    if (typeof Intl !== "undefined") {
        const spring = "2024-03-10T00:00[America/New_York]";
        assert(Duration.from("P1D").total({ unit: "hour", relativeTo: spring }), 23);
        assert(Duration.compare("P1D", "PT24H", { relativeTo: spring }), -1);
    }
}

function test_duration_intl_interoperability() {
    if (typeof Intl === "undefined")
        return;
    const duration = Duration.from("PT1.5S");
    Object.defineProperty(duration, "seconds", {
        get() { throw new Error("native Duration slots must be used"); }
    });
    const formatter = new Intl.DurationFormat("en", { style: "long" });
    const formatted = formatter.format(duration);
    assert(formatter.format("PT1.5S"), formatted);
    assert(formatter.formatToParts("PT1.5S").map(part => part.value).join(""), formatted);
    assert(duration.toLocaleString("en", { style: "long" }), formatted);
}

test_metadata_and_creation();
test_duration_conversion();
test_duration_strings();
test_duration_exact_totals();
test_duration_rounding();
test_relative_duration();
test_duration_intl_interoperability();
