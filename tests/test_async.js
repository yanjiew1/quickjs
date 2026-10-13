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

async function test_async_iterator_close_paths()
{
    function iterable(next, close) {
        const iterator = { next, [Symbol.asyncIterator]() { return this; } };
        Object.defineProperty(iterator, "return", close);
        return iterator;
    }
    const events = [];
    const exhausted = iterable(() => {
        events.push("next");
        return { done: true, get value() { throw Error("done value read"); } };
    }, { get() { throw Error("natural exhaustion must not read return"); } });
    for await (const value of exhausted)
        assert(false);
    assert(events.join(","), "next");

    events.length = 0;
    const source = iterable(() => ({ value: 1, done: false }), { value: function() {
        assert(this === source, true);
        assert(arguments.length, 0);
        events.push("close start");
        return Promise.resolve().then(() => {
            events.push("close end");
            return {};
        });
    } });
    for await (const value of source) {
        assert(value, 1);
        events.push("body");
        break;
    }
    events.push("after");
    assert(events.join(","), "body,close start,close end,after");

    for (const result of [0, undefined]) {
        let caught = false;
        try {
            for await (const value of iterable(() => ({ done: false }), {
                value() { return Promise.resolve(result); }
            })) break;
        } catch (error) { caught = error instanceof TypeError; }
        assert(caught, true);
    }
    for (const method of [null, undefined]) {
        for await (const value of iterable(() => ({ done: false }), {
            value: method
        })) break;
    }

    const bodyError = {}, closeError = {};
    for (const original of [bodyError, undefined]) {
        for (const kind of ["getter", "noncallable", "reject", "primitive"]) {
            events.length = 0;
            const close = kind === "getter" ? {
                get() { events.push("lookup"); throw closeError; }
            } : kind === "noncallable" ? { value: 1 } : {
                value() {
                    events.push("close start");
                    return Promise.resolve().then(() => {
                        events.push("close end");
                        if (kind === "reject") throw closeError;
                        return 0;
                    });
                }
            };
            let caught = false;
            try {
                for await (const value of iterable(() => ({ done: false }), close))
                    throw original;
            } catch (error) {
                caught = true;
                assert(error, original);
                events.push("caught");
            }
            assert(caught, true);
            if (kind === "reject" || kind === "primitive")
                assert(events.join(","), "close start,close end,caught");
        }
    }
    for (const close of [{ get() { throw closeError; } }, { value: 1 }]) {
        let caught;
        try {
            for await (const value of iterable(() => ({ done: false }), close))
                break;
        } catch (error) { caught = error; }
        assert(close.get ? caught === closeError : caught instanceof TypeError, true);
    }

    for (const kind of ["next throw", "next reject", "primitive", "done", "value"]) {
        let closes = 0, caught;
        const next = () => {
            if (kind === "next throw") throw bodyError;
            if (kind === "next reject") return Promise.reject(bodyError);
            if (kind === "primitive") return 0;
            return {
                get done() { if (kind === "done") throw bodyError; return false; },
                get value() { throw bodyError; }
            };
        };
        try {
            for await (const value of iterable(next, {
                value() { closes++; return {}; }
            })) assert(false);
        } catch (error) { caught = error; }
        assert(kind === "primitive" ? caught instanceof TypeError : caught === bodyError, true);
        assert(closes, 0);
    }
}

async function test_async_iterator_close_return_and_labels()
{
    const events = [];
    function source(name) {
        let index = 0;
        return {
            [Symbol.asyncIterator]() { return this; },
            next() { return { value: index++, done: false }; },
            return() {
                events.push(name + " start");
                return Promise.resolve().then(() => {
                    events.push(name + " end");
                    return {};
                });
            }
        };
    }
    async function returning() {
        for await (const value of source("return")) return 42;
    }
    assert(await returning(), 42);
    assert(events.join(","), "return start,return end");
    events.length = 0;
    async function* generator() {
        for await (const value of source("generator")) yield* [value, 100];
    }
    const iterator = generator();
    assert((await iterator.next()).value, 0);
    const result = await iterator.return(43);
    assert(result.done, true);
    assert(result.value, 43);
    assert(events.join(","), "generator start,generator end");
    async function* delegated() {
        try { yield* [7, 8]; }
        finally { events.push("delegate finally"); }
    }
    const delegate = delegated();
    assert((await delegate.next()).value, 7);
    assert((await delegate.return(9)).value, 9);
    assert(events[events.length - 1], "delegate finally");
    const closeError = {};
    let capture;
    async function divertedReturn() {
        const iterator = source("diverted");
        iterator.return = () => Promise.reject(closeError);
        try {
            for await (const value of iterator) {
                let retained = 42;
                capture = () => retained;
                return 1;
            }
        } catch (error) { assert(error, closeError); }
        { let reused = 99; assert(reused, 99); }
        return capture();
    }
    assert(await divertedReturn(), 42);
    events.length = 0;
    outer: for await (const outerValue of source("outer")) {
        for await (const innerValue of source("inner")) {
            if (outerValue === 0) continue outer;
            break outer;
        }
    }
    assert(events.join(","), "inner start,inner end,inner start,inner end,outer start,outer end");
}

await test_async_iterator_close_paths();
await test_async_iterator_close_return_and_labels();

async function test_async_iterator_eval_closure()
{
    let sum = 0;
    for await (const value of [1, 2])
        sum += eval("value");
    assert(eval("sum"), 3);
}

await test_async_iterator_eval_closure();

async function test_async_iterator_catch_join()
{
    const AsyncFunction = Object.getPrototypeOf(async function() {}).constructor;
    function source(values, closes, name) {
        let index = 0;
        return {
            [Symbol.asyncIterator]() { return this; },
            next() {
                if (index === values.length) return { done: true };
                return { value: values[index++], done: false };
            },
            return() { closes.push(name); return Promise.resolve({}); }
        };
    }

    const closes = [], values = [];
    const normal = AsyncFunction("source", "values",
        "for await (const value of source) values.push(value);");
    await normal(source([1, 2], closes, "normal"), values);
    assert(values.join(","), "1,2");
    assert(closes.length, 0);

    const continued = AsyncFunction("source", "values",
        "try { for await (const value of source) { " +
        "if (value === 1) continue; values.push(value); } } " +
        "catch (error) { throw Error('unexpected outer catch: ' + error); }");
    values.length = 0;
    await continued(source([1, 2], closes, "continue"), values);
    assert(values.join(","), "2");
    assert(closes.length, 0);

    const labeled = AsyncFunction("outerSource", "innerSource",
        "outer: for await (const outerValue of outerSource) { " +
        "for await (const innerValue of innerSource()) { " +
        "if (outerValue === 1) continue outer; break outer; } }");
    await labeled(source([1, 2], closes, "outer"),
                  () => source([7], closes, "inner"));
    assert(closes.join(","), "inner,inner,outer");
}

await test_async_iterator_catch_join();

async function test_async_iterator_result_order()
{
    let closes = 0;
    function source(next) {
        return {
            [Symbol.asyncIterator]() { return this; },
            next,
            return() { closes++; return {}; }
        };
    }

    const doneObject = { [Symbol.toPrimitive]() {
        throw Error("done must use ToBoolean without coercion");
    } };
    for (const done of [true, 1, "done", 1n, Symbol(), doneObject]) {
        let doneReads = 0, valueReads = 0;
        const iterable = source(() => ({
            get done() { doneReads++; return done; },
            get value() { valueReads++; throw Error("done value read"); }
        }));
        for await (const value of iterable) assert(false);
        assert(doneReads, 1);
        assert(valueReads, 0);
        assert(closes, 0);
    }

    for (const done of [false, 0, "", 0n, null, undefined, NaN]) {
        let index = 0;
        const events = [];
        const iterable = source(() => ({
            get done() {
                events.push("done " + index);
                return index++ === 0 ? done : true;
            },
            get value() {
                events.push("value");
                if (index === 2) throw Error("done value read");
                return 7;
            }
        }));
        for await (const value of iterable) events.push("body " + value);
        assert(events.join(","), "done 0,value,body 7,done 1");
        assert(closes, 0);
    }

    for (const result of [null, undefined, 0, false, "x", 1n, Symbol()]) {
        let caught;
        try {
            for await (const value of source(() => result)) assert(false);
        } catch (error) { caught = error; }
        assert(caught instanceof TypeError, true);
        assert(closes, 0);
    }

    const original = {};
    for (const property of ["done", "value"]) {
        let caught;
        const result = { done: false };
        Object.defineProperty(result, property, { get() { throw original; } });
        try {
            for await (const value of source(() => result)) assert(false);
        } catch (error) { caught = error; }
        assert(caught, original);
        assert(closes, 0);
    }

    let finalValueReads = 0;
    async function* delegated() {
        return yield* source(() => ({
            done: true,
            get value() { finalValueReads++; return 42; }
        }));
    }
    const final = await delegated().next();
    assert(final.done, true);
    assert(final.value, 42);
    assert(finalValueReads, 1);
    assert(closes, 0);
}

await test_async_iterator_result_order();

async function test_synchronous_using_in_async_bodies()
{
    const events = [];
    async function functionBody() {
        using resource = { [Symbol.dispose]() { events.push("function"); } };
        await Promise.resolve();
        return 42;
    }
    const pending = functionBody();
    assert(events.length, 0);
    assert(await pending, 42);
    assert(events.join(","), "function");

    events.length = 0;
    async function* generatorBody() {
        using resource = { [Symbol.dispose]() { events.push("generator"); } };
        yield 1;
    }
    const iterator = generatorBody();
    assert((await iterator.next()).value, 1);
    assert(events.length, 0);
    assert((await iterator.return(2)).value, 2);
    assert(events.join(","), "generator");
}

await test_synchronous_using_in_async_bodies();

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
