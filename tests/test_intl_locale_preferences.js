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

for (const tag of ["en", "en-US", "en-US-1996", "ar", "fa", "bn", "th", "zh-Hant"]) {
    const locale = new Intl.Locale(tag);
    const expected = Intl.NumberFormat.supportedLocalesOf(tag).length
        ? new Intl.NumberFormat(tag).resolvedOptions().numberingSystem : "latn";
    assert(locale.getNumberingSystems().join(","), expected);
}
assert(new Intl.Locale("zz").getNumberingSystems().join(","), "latn");
assert(new Intl.Locale("zz-ZZ").getNumberingSystems().join(","), "latn");
for (const numberingSystem of ["arab", "foobar", "latn"]) {
    assert(new Intl.Locale("en", {numberingSystem}).getNumberingSystems().join(","), numberingSystem);
    assert(new Intl.Locale("zz-u-nu-" + numberingSystem).getNumberingSystems().join(","), numberingSystem);
}
assert(new Intl.Locale("en-u-nu").getNumberingSystems().length, 1);
assert(new Intl.Locale("en-u-nu").getNumberingSystems()[0], "");
const numbers = new Intl.Locale("en").getNumberingSystems();
numbers[0] = "foobar";
assert(new Intl.Locale("en").getNumberingSystems().join(","), "latn");

// Explicit ca slots bypass availability, including unknown and empty values.
for (const calendar of ["foobar", "gregory", "buddhist", "islamic"]) {
    assert(new Intl.Locale("en", {calendar}).getCalendars().join(","), calendar);
    assert(new Intl.Locale("en-u-ca-" + calendar).getCalendars().join(","), calendar);
}
assert(new Intl.Locale("en-u-ca").getCalendars().length, 1);
assert(new Intl.Locale("en-u-ca").getCalendars()[0], "");
assert(new Intl.Locale("en-u-ca-islamicc").getCalendars().join(","), "islamic-civil");
const thaiCalendars = new Intl.DateTimeFormat("en", {calendar: "buddhist"})
    .resolvedOptions().calendar === "buddhist" ? "buddhist,gregory" : "gregory";
for (const tag of ["th", "en-TH", "en-u-sd-th10", "en-US-u-rg-thzzzz"])
    assert(new Intl.Locale(tag).getCalendars().join(","), thaiCalendars);
for (const tag of ["en", "en-US", "en-ZZ", "en-US-u-rg-zzzzzz"])
    assert(new Intl.Locale(tag).getCalendars().join(","), "gregory");
const calendars = new Intl.Locale("en").getCalendars();
calendars[0] = "foobar";
assert(new Intl.Locale("en").getCalendars().join(","), "gregory");
// Generic islamic remains a valid explicit negotiation request, but Stage 4's
// canonical AvailableCalendars table excludes it from implicit preference lists.
for (const tag of ["en-EG", "en-IR", "en-SA"])
    assert(new Intl.Locale(tag).getCalendars().includes("islamic"), false);

// Zone preferences use explicit region only, rather than calendar/hour rules.
for (const tag of ["en", "en-u-rg-uszzzz", "en-u-sd-usca", "en-u-tz-usnyc"])
    assert(new Intl.Locale(tag).getTimeZones(), undefined);
assert(new Intl.Locale("en-001").getTimeZones().join(","), "");
assert(new Intl.Locale("en-ZZ").getTimeZones().join(","), "");
const usZones = new Intl.Locale("en-US").getTimeZones();
assert(new Intl.Locale("en-US-u-rg-jpzzzz-sd-th10-tz-jptyo").getTimeZones().join(","), usZones.join(","));
assert(new Set(usZones).size, usZones.length);
assert([...usZones].sort().join(","), usZones.join(","));
// Minimal UTC-only timezone builds have no US regional zone capability.
if (usZones.length) {
    assert(usZones.includes("America/New_York"));
    assert(new Intl.Locale("en-IN").getTimeZones().includes("Asia/Kolkata"));
    assert(new Intl.Locale("en-UA").getTimeZones().includes("Europe/Kyiv"));
}
const originalZones = usZones.join(",");
usZones.length = 0;
assert(new Intl.Locale("en-US").getTimeZones().join(","), originalZones);

const locale = new Intl.Locale("ar-US");
for (const name of ["getWeekInfo", "getTextInfo", "getHourCycles", "getNumberingSystems", "getCalendars", "getTimeZones"]) {
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
    assert(Object.getPrototypeOf(result), ["getHourCycles", "getNumberingSystems", "getCalendars", "getTimeZones"].includes(name)
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
    assert(locale.getWeekInfo().firstDay, 7);
    assert(locale.getHourCycles().length > 0);
    assert(locale.getNumberingSystems().length, 1);
    assert(locale.getCalendars().length > 0);
    assert(Array.isArray(locale.getTimeZones()));
} finally {
    for (const key of ["direction", "firstDay", "weekend", "0"])
        delete Object.prototype[key];
}
