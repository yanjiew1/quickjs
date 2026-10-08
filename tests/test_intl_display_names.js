import { assert, assertThrows, assertArrayEquals } from "./assert.js";
function observe(names, values = {}) {
    const seen = [];
    const options = {};
    for (const name of names) Object.defineProperty(options, name, {
        get() { seen.push(name); return values[name]; },
    });
    return { options, seen };
}
function attributes(C, methods, length) {
    assert(C.length, length);
    const p = Object.getOwnPropertyDescriptor(C, "prototype");
    assert(p.writable, false); assert(p.enumerable, false); assert(p.configurable, false);
    assert(Object.getPrototypeOf(C.prototype), Object.prototype);
    const t = Object.getOwnPropertyDescriptor(C.prototype, Symbol.toStringTag);
    assert(t.writable, false); assert(t.enumerable, false); assert(t.configurable, true);
    for (const [name, n] of methods) {
        const d = Object.getOwnPropertyDescriptor(C.prototype, name);
        assert(d.value.length, n); assert(d.writable, true);
        assert(d.enumerable, false); assert(d.configurable, true);
        assertThrows(TypeError, () => C.prototype[name].call(C.prototype));
        assertThrows(TypeError, () => C.prototype[name].call(Object.create(C.prototype)));
    }
    assertThrows(TypeError, () => C("en", { type: "language" }));
    const ordering = [];
    const locales = { length: 1, get 0() { ordering.push("locale"); return "en"; } };
    const target = new Proxy(C, { get(t, k, receiver) {
        if (k === "prototype") ordering.push("prototype");
        return Reflect.get(t, k, receiver);
    } });
    Reflect.construct(C, [locales, { type: C === Intl.DisplayNames ? "language" : undefined }], target);
    assertArrayEquals(ordering, ["prototype", "locale"]);
}

attributes(Intl.DisplayNames, [["of", 1], ["resolvedOptions", 0]], 2);
assertThrows(TypeError, () => new Intl.DisplayNames("en"));
assertThrows(TypeError, () => new Intl.DisplayNames("en", {}));
const en = new Intl.DisplayNames("en", { type: "region" });
assert(en.of("us"), "United States");
assert(new Intl.DisplayNames("fr", { type: "region" }).of("US"), "États-Unis");
assert(new Intl.DisplayNames("en", { type: "script" }).of("lAtN"), "Latin");
assert(new Intl.DisplayNames("en", { type: "calendar" }).of("gregory"), "Gregorian Calendar");
assert(new Intl.DisplayNames("en", { type: "currency" }).of("usd"), "US Dollar");
for (const type of ["language", "region", "script", "currency", "calendar", "dateTimeField"])
    assert(new Intl.DisplayNames("en", { type }).resolvedOptions().type, type);
const none = new Intl.DisplayNames("en", { type: "currency", fallback: "none" });
assert(none.of("ZZZ"), undefined);
assert(new Intl.DisplayNames("en", { type: "currency" }).of("zzz"), "ZZZ");
assertThrows(RangeError, () => en.of("USA"));
assertThrows(RangeError, () => en.of("US\u0000"));
assertThrows(RangeError, () => en.of("\ud800"));
assertThrows(TypeError, () => en.of(Symbol()));
const language = new Intl.DisplayNames("en", { type: "language", fallback: "code" });
assertThrows(RangeError, () => language.of("en-u-ca-gregory"));
assertThrows(RangeError, () => language.of("en-x-private"));
assertThrows(RangeError, () => language.of("en-1901-1901"));
assert(language.of("EN-gb"), new Intl.DisplayNames("en", { type: "language" }).of("en-GB"));
const field = new Intl.DisplayNames("en", { type: "dateTimeField" });
for (const code of ["era", "year", "quarter", "month", "weekOfYear", "weekday", "day",
    "dayPeriod", "hour", "minute", "second", "timeZoneName"]) assert(typeof field.of(code), "string");
assertThrows(RangeError, () => field.of("Year"));
const o = observe(["localeMatcher", "style", "type", "fallback", "languageDisplay"], { type: "region" });
new Intl.DisplayNames("en", o.options);
assertArrayEquals(o.seen, ["localeMatcher", "style", "type", "fallback", "languageDisplay"]);
assertThrows(RangeError, () => new Intl.DisplayNames("en", { type: "region", languageDisplay: "invalid" }));
assertArrayEquals(Object.keys(en.resolvedOptions()), ["locale", "style", "type", "fallback"]);
assertArrayEquals(Object.keys(language.resolvedOptions()), ["locale", "style", "type", "fallback", "languageDisplay"]);
assertThrows(TypeError, () => en.of.call(new Proxy(en, {}), "US"));
class Sub extends Intl.DisplayNames {}
assert(new Sub("en", { type: "language" }) instanceof Sub);

// Native callback readable argument count follows declared function length.
assertThrows(TypeError, () => new Intl.DisplayNames());
assertThrows(TypeError, () => new Intl.DisplayNames("en"));
assertArrayEquals(Intl.DisplayNames.supportedLocalesOf(), []);
assertArrayEquals(Intl.DisplayNames.supportedLocalesOf([]), []);
assertArrayEquals(Intl.DisplayNames.supportedLocalesOf(["en"]), ["en"]);
