/*
 * Saved variable references and garbage collection
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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

function assert(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("assertion failed: " + actual + " !== " + expected);
}

/* Empty with environments keep references on the generic VM path. */
const make = Function("gc", `
    return {
        argument: function* (value) {
            with ({}) { value = yield "ready"; }
            gc();
            return value;
        },
        local: function* () {
            let value;
            with ({}) { [value = yield "ready"] = [undefined]; }
            gc();
            return value;
        },
        closure: function () {
            let value;
            return {
                iterator: (function* () {
                    with ({}) { value = yield "ready"; }
                    gc();
                    return value;
                })(),
                get: () => value,
            };
        },
        readonly: function () {
            let effects = 0;
            const value = 1;
            with ({}) {
                value ||= ++effects;
                try { value = (gc(), ++effects); }
                catch (error) {
                    if (error instanceof TypeError)
                        return effects;
                    throw error;
                }
            }
            throw Error("missing readonly error");
        },
        tdz: function () {
            let effects = 0;
            with ({}) {
                try { value = (gc(), ++effects); }
                catch (error) {
                    if (error instanceof ReferenceError)
                        return effects;
                    throw error;
                }
            }
            let value;
            throw Error("missing TDZ error");
        },
    };
`)(std.gc);

for (const factory of [make.argument, make.local]) {
    const iterator = factory();
    assert(iterator.next().value, "ready");
    std.gc();
    const value = { marker: 42 };
    const result = iterator.next(value);
    assert(result.done, true);
    assert(result.value, value);
}

{
    const saved = make.closure();
    assert(saved.iterator.next().value, "ready");
    std.gc();
    const value = {};
    assert(saved.iterator.next(value).value, value);
    assert(saved.get(), value);
}
assert(make.readonly(), 1);
assert(make.tdz(), 1);

/* Closing or abandoning a suspension releases the reference owner. */
for (let i = 0; i < 8; i++) {
    let value = {};
    let iterator = make.argument(value);
    value.iterator = iterator;
    iterator.next();
    if (i & 1)
        iterator.return();
    value = iterator = null;
    std.gc();
}

const failure = {};
for (const factory of [make.argument, make.local]) {
    const iterator = factory();
    iterator.next();
    std.gc();
    try {
        iterator.throw(failure);
        assert(false, true);
    } catch (error) {
        assert(error, failure);
    }
    std.gc();
}

/* Direct eval must retain its call mode while the LHS reference is live. */
for (const call of ["eval('gc(); var target = 2')",
                    "eval(...['gc(); var target = 2'])"]) {
    const result = Function("gc", `
        let target = 1;
        function write() {
            with ({}) { target = (${call}, 3); }
            return target;
        }
        return [write(), target];
    `)(std.gc);
    assert(result.join(","), "2,3");
}
assert(Function("gc", `
    let target = 1;
    with ({}) { target += eval("gc(); target = 5; 2"); }
    return target;
`)(std.gc), 3);

const AsyncFunction = (async function () {}).constructor;
const suspended = AsyncFunction("pending", "gc", `
    let value;
    with ({}) { value = await pending; }
    gc();
    return value;
`);
let resolve;
const pending = new Promise(done => { resolve = done; });
const result = suspended(pending, std.gc);
std.gc();
const value = {};
resolve(value);
assert(await result, value);

/* An unreferenced pending async frame must remain collectable. */
suspended(new Promise(() => {}), std.gc);
std.gc();

let reject;
const rejected = new Promise((resolve, fail) => { reject = fail; });
const failed = suspended(rejected, std.gc);
std.gc();
reject(failure);
try {
    await failed;
    assert(false, true);
} catch (error) {
    assert(error, failure);
}
std.gc();

function test_frame_reference_slot_indices()
{
    const parameters = [], locals = [];
    for (let i = 0; i < 260; i++) {
        parameters.push("p" + i);
        locals.push("l" + i + " = 1");
    }
    // An empty with scope preserves the reference path for known frame slots.
    const run = Function(...parameters, `
        var ${locals.join(",")};
        with ({}) {
            l0 += 2;
            l259 += 3;
            p0 += 4;
            p259 += 5;
        }
        return [l0, l259, p0, p259, arguments[0], arguments[259]];
    `);
    assert(run(...Array(260).fill(1)).join(","), "3,4,5,6,5,6");

    const make = Function(`
        var ${locals.join(",")};
        return function() {
            // Capture every slot, including closure indices above 255.
            var before = [${locals.map((_, i) => "l" + i).join(",")}];
            with ({}) { l0 += 2; l259 += 3; }
            return [before[0], before[259], l0, l259];
        };
    `);
    assert(make()().join(","), "1,1,3,4");
}

test_frame_reference_slot_indices();


async function test_suspended_frame_references()
{
    // Function keeps the with fallback available even in this strict module.
    const make = Function(`
        let captured = 10;
        return function*(argument) {
            let local = 1;
            with ({}) {
                local += yield "local";
                argument += yield "argument";
                captured += yield "closure";
            }
            return [local, argument, captured, arguments[0]];
        };
    `);
    const iterator = make()(2);
    assert(iterator.next().value, "local");
    std.gc();
    assert(iterator.next(3).value, "argument");
    std.gc();
    assert(iterator.next(4).value, "closure");
    std.gc();
    const result = iterator.next(5);
    assert(result.done, true);
    assert(result.value.join(","), "4,6,15,6");

    const run = Function("collect", `
        let captured = 10;
        return (async function(argument) {
            let local = 1;
            with ({}) {
                local += await collect(3);
                argument += await collect(4);
                captured += await collect(5);
            }
            return [local, argument, captured, arguments[0]];
        })(2);
    `);
    const values = await run(value => Promise.resolve().then(() => {
        std.gc();
        return value;
    }));
    assert(values.join(","), "4,6,15,6");
}

await test_suspended_frame_references();
