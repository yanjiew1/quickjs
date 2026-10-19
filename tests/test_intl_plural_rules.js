/* Intl.PluralRules options, exact values, rounding, branding and realms. */
import * as std from "std";
import { assert, assertThrows, assertArrayEquals } from "./assert.js";

const Plural = Intl.PluralRules;
assert(Plural.name, "PluralRules");
assert(Plural.length, 0);
assertThrows(TypeError, () => Plural());
assertThrows(TypeError, () => Plural.call(undefined));
for (const Constructor of [Intl.Locale, Intl.ListFormat, Plural])
    assertThrows(TypeError, () => Plural.call(new Constructor("en")));
assertThrows(TypeError, () => new Plural("en", null));
for (const options of [false, 1, 1n, "", Symbol()])
    assert(new Plural("en", options).select(1), "one");
assertThrows(RangeError, () => new Plural("invalid_tag"));
for (const [key, value] of [["type", "other"], ["type", "cardinal\0"], ["notation", "short"],
                            ["compactDisplay", "medium"], ["roundingMode", "half"]])
    assertThrows(RangeError, () => new Plural("en", {[key]: value}));
for (const key of ["type", "notation", "compactDisplay", "roundingMode"])
    assertThrows(TypeError, () => new Plural("en", {[key]: Symbol()}));
assertThrows(RangeError, () => new Plural("en", {notation: "standard", compactDisplay: "invalid"}));

let events = [];
const target = new Proxy(function () {}, {
    get(object, key) {
        if (key === "prototype") events.push(key);
        return Reflect.get(object, key);
    }
});
Reflect.construct(Plural, [[{toString() { events.push("locale"); return "en"; }}],
    new Proxy({}, {get(object, key) { events.push(key); }})], target);
assert(events.join(","), "prototype,locale,localeMatcher,type,notation,compactDisplay,minimumIntegerDigits,minimumFractionDigits,maximumFractionDigits,minimumSignificantDigits,maximumSignificantDigits,roundingIncrement,roundingMode,roundingPriority,trailingZeroDisplay");
events = [];
assertThrows(RangeError, () => new Plural("en", {
    get type() { events.push("type"); return "invalid"; },
    get notation() { events.push("notation"); return "standard"; }
}));
assert(events.join(), "type");

const english = new Plural("en");
assert(english.select(1), "one");
assert(english.select(-1), "one");
assert(english.select(1.2), "other");
assert(english.select(), "other");
assert(english.select(NaN), "other");
assert(english.select(Infinity), "other");
assert(english.select(-Infinity), "other");
assert(english.select(-0), "other");
assert(english.select(1n), "one");
assert(english.select(" 1.0000 "), "one");
assert(english.select("9007199254740993.123456789"), "other");
assertThrows(TypeError, () => english.select(Symbol()));
assert(new Plural("en", {minimumFractionDigits: 2}).select(1), "other");
assert(new Plural("en", {minimumFractionDigits: 2, trailingZeroDisplay: "stripIfInteger"}).select(1), "one");
for (const [value, expected] of [[1, "one"], [2, "two"], [3, "few"], [11, "other"], [22, "two"]])
    assert(new Plural("en", {type: "ordinal"}).select(value), expected);
assert(new Plural("en", {maximumFractionDigits: 0, roundingMode: "ceil"}).select(-1.1), "one");
assert(new Plural("en", {maximumFractionDigits: 0, roundingMode: "ceil"}).select(1.1), "other");

const resolved = english.resolvedOptions();
assertArrayEquals(Object.keys(resolved), ["locale", "type", "notation", "minimumIntegerDigits",
    "minimumFractionDigits", "maximumFractionDigits", "pluralCategories", "roundingIncrement",
    "roundingMode", "roundingPriority", "trailingZeroDisplay"]);
assert(resolved.locale, "en");
assert(resolved.type, "cardinal");
assert(resolved.notation, "standard");
assert(resolved.minimumIntegerDigits, 1);
assert(resolved.minimumFractionDigits, 0);
assert(resolved.maximumFractionDigits, 3);
assert(resolved.roundingIncrement, 1);
assert(resolved.roundingMode, "halfExpand");
assert(resolved.roundingPriority, "auto");
assert(resolved.trailingZeroDisplay, "auto");
assertArrayEquals(resolved.pluralCategories, ["one", "other"]);
resolved.pluralCategories.push("spoofed");
assertArrayEquals(english.resolvedOptions().pluralCategories, ["one", "other"]);
for (const notation of ["standard", "scientific", "engineering"])
    assert(new Plural("en", {notation, compactDisplay: "long"}).resolvedOptions().compactDisplay, undefined);
const compact = new Plural("en", {notation: "compact"}).resolvedOptions();
assert(compact.compactDisplay, "short");
assert(compact.minimumFractionDigits, 0);
assert(compact.maximumFractionDigits, 0);
assert(compact.minimumSignificantDigits, 1);
assert(compact.maximumSignificantDigits, 2);
assert(compact.roundingPriority, "morePrecision");
assert(new Plural("en", {minimumSignificantDigits: 2}).resolvedOptions().minimumFractionDigits, undefined);
assert(new Plural("en-u-nu-arab-x-private").resolvedOptions().locale, "en");
assertArrayEquals(Plural.supportedLocalesOf(["EN-u-kn", "en-u-kn", "en-x-private", "zz"]),
                  ["en-u-kn", "en-x-private"]);

assert(english.selectRange(1, 1), "one");
assert(english.selectRange(-1, 1), "one");
assert(english.selectRange(2, 1), "other"); // CLDR explicit range result; descending is valid.
assert(new Plural("en", {maximumFractionDigits: 0}).selectRange(1.1, 1.2), "one");
for (const endpoints of [[], [1], [undefined, 1], [1, undefined]])
    assertThrows(TypeError, () => english.selectRange(...endpoints));
for (const endpoints of [[NaN, 1], [1, NaN], ["NaN", 1]])
    assertThrows(RangeError, () => english.selectRange(...endpoints));
assert(english.selectRange(-Infinity, Infinity), "other");
events = [];
assertThrows(RangeError, () => english.selectRange(
    {[Symbol.toPrimitive](hint) { events.push("start:" + hint); return NaN; }},
    {[Symbol.toPrimitive](hint) { events.push("end:" + hint); return 1; }}));
assert(events.join(), "start:number,end:number");
events = [];
assertThrows(TypeError, () => english.selectRange(
    {[Symbol.toPrimitive]() { events.push("start"); throw new TypeError(); }},
    {[Symbol.toPrimitive]() { events.push("end"); return 1; }}));
assert(events.join(), "start");

assert(Object.prototype.toString.call(english), "[object Intl.PluralRules]");
for (const name of ["select", "selectRange", "resolvedOptions"]) {
    const method = Plural.prototype[name];
    assertThrows(TypeError, () => new method());
    for (const receiver of [undefined, null, false, 1, "", {}, Plural.prototype, new Proxy(english, {})])
        assertThrows(TypeError, () => method.call(receiver));
}
assert(Plural.prototype.select.length, 1);
assert(Plural.prototype.selectRange.length, 2);
class Sub extends Plural {}
const sub = new Sub("en");
assert(Object.getPrototypeOf(sub), Sub.prototype);
assert(sub.select(1), "one");
const realm = std.createRealm();
const foreign = realm.evalScript("Intl.PluralRules");
const foreignValue = new foreign("en");
assert(Plural.prototype.select.call(foreignValue, 1), "one");
const foreignResolved = Plural.prototype.resolvedOptions.call(foreignValue);
assert(Object.getPrototypeOf(foreignResolved), Object.prototype);
assert(Object.getPrototypeOf(foreignResolved.pluralCategories), Array.prototype);
const bareTarget = realm.evalScript("(function C() {})");
bareTarget.prototype = undefined;
const alternate = Reflect.construct(Plural, ["en"], bareTarget);
assert(Object.getPrototypeOf(alternate), foreign.prototype);
std.gc();
assert(alternate.select(1), "one");

// Native-only data assertions, intentionally absent from English minimal mode.
if (Plural.supportedLocalesOf("pl").length) {
    for (const notation of ["scientific", "engineering", "compact"])
        assert(new Plural("pl", {notation, maximumFractionDigits: 0}).select(1202), "few");
    assert(new Plural("pl", {notation: "compact"}).select(1202), "few");
    assert(new Plural("fr").select(1500000), "other");
    assert(new Plural("fr", {notation: "compact"}).select(1500000), "many");
    assert(new Plural("it", {notation: "engineering", maximumFractionDigits: 4}).select(.1), "many");
    assert(new Plural("ar").select("1000000000000000000000000000000000000000000000000000000000000000000003"), "few");
}
