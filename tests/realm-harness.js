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

/* Fresh-realm fixture supplying foreign globals and an explicit GC hook. */
import * as std from "std";

export function check(actual, expected, message)
{
    if (!Object.is(actual, expected))
        throw Error(message || "realm assertion failed");
}

/* Evaluate the original assertions in two fresh realms, then release both
   carrier objects. Returned functions and pending jobs retain their realms. */
export function run_in_realms(body)
{
    let caller = std.createRealm(), foreign = std.createRealm();
    caller.global.foreign = foreign.global;
    caller.global.runGC = std.gc;
    foreign = null;
    std.gc();
    const result = caller.evalScript("(" + body + ")()");
    caller = null;
    std.gc();
    return result;
}
