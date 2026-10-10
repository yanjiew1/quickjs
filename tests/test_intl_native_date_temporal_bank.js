/* Prepared frontend integration fixture; not executed by this source packet.
 * Run only after the typed bank is wired to the JS DateTimeFormat frontend.
 * Locale assertions inspect semantic parts and preserve exact source tags.
 */
(function () {
    function check(value, message) { if (!value) throw Error(message || "assertion failed"); }
    function same(a, b, message) { check(a === b, message || String(a) + " !== " + String(b)); }
    function throws(type, callback) {
        let error; try { callback(); } catch (caught) { error = caught; }
        check(error instanceof type, "expected " + type.name); return error;
    }
    function part(parts, name) {
        for (let i = 0; i < parts.length; i++) if (parts[i].type === name) return parts[i].value;
    }
    function text(parts) {
        let value = "";
        for (let i = 0; i < parts.length; i++) value += parts[i].value;
        return value;
    }
    function fields(parts, expected) {
        for (let i = 0; i < expected.length; i++) check(part(parts, expected[i]) !== undefined, expected[i]);
    }
    function shared(parts) {
        for (let i = 0; i < parts.length; i++) same(parts[i].source, "shared");
    }
    const date = new Temporal.PlainDate(2024, 1, 2);
    const ym = new Temporal.PlainYearMonth(2024, 1);
    const md = new Temporal.PlainMonthDay(1, 2);
    const time = new Temporal.PlainTime(3, 4, 5, 678, 901, 234);
    const datetime = new Temporal.PlainDateTime(2024, 1, 2, 3, 4, 5, 678, 901, 234);
    const instant = new Temporal.Instant(0n);
    const iso = new Intl.DateTimeFormat("en-US", { calendar: "iso8601", timeZone: "UTC" });
    const gregory = new Intl.DateTimeFormat("en-US", { calendar: "gregory", timeZone: "UTC" });
    fields(gregory.formatToParts(0), ["year", "month", "day"]);
    fields(gregory.formatToParts(date), ["year", "month", "day"]);
    fields(iso.formatToParts(ym), ["year", "month"]);
    fields(iso.formatToParts(md), ["month", "day"]);
    fields(gregory.formatToParts(time), ["hour", "minute", "second"]);
    fields(gregory.formatToParts(datetime), ["year", "month", "day", "hour", "minute", "second"]);
    fields(gregory.formatToParts(instant), ["year", "month", "day", "hour", "minute", "second"]);
    throws(RangeError, () => gregory.format(ym));
    throws(RangeError, () => gregory.format(md));
    const gregorianDate = new Temporal.PlainDate(2024, 1, 2, "gregory");
    const gregorianDateTime = new Temporal.PlainDateTime(2024, 1, 2, 3, 4, 5, 0, 0, 0, "gregory");
    throws(RangeError, () => iso.format(gregorianDate));
    throws(RangeError, () => iso.format(gregorianDateTime));
    check(gregory.format(gregorianDate).length > 0);
    const hours = new Intl.DateTimeFormat("en-US", { timeZone: "UTC", hour: "numeric" });
    throws(TypeError, () => hours.format(date));
    throws(RangeError, () => hours.format(ym)); /* mismatch precedes nullable format */
    const isoHours = new Intl.DateTimeFormat("en-US", { calendar: "iso8601", timeZone: "UTC", hour: "numeric" });
    throws(TypeError, () => isoHours.format(ym));
    throws(TypeError, () => isoHours.format(md));
    check(isoHours.format(time).length > 0);
    check(isoHours.format(datetime).length > 0);
    const eraOnly = new Intl.DateTimeFormat("en-US", { calendar: "iso8601", timeZone: "UTC", era: "short" });
    fields(eraOnly.formatToParts(date), ["era", "year", "month", "day"]);
    fields(eraOnly.formatToParts(ym), ["era", "year", "month"]);
    same(part(eraOnly.formatToParts(time), "era"), undefined);
    const zoneOnly = new Intl.DateTimeFormat("en-US", { calendar: "iso8601", timeZone: "+13:00", timeZoneName: "short" });
    fields(zoneOnly.formatToParts(date), ["year", "month", "day"]);
    same(part(zoneOnly.formatToParts(date), "timeZoneName"), undefined);
    fields(zoneOnly.formatToParts(time), ["hour", "minute", "second"]);
    same(part(zoneOnly.formatToParts(time), "timeZoneName"), undefined);
    fields(zoneOnly.formatToParts(instant), ["year", "month", "day", "hour", "minute", "second", "timeZoneName"]);
    const dateStyle = new Intl.DateTimeFormat("en-US", { calendar: "iso8601", timeZone: "UTC", dateStyle: "full" });
    check(dateStyle.format(date).length > 0);
    fields(dateStyle.formatToParts(ym), ["year", "month"]);
    same(part(dateStyle.formatToParts(ym), "day"), undefined);
    same(part(dateStyle.formatToParts(ym), "weekday"), undefined);
    fields(dateStyle.formatToParts(md), ["month", "day"]);
    same(part(dateStyle.formatToParts(md), "year"), undefined);
    same(part(dateStyle.formatToParts(md), "era"), undefined);
    throws(TypeError, () => dateStyle.format(time));
    const timeStyle = new Intl.DateTimeFormat("en-US", { calendar: "iso8601", timeZone: "UTC", timeStyle: "full" });
    throws(TypeError, () => timeStyle.format(date));
    throws(TypeError, () => timeStyle.format(ym));
    throws(TypeError, () => timeStyle.format(md));
    fields(timeStyle.formatToParts(time), ["hour", "minute", "second"]);
    same(part(timeStyle.formatToParts(time), "timeZoneName"), undefined);
    check(timeStyle.format(datetime).length > 0);
    const combinedStyle = new Intl.DateTimeFormat("en-US", {
        calendar: "iso8601", timeZone: "UTC", dateStyle: "full", timeStyle: "full"
    });
    fields(combinedStyle.formatToParts(datetime), ["year", "month", "day", "hour", "minute", "second"]);
    same(part(combinedStyle.formatToParts(datetime), "timeZoneName"), undefined);
    fields(combinedStyle.formatToParts(instant), ["timeZoneName"]);
    /* Constructor observes every option once; formatting never reads it again. */
    const keys = ["weekday", "era", "year", "month", "day", "dayPeriod", "hour", "minute", "second",
                  "fractionalSecondDigits", "timeZoneName", "dateStyle", "timeStyle", "formatMatcher"];
    const counts = Object.create(null), opts = { calendar: "iso8601", timeZone: "UTC" };
    for (let i = 0; i < keys.length; i++) {
        const key = keys[i]; counts[key] = 0;
        Object.defineProperty(opts, key, { get() { counts[key]++; return undefined; } });
    }
    const snapshot = new Intl.DateTimeFormat("en-US", opts);
    for (let i = 0; i < keys.length; i++) same(counts[keys[i]], 1, keys[i]);
    const values = [date, ym, md, time, datetime, instant];
    for (let i = 0; i < values.length; i++) snapshot.format(values[i]);
    snapshot.formatRange(datetime, datetime);
    for (let i = 0; i < keys.length; i++) same(counts[keys[i]], 1, keys[i]);
    /* Internal slots bypass user getters, prototype methods and ToNumber. */
    const saved = Object.getOwnPropertyDescriptor(Temporal.PlainDateTime.prototype, "hour");
    Object.defineProperty(Temporal.PlainDateTime.prototype, "hour", {
        configurable: true, get() { throw Error("Temporal getter consulted"); }
    });
    Object.defineProperty(datetime, "valueOf", { value() { throw Error("Temporal coercion consulted"); }, configurable: true });
    try { check(snapshot.format(datetime).length > 0); }
    finally { Object.defineProperty(Temporal.PlainDateTime.prototype, "hour", saved); delete datetime.valueOf; }
    let ordinaryCalls = 0;
    const ordinary = Object.create(Temporal.PlainDate.prototype);
    ordinary.valueOf = function () { ordinaryCalls++; return 0; };
    same(gregory.format(ordinary), gregory.format(0)); same(ordinaryCalls, 1);
    /* Start/end conversion happens before Number clipping and type handling. */
    let log = "";
    const start = { valueOf() { log += "s"; return NaN; } };
    const end = { valueOf() { log += "e"; return 0; } };
    throws(RangeError, () => gregory.formatRange(start, end)); same(log, "se");
    log = "";
    const sentinel = new Error("end conversion");
    const throwingEnd = { valueOf() { log += "e"; throw sentinel; } };
    same(throws(Error, () => gregory.formatRange(start, throwingEnd)), sentinel); same(log, "se");
    log = "";
    const throwingStart = { valueOf() { log += "s"; throw sentinel; } };
    same(throws(Error, () => gregory.formatRange(throwingStart, end)), sentinel); same(log, "s");
    log = "";
    throws(TypeError, () => gregory.formatRange(date, end)); same(log, "e");
    log = "";
    throws(TypeError, () => gregory.formatRange(start, undefined)); same(log, "");
    throws(TypeError, () => gregory.formatRange(undefined, end)); same(log, "");
    throws(TypeError, () => gregory.formatRange(date, datetime));
    /* Slot checks precede argument conversion; accessor identity is cached. */
    same(gregory.format, gregory.format);
    log = "";
    throws(TypeError, () => Intl.DateTimeFormat.prototype.formatRange.call({}, start, end)); same(log, "");
    throws(TypeError, () => Intl.DateTimeFormat.prototype.formatRangeToParts.call({}, start, end)); same(log, "");
    throws(TypeError, () => Intl.DateTimeFormat.prototype.formatToParts.call({}, end)); same(log, "");
    const legacy = Object.create(Intl.DateTimeFormat.prototype);
    Intl.DateTimeFormat.call(legacy, "en-US", { timeZone: "UTC" });
    same(legacy.format, legacy.format); check(legacy.format(0).length > 0);
    throws(TypeError, () => Intl.DateTimeFormat.prototype.formatToParts.call(legacy, 0));
    check(gregory.format().length > 0); check(gregory.format(undefined).length > 0);
    check(gregory.formatToParts().length > 0);
    const exact = new Intl.DateTimeFormat("en-US", {
        calendar: "iso8601", timeZone: "+13:00", hourCycle: "h23", year: "numeric", month: "2-digit",
        day: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit", fractionalSecondDigits: 3
    });
    same(part(exact.formatToParts(datetime), "hour"), "03");
    same(part(exact.formatToParts(datetime), "fractionalSecond"), "678");
    same(exact.resolvedOptions().timeZone, "+13:00");
    const utc = new Intl.DateTimeFormat("en-US", {
        calendar: "iso8601", timeZone: "UTC", hourCycle: "h23", hour: "2-digit", minute: "2-digit",
        second: "2-digit", fractionalSecondDigits: 3
    });
    same(part(utc.formatToParts(new Temporal.Instant(-1n)), "second"), "59");
    same(part(utc.formatToParts(new Temporal.Instant(-1n)), "fractionalSecond"), "999");
    const zero = new Temporal.Instant(0n), subms = new Temporal.Instant(999999n);
    shared(utc.formatRangeToParts(zero, subms));
    const crossing = utc.formatRangeToParts(new Temporal.Instant(-1n), zero);
    check(crossing.some(p => p.source === "startRange")); check(crossing.some(p => p.source === "endRange"));
    const reversed = utc.formatRangeToParts(new Temporal.Instant(1000000000n), zero);
    check(reversed.some(p => p.source === "startRange")); check(reversed.some(p => p.source === "endRange"));
    for (let precision = 1; precision <= 2; precision++) {
        const f = new Intl.DateTimeFormat("en-US", { timeZone: "UTC", second: "numeric", fractionalSecondDigits: precision });
        shared(f.formatRangeToParts(new Temporal.Instant(101000001n), new Temporal.Instant(109999999n)));
    }
    /* Civil/reference date noon can exceed TimeClip without invalidating the value. */
    const upperDate = new Temporal.PlainDate(275760, 9, 13);
    const lowerDate = new Temporal.PlainDate(-271821, 4, 19);
    check(iso.format(upperDate).length > 0); check(iso.format(lowerDate).length > 0);
    const upperYearMonth = new Temporal.PlainYearMonth(275760, 9, "iso8601", 30);
    const lowerYearMonth = new Temporal.PlainYearMonth(-271821, 4, "iso8601", 1);
    check(iso.format(upperYearMonth).length > 0); check(iso.format(lowerYearMonth).length > 0);
    const upperDateTime = new Temporal.PlainDateTime(275760, 9, 13, 23, 59, 59, 999, 999, 999);
    check(exact.format(upperDateTime).length > 0);
    throws(RangeError, () => iso.format(8640000000000001));
    const zoned = instant.toZonedDateTimeISO("+03:00");
    throws(TypeError, () => gregory.format(zoned));
    throws(TypeError, () => gregory.formatRange(zoned, zoned));
    check(zoned.toLocaleString("en-US").length > 0);
    throws(TypeError, () => zoned.toLocaleString("en-US", { timeZone: "UTC" }));
    /* Fresh ordinary part objects have spec property order and concatenate
     * without invoking user-controlled Array.prototype.join.
     */
    const savedJoin = Array.prototype.join;
    Array.prototype.join = function () { throw Error("join consulted"); };
    let result, string;
    try {
        result = utc.formatRangeToParts(zero, new Temporal.Instant(1000000000n));
        string = utc.formatRange(zero, new Temporal.Instant(1000000000n));
    } finally { Array.prototype.join = savedJoin; }
    same(text(result), string);
    const again = utc.formatRangeToParts(zero, new Temporal.Instant(1000000000n));
    for (let i = 0; i < result.length; i++) {
        same(Object.getPrototypeOf(result[i]), Object.prototype);
        same(Object.keys(result[i]).join(","), "type,value,source"); check(result[i] !== again[i]);
    }
})();
