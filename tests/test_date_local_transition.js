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

function assert(actual, expected) {
    if (!Object.is(actual, expected))
        throw Error(`${actual} !== ${expected}`);
}

/* A nonexistent local time uses the offset before the spring transition. */
assert(new Date(2024, 2, 10, 2, 30).toISOString(),
       '2024-03-10T07:30:00.000Z');
assert(new Date(2024, 2, 10, 3, 30).toISOString(),
       '2024-03-10T07:30:00.000Z');

/* A repeated local time selects the earlier instant. */
assert(new Date(2024, 10, 3, 1, 30).toISOString(),
       '2024-11-03T05:30:00.000Z');

/* The millisecond before the 1969 transition is still in standard time. */
const before = new Date(Date.UTC(1969, 3, 27, 7) - 1);
assert(before.getTimezoneOffset(), 300);
assert(before.getHours(), 1);
assert(before.getMinutes(), 59);
assert(before.getSeconds(), 59);
