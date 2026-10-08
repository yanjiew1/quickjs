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

attributes(Intl.RelativeTimeFormat, [["format", 2], ["formatToParts", 2], ["resolvedOptions", 0]], 0);
const en = new Intl.RelativeTimeFormat("en");
assert(en.format(-1, "day"), "1 day ago");
assert(en.format(-0, "seconds"), "0 seconds ago");
assert(en.format(0, "second"), "in 0 seconds");
const auto = new Intl.RelativeTimeFormat("en", { numeric: "auto" });
assert(auto.format(1, "day"), "tomorrow");
assert(auto.format(1.001, "day").includes("tomorrow"), false);
assert(auto.format(0.001, "day").includes("today"), false);
assert(new Intl.RelativeTimeFormat("fr", { numeric: "auto" }).format(-1, "day"), "hier");
const ar = new Intl.RelativeTimeFormat("ar-u-nu-arab");
assert(ar.resolvedOptions().numberingSystem, "arab");
const parts = en.formatToParts(-12345.67, "days");
assert(parts.map(p => p.value).join(""), en.format(-12345.67, "day"));
assert(parts.some(p => p.type === "group")); assert(parts.some(p => p.type === "fraction"));
for (const p of parts) {
    if (p.type === "literal") assert(Object.hasOwn(p, "unit"), false);
    else assert(p.unit, "day");
}
assertArrayEquals(Object.keys(auto.formatToParts(1, "day")[0]), ["type", "value"]);
const coercion = [];
assertThrows(RangeError, () => en.format(
    { valueOf() { coercion.push("number"); return NaN; } },
    { toString() { coercion.push("unit"); return "day"; } }));
assertArrayEquals(coercion, ["number", "unit"]);
for (const value of [NaN, Infinity, -Infinity]) assertThrows(RangeError, () => en.format(value, "day"));
assertThrows(TypeError, () => en.format(1n, "day"));
assertThrows(TypeError, () => en.format(1, Symbol()));
assertThrows(RangeError, () => en.format(1, "DAY"));
assertThrows(RangeError, () => en.format(1, "day\u0000"));
const o = observe(["localeMatcher", "numberingSystem", "style", "numeric"]);
new Intl.RelativeTimeFormat("en", o.options);
assertArrayEquals(o.seen, ["localeMatcher", "numberingSystem", "style", "numeric"]);
assertArrayEquals(Object.keys(en.resolvedOptions()), ["locale", "style", "numeric", "numberingSystem"]);
assertThrows(TypeError, () => en.format.call(new Proxy(en, {}), 1, "day"));
class Sub extends Intl.RelativeTimeFormat {}
assert(new Sub("en") instanceof Sub);

// Native callback readable argument count follows declared function length.
assert(typeof new Intl.RelativeTimeFormat().resolvedOptions().locale, "string");
assert(new Intl.RelativeTimeFormat("en").resolvedOptions().locale, "en");
assertArrayEquals(Intl.RelativeTimeFormat.supportedLocalesOf(), []);
assertArrayEquals(Intl.RelativeTimeFormat.supportedLocalesOf([]), []);
assertArrayEquals(Intl.RelativeTimeFormat.supportedLocalesOf(["en"]), ["en"]);
