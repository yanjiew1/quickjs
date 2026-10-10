/* Native en/en-US data profile; all observable algorithms run in C.
 * Copyright (c) 2026 Yan-Jie Wang. */
function same(actual, expected, label) {
    if (!Object.is(actual, expected)) throw Error((label || "mismatch") + ": " + actual + " != " + expected);
}
function throws(type, f) {
    try { f(); } catch (e) { if (e instanceof type) return; throw e; }
    throw Error("expected " + type.name);
}
const units = ["years", "months", "weeks", "days", "hours", "minutes", "seconds", "milliseconds", "microseconds", "nanoseconds"];
const singular = units.map(x => x.slice(0, -1));
const join = p => p.map(x => x.value).join("");
const partValue = (p, type) => p.filter(x => x.type === type).map(x => x.value).join("");
same(Intl.DurationFormat.length, 0);
same(new Intl.DurationFormat("en-US").resolvedOptions().locale, "en-US");
same(new Intl.DurationFormat("en-US").resolvedOptions().style, "short");
same(Intl.DurationFormat.supportedLocalesOf(["en", "en-US", "fi"]).join(), "en,en-US");
throws(TypeError, () => Intl.DurationFormat());
for (const style of ["long", "short", "narrow"]) {
    const f = new Intl.DurationFormat("en", {style});
    const record = Object.fromEntries(units.map((x, i) => [x, i + 1]));
    const parts = f.formatToParts(record);
    same(join(parts), f.format(record));
    for (const unit of singular) same(parts.some(x => x.unit === unit && x.type === "integer"), true, style + unit);
    same(parts.every(x => !x.unit || singular.includes(x.unit)), true);
    same(f.format({seconds:0}), "");
    same(f.format({seconds:1}), f.format("PT1S"));
    same(f.format({seconds:-1}), f.format("-PT1S"));
    same(f.format({seconds:1, milliseconds:500}), f.format("PT1.5S"));
    same(new Intl.DurationFormat("en", {style, yearsDisplay:"always"}).format({seconds:-1}).startsWith("-0"), true);
    same(partValue(f.formatToParts({years:-1, seconds:-2}), "minusSign"), "-");
    for (let i = 0; i < units.length; i++) {
        const one = f.formatToParts({[units[i]]:1});
        same(one.every(x => x.unit === singular[i]), true);
    }
}
const digital = new Intl.DurationFormat("en-US", {style:"digital", fractionalDigits:3});
same(digital.format({hours:1, minutes:2, seconds:3, milliseconds:456}), "1:02:03.456");
same(digital.format({hours:0, minutes:-2, seconds:-3}), "-0:02:03.000");
same(digital.format({hours:1234, minutes:2, seconds:3}), "1234:02:03.000");
same(digital.formatToParts({hours:1, minutes:2, seconds:3}).filter(x => x.type === "literal").every(x => !("unit" in x)), true);
same(new Intl.DurationFormat("en", {hours:"numeric", hoursDisplay:"auto", minutesDisplay:"auto", secondsDisplay:"auto"}).format({hours:1, seconds:2}), "1:00:02");
for (let fractionalDigits = 0; fractionalDigits <= 9; fractionalDigits++) {
    const f = new Intl.DurationFormat("en", {seconds:"numeric", fractionalDigits});
    const suffix = fractionalDigits ? "." + "123456789".slice(0, fractionalDigits) : "";
    const record = {seconds:2, milliseconds:123, microseconds:456, nanoseconds:789};
    same(f.format(record), "2" + suffix);
    same(f.format({...record, seconds:-2, milliseconds:-123, microseconds:-456, nanoseconds:-789}), "-2" + suffix);
    same(join(f.formatToParts(record)), f.format(record));
}
same(new Intl.DurationFormat("en", {seconds:"numeric", fractionalDigits:0}).format({nanoseconds:1}), "0");
same(new Intl.DurationFormat("en", {milliseconds:"long", microseconds:"numeric", fractionalDigits:3}).format({milliseconds:1, microseconds:2345}), "3.345 milliseconds");
/* The Number is exactly 1000000000000000128; shortest decimal prints 1000000000000000100. */
same(new Intl.DurationFormat("en", {seconds:"numeric", fractionalDigits:9}).format({nanoseconds:1000000000000000128}), "1000000000.000000128");
const arab = new Intl.DurationFormat("en-u-nu-arab", {style:"digital", fractionalDigits:3});
same(arab.resolvedOptions().numberingSystem, "arab");
same(partValue(arab.formatToParts({hours:1, minutes:2, seconds:3}), "integer"), "١٠٢٠٣");
same(new Intl.DurationFormat("en-u-nu-arab", {numberingSystem:"latn"}).resolvedOptions().numberingSystem, "latn");
const short = new Intl.DurationFormat("en");
for (const input of [undefined, null, 1, true, Symbol(), 1n, {}]) throws(TypeError, () => short.format(input));
for (const input of [{seconds:1.5}, {seconds:NaN}, {seconds:Infinity}, {seconds:1, nanoseconds:-1}, {years:4294967296}, {seconds:9007199254740992}])
    throws(RangeError, () => short.format(input));
for (const input of ["", "PT", "P1X", "PT1.1234567890S", "PT1S\0"]) throws(RangeError, () => short.format(input));
throws(TypeError, () => short.format({seconds:1n}));
throws(RangeError, () => new Intl.DurationFormat("en", {hours:"numeric", minutes:"long"}));
throws(RangeError, () => new Intl.DurationFormat("en", {milliseconds:"numeric", millisecondsDisplay:"always"}));
throws(RangeError, () => new Intl.DurationFormat("en", {numberingSystem:"ab"}));
throws(RangeError, () => new Intl.DurationFormat("en", {fractionalDigits:10}));
const inputGets = [];
short.format(new Proxy({seconds:{valueOf() {inputGets.push("coerce"); return 1;}}}, {get(t, k) {inputGets.push(k); return t[k];}}));
same(inputGets.join(), "days,hours,microseconds,milliseconds,minutes,months,nanoseconds,seconds,coerce,weeks,years");
const optionGets = [];
new Intl.DurationFormat("en", new Proxy({}, {get(t, k) {optionGets.push(k); return undefined;}}));
same(optionGets.join(), ["localeMatcher", "numberingSystem", "style", ...units.flatMap(x => [x, x + "Display"]), "fractionalDigits"].join());
const abruptGets = [], marker = {};
try { short.format(new Proxy({}, {get(t, k) {abruptGets.push(k); if (k === "minutes") throw marker;}})); }
catch (e) { same(e, marker); }
same(abruptGets.join(), "days,hours,microseconds,milliseconds,minutes");
short.format({seconds:9007199254740991, nanoseconds:999999999});
throws(RangeError, () => short.format({seconds:9007199254740991, nanoseconds:1000000000}));
for (const name of ["format", "formatToParts", "resolvedOptions"])
    throws(TypeError, () => Intl.DurationFormat.prototype[name].call({}, {seconds:1}));
same(Object.keys(digital.resolvedOptions()).join(), ["locale", "numberingSystem", "style", ...units.flatMap(x => [x, x + "Display"]), "fractionalDigits"].join());
/* Native slots precede ordinary Get, even when every own field is hostile. */
if (typeof Temporal === "object") {
    const d = Temporal.Duration.from("PT1H2M3.004005006S");
    for (const unit of units) Object.defineProperty(d, unit, {get() {throw Error("native slot Get " + unit);}});
    same(digital.format(d), digital.format("PT1H2M3.004005006S"));
    same(d.toLocaleString("en", {style:"digital", fractionalDigits:3}), digital.format(d));
    const proxy = new Proxy(d, {});
    throws(Error, () => digital.format(proxy));
    const intrinsic = Intl.DurationFormat;
    Intl.DurationFormat = function () {throw Error("replaced public constructor");};
    same(d.toLocaleString("en", {style:"digital", fractionalDigits:3}), digital.format(d));
    Intl.DurationFormat = intrinsic;
}
