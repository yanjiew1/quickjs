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

/* Intl.ListFormat construction, negotiation, branding and realm contracts. */
import * as std from "std";
import { assert, assertThrows } from "./assert.js";

const List = Intl.ListFormat;
assert(List.name, "ListFormat");
assert(List.length, 0);
assertThrows(TypeError, () => List());
assertThrows(RangeError, () => new List("not_a_locale"));
for (const value of [null, false, 1, 1n, "", Symbol()])
    assertThrows(TypeError, () => new List("en", value));
for (const localeMatcher of ["bad", "lookup\0", null, true])
    assertThrows(RangeError, () => new List("en", { localeMatcher }));
for (const type of ["and", "unit\0", null, false])
    assertThrows(RangeError, () => new List("en", { type }));
for (const style of ["wide", "short\0", null, 0])
    assertThrows(RangeError, () => new List("en", { style }));
for (const key of ["localeMatcher", "type", "style"])
    assertThrows(TypeError, () => new List("en", { [key]: Symbol() }));
for (const type of ["conjunction", "disjunction", "unit"]) {
    for (const style of ["long", "short", "narrow"]) {
        const resolved = new List("en", { type, style }).resolvedOptions();
        assert(JSON.stringify(resolved), JSON.stringify({ locale: "en", type, style }));
    }
}
assert(new List("en-u-nu-arab-x-private").resolvedOptions().locale, "en");
assert(new List("en-US").resolvedOptions().locale, "en-US");
const defaultListLocale = new List().resolvedOptions().locale;
for (const localeMatcher of ["lookup", "best fit"]) {
    for (const [locales, expected] of [
        [undefined, defaultListLocale], ["EN", "en"], [[], defaultListLocale],
        [["en", "EN"], "en"], [["EN", "en"], "en"],
        [{ 0: "EN", length: 0 }, defaultListLocale], [{ 0: "EN", length: 1 }, "en"]
    ]) {
        assert(new List(locales, { localeMatcher }).resolvedOptions().locale, expected);
    }
    for (const locales of [["en-GB-oed"], ["x-private"]])
        assertThrows(RangeError, () => new List(locales, { localeMatcher }));
}
assert(new List("zz").resolvedOptions().locale, new List().resolvedOptions().locale);
assert(Object.prototype.toString.call(new List("en")), "[object Intl.ListFormat]");
const branded = new Intl.Locale("en-u-kn");
branded.toString = () => { throw Error("Locale internal identity expected"); };
assert(new List(branded).resolvedOptions().locale, "en");

assert(JSON.stringify(List.supportedLocalesOf(["EN-u-kn", "en-u-kn", "zz", "en-x-a"])),
       '["en-u-kn","en-x-a"]');
assert(JSON.stringify(List.supportedLocalesOf("zz")), "[]");
for (const value of [false, 1, 1n, "", Symbol()])
    assert(List.supportedLocalesOf("en", value).join(","), "en");
assertThrows(TypeError, () => List.supportedLocalesOf("en", null));
assertThrows(RangeError, () => List.supportedLocalesOf("en", { localeMatcher: "bad" }));

let events = [];
const target = new Proxy(function () {}, {
    get(object, key) {
        if (key === "prototype") events.push("prototype");
        return Reflect.get(object, key);
    }
});
const input = [{ toString() { events.push("locale"); return "en"; } }];
const options = new Proxy({}, {
    get(object, key) { events.push(key); return undefined; }
});
Reflect.construct(List, [input, options], target);
assert(events.join(","), "prototype,locale,localeMatcher,type,style");
events = [];
assertThrows(RangeError, () => new List("bad_tag", options));
assert(events.length, 0);
assertThrows(RangeError, () => new List("en", {
    get localeMatcher() { events.push("matcher"); return "invalid"; },
    get type() { events.push("type"); return "unit"; }
}));
assert(events.join(","), "matcher");
events = [];
assertThrows(RangeError, () => new List("en", {
    get type() { events.push("type"); return "invalid"; },
    get style() { events.push("style"); return "short"; }
}));
assert(events.join(","), "type");
const abrupt = {};
let caught;
events = [];
try {
    Reflect.construct(List, [input, options], new Proxy(function () {}, {
        get(object, key) { if (key === "prototype") throw abrupt; return Reflect.get(object, key); }
    }));
} catch (error) {
    caught = error;
}
assert(caught, abrupt);
assert(events.length, 0);
caught = undefined;
try {
    new List("en", { type: { toString() { throw abrupt; } } });
} catch (error) {
    caught = error;
}
assert(caught, abrupt);

const instance = new List("en");
for (const key of ["locale", "type", "style"])
    Object.defineProperty(Object.prototype, key, { configurable: true, set() { throw Error("inherited setter"); } });
try {
    assert(JSON.stringify(instance.resolvedOptions()), '{"locale":"en","type":"conjunction","style":"long"}');
} finally {
    for (const key of ["locale", "type", "style"])
        delete Object.prototype[key];
}
assertThrows(TypeError, () => List.prototype.resolvedOptions());
assertThrows(TypeError, () => List.prototype.resolvedOptions.call(new Proxy(instance, {})));
const resolved = instance.resolvedOptions();
assert(Object.keys(resolved).join(","), "locale,type,style");
assert(resolved !== instance.resolvedOptions());
for (const key of Object.keys(resolved)) {
    const desc = Object.getOwnPropertyDescriptor(resolved, key);
    assert(desc.writable && desc.enumerable && desc.configurable);
}
const realm = std.createRealm();
const foreign = realm.evalScript("Intl.ListFormat");
const foreignObject = realm.evalScript("Object.prototype");
assert(Object.getPrototypeOf(foreign.prototype.resolvedOptions.call(instance)), foreignObject);
const fallback = realm.evalScript("(function Target() {})");
fallback.prototype = null;
assert(Object.getPrototypeOf(Reflect.construct(List, ["en"], fallback)), foreign.prototype);
std.gc();
assert(instance.resolvedOptions().locale, "en");

// Formatting consumes string iterables without coercing their yielded values.
const formatter = new List("en");
assert(formatter.format(), "");
assert(formatter.format([]), "");
assert(formatter.format([""]), "");
assert(formatter.format(["one"]), "one");
assert(formatter.format(["one", "two"]), "one and two");
assert(formatter.format(["one", "two", "three"]), "one, two, and three");
assert(formatter.format("abc"), "a, b, and c");
assert(formatter.format("\ud83e\udd8aA"), "\ud83e\udd8a and A");
assert(JSON.stringify(formatter.formatToParts()), "[]");
assert(JSON.stringify(formatter.formatToParts([""])),
       '[{"type":"element","value":""}]');
for (const name of ["format", "formatToParts"]) {
    const descriptor = Object.getOwnPropertyDescriptor(List.prototype, name);
    assert(descriptor.writable && descriptor.configurable && !descriptor.enumerable);
    assert(descriptor.value.name, name);
    assert(descriptor.value.length, 1);
    assertThrows(TypeError, () => new descriptor.value());
    for (const value of [null, false, 0, 1n, Symbol(), {}])
        assertThrows(TypeError, () => formatter[name](value));
    for (const value of [null, undefined, List.prototype, {}, new Proxy(formatter, {})]) {
        assertThrows(TypeError, () => descriptor.value.call(value, {
            get [Symbol.iterator]() { throw Error("brand must be checked first"); }
        }));
    }
}

let rope = "";
for (let i = 0; i < 64; i++) rope += "abcdefgh";
const strings = ["\0x", "\ud800", "\udc00", "\ud83e\udd8a", rope, ""];
const parts = formatter.formatToParts(strings);
assert(parts.map(part => part.value).join(""), formatter.format(strings));
assert(JSON.stringify(parts.filter(part => part.type === "element").map(part => part.value)),
       JSON.stringify(strings));
assert(parts.every(part => Object.getPrototypeOf(part) === Object.prototype));
for (const part of parts) {
    assert(Object.keys(part).join(","), "type,value");
    for (const key of ["type", "value"]) {
        const descriptor = Object.getOwnPropertyDescriptor(part, key);
        assert(descriptor.writable && descriptor.enumerable && descriptor.configurable);
    }
}

// IteratorStepValue abrupt completions do not invoke return; a non-string does.
for (const name of ["format", "formatToParts"]) {
    for (const stage of ["next-get", "next-call", "done", "value", "result"]) {
        let closed = 0, caught;
        const marker = {};
        const iterator = {
            get next() {
                if (stage === "next-get") throw marker;
                return function () {
                    if (stage === "next-call") throw marker;
                    if (stage === "result") return 0;
                    return {
                        get done() { if (stage === "done") throw marker; return false; },
                        get value() { throw marker; }
                    };
                };
            },
            return() { closed++; return {}; }
        };
        try { formatter[name]({ [Symbol.iterator]() { return iterator; } }); }
        catch (error) { caught = error; }
        if (stage === "result") assert(caught instanceof TypeError);
        else assert(caught, marker);
        assert(closed, 0);
    }
    for (const value of [1, undefined, null, Symbol(), new String("x"), {
        toString() { throw Error("no string coercion permitted"); }
    }]) {
        let closed = 0;
        const iterable = {
            [Symbol.iterator]() {
                return {
                    next() { return { value, done: false }; },
                    get return() { closed++; throw Error("preserve original TypeError"); }
                };
            }
        };
        assertThrows(TypeError, () => formatter[name](iterable));
        assert(closed, 1);
    }
    let reads = 0, steps = 0, closed = 0;
    const iterable = {
        [Symbol.iterator]() {
            return {
                get next() {
                    reads++;
                    return function () {
                        assert(formatter.format(["nested"]), "nested");
                        return steps++ < 2 ? { value: "x", done: false }
                                           : { get value() { throw Error("done value"); }, done: true };
                    };
                },
                return() { closed++; return {}; }
            };
        }
    };
    const formatted = formatter[name](iterable);
    assert(name === "format" ? formatted : formatted.map(p => p.value).join(""), "x and x");
    assert(reads, 1);
    assert(steps, 3);
    assert(closed, 0);
}

// Built-in result creation bypasses inherited setters and uses the method realm.
for (const key of ["type", "value"])
    Object.defineProperty(Object.prototype, key, {
        configurable: true, set() { throw Error("inherited part setter"); }
    });
let ownParts;
try { ownParts = formatter.formatToParts(["a", "b"]); }
finally {
    delete Object.prototype.type;
    delete Object.prototype.value;
}
assert(ownParts.map(p => p.value).join(""), "a and b");
Object.defineProperty(Array.prototype, "0", {
    configurable: true, set() { throw Error("inherited array setter"); }
});
try { ownParts = formatter.formatToParts(["a", "b"]); }
finally { delete Array.prototype[0]; }
assert(ownParts[0].value, "a");
const formattingRealm = std.createRealm();
const foreignFormatParts = formattingRealm.evalScript("Intl.ListFormat.prototype.formatToParts");
const foreignParts = foreignFormatParts.call(formatter, ["a", "b"]);
assert(Object.getPrototypeOf(foreignParts), formattingRealm.evalScript("Array.prototype"));
assert(Object.getPrototypeOf(foreignParts[0]), formattingRealm.evalScript("Object.prototype"));

if (List.supportedLocalesOf("es").length) {
    assert(new List("es").format(["uno", "isla"]), "uno e isla");
    assert(new List("es", { type: "disjunction" }).format(["uno", "11.000,92"]),
           "uno u 11.000,92");
    assert(new List("es", { type: "disjunction" }).format(["uno", "110"]), "uno o 110");
}
if (List.supportedLocalesOf("he").length) {
    assert(new List("he").format(["a", "b"]), "a ו-b");
    assert(new List("he").format(["a", "בית"]), "a ובית");
}
const many = Array.from({ length: 256 }, (_, i) => String(i));
assert(formatter.formatToParts(many).filter(part => part.type === "element").length, 256);
std.gc();
assert(formatter.format(["still", "alive"]), "still and alive");

for (const name of ["format", "formatToParts"]) {
    assertThrows(TypeError, () => formatter[name]({ [Symbol.iterator]: 1 }));
    assertThrows(TypeError, () => formatter[name]({ [Symbol.iterator]() { return 1; } }));
    let closed = 0;
    assertThrows(TypeError, () => formatter[name]({
        [Symbol.iterator]() {
            return { next: 0, return() { closed++; return {}; } };
        }
    }));
    assert(closed, 0);
}
function* collectedStrings() {
    let value = "";
    for (let i = 0; i < 64; i++) value += "rope";
    yield value;
    value = null;
    std.gc();
    yield "end";
    std.gc();
}
assert(formatter.format(collectedStrings()), "rope".repeat(64) + " and end");
