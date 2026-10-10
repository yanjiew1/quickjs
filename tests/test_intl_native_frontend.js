/* Native development profile only. These JS integration units were prepared,
 * not executed, by the source owner. Root gates execution and OOM behavior. */
import { assert, assertThrows, assertArrayEquals } from "./assert.js";

assertArrayEquals(Intl.getCanonicalLocales(["IW", "he", "en-us"]), ["he", "en-US"]);
assert(Intl.getCanonicalLocales("en-u-ca-islamicc-kn-true")[0],
       "en-u-ca-islamic-civil-kn");
for (const tag of ["en_US", "en\0-US", "x-private", "en-u", "sl-rozaj-ROZAJ"])
    assertThrows(RangeError, () => Intl.getCanonicalLocales(tag));
assert(new Intl.Locale("zh-Hant-TW-u-kn-x-foo").minimize().toString(),
       "zh-TW-u-kn-x-foo");
assert(new Intl.Locale("en-u-kn-x-foo").maximize().toString(),
       "en-Latn-US-u-kn-x-foo");

const localeGets = [];
new Intl.Locale("en", new Proxy({}, { get(target, key) {
    localeGets.push(key); return undefined;
} }));
assertArrayEquals(localeGets, ["language", "script", "region", "variants",
    "calendar", "collation", "firstDayOfWeek", "hourCycle", "caseFirst",
    "numeric", "numberingSystem"]);
class LocaleSubclass extends Intl.Locale {}
assert(new LocaleSubclass("en") instanceof LocaleSubclass, true);
assertThrows(TypeError, () => Intl.Locale.prototype.getWeekInfo.call({}));

for (const [tag, method, value] of [
    ["en-u-ca-unknown", "getCalendars", "unknown"],
    ["en-u-co-unknown", "getCollations", "unknown"],
    ["en-u-hc-unknown", "getHourCycles", "unknown"],
    ["en-u-nu-unknown", "getNumberingSystems", "unknown"],
    ["en-u-hc", "getHourCycles", ""],
]) assertArrayEquals(new Intl.Locale(tag)[method](), [value]);
assertThrows(RangeError, () => new Intl.Locale("en", {hourCycle: "unknown"}));
assert(new Intl.Locale("en").getTimeZones(), undefined);
assert(new Intl.Locale("ar").getTextInfo().direction, "rtl");
const undirected = new Intl.Locale("und-Zyyy").getTextInfo();
assert(undirected.direction, undefined);
assertArrayEquals(Object.keys(undirected), ["direction"]);
assert(Object.getOwnPropertyDescriptor(undirected, "direction").enumerable, true);
const week = new Intl.Locale("en-US-u-fw-mon").getWeekInfo();
assert(week.firstDay, 1); assertArrayEquals(week.weekend, [6, 7]);
assertArrayEquals(Object.keys(week), ["firstDay", "weekend"]);
for (const method of ["getCalendars", "getCollations", "getNumberingSystems"])
    assertThrows(InternalError, () => new Intl.Locale("en")[method]());
assertThrows(InternalError, () => new Intl.Locale("en-US").getTimeZones());

const f = new Intl.ListFormat();
assert(f.resolvedOptions().locale, "en-US");
assert(new Intl.ListFormat("en-GB").resolvedOptions().locale, "en");
assert(new Intl.ListFormat("zz").resolvedOptions().locale, "en-US");
assertArrayEquals(Intl.ListFormat.supportedLocalesOf(["en-US", "en-GB", "fr", "zz"]),
                  ["en-US", "en-GB"]);
assert(f.format(), ""); assertArrayEquals(f.formatToParts(), []);
assert(f.format(["A", "B", "C"]), "A, B, and C");
/* Direct witnesses from pinned CLDR49 en listPatterns, including the unit
 * narrow spacing difference. These expectations are independent of parts. */
for (const [type, style, expected] of [
    ["conjunction", "long", "A, B, and C"],
    ["conjunction", "short", "A, B, & C"],
    ["conjunction", "narrow", "A, B, C"],
    ["disjunction", "long", "A, B, or C"],
    ["disjunction", "short", "A, B, or C"],
    ["disjunction", "narrow", "A, B, or C"],
    ["unit", "long", "A, B, C"],
    ["unit", "short", "A, B, C"],
    ["unit", "narrow", "A B C"],
]) assert(new Intl.ListFormat("en", {type, style}).format(["A", "B", "C"]), expected);
for (const type of ["conjunction", "disjunction", "unit"])
    for (const style of ["long", "short", "narrow"])
        for (const list of [[], [""], ["", ""], ["A", "", "B"],
                           ["A\0B", "\ud800", "C"], ["\ud83d\ude00", "\udc00"]]) {
            const formatter = new Intl.ListFormat("en", {type, style});
            const parts = formatter.formatToParts(list);
            assert(parts.map(p => p.value).join(""), formatter.format(list));
            assertArrayEquals(parts.filter(p => p.type === "element").map(p => p.value), list);
            for (const part of parts) assertArrayEquals(Object.keys(part), ["type", "value"]);
        }
const reads = [];
new Intl.ListFormat("en", new Proxy({}, { get(target, key) {
    reads.push(key); return undefined;
} }));
assertArrayEquals(reads, ["localeMatcher", "type", "style"]);
assertThrows(TypeError, () => f.format.call(new Proxy(f, {}), []));
assertThrows(TypeError, () => f.format([new String("x")]));
class ListSubclass extends Intl.ListFormat {}
assert(new ListSubclass("en") instanceof ListSubclass, true);
let closed = 0, coerced = 0;
const input = { [Symbol.iterator]() { return {
    next() { return {done: false, value: {toString() { coerced++; return "x"; }}}; },
    return() { closed++; throw Error("must not replace TypeError"); },
}; } };
assertThrows(TypeError, () => f.format(input));
assert(closed, 1); assert(coerced, 0);
let returns = 0; const original = Error("iterator failure");
const broken = { [Symbol.iterator]() { return {
    next() { throw original; }, return() { returns++; return {}; },
}; } };
try { f.format(broken); assert(false); } catch (error) { assert(error, original); }
assert(returns, 0);
assertEquals(new Intl.NumberFormat("en").resolvedOptions().locale, "en");
assertArrayEquals(Intl.Collator.supportedLocalesOf("en"), ["en"]);
assertThrows(InternalError, () => Intl.supportedValuesOf("calendar"));
assertThrows(RangeError, () => Intl.supportedValuesOf("calendar\0"));
