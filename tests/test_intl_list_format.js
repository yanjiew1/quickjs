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

attributes(Intl.ListFormat, [["format", 1], ["formatToParts", 1], ["resolvedOptions", 0]], 0);
const en = new Intl.ListFormat("en");
assert(en.format(), ""); assertArrayEquals(en.formatToParts(), []);
assert(en.format(["A", "B", "C"]), "A, B, and C");
assert(en.format("ABC"), "A, B, and C");
assert(new Intl.ListFormat("fr").format(["A", "B"]), "A et B");
assert(new Intl.ListFormat("es").format(["Ana", "Irene"]), "Ana e Irene");
assert(new Intl.ListFormat("es", { type: "disjunction" }).format(["7", "8"]), "7 u 8");
const samples = [[], [""], ["", ""], ["A", "", "B"], ["", "", ""],
    ["A\u0000B", "\ud800", "C"], ["\ud83d\ude00", "\udc00"]];
for (const locale of ["en", "es", "he", "ja", "ar"]) {
    const f = new Intl.ListFormat(locale);
    for (const list of samples) {
        const parts = f.formatToParts(list);
        assert(parts.map(p => p.value).join(""), f.format(list));
        assertArrayEquals(parts.filter(p => p.type === "element").map(p => p.value), list);
        for (const p of parts) assertArrayEquals(Object.keys(p), ["type", "value"]);
    }
}
let closed = 0, coerced = 0;
const input = { [Symbol.iterator]() { return {
    next() { return { done: false, value: { toString() { coerced++; return "X"; } } }; },
    return() { closed++; throw Error("must not replace TypeError"); }
}; } };
assertThrows(TypeError, () => en.format(input)); assert(closed, 1); assert(coerced, 0);
let returns = 0; const original = Error("step failure");
const broken = { [Symbol.iterator]() { return {
    next() { throw original; }, return() { returns++; return {}; }
}; } };
try { en.format(broken); assert(false); } catch (e) { assert(e, original); }
assert(returns, 0);
const badValue = { [Symbol.iterator]() { return {
    next() { return { done: false, get value() { throw original; } }; },
    return() { returns++; return {}; }
}; } };
try { en.format(badValue); assert(false); } catch (e) { assert(e, original); }
assert(returns, 0);
assertThrows(TypeError, () => en.format([new String("X")]));
assertThrows(TypeError, () => en.format(null));
assertThrows(TypeError, () => en.format.call(new Proxy(en, {}), []));
const o = observe(["localeMatcher", "type", "style"]);
new Intl.ListFormat("en", o.options); assertArrayEquals(o.seen, ["localeMatcher", "type", "style"]);
assertThrows(TypeError, () => new Intl.ListFormat("en", "options"));
assertThrows(RangeError, () => new Intl.ListFormat("en", { type: "invalid" }));
assertArrayEquals(Object.keys(en.resolvedOptions()), ["locale", "type", "style"]);
class Sub extends Intl.ListFormat {}
assert(new Sub("en") instanceof Sub);

// Native callback readable argument count follows declared function length.
assert(typeof new Intl.ListFormat().resolvedOptions().locale, "string");
assert(new Intl.ListFormat("en").resolvedOptions().locale, "en");
assertArrayEquals(Intl.ListFormat.supportedLocalesOf(), []);
assertArrayEquals(Intl.ListFormat.supportedLocalesOf([]), []);
assertArrayEquals(Intl.ListFormat.supportedLocalesOf(["en"]), ["en"]);
