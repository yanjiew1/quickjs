/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

import { assert } from "./assert.js";
import * as namespace from "./fixture_module_review_cycle_keys.js";
import { snapshot } from "./fixture_module_review_cycle_probe.js";

function test_namespace_cycle_and_proxy_keys()
{
    const expected = ["01", "10", "2", Symbol.toStringTag];
    function checkKeys(actual, wanted) {
        assert(actual.length, wanted.length);
        for (let i = 0; i < wanted.length; i++)
            assert(actual[i], wanted[i], "key " + i);
    }
    checkKeys(snapshot.ownKeys, expected);
    checkKeys(snapshot.ownNames, expected.slice(0, 3));
    checkKeys(snapshot.proxyOwnKeys, expected);
    assert(snapshot.keysError instanceof ReferenceError, true);
    assert(snapshot.entriesError instanceof ReferenceError, true);
    assert(snapshot.forInError instanceof ReferenceError, true);
    checkKeys(Reflect.ownKeys(namespace), expected);
    checkKeys(Object.keys(namespace), expected.slice(0, 3));

    const reversed = expected.slice().reverse();
    const proxy = new Proxy(namespace, { ownKeys() { return reversed.slice(); } });
    checkKeys(Reflect.ownKeys(proxy), reversed);
    checkKeys(Object.keys(proxy), ["2", "10", "01"]);

    const addedSymbol = Symbol("not an export");
    assert(Reflect.defineProperty(namespace, addedSymbol, { value: 1 }), false);
    assert(Reflect.deleteProperty(namespace, addedSymbol), true);
    assert(Reflect.defineProperty(namespace, Symbol.toStringTag,
                                  { value: "Module" }), true);
    assert(Reflect.defineProperty(namespace, Symbol.toStringTag,
                                  { value: "Other" }), false);
    checkKeys(Object.getOwnPropertySymbols(namespace), [Symbol.toStringTag]);
    checkKeys(Reflect.ownKeys(namespace), expected);

    const first = Symbol("first"), second = Symbol("second");
    const ordinary = { "2": 2, "10": 10, "01": 1, [first]: 1, [second]: 2 };
    checkKeys(Reflect.ownKeys(ordinary), ["2", "10", "01", first, second]);
    const ordinaryReversed = Reflect.ownKeys(ordinary).reverse();
    const ordinaryProxy = new Proxy(ordinary, {
        ownKeys() { return ordinaryReversed.slice(); }
    });
    checkKeys(Reflect.ownKeys(ordinaryProxy), ordinaryReversed);
    checkKeys(Object.keys(ordinaryProxy), ["01", "10", "2"]);
}

test_namespace_cycle_and_proxy_keys();
