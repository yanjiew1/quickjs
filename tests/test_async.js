/*
 * QuickJS asynchronous builtin tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */

export {};

function assert(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("assertion failed: " + actual + " !== " + expected);
}

async function test_class_strict_region_async()
{
    /* A constructor body supplies sloppy code even though this is a module. */
    const AsyncFunction = (async function() {}).constructor;
    await AsyncFunction("assert", `
        const frozen = Object.freeze({});
        try {
            class C { [(await Promise.reject(1))]() {} }
        } catch (e) {
            assert(e, 1);
            frozen.x = 1;
            eval("var after_catch = 2");
        }
        assert(after_catch, 2);
        class C { [(await 1, eval("var class_only = 1"), "key")]() {} }
        assert(typeof class_only, "undefined");
        frozen.x = 1;
        let closed = 0, finalized = 0;
        const iterable = {
            [Symbol.asyncIterator]() { return this; },
            async next() { return { value: 1, done: false }; },
            async return() { closed++; return {}; }
        };
        async function* suspended() {
            try {
                for await (const value of iterable) {
                    class C { [(yield 1, eval("var class_only = 1"), "key")]() {} }
                }
            } finally {
                frozen.x = 1;
                eval("var after_finally = 3");
                assert(after_finally, 3);
                finalized++;
            }
        }
        const iterator = suspended();
        assert((await iterator.next()).value, 1);
        assert((await iterator.return(2)).done, true);
        assert(closed, 1);
        assert(finalized, 1);
        const rejected = suspended();
        await rejected.next();
        let failure;
        try { await rejected.return(Promise.reject(7)); }
        catch (error) { failure = error; }
        assert(failure, 7);
        assert(closed, 2);
        assert(finalized, 2);
    `)(assert);
}

await test_class_strict_region_async();

async function test_function_constructor_binding()
{
    const constructors = [Function, (function*() {}).constructor,
                          (async function() {}).constructor,
                          (async function*() {}).constructor];
    async function evaluate(ctor, body) {
        const result = ctor(body)();
        if (result && typeof result.next === "function")
            return (await result.next()).value;
        return await result;
    }
    const saved = Object.getOwnPropertyDescriptor(globalThis, "anonymous");
    delete globalThis.anonymous;
    try {
        for (const ctor of constructors) {
            assert(await evaluate(ctor, "return typeof anonymous;"), "undefined");
            const nested = await evaluate(ctor,
                "return function() { eval(''); return typeof anonymous; };");
            assert(nested(), "undefined");
            const named = await evaluate(ctor,
                "return function local() { return local; };");
            assert(named(), named);
            assert(ctor().name, "anonymous");
            assert(await evaluate(ctor, "return this === globalThis;"), true);
        }
        globalThis.anonymous = "global value";
        for (const ctor of constructors) {
            assert(await evaluate(ctor, "return anonymous;"), "global value");
            const nested = await evaluate(ctor,
                "return function() { return anonymous; };");
            assert(nested(), "global value");
        }
    } finally {
        delete globalThis.anonymous;
        if (saved)
            Object.defineProperty(globalThis, "anonymous", saved);
    }
}

await test_function_constructor_binding();

async function test_catch_binding_finally()
{
    const events = [];
    const error = {};
    try {
        try { throw {}; }
        catch ({ x = await Promise.reject(error) }) {}
        finally { events.push("finally"); }
    } catch (e) {
        assert(e === error, true);
        events.push("catch");
    }
    assert(events.join(), "finally,catch");
}

await test_catch_binding_finally();
