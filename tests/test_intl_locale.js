/* Intl locale-list coercion, descriptors and realm regressions.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
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

function check(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("expected " + expected + ", got " + actual);
}

function list(input, expected)
{
    check(JSON.stringify(Intl.getCanonicalLocales(input)), JSON.stringify(expected));
}

function throws(callback, expected)
{
    try {
        callback();
    } catch (error) {
        if (typeof expected === "function")
            check(Object.getPrototypeOf(error), expected.prototype);
        else
            check(error, expected);
        return;
    }
    throw Error("expected an exception");
}

const descriptor = Object.getOwnPropertyDescriptor(globalThis, "Intl");
check(descriptor.writable && descriptor.configurable, true);
check(descriptor.enumerable, false);
check(Object.getPrototypeOf(Intl), Object.prototype);
check(Object.prototype.toString.call(Intl), "[object Intl]");
const method = Object.getOwnPropertyDescriptor(Intl, "getCanonicalLocales");
check(method.writable && method.configurable, true);
check(method.enumerable, false);
check(method.value.name, "getCanonicalLocales");
check(method.value.length, 1);
throws(() => new Intl.getCanonicalLocales(), TypeError);
list(undefined, []);
list(123, []);
list(true, []);
list("iw-Qaai-BU", ["he-Zinh-MM"]);
list(["SH", "sr-Latn", "FR", "fr", "en-u-kn-true"],
     ["sr-Latn", "fr", "en-u-kn"]);
list([, "de", , "en"], ["de", "en"]);
list({ length: 2, 1: "zh-hant-tw" }, ["zh-Hant-TW"]);
for (const value of [null, [undefined], [null], [false], [3], [3n], [Symbol()]])
    throws(() => Intl.getCanonicalLocales(value), TypeError);
for (const value of ["", "en-US\0junk", "en_US", "en-u", "en-a-foo-A-bar"])
    throws(() => Intl.getCanonicalLocales(value), RangeError);

const events = [];
const locales = new Proxy({ length: 3, 0: "en", 2: "fr" }, {
    get(target, key) {
        events.push("get " + key);
        return Reflect.get(target, key);
    },
    has(target, key) {
        events.push("has " + key);
        return Reflect.has(target, key);
    }
});
list(locales, ["en", "fr"]);
check(events.join(","), "get length,has 0,get 0,has 1,has 2,get 2");

const marker = {};
throws(() => Intl.getCanonicalLocales({ get length() { throw marker; } }), marker);
throws(() => Intl.getCanonicalLocales(new Proxy({ length: 1 }, {
    has() { throw marker; }
})), marker);
throws(() => Intl.getCanonicalLocales([{
    toString() { throw marker; }
}]), marker);
const changing = {
    length: 2,
    0: { toString() { changing.length = 0; return "en"; } },
    1: "fr"
};
list(changing, ["en", "fr"]);
const inherited = Object.create({ 0: "de" });
inherited.length = 1;
list(inherited, ["de"]);

const realm = std.createRealm();
const foreign = realm.global.Intl.getCanonicalLocales;
const result = foreign(["IW", "he"]);
check(Object.getPrototypeOf(result), realm.global.Array.prototype);
check(result.length, 1);
check(result[0], "he");
check(foreign === Intl.getCanonicalLocales, false);
const savedIntl = Intl;
globalThis.Intl = null;
std.gc();
check(foreign("SH")[0], "sr-Latn");
globalThis.Intl = savedIntl;
