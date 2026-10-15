/*
 * Saved variable references and garbage collection
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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

/* An unreferenced pending async frame and its saved reference form a cycle. */
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
