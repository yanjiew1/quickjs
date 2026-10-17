/*
 * Always-valid VM top-of-stack regression tests.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
import * as std from "std";

function assert(actual, expected) {
    if (!Object.is(actual, expected))
        throw Error(`assertion failed: ${actual} !== ${expected}`);
}
function throws(fn, expected) {
    let caught = false;
    try { fn(); } catch (e) {
        caught = true;
        if (expected !== undefined) assert(e, expected);
    }
    assert(caught, true);
}

/* Distinguish the depth-zero sentinel from a real undefined operand. */
function empty() {}
function identity(a) { return a; }
for (let i = 0; i < 500; i++) {
    empty();
    identity(undefined);
    assert(empty(), undefined);
    assert(identity(undefined), undefined);
    assert([].length, 0);
    assert([undefined][0], undefined);
    assert((i & 1) ? identity(3) : identity(4), (i & 1) ? 3 : 4);
}

/* Arithmetic replacements, slow coercions, overflow, and signed zero. */
function add(a, b) { return a + b; }
function sub(a, b) { return a - b; }
function mul(a, b) { return a * b; }
assert(add(2147483647, 1), 2147483648);
assert(sub(-2147483648, 1), -2147483649);
assert(mul(0, -1), -0);
assert(add(1.5, 2), 3.5);
assert(add(2n ** 70n, 1n), 1180591620717411303425n);
assert(add("a", "b"), "ab");
const order = [];
const coercion = n => ({ valueOf() {
    std.gc(); order.push(n); return identity(n);
}});
assert(add(coercion(3), coercion(4)), 7);
assert(order.join(), "3,4");
const failure = new Error("coercion");
const bad = { [Symbol.toPrimitive]() { std.gc(); throw failure; } };
for (const fn of [x => x + 1, x => -x, x => ~x,
                  x => x < 1, x => x == 1, x => x << 1,
                  x => { x++; }, x => { ++x; }])
    throws(() => fn(bad), failure);
throws(() => add(1n, 1));

/* Stack permutations from computed references, assignment and destructuring. */
let obj = { x: 1 }, key = "x";
assert(obj[key]++, 1);
assert(++obj[key], 3);
assert(obj[key] += 4, 7);
assert(obj[key] = { v: 9 }, obj.x);
let a, b, rest;
[a, b, ...rest] = [obj, undefined, 3, 4];
assert(a, obj); assert(b, undefined); assert(rest.join(), "3,4");
const source = { a: obj, b: 2, get c() { std.gc(); return 3; } };
const { a: picked, ...remaining } = source;
assert(picked, obj); assert(remaining.c, 3);
assert(({ ...source, d: 4 }).a, obj);
function changedArguments(x, y) { x = { changed: true }; y = undefined; return x; }
assert(changedArguments(obj, obj).changed, true);
assert(changedArguments.apply(null, [obj, obj]).changed, true);
assert(changedArguments(...[obj, obj]).changed, true);
assert(({ f() { return this; } }).f().f instanceof Function, true);
function tail(f, ...args) { return f(...args); }
assert(tail(identity, obj), obj);
assert(Function("x", "return eval('x')")(obj), obj);
assert(Function("o", "with(o) { x = 6; return x; }")({ x: 1 }), 6);
class Base { constructor(x) { this.x = x; } get v() { return this.x; } set v(x) { this.x = x; } }
class Derived extends Base {
    #v = 3;
    update() { return super.v++; }
    has(o) { return #v in o; }
}
const d = new Derived(10);
assert(d.update(), 10); assert(d.v, 11); assert(d.has(d), true);
assert(d instanceof Base, true);
assert((new (class extends Base {}) (obj)).x, obj);

/* Handler restart and iterator-close must not spill a freed former top. */
let closed = 0, finalized = 0;
const iterable = {
    [Symbol.iterator]() { return this; },
    next() { std.gc(); return { value: obj, done: false }; },
    return() { closed++; std.gc(); return {}; }
};
try {
    try { for (const x of iterable) { assert(x, obj); throw failure; } }
    finally { finalized++; empty(); std.gc(); }
} catch (e) { assert(e, failure); }
assert(closed, 1); assert(finalized, 1);
throws(() => { const { [bad]: value } = obj; }, failure);
function runFinally() { try { return obj; } finally { empty(); std.gc(); } }
assert(runFinally(), obj);

/* Suspended frames retain a contiguous top slot for next/throw/return. */
function* gen(x) {
    try {
        const y = yield x;
        std.gc();
        yield identity(y);
        return x;
    } catch (e) {
        assert(e, failure);
        yield "caught";
    } finally { finalized++; std.gc(); }
}
let g = gen(obj);
assert(g.next().value, obj);
std.gc();
assert(g.next(undefined).value, undefined);
std.gc();
assert(g.next().value, obj);
assert(g.next().done, true);
g = gen(obj);
g.next();
assert(g.throw(failure).value, "caught");
assert(g.next().done, true);
g = gen(obj); g.next(); std.gc();
assert(g.return(obj).value, obj);
assert(g.next().done, true);
function* delegate() { return yield* gen(obj); }
g = delegate(); assert(g.next().value, obj);
std.gc(); assert(g.return(7).value, 7);
assert((function*() {})().next().done, true);

/* async/async-generator heap stacks and disposal may suspend while empty. */
async function asyncCall(x) {
    empty();
    try { return await identity(x); }
    finally { await empty(); std.gc(); }
}
assert(await asyncCall(obj), obj);
assert(await asyncCall(undefined), undefined);
async function* asyncGen() {
    yield await asyncCall(obj);
    yield* [undefined, obj];
    return await asyncCall(7);
}
const ag = asyncGen();
assert((await ag.next()).value, obj);
std.gc();
assert((await ag.next()).value, undefined);
assert((await ag.next()).value, obj);
assert((await ag.next()).value, 7);
assert((await ag.next()).done, true);
let disposed = 0;
{
    using resource = { [Symbol.dispose]() { disposed++; std.gc(); } };
    empty();
}
async function disposeAsync() {
    await using resource = { async [Symbol.asyncDispose]() {
        await empty(); disposed++; std.gc();
    } };
    return obj;
}
assert(await disposeAsync(), obj);
assert(disposed, 2);
std.gc();
