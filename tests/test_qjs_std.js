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

/* --std must finish before a non-module script starts. */
function assert(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("qjs --std assertion failed: " + actual + " !== " + expected);
}

assert(typeof std, "object");
assert(typeof os, "object");
assert(typeof std.sprintf, "function");
assert(typeof os.getcwd, "function");
assert(std.evalScript("40 + 2"), 42);
let sum = 0;
for (let i = 0; i < 7; i++)
    sum += i;
assert(std.sprintf("sum=%d", sum), "sum=21");
const [directory, error] = os.getcwd();
assert(error, 0);
assert(typeof directory, "string");
print("qjs-cli-std-ok");
