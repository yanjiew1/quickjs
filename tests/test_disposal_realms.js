/* Cross-realm iterator disposal regressions.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import * as std from "std";
import { check } from "./realm-harness.js";

function test_iterator_disposal_realm()
{
    let realm = std.createRealm();
    const foreignDispose = realm.evalScript("Iterator.prototype[Symbol.dispose]");
    const ForeignTypeError = realm.global.TypeError;
    const foreignNumberProto = realm.global.Number.prototype;
    realm = null;
    std.gc();

    for (const receiver of [null, undefined, { return: 1 }]) {
        let caught;
        try { foreignDispose.call(receiver); } catch (error) { caught = error; }
        check(caught instanceof ForeignTypeError, true);
        check(caught instanceof TypeError, false);
    }
    const sentinel = {};
    let caught;
    try { foreignDispose.call({ get return() { throw sentinel; } }); }
    catch (error) { caught = error; }
    check(caught, sentinel, "abrupt getter preserves thrown identity");
    const saved = Object.getOwnPropertyDescriptor(foreignNumberProto, "return");
    let reads = 0, calls = 0;
    try {
        Object.defineProperty(foreignNumberProto, "return", {
            configurable: true,
            get: function () {
                "use strict";
                check(this, 3, "primitive boxing uses the foreign Number prototype");
                reads++;
                return function () {
                    "use strict";
                    check(this, 3);
                    check(arguments.length, 0);
                    calls++;
                    return { get then() { throw sentinel; } };
                };
            },
        });
        check(foreignDispose.call(3), undefined);
    } finally {
        delete foreignNumberProto.return;
        if (saved)
            Object.defineProperty(foreignNumberProto, "return", saved);
    }
    check(reads, 1);
    check(calls, 1);
}

async function test_async_iterator_disposal_realm()
{
    let realm = std.createRealm();
    const foreignAsyncDispose = realm.evalScript(
        "Object.getPrototypeOf(Object.getPrototypeOf((async function*(){}).prototype))[Symbol.asyncDispose]");
    const ForeignPromise = realm.global.Promise;
    const ForeignTypeError = realm.global.TypeError;
    realm = null;
    std.gc();

    const input = Promise.resolve(7);
    let constructorReads = 0, thenReads = 0;
    Object.defineProperty(input, "constructor", {
        get() { constructorReads++; return Promise; },
    });
    Object.defineProperty(input, "then", {
        get() { thenReads++; return Promise.prototype.then; },
    });
    const pending = foreignAsyncDispose.call({ return() { return input; } });
    const absent = foreignAsyncDispose.call({});
    const rejected = foreignAsyncDispose.call(null);
    for (const promise of [pending, absent, rejected]) {
        check(Object.getPrototypeOf(promise), ForeignPromise.prototype);
        check(promise instanceof Promise, false);
    }
    check(pending === input, false);
    const reason = rejected.then(
        () => { throw Error("expected rejection"); },
        error => {
            check(error instanceof ForeignTypeError, true);
            check(error instanceof TypeError, false);
        });
    const completion = Promise.all([pending, absent, reason]);
    std.gc();
    const values = await completion;
    check(values[0], undefined);
    check(values[1], undefined);
    check(constructorReads, 2);
    check(thenReads, 1);
    std.gc();
}

export async function test_disposal_realms()
{
    test_iterator_disposal_realm();
    await test_async_iterator_disposal_realm();
    std.gc();
}

await test_disposal_realms();
