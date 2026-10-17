/* Cached interpreter stack boundaries and ownership.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
import * as std from "std";

function assert(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("assertion failed: " + String(actual) + " !== " + String(expected));
}

function throws_same(callback, expected)
{
    let caught = false;
    try {
        callback();
    } catch (error) {
        assert(error, expected);
        caught = true;
    }
    assert(caught, true);
}

function cycle(id)
{
    const value = { id };
    value.self = value;
    return value;
}

function triple(a, b, c)
{
    for (const value of [a, b, c])
        assert(value.self, value);
    return [a.id, b.id, c.id].join(",");
}

function test_empty_and_deep_stacks()
{
    function empty() {}
    function* empty_generator() {}
    function* captured(value = cycle(7)) { yield () => value; }
    const error = {};
    for (let i = 0; i < 16; i++) {
        assert(empty(), undefined);
        assert(empty_generator().next().done, true);
        assert(captured().return(19).value, 19);
        throws_same(() => captured().throw(error), error);
    }
    for (const depth of [1, 2, 3, 8, 32, 128]) {
        let expression = "leaf()";
        for (let i = 1; i <= depth; i++)
            expression = "(" + i + " + " + expression + ")";
        const evaluate = Function("leaf", "return " + expression);
        assert(evaluate(() => { std.gc(); return eval("0"); }),
               depth * (depth + 1) / 2);
    }
    const identity = value => value;
    for (const value of [undefined, null, false, true, 0, -0, NaN,
                         Infinity, -Infinity, 1.5, 2147483648, 1n << 80n,
                         "text", Symbol("stack"), cycle(1)])
        assert(identity(value), value);
    std.gc();
}

function test_mutable_call_arguments()
{
    /* Sloppy mapped arguments and formal parameters overwrite the caller's
       borrowed argument array, including its last, cached operand. */
    const error = {};
    const replace = Function("collect", `
        return function(a, b) {
            a = { id: 10 };
            arguments[1] = { id: 20 };
            collect();
            return a.id + b.id;
        };
    `)(std.gc);
    const fail = Function("collect", "error", `
        return function(a, b) {
            a = { id: 10 };
            arguments[1] = { id: 20 };
            collect();
            throw error;
        };
    `)(std.gc, error);
    const receiver = { replace, fail };
    function tail() { return replace({ id: 1 }, { id: 2 }); }
    function tail_method() { return receiver.replace({ id: 1 }, { id: 2 }); }
    function tail_fail() { return fail({ id: 1 }, { id: 2 }); }
    function tail_method_fail() { return receiver.fail({ id: 1 }, { id: 2 }); }
    assert(1 + replace({ id: 1 }, { id: 2 }), 31);
    assert(1 + receiver.replace({ id: 1 }, { id: 2 }), 31);
    assert(tail(), 30);
    assert(tail_method(), 30);
    throws_same(tail_fail, error);
    throws_same(tail_method_fail, error);
    throws_same(() => 1 + fail({ id: 1 }, { id: 2 }), error);
    throws_same(() => 1 + receiver.fail({ id: 1 }, { id: 2 }), error);
    const rebound_eval = Function("target", `
        var eval = target;
        return eval({ id: 1 }, { id: 2 });
    `);
    assert(rebound_eval(replace), 30);
    throws_same(() => rebound_eval(fail), error);
    const Constructor = Function("collect", `
        return function(a, b) {
            arguments[1] = { id: 40 };
            collect();
            this.value = a.id + b.id;
        };
    `)(std.gc);
    assert(new Constructor({ id: 2 }, { id: 0 }).value, 42);
    throws_same(() => new fail({ id: 1 }, { id: 2 }), error);
    assert(replace.apply(null, [{}, {}]), 30);
    assert(replace(...[{}, {}]), 30);
    assert(replace.bind(null, {})({}), 30);
    assert(new Proxy(replace, {})({}, {}), 30);
}

function test_shuffles_and_slow_paths()
{
    const events = [];
    const object = {
        _value: 10,
        get value() { std.gc(); events.push("get"); return this._value; },
        set value(value) { std.gc(); events.push("set"); this._value = value; }
    };
    const key = {
        [Symbol.toPrimitive]() { std.gc(); events.push("key"); return "value"; }
    };
    assert(object[key]++, 10);
    assert(++object[key], 12);
    assert(object[key] += 3, 15);
    assert(events.join(","), "key,get,set,key,get,set,key,get,set");
    const source = { keep: cycle(1), other: cycle(2) };
    const { keep, ...rest } = source;
    assert(keep, source.keep);
    assert(rest.other, source.other);
    assert(({ ...source }).keep, keep);
    const array = [cycle(1), ...[cycle(2), cycle(3)]];
    assert(triple(...array), "1,2,3");

    class Base {
        get value() { std.gc(); return this._value; }
        set value(value) { std.gc(); this._value = value; }
    }
    class Derived extends Base {
        #value = 3;
        update() {
            this._value = 5;
            const old = super.value++;
            return old + super.value + this.#value++ + ++this.#value;
        }
    }
    assert(new Derived().update(), 19);

    const error = {};
    const bad = { [Symbol.toPrimitive]() { std.gc(); throw error; } };
    for (const expression of ["value + 1", "value - 1", "value * 2",
                              "value / 2", "value % 2", "value ** 2",
                              "+value", "-value", "~value", "value << 1",
                              "value >>> 1", "value == 1", "value < 1",
                              "value++", "++value"]) {
        const evaluate = Function("value", "return " + expression);
        throws_same(() => triple(cycle(1), evaluate(bad), cycle(3)), error);
    }
    const getter = { get value() { std.gc(); throw error; } };
    throws_same(() => triple(cycle(1), getter.value, cycle(3)), error);
    const numeric = { [Symbol.toPrimitive]() { std.gc(); return 40n; } };
    let value = numeric;
    assert(value++, 40n);
    assert(value, 41n);
    std.gc();
}

function test_iterators_and_unwinding()
{
    const error = {};
    let closed = 0;
    const broken_start = {
        [Symbol.iterator]() {
            return {
                get next() { std.gc(); throw error; },
                return() { closed++; return {}; }
            };
        }
    };
    throws_same(() => { for (const value of broken_start) void value; }, error);
    assert(closed, 0);
    function* values() {
        try { yield cycle(1); yield cycle(2); }
        finally { std.gc(); closed++; }
    }
    function abrupt() {
        try {
            for (const value of values()) {
                assert(value.self, value);
                return cycle(3);
            }
        } finally {
            std.gc();
        }
    }
    assert(abrupt().id, 3);
    assert(closed, 1);
    throws_same(() => { for (const value of values()) throw error; }, error);
    assert(closed, 2);
    function overriding_finally() {
        try { return cycle(4); }
        finally { std.gc(); return cycle(5); }
    }
    assert(overriding_finally().id, 5);
}

function test_suspended_generators()
{
    function* suspended() {
        try { return triple(cycle(1), yield cycle(2), cycle(3)); }
        finally { std.gc(); }
    }
    let iterator = suspended();
    let first = iterator.next();
    assert(first.value.id, 2);
    first = null;
    std.gc();
    let result = iterator.next(cycle(20));
    assert(result.value, "1,20,3");
    assert(result.done, true);
    iterator = suspended();
    iterator.next();
    std.gc();
    assert(iterator.return(9).value, 9);
    iterator = suspended();
    iterator.next();
    std.gc();
    const error = {};
    throws_same(() => iterator.throw(error), error);
    function* inner() {
        try { yield 1; return 3; }
        finally { yield 2; }
    }
    function* outer() { return 10 + (yield* inner()); }
    iterator = outer();
    assert(iterator.next().value, 1);
    std.gc();
    assert(iterator.next().value, 2);
    assert(iterator.next().value, 13);
}

async function test_suspended_async()
{
    assert(await (async () => {})(), undefined);
    let release;
    const gate = new Promise(resolve => { release = resolve; });
    async function suspended() {
        return triple(cycle(1), await gate, cycle(3));
    }
    const pending = suspended();
    std.gc();
    release(cycle(20));
    assert(await pending, "1,20,3");
    const error = {};
    let closed = 0;
    const iterator = {
        [Symbol.asyncIterator]() { return this; },
        next() { std.gc(); throw error; },
        return() { closed++; return {}; }
    };
    let caught;
    try {
        for await (const value of iterator) void value;
    } catch (e) {
        caught = e;
    }
    assert(caught, error);
    assert(closed, 0);
    async function* suspended_generator() {
        try { return triple(cycle(1), yield cycle(2), cycle(3)); }
        finally { std.gc(); }
    }
    const generator = suspended_generator();
    assert((await generator.next()).value.id, 2);
    std.gc();
    assert((await generator.next(cycle(20))).value, "1,20,3");
    try {
        await (async () => triple(cycle(1), await Promise.reject(error), cycle(3)))();
        assert(false, true);
    } catch (e) {
        assert(e, error);
    }
    std.gc();
}

test_empty_and_deep_stacks();
test_mutable_call_arguments();
test_shuffles_and_slow_paths();
test_iterators_and_unwinding();
test_suspended_generators();
await test_suspended_async();
