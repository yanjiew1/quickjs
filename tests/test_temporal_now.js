/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

function assert(value, expected) {
    if (arguments.length < 2) expected = true;
    if (!Object.is(value, expected)) throw Error(`${value} !== ${expected}`);
}
const descriptor = Object.getOwnPropertyDescriptor(Temporal.Now, Symbol.toStringTag);
assert(descriptor.value, "Temporal.Now");
assert(descriptor.writable, false);
assert(descriptor.enumerable, false);
assert(descriptor.configurable, true);
assert(Object.getPrototypeOf(Temporal.Now), Object.prototype);
for (const name of ["timeZoneId", "instant", "plainDateTimeISO", "plainDateISO",
                    "plainTimeISO", "zonedDateTimeISO"]) {
    assert(Temporal.Now[name].length, 0);
    assert(Object.getOwnPropertyDescriptor(Temporal.Now[name], "prototype"), undefined);
}
assert(typeof Temporal.Now.timeZoneId(), "string");
const before = Date.now();
const instant = Temporal.Now.instant();
const after = Date.now();
assert(instant instanceof Temporal.Instant);
assert(instant.epochMilliseconds >= before - 1000 && instant.epochMilliseconds <= after + 1000);
assert(Temporal.Now.plainDateISO("UTC") instanceof Temporal.PlainDate);
assert(Temporal.Now.plainTimeISO("UTC") instanceof Temporal.PlainTime);
assert(Temporal.Now.plainDateTimeISO("UTC") instanceof Temporal.PlainDateTime);
assert(Temporal.Now.zonedDateTimeISO("+05:30").timeZoneId, "+05:30");
assert(Temporal.Now.zonedDateTimeISO().calendarId, "iso8601");
assert(Temporal.Now.plainDateISO().calendarId, "iso8601");
assert(Temporal.Now.plainDateTimeISO().calendarId, "iso8601");
