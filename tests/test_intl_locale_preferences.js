/* Locale regional preference precedence, result objects and branding.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
import * as std from "std";
import { assert, assertThrows } from "./assert.js";

function week(tag, firstDay, weekend, options) {
    const value = new Intl.Locale(tag, options).getWeekInfo();
    assert(Object.keys(value).join(","), "firstDay,weekend");
    assert(value.firstDay, firstDay);
    assert(value.weekend.join(","), weekend);
}
week("en-US", 7, "6,7");
week("fa-IR", 6, "5");
week("en-u-sd-inka", 7, "7");
week("fa-JP-u-sd-inka", 7, "6,7");
week("fa-JP-u-sd-inka-rg-afzzzz", 6, "4,5");
week("en-US-u-rg-zzzzzz", 7, "6,7");
week("en-US-u-rg-001zzzz", 1, "6,7");
week("fa-u-sd-buzzzz", 7, "6,7");
week("fa-IR-u-rg-buzzzz", 7, "6,7");
week("en-ZZ", 1, "6,7");
week("eo", 1, "6,7");
week("en-US-u-fw-mon", 1, "6,7");
week("en-US-u-fw-foobar", 7, "6,7");
week("en-US", 7, "6,7", { firstDayOfWeek: 0 });
week("en-US", 2, "6,7", { firstDayOfWeek: "tue" });
for (const tag of ["en-US-u-rg-us", "en-US-u-rg-uszzzzz", "en-US-u-rg-usa-zzzz"])
    week(tag, 7, "6,7");

for (const [tag, direction] of [["en", "ltr"], ["ar", "rtl"], ["he", "rtl"],
                                 ["ar-Latn", "ltr"], ["en-Arab", "rtl"],
                                 ["en-Zzzz", undefined], ["en-Qaaa", undefined]]) {
    const result = new Intl.Locale(tag).getTextInfo();
    assert(Object.keys(result).join(","), "direction");
    assert(result.direction, direction);
}
assert(new Intl.Locale("en-US").getHourCycles().join(","), "h12,h23");
assert(new Intl.Locale("en-IL").getHourCycles().join(","), "h23,h12");
assert(new Intl.Locale("he-IL").getHourCycles().join(","), "h23,h12");
assert(new Intl.Locale("ja-JP").getHourCycles().join(","), "h23,h11,h12");
assert(new Intl.Locale("en-ZZ").getHourCycles().join(","), "h23");
assert(new Intl.Locale("en-US-u-rg-jpzzzz").getHourCycles().join(","), "h23,h11,h12");
assert(new Intl.Locale("en-US", { hourCycle: "h24" }).getHourCycles().join(","), "h24");
assert(new Intl.Locale("en-u-hc-foobar").getHourCycles().join(","), "foobar");
assert(new Intl.Locale("en-u-hc").getHourCycles().length, 1);
assert(new Intl.Locale("en-u-hc").getHourCycles()[0], "");

const locale = new Intl.Locale("ar");
for (const name of ["getWeekInfo", "getTextInfo", "getHourCycles"]) {
    const method = Intl.Locale.prototype[name];
    assert(method.name, name);
    assert(method.length, 0);
    assertThrows(TypeError, () => method.call({}));
    assertThrows(TypeError, () => method.call(new Proxy(locale, {})));
    const descriptor = Object.getOwnPropertyDescriptor(Intl.Locale.prototype, name);
    assert(descriptor.enumerable, false);
    assert(descriptor.writable, true);
    assert(descriptor.configurable, true);
    const a = method.call(locale);
    const b = method.call(locale);
    assert(a !== b);
    const foreign = std.createRealm();
    const foreignMethod = foreign.global.Intl.Locale.prototype[name];
    const result = foreignMethod.call(locale);
    assert(Object.getPrototypeOf(result), name === "getHourCycles"
        ? foreign.global.Array.prototype : foreign.global.Object.prototype);
}
const first = locale.getWeekInfo();
first.weekend.length = 0;
assert(locale.getWeekInfo().weekend.length, 2);
for (const key of ["direction", "firstDay", "weekend", "0"]) {
    Object.defineProperty(Object.prototype, key, {
        set() { throw new Error("inherited setter must not be called"); }, configurable: true
    });
}
try {
    assert(locale.getTextInfo().direction, "rtl");
    assert(locale.getWeekInfo().firstDay, 6);
    assert(locale.getHourCycles().length > 0);
} finally {
    for (const key of ["direction", "firstDay", "weekend", "0"])
        delete Object.prototype[key];
}
