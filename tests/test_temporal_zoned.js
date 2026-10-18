/* Native ZonedDateTime units: UTC and numeric zones require no Intl backend. */
"use strict";

function assert(actual, expected, message = "ZonedDateTime") {
    if (!Object.is(actual, expected))
        throw new Error(message + ": " + String(actual) + " !== " + String(expected));
}
function assert_throws(type, callback) {
    try { callback(); } catch (error) {
        if (error instanceof type) return;
        throw error;
    }
    throw new Error("Expected " + type.name);
}
const ZDT = Temporal.ZonedDateTime;
const nsHour = 3600000000000n;
const limit = 8640000000000000000000n;

function test_metadata_and_brands() {
    assert(ZDT.name, "ZonedDateTime");
    assert(ZDT.length, 2);
    assert(ZDT.from.length, 1);
    assert(ZDT.compare.length, 2);
    assert(Object.getPrototypeOf(ZDT), Function.prototype);
    assert(Object.getPrototypeOf(ZDT.prototype), Object.prototype);
    const ctorProto = Object.getOwnPropertyDescriptor(ZDT, "prototype");
    assert(ctorProto.writable, false);
    assert(ctorProto.enumerable, false);
    assert(ctorProto.configurable, false);
    const zdt = new ZDT(0n, "UTC");
    assert(Object.prototype.toString.call(zdt), "[object Temporal.ZonedDateTime]");
    const getters = ["calendarId", "timeZoneId", "era", "eraYear", "year", "month",
        "monthCode", "day", "hour", "minute", "second", "millisecond", "microsecond",
        "nanosecond", "epochMilliseconds", "epochNanoseconds", "dayOfWeek", "dayOfYear",
        "weekOfYear", "yearOfWeek", "hoursInDay", "daysInWeek", "daysInMonth",
        "daysInYear", "monthsInYear", "inLeapYear", "offsetNanoseconds", "offset"];
    const bad = [null, undefined, {}, ZDT.prototype, Object.create(ZDT.prototype),
                 new Proxy(zdt, {}), new Temporal.Instant(0n)];
    for (const name of getters) {
        const descriptor = Object.getOwnPropertyDescriptor(ZDT.prototype, name);
        assert(descriptor.get.name, "get " + name);
        assert(descriptor.get.length, 0);
        assert(descriptor.set, undefined);
        assert(descriptor.enumerable, false);
        assert(descriptor.configurable, true);
        for (const receiver of bad)
            assert_throws(TypeError, () => descriptor.get.call(receiver));
    }
    const methods = {with: 1, withPlainTime: 0, withTimeZone: 1, withCalendar: 1,
        add: 1, subtract: 1, until: 1, since: 1, round: 1, equals: 1, toString: 0,
        toLocaleString: 0, toJSON: 0, valueOf: 0, startOfDay: 0,
        getTimeZoneTransition: 1, toInstant: 0, toPlainDate: 0, toPlainTime: 0,
        toPlainDateTime: 0};
    const poison = new Proxy({}, {get() {throw new Error("read before brand");}});
    for (const [name, length] of Object.entries(methods)) {
        const descriptor = Object.getOwnPropertyDescriptor(ZDT.prototype, name);
        assert(descriptor.value.name, name);
        assert(descriptor.value.length, length);
        assert(descriptor.writable, true);
        assert(descriptor.enumerable, false);
        assert(descriptor.configurable, true);
        for (const receiver of bad)
            assert_throws(TypeError, () => descriptor.value.call(receiver, poison));
        assert_throws(TypeError, () => new descriptor.value());
    }
    assert_throws(TypeError, () => ZDT(0n, "UTC"));
    assert_throws(TypeError, () => +zdt);
    class Sub extends ZDT {}
    const sub = new Sub(0n, "UTC");
    assert(sub instanceof Sub, true);
    for (const result of [ZDT.from(sub), sub.add({nanoseconds: 1}), sub.round("second"),
                          sub.with({minute: 1}), sub.withPlainTime(), sub.startOfDay()])
        assert(Object.getPrototypeOf(result), ZDT.prototype);
}

function test_construction_and_conversion() {
    for (const epoch of [0n, 1n, -1n, 1n << 64n, -(1n << 64n), limit, -limit]) {
        const zdt = new ZDT(epoch, "UTC");
        assert(zdt.epochNanoseconds, epoch);
        let milliseconds = epoch / 1000000n;
        if (epoch < 0n && epoch % 1000000n) milliseconds--;
        assert(zdt.epochMilliseconds, Number(milliseconds));
        assert(ZDT.from(zdt).equals(zdt), true);
        assert(ZDT.from(zdt) === zdt, false);
        assert(ZDT.from(zdt.toString()).epochNanoseconds, epoch);
    }
    for (const epoch of [limit + 1n, -limit - 1n, 1n << 200n])
        assert_throws(RangeError, () => new ZDT(epoch, "UTC"));
    for (const epoch of [undefined, null, 1, Symbol()])
        assert_throws(TypeError, () => new ZDT(epoch, "UTC"));
    assert(new ZDT(true, "UTC").epochNanoseconds, 1n);
    assert(new ZDT("-1000001", "UTC").epochMilliseconds, -2);
    const order = [];
    const epoch = {[Symbol.toPrimitive](hint) {order.push(hint); return 0n;}};
    assert(new ZDT(epoch, "uTc", "ISO8601").timeZoneId, "UTC");
    assert(order.join(), "number");
    for (const zone of [undefined, {}, new String("UTC"), null])
        assert_throws(TypeError, () => new ZDT(0n, zone));
    for (const zone of ["+01:00:00", "1970-01-01[UTC]", "UTC\0junk", "+24:00"])
        assert_throws(RangeError, () => new ZDT(0n, zone));
    assert_throws(TypeError, () => new ZDT(0n, "UTC", {}));
    assert_throws(RangeError, () => new ZDT(0n, "UTC", "1970-01-01[u-ca=iso8601]"));
    assert_throws(RangeError, () => new ZDT(0n, "UTC", "unknown"));
    assert(new ZDT(0n, "+0530").timeZoneId, "+05:30");
    assert(new ZDT(0n, "-00:00").timeZoneId, "+00:00");
    for (const item of [undefined, null, true, 0n, 1, Symbol(), {}, () => {}])
        assert_throws(TypeError, () => ZDT.from(item));
    for (const text of ["1970-01-01T00:00Z", "1970-01-01T00:00+00:00",
                        "1970-01-01T00:00[UTC][!unknown=value]", "1970-01-01[UTC]junk"])
        assert_throws(RangeError, () => ZDT.from(text));
    assert(ZDT.from("1970-01-01[UTC]").epochNanoseconds, 0n);
    assert(ZDT.from("1970-01-01T01:00+01:00[+01:00]").epochNanoseconds, 0n);
    assert(ZDT.from("1970-01-01T01:00Z[+01:00]").epochNanoseconds, nsHour);
    assert(ZDT.from("1970-01-01T00:00+01:00[UTC]", {offset: "use"}).epochNanoseconds, -nsHour);
    assert(ZDT.from("1970-01-01T00:00+01:00[UTC]", {offset: "ignore"}).epochNanoseconds, 0n);
    assert_throws(RangeError, () => ZDT.from("1970-01-01T00:00+01:00[UTC]"));
    assert(ZDT.from({year: 1970, monthCode: "M01", day: 1, timeZone: "UTC"}).epochNanoseconds, 0n);
    assert(ZDT.from({year: 1970, month: 1, day: 99, timeZone: "UTC"}).day, 31);
    assert_throws(RangeError, () => ZDT.from({year: 1970, month: 1, day: 99, timeZone: "UTC"},
                                          {overflow: "reject"}));
    assert_throws(RangeError, () => ZDT.from({year: 1970, month: 1, monthCode: "M02",
                                           day: 1, timeZone: "UTC"}));
    const x = new ZDT(0n, "UTC");
    assert(x.equals(new ZDT(1n, "UTC")), false);
    assert(x.equals(new ZDT(0n, "+00:00")), false);
    assert(new ZDT(0n, "+00:00").equals(new ZDT(0n, "-00:00")), true);
    assert_throws(RangeError, () => ZDT.from(x, {disambiguation: "bad"}));
    assert_throws(RangeError, () => ZDT.from(x, {offset: "bad"}));
    assert_throws(RangeError, () => ZDT.from(x, {overflow: "bad"}));
    assert_throws(TypeError, () => ZDT.from(x, null));
    Object.defineProperty(x, "epochNanoseconds", {get() {throw new Error("slot accessor");}});
    Object.defineProperty(x, "timeZoneId", {get() {throw new Error("zone accessor");}});
    assert(ZDT.from(x).epochNanoseconds, 0n);
    assert(ZDT.compare(x, new ZDT(1n, "+00:00")), -1);
    const date = new Temporal.PlainDate(2020, 2, 29);
    Object.defineProperty(date, "calendar", {get() {throw new Error("calendar shadow");}});
    date.timeZone = "UTC";
    assert(ZDT.from(date).toPlainDate().toString(), "2020-02-29");
}

function test_getters_and_plain_results() {
    const z = new ZDT(123456789n, "+05:30");
    const expected = {calendarId: "iso8601", timeZoneId: "+05:30", era: undefined,
        eraYear: undefined, year: 1970, month: 1, monthCode: "M01", day: 1, hour: 5,
        minute: 30, second: 0, millisecond: 123, microsecond: 456, nanosecond: 789,
        epochMilliseconds: 123, epochNanoseconds: 123456789n, dayOfWeek: 4, dayOfYear: 1,
        weekOfYear: 1, yearOfWeek: 1970, hoursInDay: 24, daysInWeek: 7, daysInMonth: 31,
        daysInYear: 365, monthsInYear: 12, inLeapYear: false,
        offsetNanoseconds: 19800000000000, offset: "+05:30"};
    for (const [name, value] of Object.entries(expected)) assert(z[name], value, name);
    assert(z.toInstant().epochNanoseconds, 123456789n);
    assert(z.toPlainDate().toString(), "1970-01-01");
    assert(z.toPlainTime().toString(), "05:30:00.123456789");
    assert(z.toPlainDateTime().toString(), "1970-01-01T05:30:00.123456789");
    assert(ZDT.from("2020-02-29[UTC]").inLeapYear, true);
    assert(ZDT.from("2020-02-29[UTC]").daysInYear, 366);
    assert(z.startOfDay().toString(), "1970-01-01T00:00:00+05:30[+05:30]");
    assert(z.withPlainTime().equals(z.startOfDay()), true);
    assert(z.withPlainTime("12:34").hour, 12);
    assert(z.withTimeZone("UTC").epochNanoseconds, z.epochNanoseconds);
    assert(z.withTimeZone("UTC").hour, 0);
    assert(z.withCalendar("iso8601").epochNanoseconds, z.epochNanoseconds);
    assert(z.with({minute: 45}).minute, 45);
    assert(z.with({hour: 100}).hour, 23);
    assert_throws(RangeError, () => z.with({hour: 100}, {overflow: "reject"}));
    for (const partial of [{}, "12:34", {calendar: "iso8601"}, {timeZone: "UTC"},
                           z, new Temporal.PlainDate(2000, 1, 1)])
        assert_throws(TypeError, () => z.with(partial));
    const retained = new ZDT(0n, "+05:30");
    const pressure = [];
    for (let i = 0; i < 20000; i++) pressure.push({a: String(i), b: new ZDT(BigInt(i), "UTC")});
    assert(retained.timeZoneId, "+05:30");
    assert(retained.toString(), "1970-01-01T05:30:00+05:30[+05:30]");
}

function test_arithmetic_and_rounding() {
    const a = ZDT.from("2020-01-31T12:30[UTC]");
    assert(a.add({months: 1}).toString(), "2020-02-29T12:30:00+00:00[UTC]");
    assert(a.subtract({months: 1}).toString(), "2019-12-31T12:30:00+00:00[UTC]");
    assert_throws(RangeError, () => a.add({months: 1}, {overflow: "reject"}));
    assert(a.add({weeks: 1, days: 1, nanoseconds: 1}).toString(),
           "2020-02-08T12:30:00.000000001+00:00[UTC]");
    const b = a.add({days: 2, hours: 3, minutes: 4, nanoseconds: 5});
    assert(a.until(b).hours, 51);
    assert(a.until(b).minutes, 4);
    assert(a.until(b).nanoseconds, 5);
    assert(a.until(b, {largestUnit: "day"}).days, 2);
    assert(a.until(b, {largestUnit: "day"}).hours, 3);
    assert(b.since(a, {largestUnit: "day"}).days, 2);
    assert(a.since(b, {largestUnit: "day"}).days, -2);
    const otherZone = b.withTimeZone("+01:00");
    assert(a.until(otherZone).hours, 51);
    assert_throws(RangeError, () => a.until(otherZone, {largestUnit: "day"}));
    assert(new ZDT(0n, "UTC").until(new ZDT(0n, "+01:00")).blank, true);
    assert_throws(RangeError, () => new ZDT(0n, "UTC").until(new ZDT(0n, "+01:00"),
                                                          {largestUnit: "day"}));
    const before = new ZDT(-1000000000000000000n, "UTC");
    assert(before.round({smallestUnit: "hour", roundingMode: "trunc"}).epochNanoseconds,
           -1000000800000000000n);
    assert(before.round({smallestUnit: "hour", roundingMode: "expand"}).epochNanoseconds,
           -999997200000000000n);
    const clock = ZDT.from("2020-05-02T12:30[+05:45]");
    assert(clock.round("hour").hour, 13);
    assert(clock.round("hour").minute, 0);
    assert(clock.round({smallestUnit: "hour", roundingMode: "halfEven"}).hour, 12);
    assert(clock.round("day").day, 3);
    assert(clock.round("nanosecond").equals(clock), true);
    assert(clock.round("nanosecond") === clock, false);
    for (const smallestUnit of ["year", "month", "week", "auto"])
        assert_throws(RangeError, () => clock.round({smallestUnit}));
    for (const roundingIncrement of [0, 7, 24, Infinity, 1000000001])
        assert_throws(RangeError, () => clock.round({smallestUnit: "hour", roundingIncrement}));
    assert_throws(TypeError, () => clock.round());
    assert_throws(TypeError, () => clock.round(null));
    assert_throws(RangeError, () => clock.round({}));
    assert_throws(RangeError, () => clock.round({smallestUnit: "day", roundingIncrement: 2}));
    assert_throws(RangeError, () => new ZDT(limit, "UTC").add({nanoseconds: 1}));
}

function test_formatting_and_transition_options() {
    const z = new ZDT(123456789n, "+05:30");
    assert(z.toString(), "1970-01-01T05:30:00.123456789+05:30[+05:30]");
    assert(z.toJSON(), z.toString());
    assert(JSON.stringify(z), JSON.stringify(z.toString()));
    assert(z.toString({calendarName: "critical", timeZoneName: "critical"}),
           "1970-01-01T05:30:00.123456789+05:30[!+05:30][!u-ca=iso8601]");
    assert(z.toString({offset: "never", timeZoneName: "never", smallestUnit: "minute"}),
           "1970-01-01T05:30");
    assert(z.toString({fractionalSecondDigits: 2.9}), "1970-01-01T05:30:00.12+05:30[+05:30]");
    assert(z.toString({smallestUnit: "microseconds", roundingMode: "ceil"}),
           "1970-01-01T05:30:00.123457+05:30[+05:30]");
    assert(new ZDT(-1n, "UTC").toString({fractionalSecondDigits: 0}),
           "1969-12-31T23:59:59+00:00[UTC]");
    for (const option of [{calendarName: "bad"}, {timeZoneName: "bad"}, {offset: "use"},
                          {smallestUnit: "hour"}, {fractionalSecondDigits: "9"}])
        assert_throws(RangeError, () => z.toString(option));
    assert_throws(TypeError, () => z.toString(null));
    assert(z.getTimeZoneTransition("next"), null);
    assert(z.getTimeZoneTransition({direction: "previous"}), null);
    assert(new ZDT(0n, "UTC").getTimeZoneTransition("next"), null);
    assert_throws(TypeError, () => z.getTimeZoneTransition());
    assert_throws(RangeError, () => z.getTimeZoneTransition({}));
    assert_throws(RangeError, () => z.getTimeZoneTransition("bad"));
    assert(typeof z.toLocaleString(), "string");
    if (typeof Intl === "undefined") assert(z.toLocaleString(), z.toString());
}

function test_observable_order_and_abrupt_completion() {
    const log = [];
    const observe = (object, label) => new Proxy(object, {get(target, name, receiver) {
        log.push(label + "." + String(name)); return Reflect.get(target, name, receiver);
    }});
    const fields = observe({year: 1970, month: 1, day: 1, timeZone: "UTC"}, "fields");
    const options = observe({disambiguation: "compatible", offset: "reject", overflow: "constrain"}, "options");
    ZDT.from(fields, options);
    assert(log.join(), ["fields.calendar", "fields.day", "fields.hour", "fields.microsecond",
        "fields.millisecond", "fields.minute", "fields.month", "fields.monthCode",
        "fields.nanosecond", "fields.offset", "fields.second", "fields.timeZone", "fields.year",
        "options.disambiguation", "options.offset", "options.overflow"].join());
    log.length = 0;
    const z = new ZDT(0n, "UTC");
    ZDT.from(z, options);
    assert(log.join(), "options.disambiguation,options.offset,options.overflow");
    log.length = 0;
    z.round(observe({smallestUnit: "second"}, "round"));
    assert(log.join(), "round.roundingIncrement,round.roundingMode,round.smallestUnit");
    log.length = 0;
    z.toString(observe({}, "string"));
    assert(log.join(), "string.calendarName,string.fractionalSecondDigits,string.offset," +
                       "string.roundingMode,string.smallestUnit,string.timeZoneName");
    log.length = 0;
    z.with(observe({minute: 1}, "partial"), options);
    assert(log.join(), ["partial.calendar", "partial.timeZone", "partial.day", "partial.hour",
        "partial.microsecond", "partial.millisecond", "partial.minute", "partial.month",
        "partial.monthCode", "partial.nanosecond", "partial.offset", "partial.second",
        "partial.year", "options.disambiguation", "options.offset", "options.overflow"].join());
    log.length = 0;
    z.getTimeZoneTransition(observe({direction: "next"}, "transition"));
    assert(log.join(), "transition.direction");
    const sentinel = {};
    const early = {get day() {throw sentinel;}, get year() {throw new Error("later field");}};
    try { ZDT.from(early); throw new Error("Expected sentinel"); }
    catch (error) {assert(error, sentinel);}
    const invalid = {year: 1970, month: 1, day: 99, timeZone: "UTC"};
    try { ZDT.from(invalid, {get overflow() {throw sentinel;}}); throw new Error("Expected sentinel"); }
    catch (error) {assert(error, sentinel);}
    try { z.with({hour: 99}, {get overflow() {throw sentinel;}}); throw new Error("Expected sentinel"); }
    catch (error) {assert(error, sentinel);}
    try { z.round({get roundingIncrement() {throw sentinel;},
                   get smallestUnit() {throw new Error("later option");}});
          throw new Error("Expected sentinel"); }
    catch (error) {assert(error, sentinel);}
}

test_metadata_and_brands();
test_construction_and_conversion();
test_getters_and_plain_results();
test_arithmetic_and_rounding();
test_formatting_and_transition_options();
test_observable_order_and_abrupt_completion();
