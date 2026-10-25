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

function assert(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw Error(message || `${actual} !== ${expected}`);
}

/* Argument coercion may mutate this Date; setters retain the original slot. */
for (const [method, first, rest] of [
    ["setDate", 12, []], ["setFullYear", 2025, [6, 14]],
    ["setHours", 15, [26, 37, 48]], ["setMilliseconds", 789, []],
    ["setMinutes", 41, [52, 63]], ["setMonth", 8, [19]],
    ["setSeconds", 31, [42]], ["setYear", 125, []], ["setUTCDate", 12, []],
    ["setUTCFullYear", 2025, [6, 14]], ["setUTCHours", 15, [26, 37, 48]],
    ["setUTCMilliseconds", 789, []], ["setUTCMinutes", 41, [52, 63]],
    ["setUTCMonth", 8, [19]], ["setUTCSeconds", 31, [42]]
]) {
    const original = Date.parse("2024-04-23T10:11:12.345Z");
    const expected = new Date(original);
    expected[method](first, ...rest);
    const changed = new Date(original);
    changed[method]({ valueOf() { changed.setTime(0); return first; } }, ...rest);
    assert(changed.getTime(), expected.getTime(), `${method} snapshots DateValue`);
}
