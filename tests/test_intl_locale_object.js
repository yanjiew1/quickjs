/* Intl.Locale identity, option order, branding and intrinsic realm regressions.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
import * as std from "std";
import { assert, assertThrows } from "./assert.js";

const Locale = Intl.Locale;
assert(Locale.name, "Locale");
assert(Locale.length, 1);
assertThrows(TypeError, () => Locale("en"));
for (const value of [undefined, null, 1, true, Symbol(), 1n])
    assertThrows(TypeError, () => new Locale(value));
assertThrows(TypeError, () => new Locale("invalid_tag", null));
assertThrows(RangeError, () => new Locale("invalid_tag", {}));
const locale = new Locale("und-Armn-SU", { language: "ru" });
assert(locale.toString(), "ru-Armn-AM");
assert(locale.baseName, "ru-Armn-AM");
assert(locale.language, "ru");
assert(locale.script, "Armn");
assert(locale.region, "AM");
assert(locale.variants, undefined);
assert(Object.prototype.toString.call(locale), "[object Intl.Locale]");

const complete = new Locale("en-US", {
    calendar: "islamicc", collation: "foobar", firstDayOfWeek: 7,
    hourCycle: "h23", caseFirst: "upper", numeric: true, numberingSystem: "arab"
});
assert(complete.toString(), "en-US-u-ca-islamic-civil-co-foobar-fw-sun-hc-h23-kf-upper-kn-nu-arab");
assert(complete.calendar, "islamic-civil");
assert(complete.collation, "foobar");
assert(complete.firstDayOfWeek, "sun");
assert(complete.hourCycle, "h23");
assert(complete.caseFirst, "upper");
assert(complete.numeric, true);
assert(complete.numberingSystem, "arab");
const empty = new Locale("en-u-ca-kn-false");
assert(empty.calendar, "");
assert(empty.numeric, false);
assert(empty.hourCycle, undefined);
assert(new Locale("en").numeric, false);
assert(new Locale("en", { variants: "rozaj-biske-1994" }).variants, "1994-biske-rozaj");

const names = ["language", "script", "region", "variants", "calendar", "collation",
               "firstDayOfWeek", "hourCycle", "caseFirst", "numeric", "numberingSystem"];
let events = [];
const target = new Proxy(function () {}, {
    get(object, key) {
        if (key === "prototype") events.push("prototype");
        return Reflect.get(object, key);
    }
});
const options = new Proxy({}, {
    get(object, key) { events.push(key); return undefined; }
});
Reflect.construct(Locale, [{ toString() { events.push("tag"); return "en"; } }, options], target);
assert(events.join(","), ["prototype", "tag"].concat(names).join(","));
events = [];
assertThrows(RangeError, () => new Locale("en", {
    get language() { events.push("language"); return "bad_language"; },
    get script() { events.push("script"); return "Latn"; }
}));
assert(events.join(","), "language");
events = [];
assertThrows(RangeError, () => new Locale("invalid_tag", options));
assert(events.length, 0);
const abrupt = {};
let caught;
try {
    new Locale("en", {
        calendar: { toString() { throw abrupt; } },
        get collation() { throw Error("later getter must not run"); }
    });
} catch (error) {
    caught = error;
}
assert(caught, abrupt);
for (const name of names.filter(name => name !== "numeric"))
    assertThrows(RangeError, () => new Locale("en", { [name]: "en\0US" }));
for (const variants of ["", "rozaj-ROZAJ", "abcde-u-kn", "abcde-x-foo", "abcd"])
    assertThrows(RangeError, () => new Locale("en", { variants }));

let calls = 0;
const original = new Locale("sh");
original.toString = () => { calls++; return "fr"; };
assert(new Locale(original).toString(), "sr-Latn");
assert(Intl.getCanonicalLocales(original)[0], "sr-Latn");
assert(Intl.getCanonicalLocales([original])[0], "sr-Latn");
assert(calls, 0);
const proxy = new Proxy(original, {});
assert(new Locale(proxy).toString(), "fr");
assert(Intl.getCanonicalLocales([proxy])[0], "fr");
assert(calls, 2);

for (const name of ["baseName", "language", "script", "region", "variants", "calendar",
                    "collation", "firstDayOfWeek", "hourCycle", "caseFirst", "numeric", "numberingSystem"]) {
    const descriptor = Object.getOwnPropertyDescriptor(Locale.prototype, name);
    assert(descriptor.enumerable, false);
    assert(descriptor.configurable, true);
    assert(descriptor.get.length, 0);
    assert(descriptor.set, undefined);
    assertThrows(TypeError, () => descriptor.get.call({}));
}
assertThrows(TypeError, () => Locale.prototype.toString());
const unbranded = new Proxy(locale, {});
assertThrows(TypeError, () => Locale.prototype.toString.call(unbranded));
assertThrows(TypeError, () => Locale.prototype.maximize.call(unbranded));
const maximizable = new Locale("zh-Hant-TW-u-kn-x-foo");
Object.defineProperty(maximizable, "constructor", { get() { throw Error("unexpected constructor"); } });
Intl.Locale = function () { throw Error("unexpected global constructor"); };
std.gc();
const minimized = maximizable.minimize();
assert(minimized.toString(), "zh-TW-u-kn-x-foo");
assert(Object.getPrototypeOf(minimized), Locale.prototype);
assert(new Locale("und").maximize().toString(), "en-Latn-US");
Intl.Locale = Locale;

class SubLocale extends Locale {}
assert(Object.getPrototypeOf(new SubLocale("en")), SubLocale.prototype);
assert(Object.getPrototypeOf(new SubLocale("en").maximize()), Locale.prototype);
const realm = std.createRealm();
const foreignTarget = realm.evalScript("(function ForeignTarget() {})");
foreignTarget.prototype = null;
const foreignPrototype = realm.global.Intl.Locale.prototype;
assert(Object.getPrototypeOf(Reflect.construct(Locale, ["en"], foreignTarget)), foreignPrototype);
assert(Object.getPrototypeOf(foreignPrototype.maximize.call(new Locale("en"))), foreignPrototype);
