/* CONFIG_INTL enabled native regression tests; no polyfill or shell tools. */
function same(actual, expected, message) {
    if (!Object.is(actual, expected)) throw Error((message || "mismatch") + ": " + actual + " != " + expected);
}
function throws(type, operation) {
    try { operation(); } catch (error) { if (error instanceof type) return; throw error; }
    throw Error("expected " + type.name);
}
function joined(parts) { return parts.map(p => p.value).join(""); }
function values(parts, type) { return parts.filter(p => p.type === type).map(p => p.value).join(""); }
function nf(options) { return new Intl.NumberFormat("en-US", {useGrouping: false, ...options}); }

if (typeof Intl !== "object") throw Error("test requires CONFIG_INTL=1");
/* Function.length is normative; missing arguments must not be read through
   callback padding assumptions (constructors length0, static methods1). */
same(Intl.NumberFormat.length, 0);
same(new Intl.NumberFormat().resolvedOptions().style, "decimal");
same(Intl.NumberFormat().format(1234), new Intl.NumberFormat().format(1234));
same(Intl.NumberFormat.supportedLocalesOf([]).length, 0);
same(Intl.NumberFormat.supportedLocalesOf().length, 0);
same(new Intl.NumberFormat("en-US").format(), "NaN");
same(joined(new Intl.NumberFormat("en-US").formatToParts()), "NaN");
const exact = nf({minimumFractionDigits: 3, maximumFractionDigits: 3});
same(exact.format("9007199254740993.125"), "9007199254740993.125");
same(exact.format(9007199254740993n), "9007199254740993.000");
same(nf().format("0x20000000000001"), "9007199254740993");
same(nf().format("\u20039007199254740993\u00a0"), "9007199254740993");
same(values(nf().formatToParts("1e1000"), "infinity"), "∞");
same(nf().format("1e-1000"), "0");
same(nf().format("-1e-1000"), "-0");
same(nf().format("1\u0000"), "NaN");
same(nf().format("+0x10"), "NaN");
throws(TypeError, () => nf().format(Symbol()));
const coercions = [];
same(nf().format({[Symbol.toPrimitive](hint) { coercions.push(hint); return "9007199254740993"; }}), "9007199254740993");
same(coercions.join(), "number");

const modes = {
    ceil: [2,-1], floor: [1,-2], expand: [2,-2], trunc: [1,-1],
    halfCeil: [2,-1], halfFloor: [1,-2], halfExpand: [2,-2],
    halfTrunc: [1,-1], halfEven: [2,-2]
};
for (const mode of Object.keys(modes)) {
    const f = nf({maximumFractionDigits: 0, roundingMode: mode});
    same(f.format("1.5"), String(modes[mode][0]), mode);
    same(f.format("-1.5"), String(modes[mode][1]), mode);
}
same(nf({maximumFractionDigits: 0, roundingMode: "halfEven"}).format("2.5"), "2");
same(nf({minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 5}).format("1.025"), "1.05");
same(nf({minimumFractionDigits: 2, maximumFractionDigits: 2, trailingZeroDisplay: "stripIfInteger"}).format(1), "1");
same(nf({minimumFractionDigits: 100, maximumFractionDigits: 100}).format(1), "1." + "0".repeat(100));
same(nf({maximumFractionDigits: 0, maximumSignificantDigits: 3, roundingPriority: "morePrecision"}).format("12.34"), "12.3");
same(nf({maximumFractionDigits: 0, maximumSignificantDigits: 3, roundingPriority: "lessPrecision"}).format("12.34"), "12");
throws(TypeError, () => nf({maximumSignificantDigits: 3, roundingIncrement: 5}));
throws(RangeError, () => nf({minimumFractionDigits: 1, maximumFractionDigits: 2, roundingIncrement: 5}));
throws(RangeError, () => nf({roundingIncrement: 3}));
same(nf({signDisplay: "negative"}).format(-0), "0");
same(nf({signDisplay: "always"}).format(0), "+0");
same(new Intl.NumberFormat("en-US", {useGrouping: "false"}).resolvedOptions().useGrouping, "auto");
same(new Intl.NumberFormat("en-US", {useGrouping: false}).resolvedOptions().useGrouping, false);
same(new Intl.NumberFormat("en-US", {notation: "compact"}).resolvedOptions().roundingPriority, "morePrecision");

const accesses = [];
const options = new Proxy({}, {get(target, key) { accesses.push(String(key)); return undefined; }});
new Intl.NumberFormat("en-US", options);
same(accesses.join(), ["localeMatcher","numberingSystem","style","currency","currencyDisplay","currencySign","unit","unitDisplay","notation","minimumIntegerDigits","minimumFractionDigits","maximumFractionDigits","minimumSignificantDigits","maximumSignificantDigits","roundingIncrement","roundingMode","roundingPriority","trailingZeroDisplay","compactDisplay","useGrouping","signDisplay"].join());
const rawOrder = [];
new Intl.NumberFormat("en-US", {
    get minimumFractionDigits() { rawOrder.push("get min"); return {valueOf() {rawOrder.push("convert min"); return 1;}}; },
    get maximumFractionDigits() { rawOrder.push("get max"); return 2; },
    get roundingMode() { rawOrder.push("rounding"); return "trunc"; }
});
same(rawOrder.join(), "get min,get max,rounding,convert min");
for (const options of [{currency: "USD\u0000X"}, {unit: "meter\u0000x"}, {numberingSystem: "latn\u0000x"}])
    throws(RangeError, () => new Intl.NumberFormat("en-US", options));
throws(RangeError, () => nf({style: "decimal", unit: "meter-per-second-per-second"}));
throws(TypeError, () => nf({style: "currency"}));
throws(TypeError, () => nf({style: "unit"}));

const money = new Intl.NumberFormat("en-US", {style: "currency", currency: "usd", currencySign: "accounting"});
same(money.resolvedOptions().currency, "USD");
same(joined(money.formatToParts(-12.5)), money.format(-12.5));
same(values(money.formatToParts(12.5), "currency"), "$" );
same(nf({style: "percent"}).format("0.12"), "12%");
same(nf({style: "unit", unit:"percent"}).formatToParts(12).some(p => p.type === "unit"), true);
same(nf({style: "unit", unit:"percent"}).formatToParts(12).some(p => p.type === "percentSign"), false);
same(nf({style: "percent",notation:"compact"}).formatToParts(100).some(p => p.type === "unit"), false);
same(values(nf({notation: "scientific"}).formatToParts("0.001"), "exponentMinusSign"), "-");
for (const style of ["decimal","percent","currency","unit"]) {
    const f = new Intl.NumberFormat("ar-EG", {style, currency: "USD", unit: "meter", signDisplay: "always"});
    same(joined(f.formatToParts("12345.67")), f.format("12345.67"));
    same(joined(f.formatRangeToParts("12345.67", "12346.89")), f.formatRange("12345.67", "12346.89"));
}
/* ICU quantity identity includes signs; ECMA identity compares final text. */
for (const [signDisplay, start, end] of [["never",-1,1],["negative",-0,0]]) {
    const equal = nf({signDisplay});
    same(equal.format(start), equal.format(end));
    same(equal.formatRange(start,end), equal.formatRange(start,start));
    const equalParts = equal.formatRangeToParts(start,end);
    same(joined(equalParts), equal.formatRange(start,end));
    same(equalParts.every(p => p.source === "shared"), true);
    same(equalParts.some(p => p.type === "approximatelySign"), true);
    same(values(equalParts, "integer"), values(equal.formatToParts(start), "integer"));
}
const range = nf({maximumFractionDigits: 0});
same(joined(range.formatRangeToParts(1, 2)), range.formatRange(1, 2));
same(range.formatRangeToParts(1, 2).some(p => p.source === "startRange"), true);
same(range.formatRangeToParts(1, 2).some(p => p.source === "endRange"), true);
same(range.formatRangeToParts(1, 1).every(p => p.source === "shared"), true);
same(range.formatRangeToParts(1, 1).some(p => p.type === "approximatelySign"), true);
throws(TypeError, () => range.formatRange(undefined, {valueOf() {throw Error("must not coerce");}}));
const endpointOrder = [];
throws(RangeError, () => range.formatRange({valueOf() {endpointOrder.push("start"); return NaN;}}, {valueOf() {endpointOrder.push("end"); return 2;}}));
same(endpointOrder.join(), "start,end");
const exactRange = exact.formatRangeToParts("9007199254740993.125", "9007199254740995.875");
same(values(exactRange.filter(p => p.source === "startRange"), "integer"), "9007199254740993");

const bound = exact.format;
same(bound, exact.format);
same(bound.length, 1); same(bound.name, "");
same(bound.call(null, "9007199254740993.125"), "9007199254740993.125");
throws(TypeError, () => Reflect.construct(bound, [1]));
for (const method of ["formatToParts", "formatRange", "formatRangeToParts", "resolvedOptions"])
    throws(TypeError, () => Intl.NumberFormat.prototype[method].call({}, 1, 2));
const fake = Object.create(Intl.NumberFormat.prototype);
const legacyResult = Intl.NumberFormat.call(fake, "en-US", {useGrouping:false});
/* Default CONFIG_INTL_LEGACY=y; disabled-profile runner sets this false. */
const legacyExpected = typeof globalThis.intlLegacyExpected === "undefined" || globalThis.intlLegacyExpected;
if (legacyExpected) {
    same(legacyResult, fake);
    same(fake.format(1234), "1234");
    same(fake.resolvedOptions().locale, "en-US");
    throws(TypeError, () => fake.formatToParts(1));
    throws(TypeError, () => fake.formatRange(1,2));
    throws(TypeError, () => Intl.NumberFormat.call(fake, "en-US"));
    const symbol = Object.getOwnPropertySymbols(fake)[0];
    const descriptor = Object.getOwnPropertyDescriptor(fake, symbol);
    same(descriptor.writable, false); same(descriptor.enumerable, false); same(descriptor.configurable, false);
} else {
    same(legacyResult instanceof Intl.NumberFormat, true);
    same(legacyResult.format(1234), "1234");
    same(Object.getOwnPropertySymbols(fake).length, 0);
    throws(TypeError, () => fake.format);
    throws(TypeError, () => fake.resolvedOptions());
}
