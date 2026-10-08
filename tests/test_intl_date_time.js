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

// ECMA-402 normalizes numeric arithmetic years <= 0 to 1 - year.
// Both tabular epochs and the early Umm al-Qura civil fallback are defined
// by the calendar table; choose a day well inside each arithmetic year.
function dtfIslamicYearDate(calendar, year) {
    const epoch = Date.parse(calendar === "islamic-tbla" ?
        "0622-07-18T00:00:00Z" : "0622-07-19T00:00:00Z");
    const days = 354 * (year - 1) + Math.floor((3 + 11 * year) / 30) + 150;
    return new Date(epoch + days * 86400000);
}
function dtfNormalizedYear(locale, width, year) {
    const number = new Intl.NumberFormat(locale, {
        useGrouping: false, minimumIntegerDigits: width === "2-digit" ? 2 : 1
    });
    const text = number.format(year <= 0 ? 1 - year : year);
    return width === "2-digit" ? Array.from(text).slice(-2).join("") : text;
}
for (const calendar of ["islamic-civil", "islamic-tbla", "islamic-umalqura"]) {
    for (const locale of ["en-u-nu-latn", "ar-u-nu-arab", "en-u-nu-mathsans"]) {
        for (const width of ["numeric", "2-digit"]) {
            const f = new Intl.DateTimeFormat(locale, {
                calendar, timeZone: "UTC", year: width
            });
            for (const year of [-124, -1, 0, 1, 2]) {
                const input = dtfIslamicYearDate(calendar, year);
                const parts = f.formatToParts(input);
                same(parts.find(part => part.type === "year").value,
                     dtfNormalizedYear(locale, width, year));
                same(parts.map(part => part.value).join(""), f.format(input));
                same(f.formatRange(input, input), f.format(input));
                const equalRange = f.formatRangeToParts(input, input);
                same(equalRange.every(part => part.source === "shared"), true);
                same(equalRange.map(part => part.value).join(""), f.format(input));
                const options = { calendar, timeZone: "UTC", year: width };
                same(input.toLocaleString(locale, options), f.format(input));
                if (typeof Temporal !== "undefined") {
                    const plain = new Temporal.PlainDate(input.getUTCFullYear(),
                        input.getUTCMonth() + 1, input.getUTCDate(), calendar);
                    same(f.formatToParts(plain).find(part => part.type === "year").value,
                         dtfNormalizedYear(locale, width, year));
                    same(f.format(plain), f.format(input));
                    same(plain.toLocaleString(locale, options), f.format(plain));
                }
            }
        }
    }
}

// The table-defined negative Islamic era is bh. ICU has no localized name
// for it, so FormatDateTimePattern falls back to the canonical identifier.
for (const calendar of ["islamic-civil", "islamic-tbla", "islamic-umalqura"]) {
    const before = dtfIslamicYearDate(calendar, 0);
    const after = dtfIslamicYearDate(calendar, 1);
    for (const locale of ["en", "ar", "fr"]) {
        for (const era of ["long", "short", "narrow"]) {
            const options = { calendar, timeZone: "UTC", year: "numeric", era };
            const f = new Intl.DateTimeFormat(locale, options);
            const beforeParts = f.formatToParts(before);
            const afterParts = f.formatToParts(after);
            const beforeEra = beforeParts.find(part => part.type === "era").value;
            const afterEra = afterParts.find(part => part.type === "era").value;
            same(beforeEra, "bh", "canonical negative era fallback");
            same(afterEra.length > 0, true);
            same(beforeEra === afterEra, false, "distinct Islamic eras");
            same(beforeParts.find(part => part.type === "year").value,
                 afterParts.find(part => part.type === "year").value);
            same(join(beforeParts), f.format(before));
            same(join(afterParts), f.format(after));
            same(before.toLocaleString(locale, options), f.format(before));
            same(f.formatRange(before, before), f.format(before));
            const equalRange = f.formatRangeToParts(before, before);
            same(equalRange.every(part => part.source === "shared"), true);
            same(join(equalRange), f.format(before));
            // ERA inspection is required even when YEAR was not requested.
            const eraOnly = new Intl.DateTimeFormat(locale, {
                calendar, timeZone: "UTC", weekday: "short", era
            });
            same(eraOnly.formatToParts(before).find(part => part.type === "era").value,
                 beforeEra);
            same(join(eraOnly.formatToParts(before)), eraOnly.format(before));
            if (typeof Temporal !== "undefined") {
                const plain = new Temporal.PlainDate(before.getUTCFullYear(),
                    before.getUTCMonth() + 1, before.getUTCDate(), calendar);
                const plainTime = plain.toPlainDateTime();
                same(f.formatToParts(plain).find(part => part.type === "era").value,
                     beforeEra);
                same(f.format(plain), f.format(before));
                same(f.format(plainTime), f.format(before));
                same(plain.toLocaleString(locale, options), f.format(plain));
                same(plainTime.toLocaleString(locale, options), f.format(plainTime));
            }
        }
    }
}

// The adopted Coptic am era includes dates before its arithmetic epoch.
// Compare against the same provider's positive-era name, at every width.
for (const locale of ["en-u-nu-latn", "ar-u-nu-arab", "en-u-nu-mathsans"]) {
    for (const era of ["short", "long", "narrow"]) {
        for (const width of ["numeric", "2-digit"]) {
            const options = { calendar: "coptic", timeZone: "UTC", era, year: width };
            const f = new Intl.DateTimeFormat(locale, options);
            const dates = [-100, 0, 250, 2025].map(year => {
                const input = new Date(0);
                input.setUTCFullYear(year, 5, 15);
                return input;
            });
            const expectedEra = f.formatToParts(dates[3])
                .find(part => part.type === "era").value;
            same(expectedEra.length > 0, true);
            for (const input of dates) {
                const parts = f.formatToParts(input);
                same(parts.find(part => part.type === "era").value, expectedEra);
                same(parts.find(part => part.type === "year").value,
                     dtfNormalizedYear(locale, width, input.getUTCFullYear() - 284));
                same(join(parts), f.format(input));
                same(f.formatRange(input, input), f.format(input));
                const range = f.formatRangeToParts(input, input);
                same(join(range), f.format(input));
                same(range.every(part => part.source === "shared"), true);
                for (const components of [{}, { hour: "numeric" }]) {
                    const eraFormat = new Intl.DateTimeFormat(locale, {
                        calendar: "coptic", timeZone: "UTC", era, ...components
                    });
                    same(eraFormat.formatToParts(input)
                        .find(part => part.type === "era").value, expectedEra);
                    same(join(eraFormat.formatToParts(input)), eraFormat.format(input));
                }
                if (typeof Temporal !== "undefined") {
                    const plain = new Temporal.PlainDate(input.getUTCFullYear(),
                        input.getUTCMonth() + 1, input.getUTCDate(), "coptic");
                    same(plain.era, "am");
                    same(f.formatToParts(plain).find(part => part.type === "era").value,
                         expectedEra);
                    same(f.format(plain), f.format(input));
                    same(plain.toLocaleString(locale, options), f.format(plain));
                    same(f.format(plain.toPlainDateTime()), f.format(input));
                }
            }
        }
    }
}

