/*
 * QuickJS asynchronous Atomics wait tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import { assert, assertThrows } from "./assert.js";

function check_result(result, async, value)
{
    assert(Object.getPrototypeOf(result), Object.prototype);
    assert(Object.keys(result).join(","), "async,value");
    assert(result.async, async);
    if (!async)
        assert(result.value, value);
    for (const name of ["async", "value"]) {
        const descriptor = Object.getOwnPropertyDescriptor(result, name);
        assert(descriptor.enumerable && descriptor.configurable &&
               descriptor.writable);
    }
}

function test_immediate()
{
    assert(Atomics.waitAsync.name, "waitAsync");
    assert(Atomics.waitAsync.length, 4);
    const words = new Int32Array(new SharedArrayBuffer(8));
    check_result(Atomics.waitAsync(words, 0, 1, Infinity), false, "not-equal");
    check_result(Atomics.waitAsync(words, 0, 0, 0), false, "timed-out");
    check_result(Atomics.waitAsync(words, 0, 0, -Infinity), false, "timed-out");
    check_result(Atomics.waitAsync(words, 0, 0x100000000, -1), false, "timed-out");
    const big = new BigInt64Array(words.buffer);
    check_result(Atomics.waitAsync(big, 0, 1n << 64n, 0), false, "timed-out");
    assertThrows(TypeError, () => Atomics.waitAsync(words, 0, 0n));
    assertThrows(TypeError, () => Atomics.waitAsync(big, 0, 0));
    assertThrows(TypeError, () => Atomics.waitAsync(new Int32Array(2), 0, 0));
    assertThrows(TypeError, () => Atomics.waitAsync(new Uint32Array(words.buffer), 0, 0));
    assertThrows(RangeError, () => Atomics.waitAsync(words, words.length, 0));
    const seen = [];
    const index = { valueOf() { seen.push("index"); return 0; } };
    const value = { valueOf() { seen.push("value"); return 0; } };
    const timeout = { valueOf() {
        seen.push("timeout");
        Atomics.store(words, 0, 1);
        return 0;
    } };
    check_result(Atomics.waitAsync(words, index, value, timeout), false, "not-equal");
    assert(seen.join(","), "index,value,timeout");
}

async function test_notify_and_intrinsics()
{
    const words = new Int32Array(new SharedArrayBuffer(8));
    const OriginalPromise = Promise;
    let wait;
    try {
        globalThis.Promise = function () { throw Error("observable constructor"); };
        wait = Atomics.waitAsync(words, 0, 0);
    } finally {
        globalThis.Promise = OriginalPromise;
    }
    check_result(wait, true);
    assert(Object.getPrototypeOf(wait.value), OriginalPromise.prototype);
    assert(Atomics.notify(words, 0, 1), 1);
    assert(await wait.value, "ok");

    for (const timeout of [undefined, NaN, Infinity, 0.5]) {
        const next = Atomics.waitAsync(words, 0, 0, timeout);
        check_result(next, true);
        assert(Atomics.notify(words, 0), 1);
        assert(await next.value, "ok");
    }
}

async function test_fifo_and_same_agent_job_order()
{
    const words = new Int32Array(new SharedArrayBuffer(8));
    const seen = [];
    const first = Atomics.waitAsync(words, 0, 0);
    const other = Atomics.waitAsync(words, 1, 0);
    const second = Atomics.waitAsync(words, 0, 0);
    first.value.then(() => seen.push("first"));
    second.value.then(() => seen.push("second"));
    other.value.then(() => seen.push("other"));
    Promise.resolve().then(() => seen.push("before"));
    assert(Atomics.notify(words, 0, 1), 1);
    Promise.resolve().then(() => seen.push("between"));
    assert(Atomics.notify(words, 0, 1), 1);
    assert(Atomics.notify(words, 1, 1), 1);
    await Promise.all([first.value, second.value, other.value]);
    assert(seen.join(","), "before,first,between,second,other");
    assert(Atomics.notify(words, 0), 0);
}

async function test_host_timeout_and_notification_race()
{
    const words = new Int32Array(new SharedArrayBuffer(4));
    const timed = Atomics.waitAsync(words, 0, 0, 0.5);
    check_result(timed, true);
    assert(await timed.value, "timed-out");
    assert(Atomics.notify(words, 0), 0);
    const delayed = Atomics.waitAsync(words, 0, 0, 1);
    const start = Date.now();
    while (Date.now() - start < 5) {}
    /* The owner's timeout job has not executed. Notify still wins. */
    assert(Atomics.notify(words, 0), 1);
    assert(await delayed.value, "ok");
}

if (typeof Atomics === "object") {
    assert(typeof Atomics.waitAsync, "function");
    test_immediate();
    await test_notify_and_intrinsics();
    await test_fifo_and_same_agent_job_order();
    await test_host_timeout_and_notification_race();
}
