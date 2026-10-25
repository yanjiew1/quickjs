/* Standard-library realm regression tests.
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
import { run_in_realms } from "./realm-harness.js";

function check(actual, expected, message)
{
    if (!Object.is(actual, expected))
        throw Error(message || "realm assertion failed");
}

async function test_create_realm()
{
    check(std.createRealm.name, "createRealm");
    check(std.createRealm.length, 0);
    let first = std.createRealm(), second = std.createRealm();
    check(first.global.globalThis, first.global);
    check(first.evalScript.name, "evalScript");
    check(first.evalScript.length, 1);
    check(first.createRealm.length, 0);
    check(Object.getPrototypeOf(first), first.global.Object.prototype);
    check(Object.getPrototypeOf(first.evalScript), first.global.Function.prototype);
    for (const name of ["Object", "Array", "Function", "TypeError", "Promise"])
        check(first.global[name] === second.global[name], false, name + " is fresh");
    check(first.global.Object === Object, false);
    for (const name of ["std", "os", "print"])
        check(Object.hasOwn(first.global, name), false, "caller globals are not copied");
    for (const name of ["global", "evalScript", "createRealm"]) {
        const descriptor = Object.getOwnPropertyDescriptor(first, name);
        check(descriptor.writable && descriptor.enumerable && descriptor.configurable, true);
    }

    globalThis.__realm_parent_marker = 41;
    try {
        check(first.evalScript("typeof __realm_parent_marker"), "undefined");
        check(first.evalScript("var realmValue = 7; let lexicalValue = 8; realmValue + lexicalValue"), 15);
        check(first.evalScript("realmValue + lexicalValue"), 15);
        check(first.global.realmValue, 7);
        check(Object.hasOwn(first.global, "lexicalValue"), false);
        check(second.evalScript("typeof realmValue"), "undefined");
        check(Object.hasOwn(globalThis, "realmValue"), false);
        check(first.evalScript(), undefined);
        const evaluate = first.evalScript;
        check(evaluate.call(second, "globalThis"), first.global,
              "detached evaluator keeps its creation realm");
        check(first.evalScript({ toString() { return "6 * 7"; } }), 42);
        const sentinel = {};
        first.global.sentinel = sentinel;
        function thrown(callback) {
            try { callback(); } catch (error) { return error; }
            throw Error("expected an exception from realm evaluation");
        }
        check(thrown(() => evaluate("throw sentinel")), sentinel,
              "thrown values retain identity");
        const typeError = thrown(() => evaluate("throw new TypeError('foreign')"));
        check(Object.getPrototypeOf(typeError), first.global.TypeError.prototype);
        check(typeError instanceof TypeError, false);
        check(Object.getPrototypeOf(thrown(() => evaluate("let ="))),
              first.global.SyntaxError.prototype);
        check(Object.getPrototypeOf(thrown(() => evaluate(Symbol()))),
              first.global.TypeError.prototype);
        check(thrown(() => evaluate({ toString() { throw sentinel; } })), sentinel,
              "source coercion preserves caller exceptions");
        check(thrown(() => evaluate("1", { get async() { throw sentinel; } })), sentinel,
              "option access preserves caller exceptions");
        check(evaluate("21 + 21"), 42, "evaluation still works after exceptions");

        first.global.reenter = () => evaluate("realmValue + 1");
        check(evaluate("reenter() + realmValue"), 15);
        first.global.reenter = null;
        const pending = evaluate("await Promise.resolve(42)", { async: true });
        check(Object.getPrototypeOf(pending), first.global.Promise.prototype);
        check((await pending).value, 42);
        const nested = first.createRealm();
        check(nested.global.Object === first.global.Object, false);
        check(nested.evalScript("typeof realmValue"), "undefined");
    } finally {
        delete globalThis.__realm_parent_marker;
    }
    first = second = null;
    std.gc();

    /* Only an escaped global, method, or result keeps these contexts alive. */
    let escapedGlobal = std.createRealm().global;
    const foreignObject = escapedGlobal.Object;
    std.gc();
    check(escapedGlobal.Function("return globalThis")(), escapedGlobal);
    check(Object.getPrototypeOf(new foreignObject()), escapedGlobal.Object.prototype);
    escapedGlobal = null;
    let escapedEval = std.createRealm().evalScript;
    std.gc();
    check(escapedEval("var retained = 23; retained"), 23);
    std.gc();
    check(escapedEval("retained"), 23);
    escapedEval = null;
    let escapedResult = std.createRealm().evalScript("() => Object.prototype");
    std.gc();
    check(escapedResult().constructor === Object, false);
    check(Object.getPrototypeOf(escapedResult()), null);
    escapedResult = null;
    for (let index = 0; index < 8; index++) {
        let realm = std.createRealm();
        realm.global.self = realm;
        realm.global.evaluate = realm.evalScript;
        realm = null;
        std.gc();
    }
}

function test_bigint_locale_realm()
{
    const result = run_in_realms(function () {
        const realms = [globalThis, foreign];
        function check(condition, message) {
            if (!condition) throw Error(message);
        }
        for (let index = 0; index < realms.length; index++) {
            const owner = realms[index], receiverRealm = realms[1 - index];
            const method = owner.BigInt.prototype.toLocaleString;
            const boxed = receiverRealm.Object(-42n);
            boxed.toString = () => { throw Error("dynamic toString"); };
            check(method.call(boxed) === "-42", "foreign boxed BigInt works");
            for (const invalid of [{}, new receiverRealm.Proxy(boxed, {})]) {
                let caught = false;
                try { method.call(invalid); } catch (error) {
                    caught = true;
                    check(Object.getPrototypeOf(error) === owner.TypeError.prototype,
                          "BigInt receiver TypeError uses the native function realm");
                }
                check(caught, "invalid foreign receiver throws");
            }
        }
        return true;
    });
    check(result, true, "bigint_locale_realm");
    std.gc();
}

export async function test_realms()
{
    await test_create_realm();
    test_bigint_locale_realm();
}

await test_realms();
