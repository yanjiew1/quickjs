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

/* Exact ToIntlMathematicalValue, independently of any unfinished formatter. */
function assert(value, expected) {
    if (arguments.length < 2)
        expected = true;
    if (!Object.is(value, expected))
        throw new Error(`expected ${String(expected)}, got ${String(value)}`);
}
function check(input, text, kind = 0) {
    const actual = readMathematicalValue(input);
    assert(actual[0], text);
    assert(actual[1], kind);
}
function throws(type, callback, exact) {
    try {
        callback();
    } catch (error) {
        assert(error instanceof type);
        if (exact !== undefined)
            assert(error, exact);
        return;
    }
    throw new Error(`missing ${type.name}`);
}

check("12345678901234567890.12345678901234567890", "12345678901234567890.12345678901234567890");
check("\uFEFF\u2007 +0001.2300e+2 \u2029", "+0001.2300e+2");
check(".1234567890123456789012345", ".1234567890123456789012345");
check("0x20000000000001", "9007199254740993");
check(" \u00a00B100000000000000000000000000000000000000000000000000001 ", "9007199254740993");
check("0o400000000000000001", "9007199254740993");
check("9007199254740993", "9007199254740993");
check(9007199254740993, "9007199254740992");
check(1234567890123456789012345678901234567890n, "1234567890123456789012345678901234567890");
check(-(10n ** 400n), "-1" + "0".repeat(400)); // BigInt bypasses RoundMVResult.
check(0.1, "0.1");
check(true, "1");
check(false, "0");
check(null, "0");
check("", "0");
check(" \u3000\n", "0");
check(0n, "0");
for (const value of [undefined, NaN, "NaN", "1_000", "1n", "1\0", "+0x1", "-0b1", "1 2", "1e", "\uD800"])
    check(value, undefined, 1);
for (const value of [Infinity, "Infinity", "+Infinity", "1e999999999999999999999999", "1.7976931348623159e308"])
    check(value, undefined, 2);
for (const value of [-Infinity, "-Infinity", "-1e999999999999999999999999"])
    check(value, undefined, 3);
for (const value of [-0, "-0", "-0.000", "-1e-999999999999999999999999"])
    check(value, "-0", 4);
check("1e-999999999999999999999999", "0");
check("1.7976931348623158e308", "1.7976931348623158e308");
check("2.4703282292062328e-324", "2.4703282292062328e-324");
check("2.4703282292062327e-324", "0");

let hints = [];
check({[Symbol.toPrimitive](hint) { hints.push(hint); return "9007199254740993.00"; }}, "9007199254740993.00");
assert(hints.join(), "number");
check(Object(123n), "123");
check(Object("1.234567890123456789"), "1.234567890123456789");
let calls = [];
check({valueOf() { calls.push("valueOf"); return {}; }, toString() { calls.push("toString"); return "12.3400"; }}, "12.3400");
assert(calls.join(), "valueOf,toString");
throws(TypeError, () => readMathematicalValue(Symbol()));
throws(TypeError, () => readMathematicalValue({[Symbol.toPrimitive]() { return {}; }}));
const error = new RangeError("sentinel");
throws(RangeError, () => readMathematicalValue({get [Symbol.toPrimitive]() { throw error; }}), error);
throws(RangeError, () => readMathematicalValue({valueOf() { throw error; }}), error);
