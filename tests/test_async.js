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

async function test_async_generator_sync_iterator_return()
{
    {
        const events = [], closeError = {};
        let thenReads = 0;
        const iterable = {
            [Symbol.iterator]() {
                return {
                    next() { return { value: 1, done: false }; },
                    return() {
                        events.push("close");
                        return { get then() {
                            thenReads++;
                            throw closeError;
                        } };
                    }
                };
            }
        };
        async function* plain() {
            for (const value of iterable)
                return 42;
        }
        const result = await plain().next();
        assert(result.done, true);
        assert(result.value, 42);
        assert(thenReads, 0);
        assert(events.join(","), "close");
    }
}

await test_async_generator_sync_iterator_return();
