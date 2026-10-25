/* Cross-realm asynchronous builtin coverage. The test_api.c unit
 * retains raw-context initialization and GC-after-every-job controls.
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
        runGC();
        settle(invalidNext.resolve, 1);
        await invalidCheck;
        runGC();

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
        runGC();
        settle(first.resolve, 17);
        const values = await arrayLikeResult;
        runGC();
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
        runGC();
        settle(next.resolve, { value: 23, done: false });
        await closedCheck;
        runGC();
        check(closes, 1, "property failure still closes exactly once");
        return true;
    });
    check(await result, true, "array_from_async_realm");
    std.gc();
}

async function test_promise_capability_realms()
{
    const result = run_in_realms(async function() {
        function check(actual, expected) {
            if (actual !== expected) throw Error("Promise realm mismatch");
        }
        const cap = Promise.withResolvers.call(foreign.Promise);
        check(Object.getPrototypeOf(cap.promise), foreign.Promise.prototype);
        check(Object.getPrototypeOf(cap.resolve), foreign.Function.prototype);
        check(Object.getPrototypeOf(cap.reject), foreign.Function.prototype);
        cap.resolve(37);
        check(await cap.promise, 37);
        const resolved = Promise.resolve.call(foreign.Promise, 41);
        check(Object.getPrototypeOf(resolved), foreign.Promise.prototype);
        check(await resolved, 41);
        const source = Promise.resolve(43);
        Object.defineProperty(source, "constructor", {
            value: { [Symbol.species]: foreign.Promise },
        });
        const chained = source.then(value => value + 1);
        check(Object.getPrototypeOf(chained), foreign.Promise.prototype);
        check(await chained, 44);
        return true;
    });
    check(await result, true, "promise_capability_realms");
    std.gc();
}

async function test_builtin_callback_creation_realm()
{
    const result = run_in_realms(async function () {
        let rejectElement;
        foreign.Promise.resolve = value => value;
        // Capture the builtin reject-element closure, then invoke it from a
        // different realm instead of through the usual Promise job path.
        const promise = foreign.Promise.any([{
            then(resolve, reject) { rejectElement = reject; },
        }]);
        const checked = promise.then(
            () => { throw Error("unexpected fulfillment"); },
            error => Object.getPrototypeOf(error) === foreign.AggregateError.prototype);
        runGC();
        rejectElement("reason");
        return await checked;
    });
    check(await result, true, "builtin callback retains its creation realm");
    std.gc();
}

async function test_promise_resolver_and_job_realms()
{
    const result = run_in_realms(async function () {
        function check(actual, expected) {
            if (actual !== expected) throw Error("Promise job realm mismatch");
        }
        const cap = foreign.Promise.withResolvers();
        const self = cap.promise.then(
            () => { throw Error("self resolution was accepted"); },
            error => Object.getPrototypeOf(error) === foreign.TypeError.prototype);
        cap.resolve(cap.promise);
        check(await self, true);

        const then = foreign.Function("resolve", "reject",
            "globalThis.resolverRealm = Object.getPrototypeOf(resolve) === Function.prototype; resolve(37)");
        check(await Promise.resolve({ then }), 37);
        check(foreign.resolverRealm, true);

        // The job keeps the then action's realm even if its Proxy is revoked
        // before the queued call. An already revoked Proxy uses the caller's
        // realm instead, because GetFunctionRealm is abrupt while enqueueing.
        for (const revokeBefore of [false, true]) {
            const pair = foreign.Proxy.revocable(foreign.Function(), {});
            if (revokeBefore) pair.revoke();
            const pending = Promise.resolve({ then: pair.proxy });
            if (!revokeBefore) pair.revoke();
            const expected = revokeBefore ? TypeError.prototype : foreign.TypeError.prototype;
            check(await pending.then(
                () => false, error => Object.getPrototypeOf(error) === expected), true);
        }
        const handler = foreign.Proxy.revocable(foreign.Function(), {});
        const reaction = Promise.resolve(1).then(handler.proxy);
        handler.revoke();
        check(await reaction.then(
            () => false,
            error => Object.getPrototypeOf(error) === foreign.TypeError.prototype), true);
        return true;
    });
    check(await result, true, "Promise resolving functions and jobs keep their realms");
    std.gc();
}

export async function test_async_realms()
{
    await test_array_from_async_realm();
    await test_promise_capability_realms();
    await test_builtin_callback_creation_realm();
    await test_promise_resolver_and_job_realms();
}
