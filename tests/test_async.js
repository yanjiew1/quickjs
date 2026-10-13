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
    for (const resourceHead of [false, true]) {
        const events = [], closeError = {};
        let thenReads = 0;
        const iterable = {
            [Symbol.iterator]() {
                return {
                    next() { return { value: { [Symbol.dispose]() {
                        events.push("dispose");
                    } }, done: false }; },
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
        async function* usingHead() {
            for (using value of iterable)
                return 42;
        }
        const result = await (resourceHead ? usingHead() : plain()).next();
        assert(result.done, true);
        assert(result.value, 42);
        assert(thenReads, 0);
        assert(events.join(","), resourceHead ? "dispose,close" : "close");
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

async function test_await_using_registration_and_mixed_order()
{
    const events = [];
    let asyncReads = 0, fallbackReads = 0;
    const asyncResource = {
        get [Symbol.asyncDispose]() {
            asyncReads++;
            return function() {
                assert(this, asyncResource);
                assert(arguments.length, 0);
                events.push("async start");
                return { then(resolve) { events.push("async end"); resolve(7); } };
            };
        },
        get [Symbol.dispose]() { throw Error("fallback must not be read"); }
    };
    const fallback = {
        [Symbol.asyncDispose]: null,
        get [Symbol.dispose]() {
            fallbackReads++;
            return function() {
                assert(this, fallback);
                events.push("fallback");
                return { get then() { throw Error("sync fallback return must be ignored"); } };
            };
        }
    };
    {
        using first = { [Symbol.dispose]() { events.push("first"); } };
        await using second = asyncResource, third = fallback;
        using last = { [Symbol.dispose]() { events.push("last"); } };
        events.push("body");
    }
    assert(asyncReads, 1);
    assert(fallbackReads, 1);
    assert(events.join(","), "body,last,fallback,async start,async end,first");

    const original = {};
    let caught;
    events.length = 0;
    try {
        await using first = { [Symbol.asyncDispose]() { events.push("first"); } },
                    second = { get [Symbol.asyncDispose]() { throw original; } };
        assert(false);
    } catch (error) { caught = error; }
    assert(caught, original);
    assert(events.join(","), "first");
    for (const bad of [0, "text", false, {}, { [Symbol.asyncDispose]: 1 }]) {
        let threw = false;
        try { await using resource = bad; }
        catch (error) { threw = error instanceof TypeError; }
        assert(threw, true);
    }
}

async function test_await_using_await_boundaries()
{
    let sameTurn = true, before, after;
    async function nullish() {
        {
            await using first = null, second = undefined;
            before = sameTurn;
        }
        after = sameTurn;
    }
    const pending = nullish();
    sameTurn = false;
    await pending;
    assert(before, true);
    assert(after, false);
    sameTurn = true;
    async function skipped() {
        outer: {
            if (true) break outer;
            await using resource = null;
        }
        after = sameTurn;
    }
    const skippedPromise = skipped();
    sameTurn = false;
    await skippedPromise;
    assert(after, true);

    sameTurn = true;
    async function syncOnly() {
        using first = { [Symbol.dispose]() { assert(sameTurn, true); } };
        if (true) return 42;
        await using unevaluated = null;
    }
    const syncPromise = syncOnly();
    sameTurn = false;
    assert(await syncPromise, 42);

    const events = [];
    async function between() {
        {
            using first = { [Symbol.dispose]() { events.push("sync"); } };
            await using second = null;
            events.push("body");
        }
        events.push("after");
    }
    const betweenPromise = between();
    assert(events.join(","), "body");
    await betweenPromise;
    assert(events.join(","), "body,sync,after");
}

async function test_await_using_suppression_and_capture()
{
    const bodyError = {}, asyncError = {}, syncError = {};
    const intrinsic = SuppressedError;
    let caught;
    globalThis.SuppressedError = function() { throw Error("intrinsic required"); };
    try {
        try {
            using first = { [Symbol.dispose]() { throw syncError; } };
            await using second = { [Symbol.asyncDispose]() {
                return Promise.reject(asyncError);
            } };
            throw bodyError;
        } catch (error) { caught = error; }
    } finally { globalThis.SuppressedError = intrinsic; }
    assert(caught instanceof intrinsic, true);
    assert(caught.error, syncError);
    assert(caught.suppressed instanceof intrinsic, true);
    assert(caught.suppressed.error, asyncError);
    assert(caught.suppressed.suppressed, bodyError);
    let threw = false;
    try {
        await using resource = { [Symbol.asyncDispose]() { throw undefined; } };
    } catch (error) { threw = true; assert(error, undefined); }
    assert(threw, true);
    threw = false;
    try { await using resource = null; throw undefined; }
    catch (error) { threw = true; assert(error, undefined); }
    assert(threw, true);

    let capture;
    async function divertedReturn() {
        try {
            await using resource = { [Symbol.asyncDispose]() {
                return Promise.reject(asyncError);
            } };
            { let retained = 42; capture = () => retained; return 1; }
        } catch (error) { assert(error, asyncError); }
        { let reused = 99; assert(reused, 99); }
        return capture();
    }
    assert(await divertedReturn(), 42);

    const events = [];
    async function returning() {
        await using resource = { [Symbol.asyncDispose]() {
            events.push("start");
            return Promise.resolve().then(() => events.push("end"));
        } };
        return 43;
    }
    const pending = returning();
    assert(events.join(","), "start");
    assert(await pending, 43);
    assert(events.join(","), "start,end");

    events.length = 0;
    async function* generator() {
        await using resource = { [Symbol.asyncDispose]() {
            events.push("start");
            return Promise.resolve().then(() => events.push("end"));
        } };
        yield 1;
    }
    const iterator = generator();
    assert((await iterator.next()).value, 1);
    assert(events.length, 0);
    const result = await iterator.return(44);
    assert(result.done, true);
    assert(result.value, 44);
    assert(events.join(","), "start,end");
}

async function test_await_using_classic_for()
{
    const events = [], captures = [];
    function resource(name) {
        return { [Symbol.asyncDispose]() {
            events.push(name + " start");
            return Promise.resolve().then(() => events.push(name + " end"));
        } };
    }
    let index = 0, retained;
    for (await using first = resource("first"), second = resource("second");
         index < 3; index++) {
        retained = first;
        captures.push(() => first);
        if (index === 0) continue;
        if (index === 1) break;
    }
    assert(index, 1);
    assert(captures[0](), retained);
    assert(captures[1](), retained);
    assert(events.join(","), "second start,second end,first start,first end");
    events.length = 0;
    const original = {};
    let caught;
    try {
        for (await using first = resource("first"), second = (() => { throw original; })();
             false;) assert(false);
    } catch (error) { caught = error; }
    assert(caught, original);
    assert(events.join(","), "first start,first end");
}

async function test_resource_iteration_heads()
{
    for (const asynchronousIterator of [false, true]) {
        for (const asynchronousDisposal of [false, true]) {
            const events = [], captures = [];
            let count = 0;
            const source = {
                [asynchronousIterator ? Symbol.asyncIterator : Symbol.iterator]() { return this; },
                next() {
                    events.push("next " + count);
                    const index = count++;
                    const value = {
                        index,
                        [Symbol.dispose]() { events.push("sync " + index); },
                        [Symbol.asyncDispose]() {
                            events.push("async start " + index);
                            return Promise.resolve().then(() => events.push("async end " + index));
                        }
                    };
                    return { value, done: index === 2 };
                },
                return() {
                    events.push("close start");
                    if (!asynchronousIterator) return {};
                    return Promise.resolve().then(() => { events.push("close end"); return {}; });
                }
            };
            if (asynchronousIterator && asynchronousDisposal) {
                for await (await using value of source) {
                    captures.push(() => value.index);
                    if (value.index === 0) continue;
                    break;
                }
            } else if (asynchronousIterator) {
                for await (using value of source) {
                    captures.push(() => value.index);
                    if (value.index === 0) continue;
                    break;
                }
            } else if (asynchronousDisposal) {
                for (await using value of source) {
                    captures.push(() => value.index);
                    if (value.index === 0) continue;
                    break;
                }
            } else {
                for (using value of source) {
                    captures.push(() => value.index);
                    if (value.index === 0) continue;
                    break;
                }
            }
            assert(captures[0](), 0);
            assert(captures[1](), 1);
            const disposal0 = asynchronousDisposal ? "async start 0,async end 0" : "sync 0";
            const disposal1 = asynchronousDisposal ? "async start 1,async end 1" : "sync 1";
            assert(events.join(","), "next 0," + disposal0 + ",next 1," + disposal1 +
                   ",close start" + (asynchronousIterator ? ",close end" : ""));
        }
    }

    const original = {}, disposalError = {}, closeError = {}, events = [];
    const source = {
        [Symbol.asyncIterator]() { return this; },
        next() { return { value: { [Symbol.asyncDispose]() {
            events.push("dispose"); return Promise.reject(disposalError);
        } }, done: false }; },
        return() { events.push("close"); return Promise.reject(closeError); }
    };
    let caught;
    try { for await (await using value of source) throw original; }
    catch (error) { caught = error; }
    assert(caught instanceof SuppressedError, true);
    assert(caught.error, disposalError);
    assert(caught.suppressed, original);
    assert(events.join(","), "dispose,close");

    for (await using of of []) assert(false);
    for await (await using of of []) assert(false);
}

async function test_await_using_syntax_and_tdz()
{
    const AsyncFunction = Object.getPrototypeOf(async function() {}).constructor;
    for (const source of [
        "await using value;", "await using [value] = null;",
        "await using {value} = null;", "await using let = null;",
        "if (true) await using value = null;", "label: await using value = null;",
        "switch (0) { case 0: await using value = null; }",
        "for (await using value in {}) {}", "for (await using value = null of []) {}",
        "for (await using first, second of []) {}", "for (await using [value] of []) {}",
        "await using value = null; var value;", "await using value = null; let value;",
        "await using value = null, value = null;", "await using value = null; value = 1;"
    ]) {
        if (source.endsWith("value = 1;")) {
            let caught = false;
            try { await new AsyncFunction(source)(); }
            catch (error) { caught = error instanceof TypeError; }
            assert(caught, true);
        } else {
            let caught = false;
            try { new AsyncFunction(source); }
            catch (error) { caught = error instanceof SyntaxError; }
            assert(caught, true);
        }
    }
    for (const source of ["await using value = null;", "{ await using value = null; }"]) {
        let caught = false;
        try { new Function(source); }
        catch (error) { caught = error instanceof SyntaxError; }
        assert(caught, true);
    }
    let caught = false;
    try { await using resource = resource; }
    catch (error) { caught = error instanceof ReferenceError; }
    assert(caught, true);
    const using = { item: 42 };
    assert((await using["item"]), 42);
    assert(await new AsyncFunction("const using = 43; return await\nusing;")(), 43);
}

async function test_await_using_suspended_roots_and_cached_method()
{
    let collectGarbage = globalThis.gc;
    if (typeof collectGarbage !== "function") {
        try {
            collectGarbage = (await import("std")).gc;
        } catch (error) {
            /* Other engines may not provide explicit garbage collection. */
        }
    }
    let release, calls = 0, capture;
    const gate = new Promise(resolve => { release = resolve; });
    async function disposing() {
        await using resource = {
            cycle: null,
            [Symbol.asyncDispose]() {
                calls++;
                assert(this.cycle, this);
                return gate;
            }
        };
        resource.cycle = resource;
        capture = () => resource;
        resource[Symbol.asyncDispose] = () => { throw Error("cached method required"); };
    }
    const pending = disposing();
    assert(calls, 1);
    if (typeof collectGarbage === "function")
        collectGarbage();
    assert(capture().cycle, capture());
    release();
    await pending;
    assert(calls, 1);
}

await test_await_using_registration_and_mixed_order();
await test_await_using_await_boundaries();
await test_await_using_suppression_and_capture();
await test_await_using_classic_for();
await test_resource_iteration_heads();
await test_await_using_syntax_and_tdz();
await test_await_using_suspended_roots_and_cached_method();

/* Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
/* Deferred unit: requires the composed native resource syntax package. */
async function test_async_generator_return_await_order()
{
    for (const asyncIterator of [false, true]) {
        for (const resourceHead of [false, true]) {
            for (const returnRequest of [false, true]) {
                for (const rejectValue of [false, true]) {
                    const events = [], valueError = {}, closeError = {};
                    let release;
                    const value = {
                        get then() {
                            events.push("value then");
                            return function(resolve, reject) {
                                release = () => rejectValue ? reject(valueError) : resolve(42);
                            };
                        }
                    };
                    const resource = {
                        [Symbol.dispose]() { events.push("dispose"); },
                        [Symbol.asyncDispose]() {
                            events.push("dispose start");
                            return Promise.resolve().then(() => events.push("dispose end"));
                        }
                    };
                    const source = {
                        [asyncIterator ? Symbol.asyncIterator : Symbol.iterator]() {
                            return this;
                        },
                        next() { return { value: resource, done: false }; },
                        return() {
                            events.push("close");
                            if (asyncIterator) {
                                return { then(resolve, reject) {
                                    events.push("close await");
                                    if (rejectValue) reject(closeError);
                                    else resolve({});
                                } };
                            }
                            return { get then() {
                                events.push("wrong sync close await");
                                throw closeError;
                            } };
                        }
                    };
                    async function* generator() {
                        if (asyncIterator && resourceHead) {
                            for await (await using item of source) {
                                if (returnRequest) yield 1;
                                return value;
                            }
                        } else if (asyncIterator) {
                            for await (const item of source) {
                                if (returnRequest) yield 1;
                                return value;
                            }
                        } else if (resourceHead) {
                            for (using item of source) {
                                if (returnRequest) yield 1;
                                return value;
                            }
                        } else {
                            for (const item of source) {
                                if (returnRequest) yield 1;
                                return value;
                            }
                        }
                    }
                    const iterator = generator();
                    let pending;
                    if (returnRequest) {
                        assert((await iterator.next()).value, 1);
                        pending = iterator.return(value);
                    } else {
                        pending = iterator.next();
                    }
                    /* Allow the async iterator's next Await to finish. A
                       fixed bound also detects failure to await the value. */
                    for (let turn = 0; !release && turn < 32; turn++)
                        await Promise.resolve();
                    assert(typeof release, "function");
                    assert(events.join(","), "value then");
                    release();
                    let caught = false;
                    try {
                        const result = await pending;
                        assert(rejectValue, false);
                        assert(result.done, true);
                        assert(result.value, 42);
                    } catch (error) {
                        caught = true;
                        assert(rejectValue, true);
                        assert(error, valueError);
                    }
                    assert(caught, rejectValue);
                    const cleanup = resourceHead ? (asyncIterator ?
                        ",dispose start,dispose end" : ",dispose") : "";
                    assert(events.join(","), "value then" + cleanup + ",close" +
                           (asyncIterator ? ",close await" : ""));
                }
            }
        }
    }

    /* Ordinary async function return assimilation follows sync cleanup. */
    const events = [];
    const source = {
        [Symbol.iterator]() { return this; },
        next() { return { value: { [Symbol.dispose]() {
            events.push("dispose");
        } }, done: false }; },
        return() { events.push("close"); return {}; }
    };
    async function ordinary() {
        for (using item of source)
            return { then(resolve) { events.push("value then"); resolve(43); } };
    }
    assert(await ordinary(), 43);
    assert(events.join(","), "dispose,close,value then");
}

await test_async_generator_return_await_order();

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
