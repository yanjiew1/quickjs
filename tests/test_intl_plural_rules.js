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

attributes(Intl.PluralRules, [["select", 1], ["selectRange", 2], ["resolvedOptions", 0]], 0);
const en = new Intl.PluralRules("en");
assert(en.select(1), "one"); assert(en.select(2), "other");
assert(new Intl.PluralRules("ar").select(0), "zero");
assert(new Intl.PluralRules("ar").select(2), "two");
assert(new Intl.PluralRules("ru").select(5), "many");
assert(new Intl.PluralRules("en", { type: "ordinal" }).select(2), "two");
assert(en.select(NaN), "other"); assert(en.select(Infinity), "other");
assert(en.select(-Infinity), "other"); assert(en.select(-1), "one");
assert(en.select(1n), "one"); assert(en.select("1.000"), "one");
// BigInt and decimal inputs must preserve low digits beyond Number precision.
const exact = new Intl.PluralRules("ru", { maximumFractionDigits: 0 });
assert(exact.select(90071992547409921n), "one");
assert(exact.select("90071992547409921"), "one");
assert(new Intl.PluralRules("en", { minimumFractionDigits: 1 }).select(1), "other");
assert(new Intl.PluralRules("en", { minimumFractionDigits: 1,
    trailingZeroDisplay: "stripIfInteger" }).select(1), "one");
assert(new Intl.PluralRules("en", { maximumFractionDigits: 0,
    roundingMode: "halfEven" }).select(1.5), "other");
assert(new Intl.PluralRules("en", { maximumFractionDigits: 0,
    roundingMode: "halfTrunc" }).select(1.5), "one");
assert(en.selectRange(1.0001, 1.0002), "one");
assert(en.selectRange(3, 1), "other"); // no start > end restriction
assertThrows(TypeError, () => en.select(Symbol()));
assertThrows(TypeError, () => en.selectRange(undefined, 1));
assertThrows(RangeError, () => en.selectRange(NaN, 1));
const conversion = [];
assertThrows(RangeError, () => en.selectRange(
    { valueOf() { conversion.push("x"); return NaN; } },
    { valueOf() { conversion.push("y"); return 1; } }));
assertArrayEquals(conversion, ["x", "y"]);
const names = ["localeMatcher", "type", "notation", "compactDisplay",
    "minimumIntegerDigits", "minimumFractionDigits", "maximumFractionDigits",
    "minimumSignificantDigits", "maximumSignificantDigits", "roundingIncrement",
    "roundingMode", "roundingPriority", "trailingZeroDisplay"];
const o = observe(names); new Intl.PluralRules("en", o.options);
assertArrayEquals(o.seen, names);
assertThrows(RangeError, () => new Intl.PluralRules("en", { compactDisplay: "invalid" }));
assertThrows(TypeError, () => new Intl.PluralRules("en", {
    roundingIncrement: 5, maximumSignificantDigits: 2 }));
const r = en.resolvedOptions();
assertArrayEquals(Object.keys(r), ["locale", "type", "notation", "minimumIntegerDigits",
    "minimumFractionDigits", "maximumFractionDigits", "pluralCategories",
    "roundingIncrement", "roundingMode", "roundingPriority", "trailingZeroDisplay"]);
assertArrayEquals(r.pluralCategories, ["one", "other"]);
r.pluralCategories.pop(); assert(en.resolvedOptions().pluralCategories.length, 2);
assert(Object.hasOwn(r, "compactDisplay"), false);
assert(new Intl.PluralRules("fr", { notation: "compact" }).resolvedOptions().compactDisplay, "short");
class Sub extends Intl.PluralRules {}
assert(new Sub("en") instanceof Sub);
assertThrows(TypeError, () => en.select.call(new Proxy(en, {}), 1));

// ResolvePlural equality is unsigned; rounding still uses the input sign.
assert(en.selectRange(-1, 1), "one");
assert(en.selectRange(-1n, 1n), "one");
assert(en.selectRange(-0, 0), en.select(-0));
assert(new Intl.PluralRules("ru").selectRange(-0, 0), "many");
const ceiling = new Intl.PluralRules("en", { maximumFractionDigits: 0, roundingMode: "ceil" });
assert(ceiling.selectRange(-1.5, 1), "one");
assert(ceiling.selectRange(-1.5, 1.5), "other");
const floor = new Intl.PluralRules("en", { maximumFractionDigits: 0, roundingMode: "floor" });
assert(floor.selectRange(-1, 1.5), "one");
assert(floor.selectRange(-1.5, 1.5), "other");
// Signed equality must also hold when the category backend uses notation.
for (const notation of ["standard", "scientific", "engineering", "compact"]) {
    const f = new Intl.PluralRules("en", { notation });
    assert(f.selectRange(-1, 1), f.select(-1));
    assert(f.selectRange(-0, 0), f.select(-0));
}

// Native callback readable argument count follows declared function length.
assert(typeof new Intl.PluralRules().resolvedOptions().locale, "string");
assert(new Intl.PluralRules("en").resolvedOptions().locale, "en");
assertArrayEquals(Intl.PluralRules.supportedLocalesOf(), []);
assertArrayEquals(Intl.PluralRules.supportedLocalesOf([]), []);
assertArrayEquals(Intl.PluralRules.supportedLocalesOf(["en"]), ["en"]);

// PluralRuleSelect receives only the raw rounded unsigned decimal string.
// Notation must not expose digits that FormatNumericToString discarded.
for (const notation of ["standard", "scientific", "engineering", "compact"]) {
    const rounded = new Intl.PluralRules("lv", { notation, maximumFractionDigits: 0 });
    assert(rounded.select("0.1"), rounded.select(0));
    assert(rounded.select("0.2"), rounded.select(0));
    assert(rounded.selectRange("0.1", "0.2"), rounded.select(0));
    assert(rounded.selectRange("-0.1", "0.2"), rounded.select(0));
    const precise = new Intl.PluralRules("lv", { notation, maximumFractionDigits: 3 });
    assert(rounded.select("1001"), precise.select("1001"));
    assert(rounded.selectRange("1001", "1002"), precise.selectRange("1001", "1002"));
}
// Visible trailing fraction digits survive locale notation scaling.
for (const notation of ["standard", "scientific", "engineering", "compact"]) {
    const padded = new Intl.PluralRules("en", { notation, minimumFractionDigits: 1 });
    assert(padded.select(1), "other");
    assert(padded.selectRange(1, 1), "other");
    const stripped = new Intl.PluralRules("en", { notation,
        minimumFractionDigits: 1, trailingZeroDisplay: "stripIfInteger" });
    assert(stripped.select(1), "one");
    assert(stripped.selectRange(1, 1), "one");
}

// Exact CLDR integer and fraction operands must retain their low digits.
assert(exact.select(90071992547409922n), "few");
assert(exact.select(90071992547409925n), "many");
assert(exact.select(100000000000000000000000000000000000000021n), "one");
assert(exact.select("100000000000000000000000000000000000000021"), "one");
const ordinalExact = new Intl.PluralRules("en", { type: "ordinal" });
assert(ordinalExact.select(90071992547409921n), "one");
assert(ordinalExact.select(90071992547409922n), "two");
assert(ordinalExact.select(90071992547409923n), "few");
assert(ordinalExact.select(90071992547409911n), "other");
assert(exact.selectRange(90071992547409921n, 90071992547409921n), "one");
const fractionExact = new Intl.PluralRules("bs", { maximumFractionDigits: 20 });
assert(fractionExact.select("0.90071992547409921"), "one");
assert(fractionExact.select("0.90071992547409922"), "few");
assert(fractionExact.select("0.90071992547409925"), "other");
assert(new Intl.PluralRules("en", { minimumFractionDigits: 2 }).select(1), "other");
assert(new Intl.PluralRules("en", { minimumFractionDigits: 2,
    trailingZeroDisplay: "stripIfInteger" }).select(1), "one");

// Nonzero notation exponents must retain the full raw visible fraction width.
for (const notation of ["scientific", "engineering", "compact"]) {
    const paddedScale = new Intl.PluralRules("ru", { notation, minimumFractionDigits: 1 });
    assert(paddedScale.select(1000), "other");
    assert(paddedScale.select(-1000), "other");
    assert(paddedScale.selectRange(1000, 1000), "other");
    const strippedScale = new Intl.PluralRules("ru", {
        notation, minimumFractionDigits: 1, trailingZeroDisplay: "stripIfInteger" });
    assert(strippedScale.select(1000), "many");
    assert(strippedScale.select(-1000), "many");
    assert(strippedScale.selectRange(1000, 1000), "many");
}
// CLDR c/e distinguish a scaled million from the same standard raw integer.
for (const locale of ["fr", "es"]) {
    const standardScale = new Intl.PluralRules(locale, { maximumFractionDigits: 0 });
    assert(standardScale.select(1000001), "other");
    for (const notation of ["scientific", "engineering", "compact"]) {
        const scaled = new Intl.PluralRules(locale, { notation, maximumFractionDigits: 0 });
        assert(scaled.select(1000001), "many");
        assert(scaled.select(-1000001), "many");
        // Raw rounding must precede choosing the notation exponent.
        assert(scaled.select("999999.4"), "other");
        assert(scaled.select("999999.6"), "many");
        const ceilingScale = new Intl.PluralRules(locale, {
            notation, maximumFractionDigits: 0, roundingMode: "ceil" });
        const floorScale = new Intl.PluralRules(locale, {
            notation, maximumFractionDigits: 0, roundingMode: "floor" });
        assert(ceilingScale.select("-999999.6"), "other");
        assert(floorScale.select("-999999.6"), "many");
    }
}
// Negative scientific exponents reach CLDR e/c, not an unsigned exponent.
assert(new Intl.PluralRules("es", { maximumFractionDigits: 6 }).select("0.001"), "other");
for (const notation of ["scientific", "engineering"]) {
    const negativeExponent = new Intl.PluralRules("es", {
        notation, maximumFractionDigits: 6 });
    assert(negativeExponent.select("0.001"), "many");
    assert(negativeExponent.select("-0.001"), "many");
}
