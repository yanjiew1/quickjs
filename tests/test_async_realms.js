/* Cross-realm Array.fromAsync behavioral coverage. The test_api.c unit
 * retains raw-context initialization and GC-after-every-job controls.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import * as std from "std";
import { check, run_in_realms } from "./realm-harness.js";

async function test_array_from_async_realm()
{
    const result = run_in_realms(async function () {
        function check(actual, expected, message) {
            if (actual !== expected) throw Error(message);
        }

        const settle = foreign.Function("resolve", "value", "resolve(value)");
        function gate() {
            let resolve;
            const promise = new Promise(function (r) { resolve = r; });
            return { promise, resolve };
        }

        const invalidNext = gate();
        const invalidResult = Array.fromAsync({
            [Symbol.asyncIterator]() {
                return { next() { return invalidNext.promise; } };
            },
        });
        check(Object.getPrototypeOf(invalidResult), Promise.prototype,
              "result promise belongs to the factory realm");
        const invalidCheck = invalidResult.then(
            function () { throw Error("primitive next result was accepted"); },
            function (error) {
                check(Object.getPrototypeOf(error), TypeError.prototype,
                      "next-result TypeError belongs to the suspended factory realm");
            });
        settle(invalidNext.resolve, 1);
        await invalidCheck;

        const first = gate();
        const second = Promise.resolve(18);
        let constructorReads = 0, thenReads = 0;
        Object.defineProperty(second, "constructor", {
            get() { constructorReads++; return Promise; },
        });
        Object.defineProperty(second, "then", {
            get() { thenReads++; throw Error("foreign intrinsic Promise was used"); },
        });
        const arrayLikeResult = Array.fromAsync({
            0: first.promise, 1: second, length: 2,
        });
        settle(first.resolve, 17);
        const values = await arrayLikeResult;
        check(values[0], 17, "first array-like value");
        check(values[1], 18, "second array-like value");
        check(constructorReads, 1, "later Await checks the original intrinsic once");
        check(thenReads, 0, "branded same-realm promise skips mutable then");

        const next = gate();
        let closes = 0;
        const closedResult = Array.fromAsync.call(function Output() {
            return Object.preventExtensions({});
        }, {
            [Symbol.asyncIterator]() {
                return {
                    next() { return next.promise; },
                    return() { closes++; return {}; },
                };
            },
        });
        const closedCheck = closedResult.then(
            function () { throw Error("non-extensible output accepted an element"); },
            function (error) {
                check(Object.getPrototypeOf(error), TypeError.prototype,
                      "indexed-definition error belongs to the factory realm");
            });
        settle(next.resolve, { value: 23, done: false });
        await closedCheck;
        check(closes, 1, "property failure still closes exactly once");
        return true;
    });
    check(await result, true, "array_from_async_realm");
    std.gc();
}

export async function test_async_realms()
{
    await test_array_from_async_realm();
}
