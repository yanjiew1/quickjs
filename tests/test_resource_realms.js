/* AsyncDisposableStack realm regressions; test_api supplies foreign and runGC. */
(async function () {
    function check(actual, expected, message) {
        if (actual !== expected)
            throw Error(message);
    }

    const realms = [globalThis, foreign];
    for (let index = 0; index < realms.length; index++) {
        const owner = realms[index], other = realms[1 - index];
        const prototype = owner.AsyncDisposableStack.prototype;
        const otherPrototype = other.AsyncDisposableStack.prototype;
        const settle = other.Function("resolve", "value", "resolve(value)");
        function gate() {
            let resolve, reject;
            const promise = new owner.Promise(function (a, b) {
                resolve = a;
                reject = b;
            });
            return { promise, resolve, reject };
        }

        /* The invoked builtin realm differs from the stack constructor realm. */
        const stack = new other.AsyncDisposableStack();
        const first = gate();
        const ready = owner.Promise.resolve();
        let constructorReads = 0, thenReads = 0, repeat;
        Object.defineProperty(ready, "constructor", {
            get() { constructorReads++; return owner.Promise; },
        });
        Object.defineProperty(ready, "then", {
            get() { thenReads++; throw Error("foreign Promise assimilation"); },
        });
        prototype.defer.call(stack, () => ready);
        prototype.defer.call(stack, () => {
            check(stack.disposed, true, "disposal starts before invoking user code");
            repeat = otherPrototype.disposeAsync.call(stack);
            check(Object.getPrototypeOf(repeat), other.Promise.prototype,
                  "reentrant call uses its own invoked builtin realm");
            runGC();
            return first.promise;
        });
        const completion = prototype.disposeAsync.call(stack);
        check(Object.getPrototypeOf(completion), owner.Promise.prototype,
              "first completion belongs to the invoked builtin realm");
        runGC();
        settle(first.resolve);
        check(await completion, undefined, "disposal fulfills after foreign settlement");
        check(await repeat, undefined, "reentrant disposal does not restart the cursor");
        check(constructorReads, 1, "later Await reads the original constructor once");
        check(thenReads, 0, "same-realm Promise bypasses its poisoned then getter");
        const completed = otherPrototype.disposeAsync.call(stack);
        check(Object.getPrototypeOf(completed), other.Promise.prototype,
              "completed receiver still uses the newly invoked builtin realm");
        check(await completed, undefined, "already completed disposal fulfills");

        const failing = new other.AsyncDisposableStack();
        const rejected = gate();
        const firstError = {}, secondError = {};
        prototype.defer.call(failing, () => { throw secondError; });
        prototype.defer.call(failing, () => rejected.promise);
        const failure = prototype.disposeAsync.call(failing);
        runGC();
        settle(rejected.reject, firstError);
        let suppression;
        try {
            await failure;
            throw Error("multiple disposal errors must reject");
        } catch (error) {
            suppression = error;
        }
        check(Object.getPrototypeOf(suppression), owner.SuppressedError.prototype,
              "SuppressedError belongs to the first invocation realm");
        check(suppression.error, secondError, "later error is the primary error");
        check(suppression.suppressed, firstError, "earlier rejection is suppressed");

        /* use creates its hidden wrapper in owner, despite the other receiver. */
        const fallback = new other.AsyncDisposableStack();
        const sentinel = {};
        let calls = 0, fallbackThenReads = 0;
        const resource = {
            [Symbol.dispose]() {
                check(this, resource, "fallback preserves its resource receiver");
                calls++;
            },
        };
        prototype.use.call(fallback, resource);
        runGC();
        const descriptor = Object.getOwnPropertyDescriptor(owner.Promise.prototype,
                                                            "then");
        let fallbackCompletion;
        try {
            Object.defineProperty(owner.Promise.prototype, "then", {
                configurable: true,
                get() { fallbackThenReads++; throw sentinel; },
            });
            fallbackCompletion = otherPrototype.disposeAsync.call(fallback);
        } finally {
            Object.defineProperty(owner.Promise.prototype, "then", descriptor);
        }
        check(Object.getPrototypeOf(fallbackCompletion), other.Promise.prototype,
              "fallback outer completion belongs to the disposeAsync realm");
        let fallbackError;
        try {
            await fallbackCompletion;
            throw Error("foreign Await must assimilate the hidden fallback Promise");
        } catch (error) {
            fallbackError = error;
        }
        check(fallbackError, sentinel, "fallback assimilation preserves thrown identity");
        check(fallbackThenReads, 1, "fallback Promise retains its creation realm");
        check(calls, 1, "fallback invokes the synchronous disposer once");
    }

    /* Pending owners with no user root must also remain collectible. */
    let abandoned = new foreign.AsyncDisposableStack();
    abandoned.defer(() => new foreign.Promise(() => {}));
    abandoned.disposeAsync();
    abandoned = null;
    runGC();
    return true;
})()
