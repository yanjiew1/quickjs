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

function assert_async_disposal_throws(constructor, callback)
{
    let caught;
    try { callback(); } catch (error) { caught = error; }
    assert(caught instanceof constructor, true);
}

async function test_async_disposable_stack_registration()
{
    const prototype = AsyncDisposableStack.prototype;
    assert(AsyncDisposableStack.length, 0);
    assert(AsyncDisposableStack.name, "AsyncDisposableStack");
    assert(prototype[Symbol.asyncDispose] === prototype.disposeAsync, true);
    assert(Object.prototype.toString.call(new AsyncDisposableStack()),
           "[object AsyncDisposableStack]");
    assert(Object.getPrototypeOf(prototype) === Object.prototype, true);
    const disposed = Object.getOwnPropertyDescriptor(prototype, "disposed");
    assert(disposed.set, undefined);
    assert(disposed.enumerable, false);
    assert(disposed.configurable, true);
    for (const [name, length] of [["use", 1], ["adopt", 2], ["defer", 1],
                                 ["move", 0], ["disposeAsync", 0]]) {
        assert(prototype[name].length, length);
        assert(prototype[name].name, name);
        assert(Object.hasOwn(prototype[name], "prototype"), false);
        const descriptor = Object.getOwnPropertyDescriptor(prototype, name);
        assert(descriptor.writable, true);
        assert(descriptor.enumerable, false);
        assert(descriptor.configurable, true);
    }
    assert_async_disposal_throws(TypeError, () => AsyncDisposableStack());
    class Derived extends AsyncDisposableStack {}
    assert(new Derived() instanceof Derived, true);
    const stack = new AsyncDisposableStack();
    const order = [];
    const resource = {
        [Symbol.asyncDispose]() {
            "use strict";
            assert(this === resource, true);
            assert(arguments.length, 0);
            order.push("use");
            return Promise.resolve().then(() => order.push("use awaited"));
        },
        get [Symbol.dispose]() { throw Error("async lookup has priority"); }
    };
    const adopted = {};
    assert(stack.use(resource) === resource, true);
    assert(stack.adopt(adopted, function(value) {
        "use strict";
        assert(this, undefined);
        assert(value === adopted, true);
        assert(arguments.length, 1);
        order.push("adopt");
        return Promise.resolve().then(() => order.push("adopt awaited"));
    }) === adopted, true);
    assert(stack.defer(function() {
        "use strict";
        assert(this, undefined);
        assert(arguments.length, 0);
        order.push("defer");
        return Promise.resolve().then(() => order.push("defer awaited"));
    }), undefined);
    resource[Symbol.asyncDispose] = () => {
        throw Error("method must be cached");
    };
    assert(stack.disposed, false);
    const promise = stack.disposeAsync();
    assert(promise instanceof Promise, true);
    assert(stack.disposed, true);
    assert(order.join(","), "defer");
    assert(await promise, undefined);
    assert(order.join(","),
           "defer,defer awaited,adopt,adopt awaited,use,use awaited");
    const repeated = stack.disposeAsync();
    assert(repeated !== promise, true);
    assert(await repeated, undefined);
    for (const receiver of [null, undefined, 1, {}, prototype,
                            new Proxy(new AsyncDisposableStack(), {}),
                            new DisposableStack()]) {
        assert_async_disposal_throws(TypeError,
            () => prototype.use.call(receiver, resource));
        assert_async_disposal_throws(TypeError,
            () => prototype.adopt.call(receiver, 0, () => {}));
        assert_async_disposal_throws(TypeError,
            () => prototype.defer.call(receiver, () => {}));
        assert_async_disposal_throws(TypeError,
            () => prototype.move.call(receiver));
        assert_async_disposal_throws(TypeError,
            () => disposed.get.call(receiver));
        const invalid = prototype.disposeAsync.call(receiver);
        assert(invalid instanceof Promise, true);
        let caught;
        try { await invalid; } catch (error) { caught = error; }
        assert(caught instanceof TypeError, true);
    }
    const pending = new AsyncDisposableStack();
    for (const value of [1, "x", true, Symbol(), 1n, {},
                         { [Symbol.asyncDispose]: null },
                         { [Symbol.asyncDispose]: 1 },
                         { [Symbol.dispose]: 1 }]) {
        assert_async_disposal_throws(TypeError, () => pending.use(value));
    }
    assert_async_disposal_throws(TypeError, () => pending.defer(1));
    assert_async_disposal_throws(TypeError, () => pending.adopt({}, null));
    let reads = 0;
    const throwing = {
        get [Symbol.asyncDispose]() { reads++; throw Error("getter"); }
    };
    assert_async_disposal_throws(Error, () => pending.use(throwing));
    assert(reads, 1);
    await pending.disposeAsync();
    assert_async_disposal_throws(ReferenceError, () => pending.use(throwing));
    assert(reads, 1);
    assert_async_disposal_throws(ReferenceError, () => pending.adopt({}, 1));
    assert_async_disposal_throws(ReferenceError, () => pending.defer(1));
    assert_async_disposal_throws(ReferenceError, () => pending.move());
}

async function test_async_disposable_stack_fallback_and_await()
{
    const stack = new AsyncDisposableStack();
    let order = "", fallbackReads = 0;
    const ignored = Promise.reject("ignored result");
    ignored.catch(() => {});
    const resource = {
        get [Symbol.asyncDispose]() { order += "async lookup;"; return null; },
        get [Symbol.dispose]() {
            fallbackReads++;
            order += "sync lookup;";
            return function() {
                assert(this === resource, true);
                assert(arguments.length, 0);
                order += "sync call;";
                return ignored;
            };
        }
    };
    stack.use(resource);
    Object.defineProperty(resource, Symbol.dispose, {
        value() { throw Error("fallback method must be cached"); }
    });
    assert(await stack.disposeAsync(), undefined);
    assert(fallbackReads, 1);
    assert(order, "async lookup;sync lookup;sync call;");
    const ignoredThen = new AsyncDisposableStack();
    ignoredThen.use({ [Symbol.dispose]() {
        return { get then() { throw Error("fallback result is ignored"); } };
    } });
    assert(await ignoredThen.disposeAsync(), undefined);
    const sentinel = {};
    const throwing = new AsyncDisposableStack();
    throwing.use({ [Symbol.dispose]() { throw sentinel; } });
    let caught;
    try { await throwing.disposeAsync(); } catch (error) { caught = error; }
    assert(caught === sentinel, true);

    const thenable = new AsyncDisposableStack();
    let awaited = false;
    thenable.use({ [Symbol.asyncDispose]() {
        return { then(resolve) { awaited = true; resolve(); } };
    } });
    await thenable.disposeAsync();
    assert(awaited, true);

    async function trace(nullish, directThrow) {
        const current = new AsyncDisposableStack();
        if (directThrow)
            current.defer(() => { throw sentinel; });
        if (nullish) {
            assert(current.use(null), null);
            assert(current.use(undefined), undefined);
        }
        const events = [];
        const disposal = current.disposeAsync();
        disposal.then(() => events.push("disposed"),
                      error => {
                          assert(error === sentinel, true);
                          events.push("disposed");
                      });
        Promise.resolve().then(() => events.push("marker"));
        try { await disposal; } catch (error) {
            assert(error === sentinel, true);
        }
        await Promise.resolve();
        return events.join(",");
    }
    assert(await trace(false, false), "disposed,marker");
    assert(await trace(true, false), "marker,disposed");
    assert(await trace(false, true), "disposed,marker");
    assert(await trace(true, true), "marker,disposed");

    const intrinsicPromise = Promise;
    const resolve = Promise.resolve;
    const then = Promise.prototype.then;
    const ownPromise = Promise.resolve();
    Object.defineProperty(ownPromise, "then", {
        get() { throw Error("Await must use internal promise reactions"); }
    });
    const poisoned = new AsyncDisposableStack();
    poisoned.defer(() => ownPromise);
    const poison = () => { throw Error("mutable Promise API must be ignored"); };
    let disposal;
    try {
        Promise.resolve = poison;
        Promise.prototype.then = poison;
        globalThis.Promise = poison;
        disposal = poisoned.disposeAsync();
    } finally {
        globalThis.Promise = intrinsicPromise;
        Promise.resolve = resolve;
        Promise.prototype.then = then;
    }
    assert(disposal instanceof intrinsicPromise, true);
    assert(await disposal, undefined);
}

async function test_async_disposable_stack_move_reentrancy_and_gc()
{
    let collectGarbage = globalThis.gc;
    if (typeof collectGarbage !== "function") {
        try {
            collectGarbage = (await import("std")).gc;
        } catch (error) {
            /* Other engines may not provide explicit garbage collection. */
        }
    }
    class Derived extends AsyncDisposableStack {}
    const source = new Derived();
    let calls = 0;
    source.defer(() => { calls++; });
    source.constructor = { get [Symbol.species]() {
        throw Error("move does not consult species");
    } };
    const moved = source.move();
    assert(Object.getPrototypeOf(moved) === AsyncDisposableStack.prototype, true);
    assert(moved instanceof Derived, false);
    assert(source.disposed, true);
    assert(moved.disposed, false);
    await source.disposeAsync();
    assert(calls, 0);
    await moved.disposeAsync();
    assert(calls, 1);

    const getterSource = new AsyncDisposableStack();
    let getterMoved;
    getterSource.use({ get [Symbol.asyncDispose]() {
        getterMoved = getterSource.move();
        return () => { calls++; };
    } });
    assert(getterSource.disposed, true);
    await getterSource.disposeAsync();
    assert(calls, 1);
    await getterMoved.disposeAsync();
    assert(calls, 2);

    const recursive = new AsyncDisposableStack();
    let repeated;
    recursive.defer(() => {
        assert(recursive.disposed, true);
        repeated = recursive.disposeAsync();
        assert_async_disposal_throws(ReferenceError,
            () => recursive.defer(() => {}));
    });
    const first = recursive.disposeAsync();
    assert(first !== repeated, true);
    assert(await repeated, undefined);
    assert(await first, undefined);

    const getterDispose = new AsyncDisposableStack();
    let release;
    const gate = new Promise(resolve => { release = resolve; });
    let active;
    getterDispose.defer(() => gate);
    getterDispose.use({ get [Symbol.asyncDispose]() {
        active = getterDispose.disposeAsync();
        return () => { throw Error("late resource must not be traversed"); };
    } });
    assert(getterDispose.disposed, true);
    assert(await getterDispose.disposeAsync(), undefined);
    release();
    assert(await active, undefined);

    let gcStack = new AsyncDisposableStack();
    let gcRelease;
    const gcGate = new Promise(resolve => { gcRelease = resolve; });
    gcStack.use({
        stack: gcStack,
        [Symbol.asyncDispose]() { calls++; }
    });
    gcStack.defer(() => gcGate);
    const gcDisposal = gcStack.disposeAsync();
    gcStack = null;
    if (typeof collectGarbage === "function")
        collectGarbage();
    gcRelease();
    await gcDisposal;
    assert(calls, 3);
}

async function test_async_disposable_stack_suppression()
{
    const first = {}, second = {}, third = {};
    const stack = new AsyncDisposableStack();
    stack.defer(() => { throw third; });
    stack.defer(() => Promise.reject(second));
    stack.use({ [Symbol.asyncDispose]() {
        return { then(resolve, reject) { reject(first); } };
    } });
    const intrinsic = SuppressedError;
    globalThis.SuppressedError = function() {
        throw Error("suppression must use the intrinsic");
    };
    let caught;
    try {
        try { await stack.disposeAsync(); } catch (error) { caught = error; }
    } finally {
        globalThis.SuppressedError = intrinsic;
    }
    assert(caught instanceof intrinsic, true);
    assert(caught.error === third, true);
    assert(caught.suppressed instanceof intrinsic, true);
    assert(caught.suppressed.error === second, true);
    assert(caught.suppressed.suppressed === first, true);
    assert(stack.disposed, true);
    const undefinedFailure = new AsyncDisposableStack();
    undefinedFailure.defer(() => { throw undefined; });
    let threw = false;
    try { await undefinedFailure.disposeAsync(); } catch (error) {
        threw = true;
        assert(error, undefined);
    }
    assert(threw, true);
    const next = new AsyncDisposableStack();
    const poisoned = Promise.resolve();
    Object.defineProperty(poisoned, "constructor", {
        get() { throw first; }
    });
    let continued = false;
    next.defer(() => { continued = true; });
    next.defer(() => poisoned);
    caught = undefined;
    try { await next.disposeAsync(); } catch (error) { caught = error; }
    assert(caught === first, true);
    assert(continued, true);
}

await test_async_disposable_stack_registration();
await test_async_disposable_stack_fallback_and_await();
await test_async_disposable_stack_move_reentrancy_and_gc();
await test_async_disposable_stack_suppression();

async function test_resource_cursor_nullish_traversal()
{
    const events = [], error = {};
    const stack = new AsyncDisposableStack();
    stack.defer(() => { events.push("last"); throw error; });
    stack.use(null);
    stack.use(undefined);
    stack.defer(() => {
        events.push("first");
        return Promise.resolve().then(() => events.push("awaited"));
    });
    const disposal = stack.disposeAsync();
    assert(events.join(","), "first");
    try {
        await disposal;
        assert(false);
    } catch (caught) { assert(caught, error); }
    assert(events.join(","), "first,awaited,last");
    assert(await stack.disposeAsync(), undefined);
}

await test_resource_cursor_nullish_traversal();

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

/* Source-only regression additions; append to tests/test_async.js. */
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

/* Source-only regression additions; append to tests/test_async.js. */
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

async function assert_array_from_async_rejects(operation, expected)
{
    let rejected = false;
    try {
        await operation();
    } catch (error) {
        rejected = true;
        assert(error, expected);
    }
    assert(rejected, true);
}

function assert_array_from_async_values(actual, expected)
{
    assert(actual.length, expected.length);
    for (let i = 0; i < expected.length; i++)
        assert(actual[i], expected[i]);
}

async function test_array_from_async_sources()
{
    assert(Array.fromAsync.name, "fromAsync");
    assert(Array.fromAsync.length, 1);
    const descriptor = Object.getOwnPropertyDescriptor(Array, "fromAsync");
    assert(descriptor.writable, true);
    assert(descriptor.enumerable, false);
    assert(descriptor.configurable, true);
    let constructionFailed = false;
    try {
        new Array.fromAsync([]);
    } catch (error) {
        constructionFailed = error instanceof TypeError;
    }
    assert(constructionFailed, true);

    let inputTouched = false;
    const invalidInput = {
        get [Symbol.asyncIterator]() { inputTouched = true; throw Error(); },
    };
    const invalidMapper = Array.fromAsync(invalidInput, null);
    assert(invalidMapper instanceof Promise, true);
    let mapperRejected = false;
    try {
        await invalidMapper;
    } catch (error) {
        mapperRejected = error instanceof TypeError;
    }
    assert(mapperRejected, true);
    assert(inputTouched, false);
    for (const input of [undefined, null]) {
        let rejected = false;
        try {
            await Array.fromAsync(input);
        } catch (error) {
            rejected = error instanceof TypeError;
        }
        assert(rejected, true);
    }
    for (const input of [7, true, 8n, Symbol("input"), {}])
        assert_array_from_async_values(await Array.fromAsync(input), []);
    assert_array_from_async_values(await Array.fromAsync("ab"), ["a", "b"]);

    const value = Promise.resolve(31);
    const asyncItems = {
        [Symbol.asyncIterator]() {
            let index = 0;
            return { next() { return { done: index++ !== 0, value }; } };
        },
        get [Symbol.iterator]() { throw Error("sync iterator was accessed"); },
    };
    const unmapped = await Array.fromAsync(asyncItems);
    assert(unmapped[0], value);
    const receiver = {};
    const mapped = await Array.fromAsync(asyncItems, function(item, index) {
        assert(this, receiver);
        assert(item, value);
        assert(index, 0);
        return Promise.resolve(32);
    }, receiver);
    assert_array_from_async_values(mapped, [32]);

    assert_array_from_async_values(await Array.fromAsync([value]), [31]);
    assert_array_from_async_values(await Array.fromAsync({ 0: value, length: 1 }),
                                   [31]);
    const indices = [];
    assert_array_from_async_values(await Array.fromAsync([11, 22],
        (item, index) => { indices.push(index); return item + index; }), [11, 23]);
    assert(indices.join(","), "0,1");
    const holes = await Array.fromAsync({ length: 2 });
    assert_array_from_async_values(holes, [undefined, undefined]);
    assert(Object.hasOwn(holes, "0"), true);
    assert(Object.hasOwn(holes, "1"), true);
    const nullAsyncMethod = {
        [Symbol.asyncIterator]: null,
        *[Symbol.iterator]() { yield 41; },
    };
    assert_array_from_async_values(await Array.fromAsync(nullAsyncMethod), [41]);

    let releaseMap, enterMap;
    const entered = new Promise(resolve => { enterMap = resolve; });
    const pendingMap = new Promise(resolve => { releaseMap = resolve; });
    let nextCount = 0;
    const sequential = Array.fromAsync({
        [Symbol.asyncIterator]() {
            return {
                next() {
                    nextCount++;
                    return { done: nextCount > 1, value: 51 };
                },
            };
        },
    }, item => { assert(item, 51); enterMap(); return pendingMap; });
    await entered;
    assert(nextCount, 1);
    releaseMap(52);
    assert_array_from_async_values(await sequential, [52]);
    assert(nextCount, 2);
}

async function test_array_from_async_construction()
{
    const events = [];
    let nextCount = 0;
    const iterator = {
        get next() {
            events.push("next");
            return function() {
                assert(this, iterator);
                assert(arguments.length, 0);
                return { done: nextCount++ > 0, value: 61 };
            };
        },
    };
    const items = {
        [Symbol.asyncIterator]() { events.push("iterator"); return iterator; },
    };
    const outputPrototype = {
        set 0(value) { throw Error("inherited setter was called"); },
    };
    function IterableOutput() {
        events.push("constructor");
        assert(arguments.length, 0);
        return Object.create(outputPrototype);
    }
    const result = await Array.fromAsync.call(IterableOutput, items);
    assert(events.join(","), "iterator,next,constructor");
    assert(result[0], 61);
    assert(result.length, 1);
    const element = Object.getOwnPropertyDescriptor(result, "0");
    assert(element.writable, true);
    assert(element.enumerable, true);
    assert(element.configurable, true);
    assert(nextCount, 2);

    const arrayLikeEvents = [];
    const arrayLike = {
        get length() { arrayLikeEvents.push("length"); return 2; },
        get 0() { arrayLikeEvents.push("0"); return 71; },
        get 1() { arrayLikeEvents.push("1"); return 72; },
    };
    function ArrayLikeOutput(length) {
        arrayLikeEvents.push("constructor");
        assert(length, 2);
        assert(arguments.length, 1);
        return {};
    }
    const arrayLikePromise = Array.fromAsync.call(ArrayLikeOutput, arrayLike);
    assert(arrayLikeEvents.join(","), "length,constructor,0");
    assert_array_from_async_values(await arrayLikePromise, [71, 72]);
    assert(arrayLikeEvents.join(","), "length,constructor,0,1");
    class SubArray extends Array {}
    assert((await SubArray.fromAsync([1, 2])) instanceof SubArray, true);
    assert_array_from_async_values(await Array.fromAsync.call(() => {}, [1]), [1]);

    let lengthReads = 0;
    const mutable = {
        get length() { lengthReads++; return 2; },
        get 0() { delete this[1]; return 81; },
        1: 82,
    };
    assert_array_from_async_values(await Array.fromAsync(mutable), [81, undefined]);
    assert(lengthReads, 1);
    const lengthError = {};
    for (const length of [4294967296, Infinity]) {
        let receivedLength;
        function LargeOutput(value) { receivedLength = value; return {}; }
        await assert_array_from_async_rejects(() => Array.fromAsync.call(
            LargeOutput, { length, get 0() { throw lengthError; } }), lengthError);
        assert(receivedLength, length === Infinity ? 9007199254740991 : length);
    }
}

async function test_array_from_async_failure_routing()
{
    const error = {};
    let closeCount = 0;
    function source(next) {
        return {
            [Symbol.asyncIterator]() {
                return {
                    next,
                    return() { closeCount++; return {}; },
                };
            },
        };
    }
    for (const next of [
        function() { throw error; },
        function() { return Promise.reject(error); },
        function() { return { get done() { throw error; } }; },
        function() { return { done: false, get value() { throw error; } }; },
    ]) {
        await assert_array_from_async_rejects(() => Array.fromAsync(source(next)),
                                              error);
        assert(closeCount, 0);
    }
    let primitiveRejected = false;
    try {
        await Array.fromAsync(source(() => 1));
    } catch (error) {
        primitiveRejected = error instanceof TypeError;
    }
    assert(primitiveRejected, true);
    assert(closeCount, 0);
    const doneSource = source(() => ({
        done: true,
        get value() { throw Error("completed value was accessed"); },
    }));
    assert_array_from_async_values(await Array.fromAsync(doneSource), []);

    let calledNext = false;
    const constructorSource = source(() => { calledNext = true; return {}; });
    function BadConstructor() { throw error; }
    await assert_array_from_async_rejects(
        () => Array.fromAsync.call(BadConstructor, constructorSource), error);
    assert(calledNext, false);
    assert(closeCount, 0);

    function BadLengthOutput() {
        return { set length(value) { throw error; } };
    }
    await assert_array_from_async_rejects(
        () => Array.fromAsync.call(BadLengthOutput, doneSource), error);
    assert(closeCount, 0);
    function BadElementOutput() {
        return new Proxy({}, { defineProperty() { throw error; } });
    }
    await assert_array_from_async_rejects(
        () => Array.fromAsync.call(BadElementOutput,
                                   source(() => ({ done: false, value: 1 }))), error);
    assert(closeCount, 1);

    for (const close of [
        { get return() { throw "close getter"; } },
        { return() { throw "close call"; } },
        { return() { return Promise.reject("close rejection"); } },
        { return() { return 17; } },
        { return: null },
        { return: 3 },
    ]) {
        let closedSource = {
            [Symbol.asyncIterator]() {
                const iterator = { next() { return { value: 1, done: false }; } };
                Object.defineProperty(iterator, "return",
                    Object.getOwnPropertyDescriptor(close, "return"));
                return iterator;
            },
        };
        await assert_array_from_async_rejects(
            () => Array.fromAsync(closedSource, () => { throw error; }), error);
        await assert_array_from_async_rejects(
            () => Array.fromAsync(closedSource, () => Promise.reject(error)), error);
    }

    let finishClose, enterClose, returnCount = 0;
    const closing = new Promise(resolve => { enterClose = resolve; });
    const closeGate = new Promise(resolve => { finishClose = resolve; });
    const gatedSource = {
        [Symbol.asyncIterator]() {
            const iterator = {
                next() { return { done: false, value: 1 }; },
                return() {
                    returnCount++;
                    assert(this, iterator);
                    assert(arguments.length, 0);
                    enterClose();
                    return closeGate;
                },
            };
            return iterator;
        },
    };
    let settled = false;
    const gatedResult = Array.fromAsync(gatedSource, () => { throw error; });
    gatedResult.then(() => { settled = true; }, () => { settled = true; });
    await closing;
    assert(settled, false);
    finishClose(3);
    await assert_array_from_async_rejects(() => gatedResult, error);
    assert(returnCount, 1);

    let syncClosed = 0;
    await assert_array_from_async_rejects(() => Array.fromAsync({
        [Symbol.iterator]() {
            return {
                next() { return { value: Promise.reject(error), done: false }; },
                return() { syncClosed++; return {}; },
            };
        },
    }), error);
    assert(syncClosed, 1);
    await assert_array_from_async_rejects(() => Array.fromAsync({
        length: 1,
        get 0() { throw error; },
    }), error);
    await assert_array_from_async_rejects(
        () => Array.fromAsync({ 0: Promise.reject(error), length: 1 }), error);
    await assert_array_from_async_rejects(
        () => Array.fromAsync([1], () => { throw undefined; }), undefined);
}

async function test_array_from_async_intrinsic_promises()
{
    const promise = Promise.resolve(91);
    let constructorReads = 0;
    Object.defineProperty(promise, "constructor", {
        get() { constructorReads++; return Promise; },
    });
    assert_array_from_async_values(await Array.fromAsync({ 0: promise, length: 1 }),
                                   [91]);
    assert(constructorReads, 1);
    const nextValue = Promise.resolve(92);
    const resolveDescriptor = Object.getOwnPropertyDescriptor(Promise, "resolve");
    const thenDescriptor = Object.getOwnPropertyDescriptor(Promise.prototype, "then");
    const speciesDescriptor = Object.getOwnPropertyDescriptor(Promise, Symbol.species);
    const savedPromise = globalThis.Promise;
    try {
        Promise.resolve = function() { throw Error("mutable Promise.resolve"); };
        Promise.prototype.then = function() { throw Error("mutable Promise.then"); };
        Object.defineProperty(Promise, Symbol.species, {
            get() { throw Error("mutable Promise species"); }, configurable: true,
        });
        globalThis.Promise = function() { throw Error("mutable Promise global"); };
        assert_array_from_async_values(
            await Array.fromAsync({ 0: nextValue, length: 1 }), [92]);
        assert_array_from_async_values(await Array.fromAsync([93]), [93]);
    } finally {
        globalThis.Promise = savedPromise;
        Object.defineProperty(Promise, "resolve", resolveDescriptor);
        Object.defineProperty(Promise.prototype, "then", thenDescriptor);
        Object.defineProperty(Promise, Symbol.species, speciesDescriptor);
    }
    let outputThenReads = 0;
    function Output() {
        return {
            get then() { outputThenReads++; return undefined; },
        };
    }
    const outputPromise = Array.fromAsync.call(Output, []);
    assert(outputThenReads, 0);
    const output = await outputPromise;
    assert(outputThenReads, 1);
    assert(output.length, 0);
    const thenEvents = [];
    const thenable = {
        get then() {
            thenEvents.push("get input then");
            return function(resolve, reject) {
                thenEvents.push("call input then");
                resolve(94);
                reject("ignored rejection");
                resolve(95);
            };
        },
    };
    assert_array_from_async_values(await Array.fromAsync({ 0: thenable, length: 1 },
        item => {
            assert(item, 94);
            thenEvents.push("map");
            return {
                get then() {
                    thenEvents.push("get mapper then");
                    return resolve => {
                        thenEvents.push("call mapper then");
                        resolve(96);
                    };
                },
            };
        }), [96]);
    assert(thenEvents.join(","), "get input then,call input then,map," +
           "get mapper then,call mapper then");
    const both = await Promise.all([Array.fromAsync([101]), Array.fromAsync([102])]);
    assert_array_from_async_values(both[0], [101]);
    assert_array_from_async_values(both[1], [102]);
}

await test_array_from_async_sources();
await test_array_from_async_construction();
await test_array_from_async_failure_routing();
await test_array_from_async_intrinsic_promises();
