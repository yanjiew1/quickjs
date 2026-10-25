/*
 * QuickJS Atomics coercion and wait tests
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

const integerTypes = [Int8Array, Uint8Array, Int16Array, Uint16Array,
                      Int32Array, Uint32Array, BigInt64Array, BigUint64Array];
const methods = ["add", "sub", "and", "or", "xor", "exchange", "store",
                 "compareExchange"];

function invoke(method, view, index, convert)
{
    if (method === "compareExchange") {
        return Atomics[method](view, index,
                               { valueOf() { return convert("expected", 0); } },
                               { valueOf() { return convert("replacement", 1); } });
    }
    return Atomics[method](view, index,
                           { valueOf() { return convert("value", 1); } });
}

function test_operand_coercion_before_revalidation()
{
    for (const C of integerTypes) {
        const size = C.BYTES_PER_ELEMENT;
        const bigint = C === BigInt64Array || C === BigUint64Array;
        for (const tracking of [false, true]) {
            for (const method of methods) {
                const buffer = new ArrayBuffer(3 * size,
                                                { maxByteLength: 3 * size });
                const view = tracking ? new C(buffer, size) :
                                       new C(buffer, size, 2);
                const seen = [];
                const index = { valueOf() {
                    seen.push("index");
                    buffer.resize(3 * size - 1);
                    return 1;
                } };
                const result = invoke(method, view, index, (name, value) => {
                    seen.push(name);
                    if (name !== "expected")
                        buffer.resize(3 * size);
                    return bigint ? BigInt(value) : value;
                });
                const expected = method === "store" ? 1 : 0;
                assert(result, bigint ? BigInt(expected) : expected);
                assert(seen.join(","), method === "compareExchange" ?
                       "index,expected,replacement" : "index,value");
            }
        }
    }
}

function test_final_partial_element_revalidation()
{
    for (const C of integerTypes) {
        const size = C.BYTES_PER_ELEMENT;
        const bigint = C === BigInt64Array || C === BigUint64Array;
        for (const method of methods) {
            const buffer = new ArrayBuffer(3 * size,
                                            { maxByteLength: 3 * size });
            const view = new C(buffer, size);
            const seen = [];
            const index = { valueOf() {
                seen.push("index");
                buffer.resize(3 * size - 1);
                return 1;
            } };
            assertThrows(RangeError, () => invoke(method, view, index,
                (name, value) => {
                    seen.push(name);
                    return bigint ? BigInt(value) : value;
                }));
            assert(seen.join(","), method === "compareExchange" ?
                   "index,expected,replacement" : "index,value");
        }
    }
}

function test_operand_exception_precedes_revalidation()
{
    class Marker extends Error {}
    for (const C of [Int32Array, BigInt64Array]) {
        const size = C.BYTES_PER_ELEMENT;
        const bigint = C === BigInt64Array;
        for (const method of methods) {
            for (const detached of [false, true]) {
                const buffer = new ArrayBuffer(size, { maxByteLength: size });
                const view = new C(buffer);
                const seen = [];
                const index = { valueOf() {
                    seen.push("index");
                    if (detached)
                        buffer.transfer();
                    else
                        buffer.resize(size - 1);
                    return 0;
                } };
                assertThrows(Marker, () => invoke(method, view, index,
                    (name, value) => {
                        seen.push(name);
                        if (name !== "expected")
                            throw new Marker();
                        return bigint ? BigInt(value) : value;
                    }));
                assert(seen.join(","), method === "compareExchange" ?
                       "index,expected,replacement" : "index,value");
            }
        }
    }
}

function test_load_and_notify_controls()
{
    for (const C of [Int32Array, BigInt64Array]) {
        const size = C.BYTES_PER_ELEMENT;
        for (const tracking of [false, true]) {
            const buffer = new ArrayBuffer(size, { maxByteLength: size });
            const view = tracking ? new C(buffer) : new C(buffer, 0, 1);
            assertThrows(tracking ? RangeError : TypeError,
                () => Atomics.load(view, { valueOf() {
                    buffer.resize(size - 1);
                    return 0;
                } }));
        }
        for (const detached of [false, true]) {
            const buffer = new ArrayBuffer(size, { maxByteLength: size });
            const view = new C(buffer);
            const seen = [];
            const index = { valueOf() {
                seen.push("index");
                if (detached)
                    buffer.transfer();
                else
                    buffer.resize(size - 1);
                return 0;
            } };
            const count = { valueOf() { seen.push("count"); return 1; } };
            assert(Atomics.notify(view, index, count), 0);
            assert(seen.join(","), "index,count");
        }
    }
}

test_operand_coercion_before_revalidation();
test_final_partial_element_revalidation();
test_operand_exception_precedes_revalidation();
test_load_and_notify_controls();

function test_compare_exchange_coercion_boundaries()
{
    class Marker extends Error {}
    for (const C of [Int32Array, BigInt64Array]) {
        const size = C.BYTES_PER_ELEMENT;
        const scalar = C === BigInt64Array ? value => BigInt(value) : value => value;
        for (const detached of [false, true]) {
            const buffer = new ArrayBuffer(size, { maxByteLength: size });
            const view = new C(buffer);
            const seen = [];
            assertThrows(Marker, () => Atomics.compareExchange(view,
                { valueOf() {
                    seen.push("index");
                    if (detached) buffer.transfer();
                    else buffer.resize(0);
                    return 0;
                } },
                { valueOf() { seen.push("expected"); throw new Marker(); } },
                { valueOf() { seen.push("replacement"); return scalar(1); } }));
            assert(seen.join(","), "index,expected");
        }
        {
            const buffer = new ArrayBuffer(size, { maxByteLength: size });
            const view = new C(buffer);
            const seen = [];
            assert(Atomics.compareExchange(view, 0,
                { valueOf() {
                    seen.push("expected"); buffer.resize(0); return scalar(0);
                } },
                { valueOf() {
                    seen.push("replacement"); buffer.resize(size); return scalar(1);
                } }), scalar(0));
            assert(view[0], scalar(1));
            assert(seen.join(","), "expected,replacement");
        }
        {
            const buffer = new ArrayBuffer(size);
            const view = new C(buffer);
            const seen = [];
            assertThrows(TypeError, () => Atomics.compareExchange(view,
                { valueOf() {
                    seen.push("index"); buffer.transfer(); return 0;
                } },
                { valueOf() { seen.push("expected"); return scalar(0); } },
                { valueOf() { seen.push("replacement"); return scalar(1); } }));
            assert(seen.join(","), "index,expected,replacement");
        }
    }
}

test_compare_exchange_coercion_boundaries();
