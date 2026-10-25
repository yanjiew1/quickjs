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

/* Native Date bridge regression. */
function same(actual, expected) {
    if (!Object.is(actual, expected)) throw Error("unexpected value");
}
function throws(type, callback) {
    let caught;
    try { callback(); } catch (error) { caught = error; }
    if (!(caught instanceof type)) throw Error("unexpected completion");
}
same(new Date(-1).toTemporalInstant().epochNanoseconds, -1000000n);
throws(RangeError, () => new Date(NaN).toTemporalInstant());
throws(TypeError, () => Date.prototype.toTemporalInstant.call({}));
same(Date.prototype.toTemporalInstant.length, 0);
let date = new Date(42);
Object.defineProperty(date, "valueOf", { get() { throw Error("read valueOf"); } });
same(date.toTemporalInstant().epochNanoseconds, 42000000n);
