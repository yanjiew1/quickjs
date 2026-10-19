/* Intl.ListFormat construction, negotiation, branding and realm contracts. */
import * as std from "std";
import { assert, assertThrows } from "./assert.js";

const List = Intl.ListFormat;
assert(List.name, "ListFormat");
assert(List.length, 0);
assertThrows(TypeError, () => List());
assertThrows(RangeError, () => new List("not_a_locale"));
for (const value of [null, false, 1, 1n, "", Symbol()])
    assertThrows(TypeError, () => new List("en", value));
for (const localeMatcher of ["bad", "lookup\0", null, true])
    assertThrows(RangeError, () => new List("en", { localeMatcher }));
for (const type of ["and", "unit\0", null, false])
    assertThrows(RangeError, () => new List("en", { type }));
for (const style of ["wide", "short\0", null, 0])
    assertThrows(RangeError, () => new List("en", { style }));
for (const key of ["localeMatcher", "type", "style"])
    assertThrows(TypeError, () => new List("en", { [key]: Symbol() }));
for (const type of ["conjunction", "disjunction", "unit"]) {
    for (const style of ["long", "short", "narrow"]) {
        const resolved = new List("en", { type, style }).resolvedOptions();
        assert(JSON.stringify(resolved), JSON.stringify({ locale: "en", type, style }));
    }
}
assert(new List("en-u-nu-arab-x-private").resolvedOptions().locale, "en");
assert(new List("en-US").resolvedOptions().locale, "en-US");
const defaultListLocale = new List().resolvedOptions().locale;
for (const localeMatcher of ["lookup", "best fit"]) {
    for (const [locales, expected] of [
        [undefined, defaultListLocale], ["EN", "en"], [[], defaultListLocale],
        [["en", "EN"], "en"], [["EN", "en"], "en"],
        [{ 0: "EN", length: 0 }, defaultListLocale], [{ 0: "EN", length: 1 }, "en"]
    ]) {
        assert(new List(locales, { localeMatcher }).resolvedOptions().locale, expected);
    }
    for (const locales of [["en-GB-oed"], ["x-private"]])
        assertThrows(RangeError, () => new List(locales, { localeMatcher }));
}
assert(new List("zz").resolvedOptions().locale, new List().resolvedOptions().locale);
assert(Object.prototype.toString.call(new List("en")), "[object Intl.ListFormat]");
const branded = new Intl.Locale("en-u-kn");
branded.toString = () => { throw Error("Locale internal identity expected"); };
assert(new List(branded).resolvedOptions().locale, "en");

assert(JSON.stringify(List.supportedLocalesOf(["EN-u-kn", "en-u-kn", "zz", "en-x-a"])),
       '["en-u-kn","en-x-a"]');
assert(JSON.stringify(List.supportedLocalesOf("zz")), "[]");
for (const value of [false, 1, 1n, "", Symbol()])
    assert(List.supportedLocalesOf("en", value).join(","), "en");
assertThrows(TypeError, () => List.supportedLocalesOf("en", null));
assertThrows(RangeError, () => List.supportedLocalesOf("en", { localeMatcher: "bad" }));

let events = [];
const target = new Proxy(function () {}, {
    get(object, key) {
        if (key === "prototype") events.push("prototype");
        return Reflect.get(object, key);
    }
});
const input = [{ toString() { events.push("locale"); return "en"; } }];
const options = new Proxy({}, {
    get(object, key) { events.push(key); return undefined; }
});
Reflect.construct(List, [input, options], target);
assert(events.join(","), "prototype,locale,localeMatcher,type,style");
events = [];
assertThrows(RangeError, () => new List("bad_tag", options));
assert(events.length, 0);
assertThrows(RangeError, () => new List("en", {
    get localeMatcher() { events.push("matcher"); return "invalid"; },
    get type() { events.push("type"); return "unit"; }
}));
assert(events.join(","), "matcher");
events = [];
assertThrows(RangeError, () => new List("en", {
    get type() { events.push("type"); return "invalid"; },
    get style() { events.push("style"); return "short"; }
}));
assert(events.join(","), "type");
const abrupt = {};
let caught;
events = [];
try {
    Reflect.construct(List, [input, options], new Proxy(function () {}, {
        get(object, key) { if (key === "prototype") throw abrupt; return Reflect.get(object, key); }
    }));
} catch (error) {
    caught = error;
}
assert(caught, abrupt);
assert(events.length, 0);
caught = undefined;
try {
    new List("en", { type: { toString() { throw abrupt; } } });
} catch (error) {
    caught = error;
}
assert(caught, abrupt);

const instance = new List("en");
for (const key of ["locale", "type", "style"])
    Object.defineProperty(Object.prototype, key, { configurable: true, set() { throw Error("inherited setter"); } });
try {
    assert(JSON.stringify(instance.resolvedOptions()), '{"locale":"en","type":"conjunction","style":"long"}');
} finally {
    for (const key of ["locale", "type", "style"])
        delete Object.prototype[key];
}
assertThrows(TypeError, () => List.prototype.resolvedOptions());
assertThrows(TypeError, () => List.prototype.resolvedOptions.call(new Proxy(instance, {})));
const resolved = instance.resolvedOptions();
assert(Object.keys(resolved).join(","), "locale,type,style");
assert(resolved !== instance.resolvedOptions());
for (const key of Object.keys(resolved)) {
    const desc = Object.getOwnPropertyDescriptor(resolved, key);
    assert(desc.writable && desc.enumerable && desc.configurable);
}
const realm = std.createRealm();
const foreign = realm.evalScript("Intl.ListFormat");
const foreignObject = realm.evalScript("Object.prototype");
assert(Object.getPrototypeOf(foreign.prototype.resolvedOptions.call(instance)), foreignObject);
const fallback = realm.evalScript("(function Target() {})");
fallback.prototype = null;
assert(Object.getPrototypeOf(Reflect.construct(List, ["en"], fallback)), foreign.prototype);
std.gc();
assert(instance.resolvedOptions().locale, "en");
