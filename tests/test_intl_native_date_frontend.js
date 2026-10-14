/* Source-only frontend integration gate; native en/en-US development profile. */
(function () {
    function assert(v, message) { if (!v) throw Error(message || "assertion failed"); }
    function same(a, b) { assert(a === b, String(a) + " != " + String(b)); }
    function throws(type, f) {
        let value; try { f(); } catch (e) { value = e; }
        assert(value instanceof type, "expected " + type.name); return value;
    }
    function field(parts, type) {
        for (let i = 0; i < parts.length; i++) if (parts[i].type === type) return parts[i].value;
    }
    /* Standalone flexible periods have their own patterns. The native B
       policy uses the morning interval at midnight and retains exact noon. */
    for (const formatMatcher of ["basic", "best fit"])
    for (const dayPeriod of ["narrow", "short", "long"]) {
        const period = new Intl.DateTimeFormat("en", {
            timeZone: "UTC", dayPeriod, formatMatcher
        });
        const timed = new Intl.DateTimeFormat("en", {
            timeZone: "UTC", dayPeriod, formatMatcher, hour: "numeric"
        });
        same(period.resolvedOptions().dayPeriod, dayPeriod);
        same(period.resolvedOptions().hour, undefined);
        same(period.resolvedOptions().hourCycle, undefined);
        const noon = dayPeriod === "narrow" ? "n" : "noon";
        for (let hour = 0; hour < 24; hour++) {
            const epoch = Date.UTC(2024, 0, 1, hour);
            const expected = hour < 12 ? "in the morning" : hour === 12 ? noon :
                hour < 18 ? "in the afternoon" : hour < 21 ? "in the evening" : "at night";
            const parts = period.formatToParts(epoch);
            same(parts.length, 1);
            same(parts[0].type, "dayPeriod");
            same(parts[0].value, expected);
            same(period.format(epoch), expected);
            const withHour = timed.formatToParts(epoch);
            same(field(withHour, "hour"), String(hour % 12 || 12));
            const timedNoon = timed.resolvedOptions().dayPeriod === "narrow" ? "n" : "noon";
            same(field(withHour, "dayPeriod"), hour === 12 ? timedNoon : expected);
            same(withHour.map(p => p.value).join(""), timed.format(epoch));
        }
        const noonEpoch = Date.UTC(2024, 0, 1, 12);
        same(period.format(noonEpoch - 1), "in the morning");
        same(period.format(noonEpoch), noon);
        same(period.format(noonEpoch + 1), "in the afternoon");
        for (const hourCycle of ["h11", "h12", "h23", "h24"]) {
            const f = new Intl.DateTimeFormat("en", {
                timeZone: "UTC", dayPeriod, formatMatcher, hourCycle
            });
            same(f.resolvedOptions().hour, undefined);
            same(f.resolvedOptions().hourCycle, undefined);
            same(f.format(0), "in the morning");
        }
        /* These candidates also join dates, including explicit era fields. */
        const dated = new Intl.DateTimeFormat("en", {
            timeZone: "UTC", year: "numeric", month: "numeric", day: "numeric",
            era: "short", dayPeriod, formatMatcher
        });
        same(dated.resolvedOptions().hour, undefined);
        same(field(dated.formatToParts(0), "dayPeriod"), "in the morning");
        assert(field(dated.formatToParts(0), "era") !== undefined);
    }
    const f = new Intl.DateTimeFormat("en-US-u-ca-iso8601-hc-h23-nu-latn", {
        timeZone: "UTC", year: "numeric", month: "2-digit", day: "2-digit",
        hour: "2-digit", minute: "2-digit", second: "2-digit", fractionalSecondDigits: 3
    });
    same(f.resolvedOptions().calendar, "iso8601");
    same(f.resolvedOptions().hourCycle, "h23");
    same(f.resolvedOptions().numberingSystem, "latn");
    same(field(f.formatToParts(-1), "second"), "59");
    same(field(f.formatToParts(-1), "fractionalSecond"), "999");
    same(f.format, f.format);
    const parts = f.formatRangeToParts(0, 1000);
    let text = "";
    for (let i = 0; i < parts.length; i++) {
        same(Object.keys(parts[i]).join(","), "type,value,source");
        text += parts[i].value;
    }
    const join = Array.prototype.join;
    Array.prototype.join = function () { throw Error("user join consulted"); };
    try { same(f.formatRange(0, 1000), text); } finally { Array.prototype.join = join; }
    for (const p of f.formatRangeToParts(0, 0)) same(p.source, "shared");
    /* The last resolved component determines the optional range-record
       prefix. Invisible finer differences must not force two endpoints. */
    const epoch = Date.UTC(2024, 0, 1, 9, 8, 7, 123);
    function rangePrecision(options, start, equalEnd, differentEnd) {
        const format = new Intl.DateTimeFormat("en", {timeZone: "UTC", ...options});
        const shared = format.formatRangeToParts(start, equalEnd);
        assert(shared.every(p => p.source === "shared"));
        same(shared.map(p => p.value).join(""), format.format(start));
        same(format.formatRange(start, equalEnd), format.format(start));
        for (const [a, b] of [[start, differentEnd], [differentEnd, start]]) {
            const parts = format.formatRangeToParts(a, b);
            assert(parts.some(p => p.source === "startRange"));
            assert(parts.some(p => p.source === "endRange"));
            same(parts.map(p => p.value).join(""), format.formatRange(a, b));
        }
    }
    for (const formatMatcher of ["basic", "best fit"]) {
        const bce = Date.parse("0000-01-01T00:00:00Z");
        const ce = Date.parse("0001-01-01T00:00:00Z");
        rangePrecision({year: "numeric", formatMatcher}, bce, bce + 1000, ce);
        rangePrecision({calendar: "chinese", month: "long", formatMatcher},
                       Date.UTC(2023, 1, 22), Date.UTC(2023, 1, 23),
                       Date.UTC(2023, 2, 22));
        rangePrecision({year: "numeric", formatMatcher}, epoch,
                       Date.UTC(2024, 11, 31), Date.UTC(2025, 0, 1));
        rangePrecision({month: "long", formatMatcher}, epoch,
                       Date.UTC(2024, 0, 31), Date.UTC(2024, 1, 1));
        rangePrecision({weekday: "long", formatMatcher}, epoch,
                       epoch + 1000, epoch + 86400000);
        rangePrecision({dateStyle: "short", formatMatcher}, epoch,
                       epoch + 1000, epoch + 86400000);
        rangePrecision({timeStyle: "short", formatMatcher}, epoch,
                       epoch + 1000, epoch + 60000);
        for (const hourCycle of ["h11", "h12", "h23", "h24"])
            rangePrecision({hour: "numeric", hourCycle, formatMatcher}, epoch,
                           epoch + 1000, epoch + 3600000);
        for (let fractionalSecondDigits = 1; fractionalSecondDigits <= 3;
             fractionalSecondDigits++) {
            const step = 10 ** (3 - fractionalSecondDigits);
            const start = epoch - 123 + step;
            rangePrecision({second: "numeric", fractionalSecondDigits, formatMatcher},
                           start, start + step - 1, start + step);
        }
        rangePrecision({dayPeriod: "long", formatMatcher},
                       Date.UTC(2024, 0, 1, 9), Date.UTC(2024, 0, 1, 10),
                       Date.UTC(2024, 0, 1, 18));
    }
    const reversed = f.formatRangeToParts(1000, 0);
    assert(reversed.some(p => p.source === "startRange"));
    assert(reversed.some(p => p.source === "endRange"));
    let order = "";
    const start = { valueOf() { order += "s"; return NaN; } };
    const end = { valueOf() { order += "e"; return 0; } };
    throws(RangeError, () => f.formatRange(start, end)); same(order, "se");
    order = ""; const sentinel = Error("end");
    same(throws(Error, () => f.formatRange(start, { valueOf() { order += "e"; throw sentinel; } })), sentinel);
    same(order, "se");
    order = ""; throws(TypeError, () => f.formatRange(start, undefined)); same(order, "");
    const optionOrder = [];
    const names = ["localeMatcher", "calendar", "numberingSystem", "hour12", "hourCycle", "timeZone",
        "weekday", "era", "year", "month", "day", "dayPeriod", "hour", "minute", "second",
        "fractionalSecondDigits", "timeZoneName", "formatMatcher", "dateStyle", "timeStyle"];
    const options = {};
    for (const name of names) Object.defineProperty(options, name, {
        get() { optionOrder.push(name); return name === "timeZone" ? "UTC" : undefined; }
    });
    const snapshot = new Intl.DateTimeFormat("en-US", options);
    same(optionOrder.join(","), names.join(","));
    snapshot.format(0); snapshot.formatRangeToParts(-1, 0);
    same(optionOrder.length, names.length);
    const hour12 = new Intl.DateTimeFormat("en-US-u-hc-h23", {
        timeZone: "UTC", hour: "numeric", hourCycle: "h24", hour12: true
    }).resolvedOptions();
    assert(hour12.hourCycle === "h11" || hour12.hourCycle === "h12");
    assert(!hour12.locale.includes("hc-h23"));
    /* LDML matching preserves a locale's contextual time-field widths.
       English Hms uses HH:mm:ss; hms uses h:mm:ss a. */
    for (const formatMatcher of ["basic", "best fit"])
    for (const hourCycle of ["h11", "h12", "h23", "h24"]) {
        const padded = hourCycle === "h23" || hourCycle === "h24";
        const contextual = new Intl.DateTimeFormat("en", {
            timeZone: "UTC", hourCycle, formatMatcher,
            hour: "numeric", minute: "numeric", second: "numeric"
        });
        same(contextual.resolvedOptions().hour, padded ? "2-digit" : "numeric");
        same(contextual.resolvedOptions().minute, "2-digit");
        same(contextual.resolvedOptions().second, "2-digit");
        const zero = hourCycle === "h12" ? "12" : hourCycle === "h24" ? "24" : padded ? "00" : "0";
        same(field(contextual.formatToParts(0), "hour"), zero);
        same(field(contextual.formatToParts(0), "minute"), "00");
        same(field(contextual.formatToParts(0), "second"), "00");
        same(field(contextual.formatToParts(9 * 3600000), "hour"), padded ? "09" : "9");
        const explicit = new Intl.DateTimeFormat("en", {
            timeZone: "UTC", hourCycle, hour: "2-digit", formatMatcher: "best fit"
        });
        same(explicit.resolvedOptions().hour, "2-digit");
        same(field(explicit.formatToParts(9 * 3600000), "hour"), "09");
    }
    if (typeof Temporal === "object") {
        const midnight = [Temporal.Instant.from("1970-01-01T00:00Z"),
            Temporal.PlainTime.from("00:00"),
            Temporal.PlainDateTime.from("1970-01-01T00:00"),
            Temporal.ZonedDateTime.from("1970-01-01T00:00Z[UTC]")];
        for (const value of midnight) {
            const options = {hour12: false};
            if (!(value instanceof Temporal.ZonedDateTime)) options.timeZone = "UTC";
            assert(value.toLocaleString("en", options).includes("00:00:00"));
        }
    }
    same(new Intl.DateTimeFormat("en-US", {timeZone:"-00"}).resolvedOptions().timeZone, "+00:00");
    throws(RangeError, () => new Intl.DateTimeFormat("en-US", {timeZone:"+24:00"}));
    same(Intl.DateTimeFormat.supportedLocalesOf(["en", "en-US", "fr"]).join(","), "en,en-US");
    /* A valid component combination may have no exact locale pattern.
       Best fit still returns a coherent format, as basic matching does. */
    for (const timeZoneName of ["short", "long", "shortOffset", "longOffset",
                                "shortGeneric", "longGeneric"]) {
        for (const formatMatcher of [undefined, "best fit", "basic"]) {
            const unmatched = new Intl.DateTimeFormat("en-US", {
                calendar: "gregory", numberingSystem: "latn", timeZone: "UTC",
                hourCycle: "h12", hour: "numeric", dayPeriod: "long",
                timeZoneName, formatMatcher
            });
            const text = unmatched.format(0);
            assert(text.length > 0);
            same(unmatched.formatToParts(0).map(p => p.value).join(""), text);
            same(unmatched.resolvedOptions().timeZone, "UTC");
        }
    }
    /* For this deterministic packed fallback, retain the complete basic record
       before changing any widths. This equality is specific to longGeneric. */
    const fallbackOptions = {
        calendar: "gregory", numberingSystem: "latn", timeZone: "UTC",
        hourCycle: "h12", hour: "numeric", dayPeriod: "long",
        timeZoneName: "longGeneric"
    };
    const basicFallback = new Intl.DateTimeFormat("en-US", {
        ...fallbackOptions, formatMatcher: "basic"
    });
    for (const formatMatcher of [undefined, "best fit"]) {
        const bestFallback = new Intl.DateTimeFormat("en-US", {
            ...fallbackOptions, formatMatcher
        });
        same(JSON.stringify(bestFallback.resolvedOptions()),
             JSON.stringify(basicFallback.resolvedOptions()));
        same(bestFallback.format(0), basicFallback.format(0));
        same(JSON.stringify(bestFallback.formatToParts(0)),
             JSON.stringify(basicFallback.formatToParts(0)));
    }
    /* A detached bound function roots its bank across global/property changes. */
    const bound = new Intl.DateTimeFormat("en-US", {timeZone:"UTC"}).format;
    const expected = bound(0), original = globalThis.Intl;
    globalThis.Intl = {};
    try { same(bound(0), expected); } finally { globalThis.Intl = original; }
})();
