function assert(value) { if (!value) throw Error("assertion failed"); }
function throws(type, callback) {
    let caught; try { callback(); } catch (error) { caught = error; }
    assert(caught instanceof type);
}
let plain = new Temporal.PlainDateTime(2024, 1, 2, 3, 4, 5, 678);
let formatter = new Intl.DateTimeFormat("en-US", {
    timeZone: "+13:00", hourCycle: "h23", year: "numeric", month: "2-digit",
    day: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit",
    fractionalSecondDigits: 3
});
let parts = formatter.formatToParts(plain);
assert(parts.find(part => part.type === "hour").value === "03");
assert(parts.find(part => part.type === "fractionalSecond").value === "678");
let nativeHour = Object.getOwnPropertyDescriptor(
    Temporal.PlainDateTime.prototype, "hour");
Object.defineProperty(Temporal.PlainDateTime.prototype, "hour", {
    get() { throw Error("overridden getter read"); }, configurable: true
});
try { assert(formatter.format(plain).length > 0); }
finally { Object.defineProperty(Temporal.PlainDateTime.prototype, "hour", nativeHour); }
let date = new Temporal.PlainDate(2024, 1, 2);
let timeOnly = new Intl.DateTimeFormat("en", { hour: "numeric" });
throws(TypeError, () => timeOnly.format(date));
throws(TypeError, () => formatter.formatRange(date, plain));
let range = formatter.formatRangeToParts(plain, plain);
assert(range.every(part => part.source === "shared"));
let yearMonth = new Temporal.PlainYearMonth(2024, 1);
throws(RangeError, () => new Intl.DateTimeFormat("en-US").format(yearMonth));
assert(new Intl.DateTimeFormat("en-US", { calendar: "iso8601" })
    .format(yearMonth).length > 0);
let instant = new Temporal.Instant(-1n);
assert(formatter.format(instant).length > 0);
let zoned = instant.toZonedDateTimeISO("+03:00");
throws(TypeError, () => formatter.format(zoned));
assert(zoned.toLocaleString("en-US").length > 0);
throws(TypeError, () => zoned.toLocaleString("en-US", { timeZone: "UTC" }));
