/* Cross-realm behavioral regressions migrated from test_api.c.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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

export function test_iterator_realms()
{
    test_native_iterator_next_realm();
    test_iterator_constructor_realm();
}

test_iterator_realms();
