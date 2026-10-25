/* Cross-realm behavioral regressions migrated from test_api.c.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
import * as std from "std";
import { check, run_in_realms } from "./realm-harness.js";

function test_native_iterator_next_realm()
{
    const result = run_in_realms(function () {
        const realms = [globalThis, foreign];
        function check(actual, expected, message) {
            if (actual !== expected) throw Error(message);
        }
        function wrongReceiver(callback, realm) {
            let caught = false;
            try { callback(); } catch (error) {
                caught = true;
                check(Object.getPrototypeOf(error), realm.TypeError.prototype,
                      "native next receiver validation uses its function realm");
            }
            check(caught, true, "invalid native next receiver must throw");
        }
        for (let i = 0; i < 2; i++) {
            const owner = realms[i], consumer = realms[1 - i];
            const methods = [
                owner.Array.prototype.values.call([]).next,
                new owner.Map().entries().next,
                new owner.Set().values().next,
                owner.String.prototype[Symbol.iterator].call("").next,
            ];
            const consumers = [
                iterable => consumer.Array.from(iterable),
                iterable => new consumer.Set(iterable),
                iterable => consumer.Iterator.prototype.toArray.call(iterable),
                iterable => { for (const value of iterable) break; },
            ];
            for (const next of methods) {
                for (const consume of consumers) {
                    const input = { next, [Symbol.iterator]() { return this; } };
                    wrongReceiver(() => consume(input), owner);
                }
            }

            // Entries create their row arrays inside the cached native method.
            // An iterable consumer must preserve that function's array realm.
            const inputs = [
                owner.Array.prototype.entries.call([11]),
                new owner.Map([[12, 13]]).entries(),
                new owner.Set([14]).entries(),
            ];
            for (const input of inputs) {
                const nativeNext = input.next;
                let reads = 0;
                Object.defineProperty(input, "next", { get() {
                    reads++;
                    return nativeNext;
                } });
                const rows = consumer.Array.from(input);
                check(reads, 1, "native next is cached once");
                check(rows.length, 1, "one entry is yielded");
                check(Object.getPrototypeOf(rows), consumer.Array.prototype,
                      "Array.from result belongs to the consuming function");
                check(Object.getPrototypeOf(rows[0]), owner.Array.prototype,
                      "entry row belongs to the native next function");
                check(rows[0].length, 2, "entry row has key and value");
            }
        }
        return true;
    });
    check(result, true, "native_iterator_next_realm");
    std.gc();
}

function test_iterator_constructor_realm()
{
    let one = std.createRealm(), two = std.createRealm();
    one.global.otherIterator = two.global.Iterator;
    two.global.otherIterator = one.global.Iterator;
    for (const realm of [one, two]) {
        check(realm.evalScript(`(() => {
            const result = Reflect.construct(Iterator, [], otherIterator);
            if (Object.getPrototypeOf(result) !== otherIterator.prototype)
                throw Error("foreign Iterator prototype");
            if (result instanceof Iterator)
                throw Error("wrong Iterator realm");
            return true;
        })()`), true);
    }
    one = two = null;
    std.gc();
}

function test_iterator_helper_creation_realm()
{
    const result = run_in_realms(function () {
        const realms = [globalThis, foreign];
        function check(actual, expected, message) {
            if (actual !== expected) throw Error(message);
        }
        function resultRealm(result, realm, message) {
            check(Object.getPrototypeOf(result), realm.Object.prototype, message);
        }
        function thrownRealm(callback, realm, message) {
            let caught = false;
            try { callback(); } catch (error) {
                caught = true;
                check(Object.getPrototypeOf(error), realm.TypeError.prototype, message);
            }
            check(caught, true, message + ": expected an exception");
        }
        function finiteSource(value) {
            let used = false;
            return { next() {
                if (used) return { done: true };
                used = true;
                return { value };
            } };
        }
        for (let i = 0; i < 2; i++) {
            const creator = realms[i], caller = realms[1 - i];
            const borrowed = Object.getPrototypeOf(caller.Iterator.from([]).map(x => x));
            const next = borrowed.next, close = borrowed.return;
            const makers = [
                input => creator.Iterator.prototype.map.call(input, x => x),
                input => creator.Iterator.prototype.filter.call(input, x => true),
                input => creator.Iterator.prototype.flatMap.call(input, x => [x]),
                input => creator.Iterator.prototype.drop.call(input, 0),
                input => creator.Iterator.prototype.take.call(input, 1),
            ];
            for (const make of makers) {
                let helper = make(finiteSource(1));
                Object.setPrototypeOf(helper, null);
                let result = next.call(helper);
                resultRealm(result, creator, "yielded result uses the saved creation realm");
                check(result.value, 1, "borrowed next yields the input value");
                check(result.done, false, "first resume yields");
                result = next.call(helper);
                resultRealm(result, creator, "first completion uses the creation realm");
                check(result.done, true, "second resume completes");
                resultRealm(next.call(helper), caller,
                            "already-completed next uses the invoked method realm");
                resultRealm(close.call(helper), caller,
                            "already-completed return uses the invoked method realm");

                helper = make(finiteSource(2));
                resultRealm(close.call(helper), caller,
                            "suspended-start return uses the invoked method realm");
                resultRealm(next.call(helper), caller,
                            "next after suspended-start return stays in the method realm");

                helper = make(finiteSource(3));
                next.call(helper);
                resultRealm(close.call(helper), creator,
                            "suspended-yield return resumes the saved creation realm");
                resultRealm(close.call(helper), caller,
                            "return after completion uses the method realm");

                helper = make({ next: null });
                thrownRealm(() => next.call(helper), creator,
                            "uncallable cached next is validated in the generator realm");
                resultRealm(next.call(helper), caller,
                            "next after failure uses the method realm");

                const badClose = () => ({ next() { return { value: 4 }; },
                                         return() { return 0; } });
                helper = make(badClose());
                thrownRealm(() => close.call(helper), caller,
                            "suspended-start close validation uses the method realm");
                helper = make(badClose());
                next.call(helper);
                thrownRealm(() => close.call(helper), creator,
                            "suspended-yield close validation uses the generator realm");

                const source = { next() {
                    thrownRealm(() => next.call(helper), caller,
                                "running next validation uses the method realm");
                    thrownRealm(() => close.call(helper), caller,
                                "running return validation uses the method realm");
                    return { value: 5 };
                }, return() {
                    thrownRealm(() => next.call(helper), caller,
                                "active close keeps generator execution guarded");
                    return {};
                } };
                helper = make(source);
                resultRealm(next.call(helper), creator,
                            "reentry does not change the outer generator realm");
                close.call(helper);

                const startClose = { next() { throw 42; }, return() {
                    resultRealm(next.call(helper), caller,
                                "suspended-start return completes before callbacks");
                    resultRealm(close.call(helper), caller,
                                "completed return reentry uses the method realm");
                    return {};
                } };
                helper = make(startClose);
                close.call(helper);
            }
            const source = { next() { return { value: 6 }; } };
            const flat = creator.Iterator.prototype.flatMap.call(source, () => 0);
            thrownRealm(() => next.call(flat), creator,
                        "flatMap primitive-result error is created by the saved realm");
            thrownRealm(() => next.call({}), caller,
                        "invalid helper receiver is validated in the method realm");
            thrownRealm(() => close.call({}), caller,
                        "invalid return receiver is validated in the method realm");

            // The JS harness releases both realm roots and runs GC after this
            // script. This unreachable global/helper/context cycle requires
            // the retained context to be marked and freed by the helper owner.
            const cycle = creator.Iterator.prototype.map.call(finiteSource(17), x => x);
            Object.setPrototypeOf(cycle, null);
            cycle.contextGlobal = creator;
            creator.__helperRealmCycle = cycle;
        }
        return true;
    });
    check(result, true, "iterator_helper_creation_realm");
    std.gc();
}

function test_iterator_concat_creation_realm()
{
    const result = run_in_realms(function () {
        const realms = [globalThis, foreign];
        function check(actual, expected, message) {
            if (actual !== expected) throw Error(message);
        }
        function resultRealm(result, realm, message) {
            check(Object.getPrototypeOf(result), realm.Object.prototype, message);
        }
        function thrownRealm(callback, realm, message) {
            let caught = false;
            try { callback(); } catch (error) {
                caught = true;
                check(Object.getPrototypeOf(error), realm.TypeError.prototype, message);
            }
            check(caught, true, message + ": expected an exception");
        }
        for (let i = 0; i < 2; i++) {
            const creator = realms[i], caller = realms[1 - i];
            const borrowed = Object.getPrototypeOf(caller.Iterator.from([]).map(x => x));
            const next = borrowed.next, close = borrowed.return;
            let helper = creator.Iterator.concat([1]);
            Object.setPrototypeOf(helper, null);
            let result = next.call(helper);
            resultRealm(result, creator, "concat yield uses the saved creation realm");
            check(result.value, 1, "concat yields its input");
            result = next.call(helper);
            resultRealm(result, creator, "concat first completion uses the creation realm");
            check(result.done, true, "concat completes");
            resultRealm(next.call(helper), caller,
                        "concat completed next uses the invoked method realm");
            resultRealm(close.call(helper), caller,
                        "concat completed return uses the invoked method realm");

            helper = creator.Iterator.concat();
            resultRealm(next.call(helper), creator,
                        "empty concat first resume still uses its creation realm");
            resultRealm(next.call(helper), caller,
                        "empty concat next after completion uses the method realm");
            helper = creator.Iterator.concat();
            resultRealm(close.call(helper), caller,
                        "empty concat return before starting uses the method realm");

            let opens = 0, closes = 0;
            const input = { [Symbol.iterator]() {
                opens++;
                return { next() { return { value: 2 }; }, return() {
                    closes++;
                    return {};
                } };
            } };
            helper = creator.Iterator.concat(input);
            resultRealm(close.call(helper), caller,
                        "concat suspended-start return uses the method realm");
            check(opens, 0, "return before starting does not open an input");
            check(closes, 0, "return before starting does not close an unopened input");
            helper = creator.Iterator.concat(input);
            next.call(helper);
            resultRealm(close.call(helper), creator,
                        "concat suspended-yield return uses the generator realm");
            check(opens, 1, "concat opens once");
            check(closes, 1, "active return closes once");
            resultRealm(close.call(helper), caller,
                        "concat return after completion uses the method realm");

            for (const input of [
                { [Symbol.iterator]() { return 0; } },
                { [Symbol.iterator]() { return { next: null }; } },
            ]) {
                helper = creator.Iterator.concat(input);
                thrownRealm(() => next.call(helper), creator,
                            "concat protocol validation uses the saved creation realm");
                resultRealm(next.call(helper), caller,
                            "concat next after protocol failure uses the method realm");
            }
            helper = creator.Iterator.concat({ [Symbol.iterator]() {
                return { next() { return { value: 3 }; }, return() { return 0; } };
            } });
            next.call(helper);
            thrownRealm(() => close.call(helper), creator,
                        "active concat close validation uses the generator realm");

            helper = creator.Iterator.concat({ [Symbol.iterator]() {
                return { next() {
                    thrownRealm(() => next.call(helper), caller,
                                "concat running next validation uses the method realm");
                    thrownRealm(() => close.call(helper), caller,
                                "concat running return validation uses the method realm");
                    return { value: 4 };
                }, return() {
                    thrownRealm(() => next.call(helper), caller,
                                "concat active close keeps execution guarded");
                    return {};
                } };
            } });
            resultRealm(next.call(helper), creator,
                        "concat outer resume keeps its realm after reentry");
            close.call(helper);

            // Both context roots are released by the bootstrap before its final
            // GC. Keep a deliberate global/concat/context cycle until then.
            const cycle = creator.Iterator.concat([18]);
            Object.setPrototypeOf(cycle, null);
            cycle.contextGlobal = creator;
            creator.__concatRealmCycle = cycle;
        }
        return true;
    });
    check(result, true, "iterator_concat_creation_realm");
    std.gc();
}

function test_iterator_buffer_creation_realm()
{
    const result = run_in_realms(function () {
        const realms = [globalThis, foreign];
        function check(actual, expected, message) {
            if (actual !== expected) throw Error(message);
        }
        for (let index = 0; index < realms.length; index++) {
            const creator = realms[index], caller = realms[1 - index];
            const borrowed = Object.getPrototypeOf(caller.Iterator.from([]).map(x => x));
            const next = borrowed.next, close = borrowed.return;
            const makers = [
                source => creator.Iterator.prototype.chunks.call(source, 2),
                source => creator.Iterator.prototype.windows.call(source, 2),
            ];
            for (const make of makers) {
                let advances = 0, closes = 0;
                function source(length) {
                    let position = 0;
                    return {
                        next() {
                            advances++;
                            return position < length ? { value: ++position } : { done: true };
                        },
                        return() { closes++; return {}; },
                    };
                }
                let helper = make(source(3));
                Object.setPrototypeOf(helper, null);
                let result = next.call(helper);
                check(Object.getPrototypeOf(result), creator.Object.prototype,
                      "buffer result uses creator realm");
                check(Object.getPrototypeOf(result.value), creator.Array.prototype,
                      "buffer row uses creator realm");
                check(result.value.join(), "1,2", "buffer has first two values");
                const second = next.call(helper);
                check(Object.getPrototypeOf(second), creator.Object.prototype,
                      "resumed buffer result uses creator realm");
                check(Object.getPrototypeOf(second.value), creator.Array.prototype,
                      "resumed buffer row uses creator realm");
                check(next.call(helper).done, true, "buffer completes");
                check(Object.getPrototypeOf(next.call(helper)), caller.Object.prototype,
                      "completed buffer next uses method realm");
                check(Object.getPrototypeOf(close.call(helper)), caller.Object.prototype,
                      "completed buffer return uses method realm");
                check(closes, 0, "natural exhaustion does not close");

                helper = make(source(4));
                const before = advances;
                check(Object.getPrototypeOf(close.call(helper)), caller.Object.prototype,
                      "buffer return before start uses method realm");
                check(advances, before, "early return does not advance source");
                check(closes, 1, "early return closes source once");

                helper = make(source(4));
                next.call(helper);
                check(Object.getPrototypeOf(close.call(helper)), creator.Object.prototype,
                      "buffer active return uses creator realm");
                check(closes, 2, "active return closes source once");
                check(Object.getPrototypeOf(close.call(helper)), caller.Object.prototype,
                      "buffer completed return uses method realm");

                helper = make({ next: null });
                let caught = false;
                try { next.call(helper); } catch (error) {
                    caught = true;
                    check(Object.getPrototypeOf(error), creator.TypeError.prototype,
                          "buffer protocol TypeError uses creator realm");
                }
                check(caught, true, "bad cached next throws");
                check(Object.getPrototypeOf(next.call(helper)), caller.Object.prototype,
                      "buffer completion after failure uses method realm");

                helper = make(source(3));
                helper.contextGlobal = creator;
                creator.__bufferRealmCycle = helper;
                next.call(helper);
            }
        }
        return true;
    });
    check(result, true, "iterator_buffer_creation_realm");
    std.gc();
}

function test_iterator_zip_realm()
{
    const result = run_in_realms(function () {
        const other = typeof foreign !== "undefined" ? foreign : $262.createRealm().global;
        const realms = [globalThis, other];

        function check(value, expected, message) {
            if (value !== expected) throw Error(message);
        }

        function resultRealm(result, realm, message) {
            check(Object.getPrototypeOf(result), realm.Object.prototype, message);
        }

        function thrownRealm(callback, realm, message) {
            let caught = false;
            try { callback(); } catch (error) {
                caught = true;
                check(Object.getPrototypeOf(error), realm.TypeError.prototype, message);
            }
            check(caught, true, message + ": expected an exception");
        }

        for (let i = 0; i < 2; i++) {
            const creator = realms[i], caller = realms[1 - i];
            const borrowed = Object.getPrototypeOf(caller.Iterator.zip([]));
            const next = borrowed.next, close = borrowed.return;
            const makers = [
                (sources, options) => creator.Iterator.zip(sources, options),
            ];
            if (typeof creator.Iterator.zipKeyed === "function")
                makers.push((sources, options) => creator.Iterator.zipKeyed(
                    { first: sources[0], second: sources[1] }, options));

            for (let kind = 0; kind < makers.length; kind++) {
                const make = makers[kind];
                let helper = make([[1], [2]]);
                let result = next.call(helper);
                resultRealm(result, creator, "yielded result uses the generator realm");
                check(result.done, false, "first result yields");
                if (kind === 0)
                    check(Object.getPrototypeOf(result.value), creator.Array.prototype,
                          "zip row uses the generator realm");
                else
                    check(Object.getPrototypeOf(result.value), null,
                          "zipKeyed row has a null prototype");
                result = next.call(helper);
                resultRealm(result, creator, "first completion uses the generator realm");
                check(result.done, true, "second result completes");
                resultRealm(next.call(helper), caller,
                            "already-completed next uses the invoked method realm");
                resultRealm(close.call(helper), caller,
                            "already-completed return uses the invoked method realm");

                helper = make([[3], [4]]);
                resultRealm(close.call(helper), caller,
                            "suspended-start return uses the invoked method realm");
                resultRealm(next.call(helper), caller,
                            "next after suspended-start return uses the method realm");

                helper = make([[5], [6]]);
                next.call(helper);
                resultRealm(close.call(helper), creator,
                            "suspended-yield return resumes the generator realm");
                resultRealm(close.call(helper), caller,
                            "return after generator completion uses the method realm");

                helper = make([[], [7]], { mode: "strict" });
                thrownRealm(() => next.call(helper), creator,
                            "strict mismatch is thrown by the resumed generator");
                resultRealm(next.call(helper), caller,
                            "next after strict failure uses the method realm");

                helper = make([{ next: null }, [8]]);
                thrownRealm(() => next.call(helper), creator,
                            "uncallable cached next fails in the generator realm");

                const badClose = () => ({ next() { return { value: 9 }; },
                                         return() { return 0; } });
                helper = make([badClose(), [10]]);
                thrownRealm(() => close.call(helper), caller,
                            "suspended-start close validation uses the method realm");
                helper = make([badClose(), [11]]);
                next.call(helper);
                thrownRealm(() => close.call(helper), creator,
                            "suspended-yield close validation uses the generator realm");

                const source = { next() {
                    thrownRealm(() => next.call(helper), caller,
                                "running generator validation uses the method realm");
                    thrownRealm(() => close.call(helper), caller,
                                "running generator return validation uses the method realm");
                    return { value: 12 };
                } };
                helper = make([source, [13]]);
                resultRealm(next.call(helper), creator,
                            "outer resume keeps its generator realm after reentry");
                close.call(helper);
            }
            thrownRealm(() => next.call({}), caller,
                        "invalid receiver validation uses the invoked method realm");
            thrownRealm(() => close.call({}), caller,
                        "invalid return receiver uses the invoked method realm");
        }
        return true;
    });
    check(result, true, "iterator_zip_realm");
    std.gc();
}

export function test_iterator_realms()
{
    test_native_iterator_next_realm();
    test_iterator_helper_creation_realm();
    test_iterator_concat_creation_realm();
    test_iterator_buffer_creation_realm();
    test_iterator_zip_realm();
    test_iterator_constructor_realm();
}

test_iterator_realms();
