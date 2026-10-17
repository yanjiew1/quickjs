/*
 * VM stack materialization boundaries
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import * as std from "std";

function assert(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("assertion failed: " + actual + " !== " + expected);
}

function test_numbers(a, b)
{
    assert((a + b) * (a - b), 21);
    assert((a * b) / (a + b), 10 / 7);
    assert((a * b) % (a + b), 3);
    const zero = 0;
    assert(zero * -1, -0);
    assert(zero / -1, -0);
    assert(zero / zero, NaN);
    assert(1 / zero, Infinity);
    assert(-1 / zero, -Infinity);
    assert((-5) % 2, -1);
    assert((-4) % 2, -0);
    assert(2147483647 + a, 2147483652);
    assert((-2147483648) - a, -2147483653);
    assert(2147483647 * b, 4294967294);
}

function test_grouped_handlers(a, b)
{
    assert((a + b) == 7, true);
    assert((a + b) != 7, false);
    assert((a + b) === 7, true);
    assert((a + b) !== 7, false);
    assert((a + b) < 8, true);
    assert((a + b) <= 7, true);
    assert((a + b) > 6, true);
    assert((a + b) >= 7, true);
    assert((a + b) << 1, 14);
    assert((a + b) >> 1, 3);
    assert((a + b) >>> 1, 3);
    assert((a + b) & 3, 3);
    assert((a + b) | 8, 15);
    assert((a + b) ^ 3, 4);
    assert(!(a + b), false);
    assert(typeof (a + b), "number");
}

function test_stores(a, b, c, d, e)
{
    let x, y, z, w, fifth;
    x = a + b;
    y = (z = x * c);
    w = (fifth = y - d);
    e = w / 2;
    a = (b = e % 3);
    assert([x, y, z, w, fifth, a, b].join(), "7,21,21,17,17,2.5,2.5");
    let obj = { value: 3 };
    const alias = obj;
    obj = obj;
    obj = (obj = alias);
    assert(obj, alias);
    assert((obj = alias).value, 3);
    function read() { return x; }
    eval("x = 11");
    assert(read(), 11);
}

function test_coercion()
{
    const log = [];
    function value(n) {
        return { [Symbol.toPrimitive](hint) {
            std.gc();
            log.push(n + ":" + hint);
            return n;
        } };
    }
    assert((value(3) + value(4)) * 2, 14);
    assert(log.join(), "3:default,4:default");
    log.length = 0;
    assert(value(8) - value(3), 5);
    assert(log.join(), "8:number,3:number");
    assert(("a" + "b") + "c", "abc");
    assert((2n + 3n) * 4n, 20n);
    assert((9n - 2n) / 2n, 3n);
    const marker = {};
    for (const expression of [
        () => 1 + Symbol(),
        () => 1n + 2,
        () => 1n / 0n,
        () => 1 - { valueOf() { throw marker; } },
        () => 1 + { toString() { throw marker; } },
    ]) {
        let caught = false;
        try {
            expression();
        } catch (e) {
            assert(e === marker || e instanceof TypeError || e instanceof RangeError, true);
            caught = true;
        }
        assert(caught, true);
    }
}

function test_boundaries()
{
    const identity = x => x;
    assert(identity(1 + 2) + identity(3 * 4), 15);
    assert(Math.max(1 + 2, 3 + 4, 5 + 6), 11);
    assert([1 + 2, ...(function* () { yield 3 * 4; })(), 7 - 1].join(), "3,12,6");
    assert(({ get value() { std.gc(); return 4; } }).value + 3, 7);
    let result = 0;
    try {
        try { throw 1 + 2; }
        finally { result = 4 * 5; }
    } catch (e) {
        assert(e, 3);
    }
    assert(result, 20);
}

function* suspended(a, b)
{
    try {
        return (a + b) * (yield { value: 3 });
    } finally {
        a = a + 1;
        assert(a, 6);
    }
}

async function test_suspension()
{
    let iterator = suspended(5, 2);
    assert(iterator.next().value.value, 3);
    std.gc();
    assert(iterator.next(4).value, 28);
    iterator = suspended(5, 2);
    iterator.next();
    std.gc();
    assert(iterator.return(17).value, 17);
    iterator = suspended(5, 2);
    iterator.next();
    const marker = {};
    let caught = false;
    try { iterator.throw(marker); }
    catch (e) { assert(e, marker); caught = true; }
    assert(caught, true);
    async function run(a, b) {
        return (a + b) * (await Promise.resolve(4));
    }
    const pending = run(5, 2);
    std.gc();
    assert(await pending, 28);
    async function* async_generator() {
        yield 1 + 2;
        return (await Promise.resolve(3)) * 4;
    }
    const async_iterator = async_generator();
    assert((await async_iterator.next()).value, 3);
    std.gc();
    assert((await async_iterator.next()).value, 12);
}

test_numbers(5, 2);
test_grouped_handlers(5, 2);
test_stores(5, 2, 3, 4, 0);
test_coercion();
test_boundaries();
await test_suspension();
