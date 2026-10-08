/* Native DateTimeFormat/Date integration, enabled configuration only. */
function same(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw Error((message || "value") + ": " + actual + " !== " + expected);
}
function throws(C, callback) {
    try { callback(); } catch (e) {
        if (e instanceof C) return;
        throw e;
    }
    throw Error("expected " + C.name);
}
function values(parts, type) {
    return parts.filter(p => p.type === type).map(p => p.value).join("");
}
function join(parts) { return parts.map(p => p.value).join(""); }
const DTF = Intl.DateTimeFormat;
const date = Date.UTC(2020, 0, 2, 3, 4, 5, 678);
const basic = { timeZone: "UTC", calendar: "gregory", numberingSystem: "latn" };
const components = {
    year: "numeric", month: "2-digit", day: "2-digit", hour: "2-digit",
    minute: "2-digit", second: "2-digit", fractionalSecondDigits: 3,
    hourCycle: "h23", timeZoneName: "short"
};
const format = new DTF("en-US", { ...basic, ...components });
const parts = format.formatToParts(date);
same(join(parts), format.format(date), "parts reconstruct format");
same(values(parts, "year"), "2020");
same(values(parts, "month"), "01");
same(values(parts, "day"), "02");
same(values(parts, "hour"), "03");
same(values(parts, "minute"), "04");
same(values(parts, "second"), "05");
same(values(parts, "fractionalSecond"), "678");
same(format.format, format.format, "cached bound formatter");
same(format.format.length, 1);
same(format.format.name, "");
same(Object.prototype.hasOwnProperty.call(format.format, "prototype"), false);
throws(TypeError, () => new format.format());
throws(TypeError, () => format.format(1n));
throws(TypeError, () => format.format(Symbol()));
throws(RangeError, () => format.format(NaN));
throws(RangeError, () => format.format(Infinity));
throws(RangeError, () => format.format(8640000000000001));
throws(TypeError, () => DTF.prototype.formatToParts.call({}));
throws(TypeError, () => DTF.prototype.resolvedOptions.call({}));
throws(TypeError, () => Object.getOwnPropertyDescriptor(DTF.prototype, "format").get.call({}));

for (const [cycle, expected] of [["h11", "00"], ["h12", "12"], ["h23", "00"], ["h24", "24"]]) {
    const f = new DTF("en-US", { ...basic, hour: "2-digit", hourCycle: cycle });
    same(f.resolvedOptions().hourCycle, cycle);
    same(f.resolvedOptions().hour12, cycle === "h11" || cycle === "h12");
    same(values(f.formatToParts(0), "hour"), expected, cycle + " midnight");
}
for (const [digits, expected] of [[1, "6"], [2, "67"], [3, "678"]]) {
    const f = new DTF("en", { ...basic, second: "numeric", fractionalSecondDigits: digits });
    same(values(f.formatToParts(date), "fractionalSecond"), expected);
    same(f.resolvedOptions().fractionalSecondDigits, digits);
}
for (const [zone, expected] of [["+01", "+01:00"], ["-0230", "-02:30"],
                              ["+05:45", "+05:45"], ["-00", "+00:00"],
                              ["utc", "UTC"], ["Etc/GMT", "Etc/GMT"]]) {
    const f = new DTF("en", { ...basic, timeZone: zone });
    same(f.resolvedOptions().timeZone, expected);
}
/* Stage 4 CreateDateTimeFormat stores Identifier, not PrimaryIdentifier.
   Lookup fixes ASCII casing while preserving distinct IANA Link names. */
const zoneComponents = {
    year: "numeric", month: "long", day: "numeric", hour: "numeric",
    minute: "numeric"
};
for (const [input, identifier, equivalent] of [
    ["eTc/gMt", "Etc/GMT", "UTC"],
    ["eTc/uTc", "Etc/UTC", "UTC"],
    ["gmt", "GMT", "UTC"],
    ["aSiA/cAlCuTtA", "Asia/Calcutta", "Asia/Kolkata"],
    ["aSiA/kOlKaTa", "Asia/Kolkata", "Asia/Calcutta"],
    ["us/eastern", "US/Eastern", "America/New_York"]
]) {
    const explicit = new DTF("en", { ...zoneComponents, timeZone: input });
    const other = new DTF("en", { ...zoneComponents, timeZone: equivalent });
    same(explicit.resolvedOptions().timeZone, identifier, input + " identifier");
    same(other.resolvedOptions().timeZone, equivalent, equivalent + " identifier");
    for (const epoch of [0, date, Date.UTC(2020, 6, 2, 3, 4)]) {
        same(explicit.format(epoch), other.format(epoch), input + " same rules");
        same(join(explicit.formatToParts(epoch)), explicit.format(epoch));
    }
}
/* A Link and its target also share the localized display name. UTC and
   GMT names above are checked only for equal civil/clock output: their
   localized labels need not be identical despite matching offsets. */
const aliasDisplayOptions = { ...zoneComponents, timeZoneName: "long" };
const calcutta = new DTF("en", { ...aliasDisplayOptions, timeZone: "Asia/Calcutta" });
const kolkata = new DTF("en", { ...aliasDisplayOptions, timeZone: "Asia/Kolkata" });
same(calcutta.format(0), kolkata.format(0), "Link and target display names");
const primaryZones = Intl.supportedValuesOf("timeZone");
same(primaryZones.includes("UTC"), true, "UTC remains primary");
for (const alias of ["Etc/GMT", "Etc/UTC", "GMT", "Asia/Calcutta", "US/Eastern"])
    same(primaryZones.includes(alias), false, alias + " not enumerated as primary");

for (const zone of ["+24", "-2360", "+01:00:00", "+010000", "UTC\0",
                    "\u221201:00", "ACT", "AET", "PST", "not-a-zone"]) {
    throws(RangeError, () => new DTF("en", { ...basic, timeZone: zone }));
}
const offset = new DTF("en", { ...basic, timeZone: "+05:45", hour: "2-digit", minute: "2-digit", hourCycle: "h23" });
same(values(offset.formatToParts(0), "hour"), "05");
same(values(offset.formatToParts(0), "minute"), "45");

for (const dateStyle of ["full", "long", "medium", "short"]) {
    const f = new DTF("en", { ...basic, dateStyle, timeStyle: "short", hourCycle: "h23" });
    const r = f.resolvedOptions();
    same(r.dateStyle, dateStyle);
    same(r.timeStyle, "short");
    same(r.hourCycle, "h23");
    same(r.hour12, false);
    same("year" in r, false, "style hides component fields");
    same(join(f.formatToParts(date)), f.format(date));
}
throws(TypeError, () => new DTF("en", { ...basic, dateStyle: "short", year: "numeric" }));
throws(RangeError, () => new DTF("en", { ...basic, fractionalSecondDigits: 4 }));
throws(RangeError, () => new DTF("en", { ...basic, calendar: "bad_value" }));
throws(RangeError, () => new DTF("en", { ...basic, numberingSystem: "latn\0" }));
const overridden = new DTF("en-u-hc-h24", { ...basic, hour: "numeric", hour12: true, hourCycle: "h23" });
same(overridden.resolvedOptions().hour12, true);
same(overridden.resolvedOptions().locale.includes("hc-"), false);

const expectedOrder = ["localeMatcher", "calendar", "numberingSystem", "hour12",
    "hourCycle", "timeZone", "weekday", "era", "year", "month", "day",
    "dayPeriod", "hour", "minute", "second", "fractionalSecondDigits",
    "timeZoneName", "formatMatcher", "dateStyle", "timeStyle"];
let order = [];
const options = new Proxy({ ...basic, ...components }, {
    get(target, name, receiver) { order.push(name); return Reflect.get(target, name, receiver); }
});
new DTF("en", options);
same(order.join(","), expectedOrder.join(","), "option getter order and single access");
order = [];
new Date(0).toLocaleString("en", new Proxy({ timeZone: "UTC" }, {
    get(target, name) { order.push(name); return target[name]; }
}));
same(order.join(","), expectedOrder.join(","), "Date option getter order");

const all = { ...basic, year: "numeric", month: "numeric", day: "numeric",
              hour: "numeric", minute: "numeric", second: "numeric" };
same(new Date(date).toLocaleString("en", basic), new DTF("en", all).format(date));
same(new Date(date).toLocaleDateString("en", basic), new DTF("en", { ...basic, year: "numeric", month: "numeric", day: "numeric" }).format(date));
same(new Date(date).toLocaleTimeString("en", basic), new DTF("en", { ...basic, hour: "numeric", minute: "numeric", second: "numeric" }).format(date));
throws(TypeError, () => new Date(0).toLocaleDateString("en", { timeStyle: "short" }));
throws(TypeError, () => new Date(0).toLocaleTimeString("en", { dateStyle: "short" }));
const poison = new Proxy({}, { get() { throw Error("must not read NaN arguments"); } });
same(new Date(NaN).toLocaleString(poison, poison), "Invalid Date");
same(new Date(NaN).toLocaleDateString(poison, poison), "Invalid Date");
same(new Date(NaN).toLocaleTimeString(poison, poison), "Invalid Date");
throws(TypeError, () => Date.prototype.toLocaleString.call({}));
throws(TypeError, () => Date.prototype.toLocaleString.call(new Proxy(new Date(), {})));

const rangeParts = format.formatRangeToParts(date, date + 123456);
same(join(rangeParts), format.formatRange(date, date + 123456));
for (const part of rangeParts) {
    same(["shared", "startRange", "endRange"].includes(part.source), true);
    same(Object.keys(part).join(","), "type,value,source");
}
same(typeof format.formatRange(date + 123456, date), "string", "reversed range permitted");
throws(TypeError, () => format.formatRange(date));
throws(TypeError, () => DTF.prototype.formatRange.call({}, date, date));
throws(TypeError, () => DTF.prototype.formatRangeToParts.call({}, date, date));
throws(RangeError, () => format.formatRange(NaN, date));
let coerced = false;
throws(TypeError, () => format.formatRange({ valueOf() { coerced = true; return date; } }, undefined));
same(coerced, false, "missing endpoint checked before coercion");
const sentinel = {};
try { format.formatRange(NaN, { valueOf() { throw sentinel; } }); throw Error("missing endpoint exception"); }
catch (e) { same(e, sentinel, "both ToNumber before TimeClip"); }
const dateOnly = new DTF("en", basic);
const collapsed = dateOnly.formatRangeToParts(0, 1000);
same(join(collapsed), dateOnly.format(0));
same(collapsed.every(p => p.source === "shared"), true);
for (const calendar of ["gregory", "buddhist", "hebrew", "japanese", "chinese"]) {
    const f = new DTF("en", { ...basic, calendar, year: "numeric", month: "long", day: "numeric" });
    same(f.resolvedOptions().calendar, calendar);
    same(join(f.formatToParts(date)), f.format(date));
}
/* The Gregorian calendar must extend its modern leap rules before 1582. */
const historical = new DTF("en", { ...basic, year: "numeric", month: "numeric", day: "numeric" });
same(values(historical.formatToParts(Date.UTC(1500, 2, 1)), "day"), "1");
for (const matcher of ["basic", "best fit"]) {
    const f = new DTF("en", { ...basic, ...components, formatMatcher: matcher });
    same(join(f.formatToParts(date)), f.format(date));
    same(typeof f.format(date), "string");
}
