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
