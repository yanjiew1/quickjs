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
