/*
 * QuickJS Atomics wait tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import { assert, assertThrows } from "./assert.js";

function test_wait_expected_value()
{
    const words = new Int32Array(new SharedArrayBuffer(8));
    assert(Atomics.wait(words, 0, 0, 0), "timed-out");
    assert(Atomics.wait(words, 0, 1, 0), "not-equal");
    assert(Atomics.wait(words, 0, 0x100000000, 0), "timed-out");
    Atomics.store(words, 0, -1);
    assert(Atomics.wait(words, 0, 0xffffffff, 0), "timed-out");

    const big = new BigInt64Array(new SharedArrayBuffer(8));
    assert(Atomics.wait(big, 0, 1n << 64n, 0), "timed-out");
    Atomics.store(big, 0, -1n);
    assert(Atomics.wait(big, 0, (1n << 64n) - 1n, 0), "timed-out");
    assert(Atomics.wait(big, 0, 0n, 0), "not-equal");
}

function test_wait_coercion_order()
{
    const words = new Int32Array(new SharedArrayBuffer(8));
    const seen = [];
    const index = { valueOf() { seen.push("index"); return 0; } };
    const value = { valueOf() { seen.push("value"); return 0; } };
    const timeout = { valueOf() {
        seen.push("timeout");
        Atomics.store(words, 0, 1);
        return 0;
    } };
    assert(Atomics.wait(words, index, value, timeout), "not-equal");
    assert(seen.join(","), "index,value,timeout");

    const big = new BigInt64Array(new SharedArrayBuffer(8));
    const expected = { valueOf() {
        Atomics.store(big, 0, 5n);
        return 5n;
    } };
    assert(Atomics.wait(big, 0, expected, 0), "timed-out");

    assertThrows(TypeError, () => Atomics.wait(new Int32Array(2), 0, 0, 0));
    assertThrows(TypeError, () => Atomics.wait(new Uint32Array(words.buffer), 0, 0, 0));
    assertThrows(RangeError, () => Atomics.wait(words, words.length, 0, 0));
    assertThrows(TypeError, () => Atomics.wait(big, 0, 0, 0));
}

test_wait_expected_value();
test_wait_coercion_order();
