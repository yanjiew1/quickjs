/*
 * Copyright (c) 2026 Yan-Jie Wang
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

/* Full ICU4X date-program, name and calendar bindings. */
import * as std from "std";
import { assert } from "./assert.js";

// Exercise the full binding across public calendar, number and timezone
// providers. Rendering/parts must agree even when names use a companion gap.
const instant = new Date("2020-05-25T12:00:00Z");
const calendars = Intl.supportedValuesOf("calendar");
for (const locale of ["en", "ru", "ja", "ar-EG"]) {
    for (const calendar of calendars) {
        for (const style of ["full", "long", "medium", "short"]) {
            const formatter = new Intl.DateTimeFormat(locale, {
                calendar, dateStyle: style, timeStyle: style,
                timeZone: "America/New_York",
            });
            const value = formatter.format(instant);
            assert(value.length > 0);
            assert(formatter.formatToParts(instant).map(p => p.value).join(""), value);
        }
    }
}
const chinese = new Intl.DateTimeFormat("en", {
    calendar: "chinese", month: "long", day: "numeric", timeZone: "UTC",
});
assert(chinese.format(instant), "Fourth Monthbis 3");
const japanese = new Intl.DateTimeFormat("en", {
    calendar: "japanese", era: "long", year: "numeric", timeZone: "UTC",
});
// Historical Japanese uses Gregorian era sources, not modern-era indexes.
assert(japanese.format(new Date("1800-01-01T00:00:00Z")), "1800 AD");
const russian = new Intl.DateTimeFormat("ru", {
    month: "long", day: "numeric", timeZone: "UTC",
});
assert(russian.format(instant), "25 мая");
const range = russian.formatRange(instant, new Date("2020-05-27T12:00:00Z"));
assert(russian.formatRangeToParts(instant, new Date("2020-05-27T12:00:00Z"))
    .map(p => p.value).join(""), range);
std.gc();
assert(russian.format(instant), "25 мая");
