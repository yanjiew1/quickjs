/* Global Intl inventories, coercion, result realms and executable capabilities.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
import * as std from "std";
import { assert, assertThrows } from "./assert.js";

const supported = Intl.supportedValuesOf;
const descriptor = Object.getOwnPropertyDescriptor(Intl, "supportedValuesOf");
assert(supported.name, "supportedValuesOf");
assert(supported.length, 1);
assert(descriptor.writable && descriptor.configurable);
assert(descriptor.enumerable, false);
assertThrows(TypeError, () => new supported("calendar"));

const inventories = {};
for (const key of ["calendar", "collation", "currency", "numberingSystem", "timeZone", "unit"]) {
    const values = supported.call(null, key);
    assert(Array.isArray(values));
    assert(values.every(value => typeof value === "string"));
    assert(new Set(values).size, values.length);
    assert(values.join(","), [...values].sort().join(","));
    assert(values !== supported(key));
    inventories[key] = values;
    const fresh = supported(key);
    fresh[0] = "not-a-supported-value";
    assert(supported(key).join(","), values.join(","));
}

// Meson supplies the selected arithmetic/rule providers. Assert their promised
// capabilities, so adapting individual witnesses cannot hide under-reporting.
if (scriptArgs.length > 1) {
    assert(["iso", "native"].includes(scriptArgs[1]));
    const calendars = scriptArgs[1] === "iso" ? ["gregory", "iso8601"] : [
        "buddhist", "chinese", "coptic", "dangi", "ethioaa", "ethiopic",
        "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla",
        "islamic-umalqura", "iso8601", "japanese", "persian", "roc",
    ];
    assert(inventories.calendar.join(","), calendars.join(","));
    assert(["minimal", "native"].includes(scriptArgs[2]));
    if (scriptArgs[2] === "minimal")
        assert(inventories.timeZone.join(","), "UTC");
    else
        assert(inventories.timeZone.includes("America/New_York"));
}

let calls = 0;
assert(supported({ [Symbol.toPrimitive](hint) {
    assert(hint, "string");
    calls++;
    return "unit";
} }).join(","), inventories.unit.join(","));
assert(calls, 1);
for (const key of [undefined, null, 1, true, "Calendar", "timezone", "", "unit\0", "unit\0junk"])
    assertThrows(RangeError, () => supported(key));
assertThrows(RangeError, () => supported());
assertThrows(TypeError, () => supported(Symbol()));
const marker = {};
try {
    supported({ toString() { throw marker; } });
    throw Error("expected coercion to throw");
} catch (error) {
    assert(error, marker);
}

// Enumeration uses canonical names, while negotiation may accept aliases.
assert(inventories.calendar.includes("gregory"));
assert(inventories.calendar.includes("iso8601"));
for (const alias of ["gregorian", "islamic", "islamicc", "ethiopic-amete-alem"])
    assert(inventories.calendar.includes(alias), false);
for (const calendar of inventories.calendar)
    assert(new Intl.DateTimeFormat("en", { calendar }).resolvedOptions().calendar, calendar);
for (const collation of inventories.collation)
    assert(!["default", "search", "standard"].includes(collation));

// Both shipped profiles expose their numeric digit mappings under English;
// this also checks actual option negotiation beyond the service catalog query.
for (const numberingSystem of inventories.numberingSystem) {
    assert(new Intl.NumberFormat("en", { numberingSystem }).resolvedOptions().numberingSystem,
           numberingSystem);
    assert(new Intl.DateTimeFormat("en", { numberingSystem }).resolvedOptions().numberingSystem,
           numberingSystem);
    assert(new Intl.RelativeTimeFormat("en", { numberingSystem }).resolvedOptions().numberingSystem,
           numberingSystem);
}
assert(inventories.numberingSystem.includes("latn"));
for (const unit of inventories.unit) {
    assert(unit.includes("-per-"), false);
    assert(new Intl.NumberFormat("en", { style: "unit", unit }).resolvedOptions().unit, unit);
}
assert(inventories.unit.includes("meter"));
assert(inventories.timeZone.includes("UTC"));
for (const timeZone of inventories.timeZone)
    assert(new Intl.DateTimeFormat("en", { timeZone }).resolvedOptions().timeZone, timeZone);
if (inventories.timeZone.length > 1) {
    assert(inventories.timeZone.includes("Etc/GMT+1"));
    assert(inventories.timeZone.includes("Asia/Kolkata"));
    assert(inventories.timeZone.includes("Asia/Calcutta"), false);
}

// The global currency inventory is not limited to English translations. CLDR's
// Chuvash XAD name is available only when that locale belongs to this profile.
assert(inventories.currency.includes("USD"));
assert(inventories.currency.includes("QQQ"), false);
const hasChuvash = Intl.DisplayNames.supportedLocalesOf("cv").length !== 0;
assert(inventories.currency.includes("XAD"), hasChuvash);
if (hasChuvash) {
    assert(new Intl.DisplayNames("cv", { type: "currency", fallback: "none" }).of("XAD") !== undefined);
    assert(new Intl.DisplayNames("en", { type: "currency", fallback: "none" }).of("XAD"), undefined);
}
for (const currency of inventories.currency)
    assert(new Intl.NumberFormat("en", { style: "currency", currency }).resolvedOptions().currency,
           currency);

const foreign = std.createRealm();
const result = foreign.global.Intl.supportedValuesOf.call(Intl, "unit");
assert(Object.getPrototypeOf(result) === foreign.global.Array.prototype);
assert(result.join(","), inventories.unit.join(","));
Object.defineProperty(Array.prototype, "0", {
    set() { throw Error("inherited array setter must not be called"); },
    configurable: true
});
try {
    assert(supported("unit")[0], inventories.unit[0]);
} finally {
    delete Array.prototype[0];
}
