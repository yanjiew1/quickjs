"use strict";

var status = 0;
var throw_errors = true;

function throw_error(msg) {
    if (throw_errors)
        throw Error(msg);
    console.log(msg);
    status = 1;
}

function assert(actual, expected, message) {
    function get_full_type(o) {
        var type = typeof(o);
        if (type === 'object') {
            if (o === null)
                return 'null';
            if (o.constructor && o.constructor.name)
                return o.constructor.name;
        }
        return type;
    }

    if (arguments.length == 1)
        expected = true;

    if (typeof actual === typeof expected) {
        if (actual === expected) {
            if (actual !== 0 || (1 / actual) === (1 / expected))
                return;
        }
        if (typeof actual === 'number') {
            if (isNaN(actual) && isNaN(expected))
                return true;
        }
        if (typeof actual === 'object') {
            if (actual !== null && expected !== null
            &&  actual.constructor === expected.constructor
            &&  actual.toString() === expected.toString())
                return;
        }
    }
    // Should output the source file and line number and extract
    //   the expression from the assert call
    throw_error("assertion failed: got " +
                get_full_type(actual) + ":|" + actual + "|, expected " +
                get_full_type(expected) + ":|" + expected + "|" +
                (message ? " (" + message + ")" : ""));
}

function assert_throws(expected_error, func)
{
    var err = false;
    try {
        func();
    } catch(e) {
        err = true;
        if (!(e instanceof expected_error)) {
            // Should output the source file and line number and extract
            //   the expression from the assert_throws() call
            throw_error("unexpected exception type");
            return;
        }
    }
    if (!err) {
        // Should output the source file and line number and extract
        //   the expression from the assert_throws() call
        throw_error("expected exception");
    }
}

// load more elaborate version of assert if available
try { __loadScript("test_assert.js"); } catch(e) {}

/*----------------*/

function my_func(a, b)
{
    return a + b;
}

function test_function_native_fallback()
{
    const native = "function () {\n    [native code]\n}";
    const toString = Function.prototype.toString;
    const fail = () => { throw Error("unexpected property read"); };
    function original() { /* preserved source */ return 42; }
    assert(toString.call(original),
           "function original() { /* preserved source */ return 42; }");
    const bound = original.bind(null);
    Object.defineProperty(bound, "name", { get: fail });
    assert(toString.call(bound), native);
    const proxy = new Proxy(original, { get: fail });
    assert(toString.call(proxy), native);
    const revocable = Proxy.revocable(original, { get: fail });
    revocable.revoke();
    assert(toString.call(revocable.proxy), native);
    assert_throws(TypeError, () => toString.call(new Proxy({}, {})));
    assert_throws(TypeError, () => toString.call(null));
}

function test_function_initial_name()
{
    const toString = Function.prototype.toString;
    const suffix = "() {\n    [native code]\n}";
    const samples = [
        [Math.abs, "abs"],
        [Array.prototype[Symbol.iterator], "values"],
        [RegExp.prototype[Symbol.match], "[Symbol.match]"],
        [Object.getOwnPropertyDescriptor(Map.prototype, "size").get,
         "get size"],
        [Object.getOwnPropertyDescriptor(Iterator.prototype, "constructor").get,
         "get constructor"],
        [Object.getOwnPropertyDescriptor(Iterator.prototype, "constructor").set,
         "set constructor"],
    ];
    for (const [fn, name] of samples) {
        const expected = "function " + name + suffix;
        const descriptor = Object.getOwnPropertyDescriptor(fn, "name");
        assert(toString.call(fn), expected);
        try {
            Object.defineProperty(fn, "name", { value: "changed", configurable: true });
            assert(toString.call(fn), expected);
            delete fn.name;
            assert(toString.call(fn), expected);
            Object.defineProperty(fn, "name", {
                get() { throw Error("native name read"); }, configurable: true,
            });
            assert(toString.call(fn), expected);
        } finally {
            Object.defineProperty(fn, "name", descriptor);
        }
    }
    const anonymous = [Proxy.revocable({}, {}).revoke];
    new Promise((resolve, reject) => anonymous.push(resolve, reject));
    for (const fn of anonymous) {
        Object.defineProperty(fn, "name", {
            get() { throw Error("anonymous native name read"); },
        });
        assert(toString.call(fn), "function " + suffix);
    }
}

function test_function_constructor_boundaries()
{
    const constructors = [Function, (function*() {}).constructor,
                          (async function() {}).constructor,
                          (async function*() {}).constructor];
    const invalid = [
        ["/*", "*/) {"], ["//", ") {"], ["a = `", "` ) {"],
        [") { var x = function (", "} "], ["x = function (", "}) {"],
        ["", "}); globalThis.constructorSideEffect = true; (function() {"],
    ];
    globalThis.constructorSideEffect = false;
    for (const ctor of constructors) {
        for (const args of invalid)
            assert_throws(SyntaxError, () => ctor(...args));
        const valid = ctor("café", "x = function nested(a) { return a; }",
                           "/* body comment */ return café;");
        assert(valid.name, "anonymous");
        assert(valid.length, 1);
        assert(valid.toString().includes("café"));
        assert(ctor("// parameter comment", "// body comment").length, 0);
        assert(ctor("a = `text`", "return a;").length, 0);
        const order = [];
        ctor({ toString() { order.push(1); return "x"; } },
             { toString() { order.push(2); return "y"; } },
             { toString() { order.push(3); return "return x + y;"; } });
        assert(JSON.stringify(order), "[1,2,3]");
        const target = new Proxy(function() {}, {
            get(object, key) { if (key === "prototype") throw Error("prototype read");
                               return Reflect.get(object, key); },
        });
        assert_throws(SyntaxError, () => Reflect.construct(ctor, ["/*", "*/"], target));
    }
    assert(globalThis.constructorSideEffect, false);
    delete globalThis.constructorSideEffect;
    assert(Function("value", "return value + 1;")(41), 42);
}

function test_function()
{
    function f(a, b) {
        var i, tab = [];
        tab.push(this);
        for(i = 0; i < arguments.length; i++)
            tab.push(arguments[i]);
        return tab;
    }
    function constructor1(a) {
        this.x = a;
    }

    var r, g;

    r = my_func.call(null, 1, 2);
    assert(r, 3, "call");

    r = my_func.apply(null, [1, 2]);
    assert(r, 3, "apply");

    r = (function () { return 1; }).apply(null, undefined);
    assert(r, 1);

    assert_throws(TypeError, (function() {
        Reflect.apply((function () { return 1; }), null, undefined);
    }));

    r = new Function("a", "b", "return a + b;");
    assert(r(2,3), 5, "function");

    g = f.bind(1, 2);
    assert(g.length, 1);
    assert(g.name, "bound f");
    assert(g(3), [1,2,3]);

    g = constructor1.bind(null, 1);
    r = new g();
    assert(r.x, 1);
}

function test()
{
    var r, a, b, c, err;

    r = Error("hello");
    assert(r.message, "hello", "Error");

    a = new Object();
    a.x = 1;
    assert(a.x, 1, "Object");

    assert(Object.getPrototypeOf(a), Object.prototype, "getPrototypeOf");
    Object.defineProperty(a, "y", { value: 3, writable: true, configurable: true, enumerable: true });
    assert(a.y, 3, "defineProperty");

    Object.defineProperty(a, "z", { get: function () { return 4; }, set: function(val) { this.z_val = val; }, configurable: true, enumerable: true });
    assert(a.z, 4, "get");
    a.z = 5;
    assert(a.z_val, 5, "set");

    a = { get z() { return 4; }, set z(val) { this.z_val = val; } };
    assert(a.z, 4, "get");
    a.z = 5;
    assert(a.z_val, 5, "set");

    b = Object.create(a);
    assert(Object.getPrototypeOf(b), a, "create");
    c = {u:2};
    /* XXX: refcount bug in 'b' instead of 'a' */
    Object.setPrototypeOf(a, c);
    assert(Object.getPrototypeOf(a), c, "setPrototypeOf");

    a = {};
    assert(a.toString(), "[object Object]", "toString");

    a = {x:1};
    assert(Object.isExtensible(a), true, "extensible");
    Object.preventExtensions(a);

    err = false;
    try {
        a.y = 2;
    } catch(e) {
        err = true;
    }
    assert(Object.isExtensible(a), false, "extensible");
    assert(typeof a.y, "undefined", "extensible");
    assert(err, true, "extensible");
}

function test_enum()
{
    var a, tab;
    a = {x:1,
         "18014398509481984": 1,
         "9007199254740992": 1,
         "9007199254740991": 1,
         "4294967296": 1,
         "4294967295": 1,
         y:1,
         "4294967294": 1,
         "1": 2};
    tab = Object.keys(a);
//    console.log("tab=" + tab.toString());
    assert(tab, ["1","4294967294","x","18014398509481984","9007199254740992","9007199254740991","4294967296","4294967295","y"], "keys");
}

function test_array_sort_writeback()
{
    for (const values of [[1], [1, 2], [undefined]])
        assert_throws(TypeError, () => Object.freeze(values).sort(() => 0));
    const writes = [];
    const accessors = {
        length: 2,
        get 0() { return 1; },
        set 0(value) { writes.push([0, value]); },
        get 1() { return 2; },
        set 1(value) { writes.push([1, value]); },
    };
    assert(Array.prototype.sort.call(accessors, () => 0) === accessors);
    assert(JSON.stringify(writes), "[[0,1],[1,2]]");
    writes.length = 0;
    const proxy = new Proxy([1, 2], {
        set(target, key, value, receiver) {
            writes.push([key, value]);
            return Reflect.set(target, key, value, receiver);
        },
    });
    assert(proxy.sort(() => 0) === proxy);
    assert(JSON.stringify(writes), '[["0",1],["1",2]]');
    const values = [1, 2];
    values.sort(() => { values[0] = 99; return 0; });
    assert(JSON.stringify(values), "[1,2]");
    const shrunk = [1, 2];
    shrunk.sort(() => { shrunk.length = 0; return 0; });
    assert(JSON.stringify(shrunk), "[1,2]");
    const rejected = new Proxy([1], { set() { return false; } });
    assert_throws(TypeError, () => rejected.sort());
}

function test_array()
{
    var a, err;

    a = [1, 2, 3];
    assert(a.length, 3, "array");
    assert(a[2], 3, "array1");

    a = new Array(10);
    assert(a.length, 10, "array2");

    a = new Array(1, 2);
    assert(a.length === 2 && a[0] === 1 && a[1] === 2, true, "array3");

    a = [1, 2, 3];
    a.length = 2;
    assert(a.length === 2 && a[0] === 1 && a[1] === 2, true, "array4");

    a = [];
    a[1] = 10;
    a[4] = 3;
    assert(a.length, 5);

    a = [1,2];
    a.length = 5;
    a[4] = 1;
    a.length = 4;
    assert(a[4] !== 1, true, "array5");

    a = [1,2];
    a.push(3,4);
    assert(a.join(), "1,2,3,4", "join");

    a = [1,2,3,4,5];
    Object.defineProperty(a, "3", { configurable: false });
    err = false;
    try {
        a.length = 2;
    } catch(e) {
        err = true;
    }
    assert(err && a.toString() === "1,2,3,4");
}

function test_string()
{
    var a;
    a = String("abc");
    assert(a.length, 3, "string");
    assert(a[1], "b", "string");
    assert(a.charCodeAt(1), 0x62, "string");
    assert(String.fromCharCode(65), "A", "string");
    assert(String.fromCharCode.apply(null, [65, 66, 67]), "ABC", "string");
    assert(a.charAt(1), "b");
    assert(a.charAt(-1), "");
    assert(a.charAt(3), "");

    a = "abcd";
    assert(a.substring(1, 3), "bc", "substring");
    a = String.fromCharCode(0x20ac);
    assert(a.charCodeAt(0), 0x20ac, "unicode");
    assert(a, "€", "unicode");
    assert(a, "\u20ac", "unicode");
    assert(a, "\u{20ac}", "unicode");
    assert("a", "\x61", "unicode");

    a = "\u{10ffff}";
    assert(a.length, 2, "unicode");
    assert(a, "\u{dbff}\u{dfff}", "unicode");
    assert(a.codePointAt(0), 0x10ffff);
    assert(String.fromCodePoint(0x10ffff), a);

    assert("a".concat("b", "c"), "abc");

    assert("abcabc".indexOf("cab"), 2);
    assert("abcabc".indexOf("cab2"), -1);
    assert("abc".indexOf("c"), 2);

    assert("aaa".indexOf("a"), 0);
    assert("aaa".indexOf("a", NaN), 0);
    assert("aaa".indexOf("a", -Infinity), 0);
    assert("aaa".indexOf("a", -1), 0);
    assert("aaa".indexOf("a", -0), 0);
    assert("aaa".indexOf("a", 0), 0);
    assert("aaa".indexOf("a", 1), 1);
    assert("aaa".indexOf("a", 2), 2);
    assert("aaa".indexOf("a", 3), -1);
    assert("aaa".indexOf("a", 4), -1);
    assert("aaa".indexOf("a", Infinity), -1);

    assert("aaa".indexOf(""), 0);
    assert("aaa".indexOf("", NaN), 0);
    assert("aaa".indexOf("", -Infinity), 0);
    assert("aaa".indexOf("", -1), 0);
    assert("aaa".indexOf("", -0), 0);
    assert("aaa".indexOf("", 0), 0);
    assert("aaa".indexOf("", 1), 1);
    assert("aaa".indexOf("", 2), 2);
    assert("aaa".indexOf("", 3), 3);
    assert("aaa".indexOf("", 4), 3);
    assert("aaa".indexOf("", Infinity), 3);

    assert("aaa".lastIndexOf("a"), 2);
    assert("aaa".lastIndexOf("a", NaN), 2);
    assert("aaa".lastIndexOf("a", -Infinity), 0);
    assert("aaa".lastIndexOf("a", -1), 0);
    assert("aaa".lastIndexOf("a", -0), 0);
    assert("aaa".lastIndexOf("a", 0), 0);
    assert("aaa".lastIndexOf("a", 1), 1);
    assert("aaa".lastIndexOf("a", 2), 2);
    assert("aaa".lastIndexOf("a", 3), 2);
    assert("aaa".lastIndexOf("a", 4), 2);
    assert("aaa".lastIndexOf("a", Infinity), 2);

    assert("aaa".lastIndexOf(""), 3);
    assert("aaa".lastIndexOf("", NaN), 3);
    assert("aaa".lastIndexOf("", -Infinity), 0);
    assert("aaa".lastIndexOf("", -1), 0);
    assert("aaa".lastIndexOf("", -0), 0);
    assert("aaa".lastIndexOf("", 0), 0);
    assert("aaa".lastIndexOf("", 1), 1);
    assert("aaa".lastIndexOf("", 2), 2);
    assert("aaa".lastIndexOf("", 3), 3);
    assert("aaa".lastIndexOf("", 4), 3);
    assert("aaa".lastIndexOf("", Infinity), 3);

    assert("a,b,c".split(","), ["a","b","c"]);
    assert(",b,c".split(","), ["","b","c"]);
    assert("a,b,".split(","), ["a","b",""]);

    assert("aaaa".split(), [ "aaaa" ]);
    assert("aaaa".split(undefined, 0), [ ]);
    assert("aaaa".split(""), [ "a", "a", "a", "a" ]);
    assert("aaaa".split("", 0), [ ]);
    assert("aaaa".split("", 1), [ "a" ]);
    assert("aaaa".split("", 2), [ "a", "a" ]);
    assert("aaaa".split("a"), [ "", "", "", "", "" ]);
    assert("aaaa".split("a", 2), [ "", "" ]);
    assert("aaaa".split("aa"), [ "", "", "" ]);
    assert("aaaa".split("aa", 0), [ ]);
    assert("aaaa".split("aa", 1), [ "" ]);
    assert("aaaa".split("aa", 2), [ "", "" ]);
    assert("aaaa".split("aaa"), [ "", "a" ]);
    assert("aaaa".split("aaaa"), [ "", "" ]);
    assert("aaaa".split("aaaaa"), [ "aaaa" ]);
    assert("aaaa".split("aaaaa", 0), [  ]);
    assert("aaaa".split("aaaaa", 1), [ "aaaa" ]);

    assert(eval('"\0"'), "\0");

    assert("abc".padStart(Infinity, ""), "abc");
}

function test_string_normalize()
{
    for (const form of ["NFC", "NFD", "NFKC", "NFKD"]) {
        assert("".normalize(form), "");
        assert("abc".normalize(form), "abc");
    }
    assert("A\u030a".repeat(128).normalize("NFC"), "\u00c5".repeat(128));
    assert("\u00c5".repeat(128).normalize("NFD"), "A\u030a".repeat(128));
    assert("\ufb03".repeat(128).normalize("NFKC"), "ffi".repeat(128));
    assert("\ufb03".repeat(128).normalize("NFKD"), "ffi".repeat(128));
    for (const form of ["NFC", "NFKC"]) {
        assert("\uac00\u11a7".normalize(form), "\uac00\u11a7");
        assert("\u1100\u1161\u11a7".normalize(form), "\uac00\u11a7");
        assert("\uac00\u11a8".normalize(form), "\uac01");
        assert("\uac00\u11c2".normalize(form), "\uac1b");
        assert("\uac00\u11c3".normalize(form), "\uac00\u11c3");
    }
    for (const form of ["NFD", "NFKD"])
        assert("\uac00\u11a7".normalize(form), "\u1100\u1161\u11a7");
    for (const [source, target] of [["\u209d", "w"], ["\u0558", "\u0567"],
                                   ["\u{1df95}", "\u00df"], ["\u{1d6a6}", "\u00df"]]) {
        for (const form of ["NFC", "NFD"])
            assert(source.normalize(form), source);
        for (const form of ["NFKC", "NFKD"])
            assert(source.normalize(form), target);
    }
    const reordered = "a\u05ae\u0300\u{10ecb}\u0315b";
    const composed = "\u00e0\u05ae\u{10ecb}\u0315b";
    const input = "a\u0315\u0300\u05ae\u{10ecb}b";
    for (const form of ["NFD", "NFKD"])
        assert(input.normalize(form), reordered);
    for (const form of ["NFC", "NFKC"])
        assert(input.normalize(form), composed);
}

function test_string_unicode_18()
{
    const pairs = [
        [0xa7dd, 0x0277], [0xa7e2, 0x027c], [0xab6c, 0xab4b], [0xab6d, 0xab4c],
        [0x1df40, 0x1df41], [0x1df48, 0x1df49], [0x1df4a, 0x1df4b],
        [0x1df4d, 0x1df4e], [0x1df51, 0x1df52], [0x1df68, 0x1df69],
        [0x1df6a, 0x1df6b], [0x1df6c, 0x1df6d], [0x1df6e, 0x1df6f],
        [0x1df72, 0x1df73], [0x1df74, 0x1df75], [0x1df76, 0x1df77],
        [0x1df78, 0x1df79], [0x1df7a, 0x1df7b], [0x1df7c, 0x1df7d],
        [0x1df7e, 0x1df7f],
    ];
    for (const [upperCode, lowerCode] of pairs) {
        const upper = String.fromCodePoint(upperCode);
        const lower = String.fromCodePoint(lowerCode);
        assert(upper.toLowerCase(), lower);
        assert(lower.toUpperCase(), upper);
        for (const flags of ["iu", "iv"]) {
            assert(new RegExp("^" + upper + "$", flags).test(lower), true);
            assert(new RegExp("^[" + upper + "-" + upper + "]$", flags).test(lower), true);
            assert(new RegExp("^(" + lower + ")\\1$", flags).test(lower + upper), true);
            assert(new RegExp("^" + lower + "$", flags).test("A"), false);
        }
    }
    assert("\u{1df95}".toUpperCase(), "SS");
    assert("\u{1df95}".toLowerCase(), "\u{1df95}");
}

function test_regexp_unicode_18()
{
    const scripts = [
        ["Jurchen", "Jurc", 0x18e00, 0x191d2, "Lo"],
        ["Proto_Cuneiform", "Pcun", 0x125a8, 0x1264b, "Nl"],
        ["Seal", "Seal", 0x3d000, 0x3fc3f, "Lo"],
    ];
    for (const [script, alias, first, last, category] of scripts) {
        const text = String.fromCodePoint(first, last);
        for (const flags of ["u", "v"]) {
            for (const property of ["Script", "sc", "Script_Extensions", "scx"]) {
                for (const name of [script, alias]) {
                    const expression = property + "=" + name;
                    assert(new RegExp("^\\p{" + expression + "}+$", flags).test(text), true);
                    assert(new RegExp("^\\p{" + expression + "}+$", flags).test("A"), false);
                    assert(new RegExp("^\\P{" + expression + "}+$", flags).test("A"), true);
                }
            }
            assert(new RegExp("^\\p{" + category + "}+$", flags).test(text), true);
            assert(new RegExp("^\\p{ID_Start}+$", flags).test(text), true);
            assert(new RegExp("^\\p{ID_Continue}+$", flags).test(text), true);
        }
        assert(new RegExp("^[\\p{sc=" + script + "}&&\\p{" + category + "}]+$", "v").test(text), true);
    }
    assert(/^[\p{sc=Han}&&\p{Lo}]$/v.test("\u{2b81e}"), true);
    assert(/^\p{Sc}+$/u.test("\u20c2\u20c3\u20c4"), true);
    assert(/^\p{ID_Start}$/u.test("\u20c2"), false);

    for (const flags of ["iu", "iv"]) {
        for (const target of ["\u00df", "\u1e9e", "\u{1df95}"]) {
            assert(new RegExp("^\\u{1df95}$", flags).test(target), true);
            assert(new RegExp("^[\\u{1df95}]$", flags).test(target), true);
            assert(new RegExp("^(\\u{1df95})\\1$", flags).test("\u{1df95}" + target), true);
        }
        assert(new RegExp("^\\u{1df95}$", flags).test("ss"), false);
        for (const [source, target] of [["\u1fd3", "\u0390"], ["\u1fe3", "\u03b0"],
                                       ["\ufb05", "\ufb06"]]) {
            assert(new RegExp("^" + source + "$", flags).test(target), true);
            assert(new RegExp("^" + target + "$", flags).test(source), true);
        }
    }
    assert(new RegExp("^" + "\u{1df95}" + "$", "i").test("\u00df"), false);
    assert(/^\p{Emoji}+$/u.test("\u{1f6d9}\u{1fadd}"), true);
    assert(/^\p{Basic_Emoji}+$/v.test("\u{1f6d9}\u{1fadd}"), true);
    assert(/^\p{RGI_Emoji_Modifier_Sequence}$/v.test("\u{1faf9}\u{1f3fb}"), true);
    assert(/^\p{RGI_Emoji_Modifier_Sequence}$/v.test("\u{1faf9}A"), false);
}

function test_math()
{
    var a;
    a = 1.4;
    assert(Math.floor(a), 1);
    assert(Math.ceil(a), 2);
    assert(Math.imul(0x12345678, 123), -1088058456);
    assert(Math.imul(0xB505, 0xB504), 2147441940);
    assert(Math.imul(0xB505, 0xB505), -2147479015);
    assert(Math.imul((-2)**31, (-2)**31), 0);
    assert(Math.imul(2**31-1, 2**31-1), 1);
    assert(Math.fround(0.1), 0.10000000149011612);
    assert(Math.hypot(), 0);
    assert(Math.hypot(-2), 2);
    assert(Math.hypot(3, 4), 5);
    assert(Math.abs(Math.hypot(3, 4, 5) - 7.0710678118654755) <= 1e-15);
    assert(Math.sumPrecise([1,Number.EPSILON/2,Number.MIN_VALUE]), 1.0000000000000002);
}

function test_number()
{
    assert(parseInt("123"), 123);
    assert(parseInt("  123r"), 123);
    assert(parseInt("0x123"), 0x123);
    assert(parseInt("0o123"), 0);
    assert(+"  123   ", 123);
    assert(+"0b111", 7);
    assert(+"0o123", 83);
    assert(parseFloat("2147483647"), 2147483647);
    assert(parseFloat("2147483648"), 2147483648);
    assert(parseFloat("-2147483647"), -2147483647);
    assert(parseFloat("-2147483648"), -2147483648);
    assert(parseFloat("0x1234"), 0);
    assert(parseFloat("Infinity"), Infinity);
    assert(parseFloat("-Infinity"), -Infinity);
    assert(parseFloat("123.2"), 123.2);
    assert(parseFloat("123.2e3"), 123200);
    assert(Number.isNaN(Number("+")));
    assert(Number.isNaN(Number("-")));
    assert(Number.isNaN(Number("\x00a")));

    assert((1-2**-53).toString(12), "0.bbbbbbbbbbbbbba");
    assert((1000000000000000128).toString(), "1000000000000000100");
    assert((1000000000000000128).toFixed(0), "1000000000000000128");
    assert((25).toExponential(0), "3e+1");
    assert((-25).toExponential(0), "-3e+1");
    assert((2.5).toPrecision(1), "3");
    assert((-2.5).toPrecision(1), "-3");
    assert((25).toPrecision(1) === "3e+1");
    assert((1.125).toFixed(2), "1.13");
    assert((-1.125).toFixed(2), "-1.13");
    assert((0.5).toFixed(0), "1");
    assert((-0.5).toFixed(0), "-1");
    assert((-1e-10).toFixed(0), "-0");

    assert((1.3).toString(7), "1.2046204620462046205");
    assert((1.3).toString(35), "1.ahhhhhhhhhm");
}

function test_eval2()
{
    var g_call_count = 0;
    /* force non strict mode for f1 and f2 */
    var f1 = new Function("eval", "eval(1, 2)");
    var f2 = new Function("eval", "eval(...[1, 2])");
    function g(a, b) {
        assert(a, 1);
        assert(b, 2);
        g_call_count++;
    }
    f1(g);
    f2(g);
    assert(g_call_count, 2);
}

function test_eval()
{
    function f(b) {
        var x = 1;
        return eval(b);
    }
    var r, a;

    r = eval("1+1;");
    assert(r, 2, "eval");

    r = eval("var my_var=2; my_var;");
    assert(r, 2, "eval");
    assert(typeof my_var, "undefined");

    assert(eval("if (1) 2; else 3;"), 2);
    assert(eval("if (0) 2; else 3;"), 3);

    assert(f.call(1, "this"), 1);

    a = 2;
    assert(eval("a"), 2);

    eval("a = 3");
    assert(a, 3);

    assert(f("arguments.length", 1), 2);
    assert(f("arguments[1]", 1), 1);

    a = 4;
    assert(f("a"), 4);
    f("a=3");
    assert(a, 3);

    test_eval2();
}

function test_array_buffer_max_index()
{
    for (const C of [ArrayBuffer, SharedArrayBuffer]) {
        for (const value of [Infinity, -Infinity, 2 ** 64, -(2 ** 64), -1])
            assert_throws(RangeError, () => new C(0, { maxByteLength: value }));
        assert_throws(TypeError, () => new C(0, { maxByteLength: Symbol("max") }));
        for (const value of [NaN, -0.5, 0.5, 1.9, "2"])
            assert(new C(0, { maxByteLength: value }).maxByteLength,
                   Number.isNaN(value) ? 0 : Math.trunc(Number(value)) || 0);
        assert_throws(RangeError, () => new C(2, { maxByteLength: 1.9 }));
        assert(new C(0, { maxByteLength: undefined }).maxByteLength, 0);

        const marker = {};
        let caught;
        try {
            new C(0, { maxByteLength: { valueOf() { throw marker; } } });
        } catch (error) {
            caught = error;
        }
        assert(caught === marker, true);

        const events = [];
        const length = { valueOf() { events.push("length"); return 0; } };
        const options = {
            get maxByteLength() {
                events.push("max");
                return { valueOf() { events.push("index"); return 2; } };
            }
        };
        const target = new Proxy(function() {}, {
            get(target, key) {
                assert(key, "prototype");
                events.push("prototype");
                return C.prototype;
            }
        });
        assert(Reflect.construct(C, [length, options], target).maxByteLength, 2);
        assert(events.join(","), "length,max,index,prototype");
    }
}

function test_shared_array_buffer_atomics()
{
    if (typeof Atomics === "undefined")
        return;
    for (const C of [BigInt64Array, BigUint64Array]) {
        for (const length of [0, 1, 7, 8, 9, 16]) {
            const fixed = new SharedArrayBuffer(length);
            assert(fixed.byteLength, length);
            const growable = new SharedArrayBuffer(length, { maxByteLength: 16 });
            assert(growable.byteLength, length);
            growable.grow(16);
            assert(growable.byteLength, 16);
            for (const buffer of [fixed, growable]) {
                const view = new C(buffer, 0, Math.floor(buffer.byteLength / 8));
                for (let i = 0; i < view.length; i++) {
                    assert(Atomics.load(view, i), 0n);
                    assert(Atomics.store(view, i, 0x1122334455667788n),
                           0x1122334455667788n);
                    assert(Atomics.add(view, i, 1n), 0x1122334455667788n);
                    assert(Atomics.load(view, i), 0x1122334455667789n);
                    assert(Atomics.compareExchange(view, i,
                                                  0x1122334455667789n, 37n),
                           0x1122334455667789n);
                    assert(view[i], 37n);
                }
            }
        }
    }
}

function test_array_buffer_resize_order()
{
    for (const [buffer, method] of [[new ArrayBuffer(0), "resize"],
                                    [new SharedArrayBuffer(0), "grow"]]) {
        let calls = 0;
        assert_throws(TypeError, () => buffer[method]({
            valueOf() { calls++; return 0; }
        }));
        assert(calls, 0);
    }

    const detached = new ArrayBuffer(0, { maxByteLength: 2 });
    detached.transfer();
    const marker = {};
    let calls = 0, caught;
    try {
        detached.resize({ valueOf() { calls++; throw marker; } });
    } catch (error) {
        caught = error;
    }
    assert(caught === marker, true);
    assert(calls, 1);
    assert_throws(TypeError, () => detached.resize({
        valueOf() { calls++; return 0; }
    }));
    assert(calls, 2);

    const buffer = new ArrayBuffer(0, { maxByteLength: 2 });
    assert_throws(TypeError, () => buffer.resize({
        valueOf() { buffer.transfer(); return 0; }
    }));
    assert(buffer.detached, true);
}

function test_array_buffer_transfer_range()
{
    const buffer = new ArrayBuffer(1, { maxByteLength: 2 });
    new Uint8Array(buffer)[0] = 42;
    assert_throws(RangeError, () => buffer.transfer(3));
    assert(buffer.detached, false);
    assert(buffer.byteLength, 1);
    assert(buffer.maxByteLength, 2);
    assert(new Uint8Array(buffer)[0], 42);
    const fixed = buffer.transferToFixedLength(3);
    assert(fixed.byteLength, 3);
    assert(fixed.resizable, false);
    assert([...new Uint8Array(fixed)].join(","), "42,0,0");
    assert(buffer.detached, true);

    const next = new ArrayBuffer(0, { maxByteLength: 2 });
    const resizable = next.transfer(2);
    assert(resizable.byteLength, 2);
    assert(resizable.maxByteLength, 2);
    assert(resizable.resizable, true);
    assert(next.detached, true);
}

function test_array_buffer_slice_shrink()
{
    for (const resized of [0, 1, 2, 3, 6]) {
        const buffer = new ArrayBuffer(4, { maxByteLength: 8 });
        new Uint8Array(buffer).set([1, 2, 3, 4]);
        let calls = 0;
        let destination;
        buffer.constructor = {
            [Symbol.species]: function(length) {
                calls++;
                assert(length, 3);
                buffer.resize(resized);
                destination = new ArrayBuffer(length);
                new Uint8Array(destination).fill(99);
                return destination;
            }
        };
        const result = buffer.slice(1, 4);
        assert(result === destination, true);
        assert(result.byteLength, 3);
        assert(calls, 1);
        const expected = [99, 99, 99];
        for (let i = 0; i < Math.min(3, Math.max(resized - 1, 0)); i++)
            expected[i] = i + 2;
        assert([...new Uint8Array(result)].join(","), expected.join(","));
    }

    const buffer = new ArrayBuffer(4, { maxByteLength: 4 });
    buffer.constructor = {
        [Symbol.species]: function(length) {
            buffer.resize(0);
            return new ArrayBuffer(length);
        }
    };
    assert(buffer.slice(4, 4).byteLength, 0);

    for (const end of [0, 3]) {
        const detached = new ArrayBuffer(3, { maxByteLength: 3 });
        detached.constructor = {
            [Symbol.species]: function(length) {
                detached.transfer();
                return new ArrayBuffer(length);
            }
        };
        assert_throws(TypeError, () => detached.slice(0, end));
    }
}

function test_typed_array_with_conversion()
{
    const numberTypes = [Uint8ClampedArray, Uint8Array, Int8Array, Uint16Array,
                         Int16Array, Uint32Array, Int32Array, Float16Array,
                         Float32Array, Float64Array];
    for (const C of numberTypes) {
        const empty = new C(0);
        assert_throws(TypeError, () => empty.with(0, 1n));
        assert_throws(TypeError, () => empty.with(0, Symbol("value")));
        assert_throws(RangeError, () => empty.with(0, 1));
        const events = [];
        assert(new C([1, 2]).with({
            valueOf() { events.push("index"); return 1; }
        }, {
            [Symbol.toPrimitive](hint) {
                events.push("value");
                assert(hint, "number");
                return "3";
            }
        })[1], 3);
        assert(events.join(","), "index,value");
    }
    for (const C of [BigInt64Array, BigUint64Array]) {
        const empty = new C(0);
        assert_throws(TypeError, () => empty.with(0, 1));
        assert_throws(TypeError, () => empty.with(0, Symbol("value")));
        assert_throws(RangeError, () => empty.with(0, 1n << 128n));
        let calls = 0;
        assert(new C([1n, 2n]).with(-1, {
            [Symbol.toPrimitive](hint) {
                calls++;
                assert(hint, "number");
                return "3";
            }
        })[1], 3n);
        assert(calls, 1);
    }

    const marker = {};
    let caught;
    try {
        new Uint8Array(0).with(0, {
            valueOf() { throw marker; }
        });
    } catch (error) {
        caught = error;
    }
    assert(caught === marker, true);

    const buffer = new ArrayBuffer(2, { maxByteLength: 2 });
    const array = new Uint8Array(buffer);
    assert_throws(RangeError, () => array.with(1, {
        valueOf() { buffer.resize(1); return 3; }
    }));
}

function test_typed_array_copywithin_zero()
{
    for (const C of [Uint8Array, BigInt64Array]) {
        for (const argument of [0, 1, 2]) {
            const array = new C(2);
            const buffer = array.buffer;
            const args = [0, 0, 2];
            args[argument] = {
                valueOf() {
                    buffer.transfer();
                    return argument === 2 ? 0 : 2;
                }
            };
            assert(array.copyWithin(...args) === array, true);
            assert(buffer.detached, true);
        }
        const array = new C(2);
        assert_throws(TypeError, () => array.copyWithin({
            valueOf() { array.buffer.transfer(); return 0; }
        }, 0));

        const empty = new C(0);
        assert(empty.copyWithin({
            valueOf() { empty.buffer.transfer(); return 0; }
        }, 0) === empty, true);
    }

    const buffer = new ArrayBuffer(2, { maxByteLength: 2 });
    const tracking = new Uint8Array(buffer);
    assert(tracking.copyWithin({
        valueOf() { buffer.resize(0); return 0; }
    }, 0) === tracking, true);
    assert(tracking.length, 0);

    buffer.resize(2);
    const fixed = new Uint8Array(buffer, 0, 2);
    assert_throws(TypeError, () => fixed.copyWithin({
        valueOf() { buffer.resize(1); return 0; }
    }, 0));
}

function test_typed_array_set_content()
{
    const numberTypes = [Uint8ClampedArray, Uint8Array, Int8Array, Uint16Array,
                         Int16Array, Uint32Array, Int32Array, Float16Array,
                         Float32Array, Float64Array];
    for (const NumberType of numberTypes) {
        for (const BigType of [BigInt64Array, BigUint64Array]) {
            assert_throws(TypeError, () => new NumberType(0).set(new BigType(0)));
            assert_throws(TypeError, () => new BigType(0).set(new NumberType(0)));
            assert_throws(TypeError, () => new NumberType(1).set(new BigType(0), 1));
            assert_throws(TypeError, () => new BigType(1).set(new NumberType(0), 1));
            assert_throws(RangeError, () => new NumberType(0).set(new BigType(1)));
            assert_throws(RangeError, () => new BigType(0).set(new NumberType(1)));
        }
        const array = new NumberType(1);
        assert(array.set(new Float64Array([3])), undefined);
        assert(array[0], 3);
        assert(array.set(new Float64Array(0), 1), undefined);
        assert(array.set([], 1), undefined);
    }
    const big = new BigInt64Array(1);
    assert(big.set(new BigUint64Array([3n])), undefined);
    assert(big[0], 3n);
    assert(big.set([], 1), undefined);
}

function test_typed_array_constructor_content()
{
    const numberTypes = [Uint8ClampedArray, Uint8Array, Int8Array, Uint16Array,
                         Int16Array, Uint32Array, Int32Array, Float16Array,
                         Float32Array, Float64Array];
    for (const NumberType of numberTypes) {
        for (const BigType of [BigInt64Array, BigUint64Array]) {
            assert_throws(TypeError, () => new NumberType(new BigType(0)));
            assert_throws(TypeError, () => new BigType(new NumberType(0)));
            assert_throws(TypeError, () => new NumberType(new BigType(1)));
            assert_throws(TypeError, () => new BigType(new NumberType(1)));
        }
        assert(new NumberType(new Float64Array(0)).length, 0);
        assert(new NumberType(new Float64Array([3]))[0], 3);
    }
    assert(new BigInt64Array(new BigUint64Array(0)).length, 0);
    assert(new BigInt64Array(new BigUint64Array([3n]))[0], 3n);
}

function test_typed_array_species_content()
{
    for (const [Source, Destination] of [[Uint8Array, BigInt64Array],
                                        [Float64Array, BigUint64Array],
                                        [BigInt64Array, Uint8Array],
                                        [BigUint64Array, Float64Array]]) {
        const source = new Source(0);
        let calls = 0;
        source.constructor = {
            [Symbol.species]: function(length) {
                calls++;
                return new Destination(length);
            }
        };
        assert_throws(TypeError, () => source.slice());
        assert_throws(TypeError, () => source.map(() => {
            throw Error("empty mapper");
        }));
        assert_throws(TypeError, () => source.filter(() => {
            throw Error("empty filter");
        }));
        assert(calls, 3);
    }
    const numbers = new Uint8Array(0);
    numbers.constructor = { [Symbol.species]: Float64Array };
    assert(numbers.slice() instanceof Float64Array, true);
    const big = new BigInt64Array(0);
    big.constructor = { [Symbol.species]: BigUint64Array };
    assert(big.slice() instanceof BigUint64Array, true);
}

function test_typed_array_set_overlap()
{
    for (const [Source, Destination] of [[Uint8Array, Uint16Array],
                                        [Uint16Array, Uint8Array],
                                        [Float32Array, Uint8Array],
                                        [Uint8Array, Float32Array],
                                        [BigInt64Array, BigUint64Array]]) {
        for (const sourceOffset of [0, Source.BYTES_PER_ELEMENT]) {
            for (const destinationOffset of [0, Destination.BYTES_PER_ELEMENT]) {
                const buffer = new ArrayBuffer(64);
                const source = new Source(buffer, sourceOffset, 3);
                const destination = new Destination(buffer, destinationOffset, 3);
                source.set(Source === BigInt64Array ? [1n, 2n, 3n] : [1, 2, 3]);
                const expected = new Destination(new Source(source));
                assert(destination.set(source), undefined);
                assert([...destination].join(","), [...expected].join(","));
            }
        }
    }

    const buffer = new ArrayBuffer(8);
    const bytes = new Uint8Array(buffer);
    bytes.set([1, 2, 3, 4, 5, 6, 7, 8]);
    const words = new Uint16Array(buffer);
    words.set(bytes.subarray(0, 2), 1);
    assert(words[1], 1);
    assert(words[2], 2);

    const source = new Uint8Array([1, 2, 3]);
    const destination = new Uint16Array(3);
    destination.set(source);
    assert([...destination].join(","), "1,2,3");
    assert([...source].join(","), "1,2,3");
}

function test_typed_array_constructor_length()
{
    for (const C of [Uint8Array, Float32Array, BigInt64Array]) {
        const size = C.BYTES_PER_ELEMENT;
        for (const resized of [0, 2, 6]) {
            const buffer = new ArrayBuffer(4 * size, { maxByteLength: 8 * size });
            const source = new C(buffer);
            source.set(C === BigInt64Array ? [1n, 2n, 3n, 4n] : [1, 2, 3, 4]);
            let reads = 0;
            const target = new Proxy(function() {}, {
                get(target, key) {
                    assert(key, "prototype");
                    reads++;
                    buffer.resize(resized * size);
                    return C.prototype;
                }
            });
            const copy = Reflect.construct(C, [source], target);
            assert(reads, 1);
            assert(copy.length, resized);
            assert([...copy].join(","), [...source].join(","));
            assert(copy.buffer === buffer, false);
        }
    }

    const buffer = new ArrayBuffer(4, { maxByteLength: 4 });
    const fixed = new Uint8Array(buffer, 0, 4);
    const target = new Proxy(function() {}, {
        get(target, key) { buffer.resize(2); return Uint8Array.prototype; }
    });
    assert_throws(TypeError, () => Reflect.construct(Uint8Array, [fixed], target));

    buffer.resize(4);
    const detachedTarget = new Proxy(function() {}, {
        get(target, key) { buffer.transfer(); return Uint8Array.prototype; }
    });
    assert_throws(TypeError, () => Reflect.construct(Uint8Array, [fixed], detachedTarget));
}

function test_typed_array()
{
    var buffer, a, i, str;

    a = new Uint8Array(4);
    assert(a.length, 4);
    for(i = 0; i < a.length; i++)
        a[i] = i;
    assert(a.join(","), "0,1,2,3");
    a[0] = -1;
    assert(a[0], 255);

    a = new Int8Array(3);
    a[0] = 255;
    assert(a[0], -1);

    a = new Int32Array(3);
    a[0] = Math.pow(2, 32) - 1;
    assert(a[0], -1);
    assert(a.BYTES_PER_ELEMENT, 4);

    a = new Uint8ClampedArray(4);
    a[0] = -100;
    a[1] = 1.5;
    a[2] = 0.5;
    a[3] = 1233.5;
    assert(a.toString(), "0,2,0,255");

    buffer = new ArrayBuffer(16);
    assert(buffer.byteLength, 16);
    a = new Uint32Array(buffer, 12, 1);
    assert(a.length, 1);
    a[0] = -1;

    a = new Uint16Array(buffer, 2);
    a[0] = -1;

    a = new Float16Array(buffer, 8, 1);
    a[0] = 1;

    a = new Float32Array(buffer, 8, 1);
    a[0] = 1;

    a = new Uint8Array(buffer);

    str = a.toString();
    /* test little and big endian cases */
    if (str !== "0,0,255,255,0,0,0,0,0,0,128,63,255,255,255,255" &&
        str !== "0,0,255,255,0,0,0,0,63,128,0,0,255,255,255,255") {
        assert(false);
    }

    assert(a.buffer, buffer);

    a = new Uint8Array([1, 2, 3, 4]);
    assert(a.toString(), "1,2,3,4");
    a.set([10, 11], 2);
    assert(a.toString(), "1,2,10,11");

    // https://github.com/quickjs-ng/quickjs/issues/1208
    buffer = new ArrayBuffer(16);
    a = new Uint8Array(buffer);
    a.fill(42);
    assert(a[0], 42);
    buffer.transfer();
    assert(a[0], undefined);
}

function test_typed_array_slice_resize()
{
    const constructors = [
        Uint8ClampedArray, Int8Array, Uint8Array, Int16Array, Uint16Array,
        Int32Array, Uint32Array, BigInt64Array, BigUint64Array,
        Float16Array, Float32Array, Float64Array,
    ];
    for (const C of constructors) {
        const zero = C === BigInt64Array || C === BigUint64Array ? 0n : 0;
        for (const argument of ["start", "end"]) {
            for (let start = 0; start < 4; start++) {
                const buffer = new ArrayBuffer(4 * C.BYTES_PER_ELEMENT,
                                              { maxByteLength: 4 * C.BYTES_PER_ELEMENT });
                const source = new C(buffer);
                source.fill(zero === 0n ? 1n : 1);
                const resize = {
                    valueOf() {
                        buffer.resize(0);
                        return argument === "start" ? start : 4;
                    }
                };
                const result = source.slice(argument === "start" ? resize : start,
                                            argument === "end" ? resize : 4);
                assert(buffer.detached, false);
                assert(buffer.byteLength, 0);
                assert(source.length, 0);
                assert(result.length, 4 - start);
                for (const value of result)
                    assert(value, zero);
            }
        }
    }
}

function test_empty_array_buffer()
{
    for (const resizable of [false, true]) {
        const empty = resizable ? new ArrayBuffer(0, { maxByteLength: 8 }) :
                                  new ArrayBuffer(0);
        assert(empty.slice(0).byteLength, 0);
        assert(empty.detached, false);
        for (const method of ["transfer", "transferToFixedLength"]) {
            for (const length of [0, 1, 8]) {
                const source = resizable ? new ArrayBuffer(0, { maxByteLength: 8 }) :
                                           new ArrayBuffer(0);
                const moved = source[method](length);
                assert(source.detached, true);
                assert(moved.byteLength, length);
                assert([...new Uint8Array(moved)].every(x => x === 0));
            }
        }
    }
    const view = new DataView(new ArrayBuffer(0));
    assert(view.byteLength, 0);
    assert_throws(RangeError, () => view.getUint8(0));
    assert_throws(RangeError, () => view.setUint8(0, 1));
}

function test_empty_typed_array()
{
    const constructors = [
        Uint8ClampedArray, Int8Array, Uint8Array, Int16Array, Uint16Array,
        Int32Array, Uint32Array, BigInt64Array, BigUint64Array,
        Float16Array, Float32Array, Float64Array,
    ];
    for (const C of constructors) {
        const buffer = new ArrayBuffer(0);
        const a = new C(buffer);
        assert(a.length, 0);
        assert(a.buffer, buffer);
        a.set(a);
        a.set(new C(0));
        a.set(new C(new ArrayBuffer(0, { maxByteLength: 8 })));
        const copy = new C(a);
        assert(copy.length, 0);
        assert(copy.buffer !== buffer);
        assert(a.slice(0).length, 0);
        assert(buffer.detached, false);
        assert(new C([]).length, 0);
        assert(new C({ length: 0 }).length, 0);
        assert(C.from([]).length, 0);
        assert(C.of().length, 0);
        assert(a.copyWithin(0, 0), a);
        assert(a.fill(C === BigInt64Array || C === BigUint64Array ? 0n : 0), a);
        assert(a.sort(), a);
        assert(a.sort(() => { throw Error(); }), a);
        assert(a.toSorted().length, 0);
        assert(a.toString(), "");
    }
}

function test_typed_array_resize_bounds()
{
    const constructors = [
        Uint8ClampedArray, Int8Array, Uint8Array, Int16Array, Uint16Array,
        Int32Array, Uint32Array, BigInt64Array, BigUint64Array,
        Float16Array, Float32Array, Float64Array,
    ];
    for (const C of constructors) {
        const size = C.BYTES_PER_ELEMENT;
        const one = C === BigInt64Array || C === BigUint64Array ? 1n : 1;
        const buffer = new ArrayBuffer(4 * size, { maxByteLength: 4 * size });
        const tracking = new C(buffer);
        const offsetTracking = new C(buffer, size);
        const fixedEmpty = new C(buffer, 0, 0);
        const endEmpty = new C(buffer, 4 * size, 0);
        const fixedOne = new C(buffer, size, 1);

        buffer.resize(size);
        assert(offsetTracking.length, 0);
        assert(offsetTracking.byteOffset, size);
        offsetTracking.set(new C(0));
        assert(offsetTracking.fill(one), offsetTracking);
        assert(offsetTracking.slice().length, 0);
        assert_throws(TypeError, () => endEmpty.fill(one));
        assert_throws(TypeError, () => fixedOne.fill(one));

        buffer.resize(size - 1);
        assert(tracking.length, 0);
        assert(offsetTracking.byteOffset, 0);
        assert_throws(TypeError, () => offsetTracking.fill(one));
        for (const view of [tracking, fixedEmpty]) {
            view.set(new C(0));
            assert(view.copyWithin(0, 0), view);
            assert(view.fill(one), view);
            assert(view.sort(), view);
            assert(view.slice().length, 0);
        }
        buffer.resize(0);
        buffer.resize(0);
        assert(tracking.fill(one), tracking);
        assert(fixedEmpty.fill(one), fixedEmpty);

        buffer.resize(4 * size);
        assert(tracking.length, 4);
        assert(offsetTracking.length, 3);
        assert(fixedOne.length, 1);
        tracking.fill(one);
        assert(fixedOne[0], one);
        assert(endEmpty.byteOffset, 4 * size);
        assert(endEmpty.fill(one), endEmpty);
        buffer.transfer();
        for (const view of [tracking, offsetTracking, fixedEmpty, endEmpty, fixedOne])
            assert_throws(TypeError, () => view.fill(one));

        const shared = new SharedArrayBuffer(0, { maxByteLength: 4 * size });
        const sharedTracking = new C(shared);
        shared.grow(size - 1);
        assert(sharedTracking.length, 0);
        assert(sharedTracking.fill(one), sharedTracking);
        shared.grow(4 * size);
        assert(sharedTracking.length, 4);
        sharedTracking.fill(one);
        assert(sharedTracking[3], one);
    }
}

function test_empty_typed_array_transfer()
{
    const constructors = [
        Uint8ClampedArray, Int8Array, Uint8Array, Int16Array, Uint16Array,
        Int32Array, Uint32Array, BigInt64Array, BigUint64Array,
        Float16Array, Float32Array, Float64Array,
    ];
    for (const C of constructors) {
        for (const method of ["transfer", "transferToFixedLength"]) {
            for (const resizable of [false, true]) {
                for (const length of [0, C.BYTES_PER_ELEMENT]) {
                    const buffer = resizable ?
                        new ArrayBuffer(length, { maxByteLength: C.BYTES_PER_ELEMENT }) :
                        new ArrayBuffer(length);
                    const empty = new C(buffer, 0, 0);
                    const tracking = new C(buffer);
                    const transferred = buffer[method]();
                    assert(buffer.detached);
                    assert(empty.length, 0);
                    assert(empty.byteLength, 0);
                    assert(empty.byteOffset, 0);
                    assert(empty.buffer, buffer);
                    assert(tracking.length, 0);
                    assert(transferred.byteLength, length);
                    assert(new C(transferred).length, length / C.BYTES_PER_ELEMENT);
                    assert(transferred.resizable, resizable && method === "transfer");
                    let threw = false;
                    try {
                        empty.set([]);
                    } catch (e) {
                        threw = e instanceof TypeError;
                    }
                    assert(threw);
                }
            }
        }
    }
}

/* return [s, line_num, col_num] where line_num and col_num are the
   position of the '@' character in 'str'. 's' is str without the '@'
   character */
function get_string_pos(str)
{
    var p, line_num, col_num, s, q, r;
    p = str.indexOf('@');
    assert(p >= 0, true);
    q = 0;
    line_num = 1;
    for(;;) {
        r = str.indexOf('\n', q);
        if (r < 0 || r >= p)
            break;
        q = r + 1;
        line_num++;
    }
    col_num = p - q + 1;
    s = str.slice(0, p) + str.slice(p + 1);
    return [s, line_num, col_num];
}

function check_error_pos(e, expected_error, line_num, col_num, level)
{
    var expected_pos, tab, line;
    level |= 0;
    expected_pos = ":" + line_num + ":" + col_num;
    tab = e.stack.split("\n");
    line = tab[level];
    if (line.slice(-1) == ')')
        line = line.slice(0, -1);
    if (line.indexOf(expected_pos) < 0) {
        throw_error("unexpected line or column number. error=" + e.message +
                    ".got |" + line + "|, expected |" + expected_pos + "|");
    }
}

function assert_json_error(str, line_num, col_num)
{
    var err = false;
    var expected_pos, tab;

    tab = get_string_pos(str);
    
    try {
        JSON.parse(tab[0]);
    } catch(e) {
        err = true;
        if (!(e instanceof SyntaxError)) {
            throw_error("unexpected exception type");
            return;
        }
        /* XXX: the way quickjs returns JSON errors is not similar to Node or spiderMonkey */
        check_error_pos(e, SyntaxError, tab[1], tab[2]);
    }
    if (!err) {
        throw_error("expected exception");
    }
}

function test_typed_array_signed_search()
{
    const array = new Int8Array([-128, -7, -1, -7, -128]);
    assert(array.indexOf(-128), 0);
    assert(array.lastIndexOf(-128), 4);
    assert(array.lastIndexOf(-7), 3);
    assert(array.lastIndexOf(-7, 2), 1);
    assert(array.lastIndexOf(-1), 2);
    assert(array.includes(-7), true);
}

function test_error_stack()
{
    function collect_stack_trace(depth) {
        if (depth === 0)
            return new Error("stack allocation");
        return collect_stack_trace(depth - 1);
    }
    const error = collect_stack_trace(32);
    assert(error.message, "stack allocation");
    assert(error.stack.split("collect_stack_trace").length - 1, 33);
}

function test_json()
{
    var a, s;
    s = '{"x":1,"y":true,"z":null,"a":[1,2,3],"s":"str"}';
    a = JSON.parse(s);
    assert(a.x, 1);
    assert(a.y, true);
    assert(a.z, null);
    assert(JSON.stringify(a), s);

    /* indentation test */
    assert(JSON.stringify([[{x:1,y:{},z:[]},2,3]],undefined,1),
`[
 [
  {
   "x": 1,
   "y": {},
   "z": []
  },
  2,
  3
 ]
]`);

    assert_json_error('\n"  \\@x"');
    assert_json_error('\n{ "a": @x }"');
}

function test_date()
{
    // Date Time String format is YYYY-MM-DDTHH:mm:ss.sssZ
    // accepted date formats are: YYYY, YYYY-MM and YYYY-MM-DD
    // accepted time formats are: THH:mm, THH:mm:ss, THH:mm:ss.sss
    // expanded years are represented with 6 digits prefixed by + or -
    // -000000 is invalid.
    // A string containing out-of-bounds or nonconforming elements
    //   is not a valid instance of this format.
    // Hence the fractional part after . should have 3 digits and how
    // a different number of digits is handled is implementation defined.
    assert(Date.parse(""), NaN);
    assert(Date.parse("2000"), 946684800000);
    assert(Date.parse("2000-01"), 946684800000);
    assert(Date.parse("2000-01-01"), 946684800000);
    //assert(Date.parse("2000-01-01T"), NaN);
    //assert(Date.parse("2000-01-01T00Z"), NaN);
    assert(Date.parse("2000-01-01T00:00Z"), 946684800000);
    assert(Date.parse("2000-01-01T00:00:00Z"), 946684800000);
    assert(Date.parse("2000-01-01T00:00:00.1Z"), 946684800100);
    assert(Date.parse("2000-01-01T00:00:00.10Z"), 946684800100);
    assert(Date.parse("2000-01-01T00:00:00.100Z"), 946684800100);
    assert(Date.parse("2000-01-01T00:00:00.1000Z"), 946684800100);
    assert(Date.parse("2000-01-01T00:00:00+00:00"), 946684800000);
    //assert(Date.parse("2000-01-01T00:00:00+00:30"), 946686600000);
    var d = new Date("2000T00:00");  // Jan 1st 2000, 0:00:00 local time
    assert(typeof d === 'object' && d.toString() != 'Invalid Date');
    assert((new Date('Jan 1 2000')).toISOString(),
           d.toISOString());
    assert((new Date('Jan 1 2000 00:00')).toISOString(),
           d.toISOString());
    assert((new Date('Jan 1 2000 00:00:00')).toISOString(),
           d.toISOString());
    assert((new Date('Jan 1 2000 00:00:00 GMT+0100')).toISOString(),
           '1999-12-31T23:00:00.000Z');
    assert((new Date('Jan 1 2000 00:00:00 GMT+0200')).toISOString(),
           '1999-12-31T22:00:00.000Z');
    assert((new Date('Sat Jan 1 2000')).toISOString(),
           d.toISOString());
    assert((new Date('Sat Jan 1 2000 00:00')).toISOString(),
           d.toISOString());
    assert((new Date('Sat Jan 1 2000 00:00:00')).toISOString(),
           d.toISOString());
    assert((new Date('Sat Jan 1 2000 00:00:00 GMT+0100')).toISOString(),
           '1999-12-31T23:00:00.000Z');
    assert((new Date('Sat Jan 1 2000 00:00:00 GMT+0200')).toISOString(),
           '1999-12-31T22:00:00.000Z');

    var d = new Date(1506098258091);
    assert(d.toISOString(), "2017-09-22T16:37:38.091Z");
    d.setUTCHours(18, 10, 11);
    assert(d.toISOString(), "2017-09-22T18:10:11.091Z");
    var a = Date.parse(d.toISOString());
    assert((new Date(a)).toISOString(), d.toISOString());

    assert((new Date("2020-01-01T01:01:01.123Z")).toISOString(),
                     "2020-01-01T01:01:01.123Z");
    /* implementation defined behavior */
    assert((new Date("2020-01-01T01:01:01.1Z")).toISOString(),
                     "2020-01-01T01:01:01.100Z");
    assert((new Date("2020-01-01T01:01:01.12Z")).toISOString(),
                     "2020-01-01T01:01:01.120Z");
    assert((new Date("2020-01-01T01:01:01.1234Z")).toISOString(),
                     "2020-01-01T01:01:01.123Z");
    assert((new Date("2020-01-01T01:01:01.12345Z")).toISOString(),
                     "2020-01-01T01:01:01.123Z");
    assert((new Date("2020-01-01T01:01:01.1235Z")).toISOString(),
                     "2020-01-01T01:01:01.123Z");
    assert((new Date("2020-01-01T01:01:01.9999Z")).toISOString(),
                     "2020-01-01T01:01:01.999Z");

    assert(Date.UTC(2017), 1483228800000);
    assert(Date.UTC(2017, 9), 1506816000000);
    assert(Date.UTC(2017, 9, 22), 1508630400000);
    assert(Date.UTC(2017, 9, 22, 18), 1508695200000);
    assert(Date.UTC(2017, 9, 22, 18, 10), 1508695800000);
    assert(Date.UTC(2017, 9, 22, 18, 10, 11), 1508695811000);
    assert(Date.UTC(2017, 9, 22, 18, 10, 11, 91), 1508695811091);

    assert(Date.UTC(NaN), NaN);
    assert(Date.UTC(2017, NaN), NaN);
    assert(Date.UTC(2017, 9, NaN), NaN);
    assert(Date.UTC(2017, 9, 22, NaN), NaN);
    assert(Date.UTC(2017, 9, 22, 18, NaN), NaN);
    assert(Date.UTC(2017, 9, 22, 18, 10, NaN), NaN);
    assert(Date.UTC(2017, 9, 22, 18, 10, 11, NaN), NaN);
    assert(Date.UTC(2017, 9, 22, 18, 10, 11, 91, NaN), 1508695811091);

    // TODO: Fix rounding errors on Windows/Cygwin.
    if (!(typeof os !== 'undefined' && ['win32', 'cygwin'].includes(os.platform))) {
        // from test262/test/built-ins/Date/UTC/fp-evaluation-order.js
        assert(Date.UTC(1970, 0, 1, 80063993375, 29, 1, -288230376151711740), 29312,
               'order of operations / precision in MakeTime');
        assert(Date.UTC(1970, 0, 213503982336, 0, 0, 0, -18446744073709552000), 34447360,
               'precision in MakeDate');
    }
    //assert(Date.UTC(2017 - 1e9, 9 + 12e9), 1506816000000);  // node fails this
    assert(Date.UTC(2017, 9, 22 - 1e10, 18 + 24e10), 1508695200000);
    assert(Date.UTC(2017, 9, 22, 18 - 1e10, 10 + 60e10), 1508695800000);
    assert(Date.UTC(2017, 9, 22, 18, 10 - 1e10, 11 + 60e10), 1508695811000);
    assert(Date.UTC(2017, 9, 22, 18, 10, 11 - 1e12, 91 + 1000e12), 1508695811091);
}

function test_regexp()
{
    var a, str;
    str = "abbbbbc";
    a = /(b+)c/.exec(str);
    assert(a[0], "bbbbbc");
    assert(a[1], "bbbbb");
    assert(a.index, 1);
    assert(a.input, str);
    a = /(b+)c/.test(str);
    assert(a, true);
    assert(/\x61/.exec("a")[0], "a");
    assert(/\u0061/.exec("a")[0], "a");
    assert(/\ca/.exec("\x01")[0], "\x01");
    assert(/\\a/.exec("\\a")[0], "\\a");
    assert(/\c0/.exec("\\c0")[0], "\\c0");

    a = /(\.(?=com|org)|\/)/.exec("ah.com");
    assert(a.index === 2 && a[0] === ".");

    a = /(\.(?!com|org)|\/)/.exec("ah.com");
    assert(a, null);

    a = /(?=(a+))/.exec("baaabac");
    assert(a.index === 1 && a[0] === "" && a[1] === "aaa");

    a = /(z)((a+)?(b+)?(c))*/.exec("zaacbbbcac");
    assert(a, ["zaacbbbcac","z","ac","a",,"c"]);

    a = eval("/\0a/");
    assert(a.toString(), "/\0a/");
    assert(a.exec("\0a")[0], "\0a");

    assert(/{1a}/.toString(), "/{1a}/");
    a = /a{1+/.exec("a{11");
    assert(a, ["a{11"]);

    /* test zero length matches */
    a = /(?:(?=(abc)))a/.exec("abc");
    assert(a, ["a", "abc"]);
    a = /(?:(?=(abc)))?a/.exec("abc");
    assert(a, ["a", undefined]);
    a = /(?:(?=(abc))){0,2}a/.exec("abc");
    assert(a, ["a", undefined]);
    a = /(?:|[\w])+([0-9])/.exec("123a23");
    assert(a, ["123a23", "3"]);
    a = /()*?a/.exec(",");
    assert(a, null);

    /* test \b escape */
    assert(/[\q{a\b}]/.test("a\b"), true);
    assert(/[\b]/.test("\b"), true);
    
    assert(/A$/i.test("\u0100a"), true);
    assert(/^[a-z]$/iu.test("\u212a"), true);
    assert(/^[a-z]$/iv.test("\u212a"), true);
    assert(/^\u212a$/iu.test("k"), true);
    assert(/^\u007f$/i.test("\u007f"), true);
    assert(/^\u0080$/iu.test("\u0080"), true);

    /* ASCII and Unicode folding differ for Kelvin sign and long s. */
    assert(/^a$/i.test("A"), true);
    assert(/^a$/iu.test("A"), true);
    assert(/^k$/i.test("\u212a"), false);
    assert(/^k$/iu.test("\u212a"), true);
    assert(/^s$/i.test("\u017f"), false);
    assert(/^s$/iu.test("\u017f"), true);
    assert(/^(k)\1$/iu.test("k\u212a"), true);
    assert(/^(k)\1$/i.test("k\u212a"), false);
    assert(/^\u00e9$/i.test("\u00c9"), true);
    assert(/^\u00e9$/iu.test("\u00c9"), true);
    assert(/^\u{10400}$/iu.test("\u{10428}"), true);
    assert(/^ss$/iu.test("\u00df"), false);

    /* test case insensitive matching (test262 hardly tests it) */
    assert("aAbBcC#4".replace(/\p{Lower}/gu,"X"), "XAXBXC#4");

    assert("aAbBcC#4".replace(/\p{Lower}/gui,"X"), "XXXXXX#4");
    assert("aAbBcC#4".replace(/\p{Upper}/gui,"X"), "XXXXXX#4");
    assert("aAbBcC#4".replace(/\P{Lower}/gui,"X"), "XXXXXXXX");
    assert("aAbBcC#4".replace(/\P{Upper}/gui,"X"), "XXXXXXXX");
    assert("aAbBcC".replace(/[^b]/gui, "X"), "XXbBXX");
    assert("aAbBcC".replace(/[^A-B]/gui, "X"), "aAbBXX");

    assert("aAbBcC#4".replace(/\p{Lower}/gvi,"X"), "XXXXXX#4");
    assert("aAbBcC#4".replace(/\P{Lower}/gvi,"X"), "aAbBcCXX");
    assert("aAbBcC#4".replace(/[^\P{Lower}]/gvi,"X"), "XXXXXX#4");
    assert("aAbBcC#4".replace(/\P{Upper}/gvi,"X"), "aAbBcCXX");
    assert("aAbBcC".replace(/[^b]/gvi, "X"), "XXbBXX");
    assert("aAbBcC".replace(/[^A-B]/gvi, "X"), "aAbBXX");
    assert("aAbBcC".replace(/[[a-c]&&B]/gvi, "X"), "aAXXcC");
    assert("aAbBcC".replace(/[[a-c]--B]/gvi, "X"), "XXbBXX");
    
    assert("abcAbC".replace(/[\q{AbC}]/gvi,"X"), "XX");
    /* Note: SpiderMonkey and v8 may not be correct */
    assert("abcAbC".replace(/[\q{BC|A}]/gvi,"X"), "XXXX");
    assert("abcAbC".replace(/[\q{BC|A}--a]/gvi,"X"), "aXAX");

    /* case where lastIndex points to the second element of a
       surrogate pair */
    a = /(?:)/gu;
    a.lastIndex = 1;
    a.exec("🐱");
    assert(a.lastIndex, 0);

    a.lastIndex = 1;
    a.exec("a\udc00");
    assert(a.lastIndex, 1);

    a = /\u{10000}/vgd;
    a.lastIndex = 1;
    a = a.exec("\u{10000}_\u{10000}");
    assert(a.indices[0][0], 0);
    assert(a.indices[0][1], 2);

    /* Empty string alternatives must not copy or compare a NULL buffer. */
    assert(/[\q{}]/v.exec("")[0], "");
    assert(/[\q{|}]/v.exec("")[0], "");
    assert(/[\q{|a}]/v.test("a"));

    /* syntax check */
    assert(/-&&/v.test("-&&"), true);
}

function test_symbol()
{
    var a, b, obj, c;
    a = Symbol("abc");
    obj = {};
    obj[a] = 2;
    assert(obj[a], 2);
    assert(typeof obj["abc"], "undefined");
    assert(String(a), "Symbol(abc)");
    b = Symbol("abc");
    assert(a == a);
    assert(a === a);
    assert(a != b);
    assert(a !== b);

    b = Symbol.for("abc");
    c = Symbol.for("abc");
    assert(b === c);
    assert(b !== a);

    assert(Symbol.keyFor(b), "abc");
    assert(Symbol.keyFor(a), undefined);

    a = Symbol("aaa");
    assert(a.valueOf(), a);
    assert(a.toString(), "Symbol(aaa)");

    b = Object(a);
    assert(b.valueOf(), a);
    assert(b.toString(), "Symbol(aaa)");
}

function test_map1(key_type, n)
{
    var a, i, tab, o, v;
    a = new Map();
    tab = [];
    for(i = 0; i < n; i++) {
        v = { };
        switch(key_type) {
        case "small_bigint":
            o = BigInt(i);
            break;
        case "bigint":
            o = BigInt(i) + (1n << 128n);
            break;
        case "object":
            o = { id: i };
            break;
        default:
            assert(false);
        }
        tab[i] = [o, v];
        a.set(o, v);
    }

    assert(a.size, n);
    for(i = 0; i < n; i++) {
        assert(a.get(tab[i][0]), tab[i][1]);
    }

    i = 0;
    a.forEach(function (v, o) {
        assert(o, tab[i++][0]);
        assert(a.has(o));
        assert(a.delete(o));
        assert(!a.has(o));
    });

    assert(a.size, 0);
}

function test_map()
{
    var a, i, n, tab, o, v;
    n = 1000;

    a = new Map();
    for (var i = 0; i < n; i++) {
        a.set(i, i);
    }
    a.set(-2147483648, 1);
    assert(a.get(-2147483648), 1);
    assert(a.get(-2147483647 - 1), 1);
    assert(a.get(-2147483647.5 - 0.5), 1);

    a.set(1n, 1n);
    assert(a.get(1n), 1n);
    assert(a.get(2n**1000n - (2n**1000n - 1n)), 1n);

    test_map1("object", n);
    test_map1("small_bigint", n);
    test_map1("bigint", n);
}

function test_map_computed_reentrancy()
{
    const map = new Map([["first", 0]]);
    const iterator = map.keys();
    assert(iterator.next().value, "first");
    assert(map.getOrInsertComputed("key", function(key) {
        assert(arguments.length, 1);
        assert(key, "key");
        assert(this, undefined);
        map.set(key, 1);
        map.set("last", 2);
        return 3;
    }), 3);
    assert(map.get("key"), 3);
    assert([...map.keys()].join(","), "first,key,last");
    assert(iterator.next().value, "key");
    assert(iterator.next().value, "last");
    assert(iterator.next().done, true);
    assert(map.getOrInsertComputed("key", () => {
        throw Error("existing key callback");
    }), 3);

    for (const action of ["clear", "delete", "reinsert"]) {
        const current = new Map([["first", 0]]);
        assert(current.getOrInsertComputed("key", key => {
            current.set(key, 1);
            if (action === "clear")
                current.clear();
            else
                current.delete(key);
            current.set("last", 2);
            if (action === "reinsert")
                current.set(key, 4);
            return 3;
        }), 3);
        assert(current.get("key"), 3);
        assert([...current.keys()].join(","),
               action === "clear" ? "last,key" : "first,last,key");
    }

    for (const key of [NaN, -0]) {
        const current = new Map();
        assert(current.getOrInsertComputed(key, normalized => {
            if (Object.is(key, -0))
                assert(Object.is(normalized, 0), true);
            else
                assert(Number.isNaN(normalized), true);
            current.set(normalized, 1);
            current.set("last", 2);
            return 3;
        }), 3);
        assert(current.size, 2);
        assert(current.get(key), 3);
        assert([...current.values()].join(","), "3,2");
    }

    const marker = {};
    let caught;
    try {
        map.getOrInsertComputed("throw", key => {
            map.set(key, 4);
            throw marker;
        });
    } catch (error) {
        caught = error;
    }
    assert(caught === marker, true);
    assert(map.get("throw"), 4);

    for (const key of [{}, Symbol("key")]) {
        const weak = new WeakMap();
        assert(weak.getOrInsertComputed(key, key => {
            weak.set(key, 1);
            return 3;
        }), 3);
        assert(weak.get(key), 3);
    }
}

function test_set_record()
{
    const methods = ["difference", "intersection", "isDisjointFrom",
                     "isSubsetOf", "isSupersetOf", "symmetricDifference", "union"];
    const marker = {};
    for (const method of methods) {
        const other = new Set([1]);
        let reads = 0;
        Object.defineProperty(other, "size", {
            get() {
                reads++;
                throw marker;
            }
        });
        let caught;
        try {
            new Set([1])[method](other);
        } catch (error) {
            caught = error;
        }
        assert(caught === marker, true);
        assert(reads, 1);
    }

    for (const method of methods) {
        const events = [];
        const other = {
            get size() {
                events.push("size");
                return { valueOf() { events.push("number"); return 0; } };
            },
            get has() {
                events.push("has");
                return () => false;
            },
            get keys() {
                events.push("keys");
                return () => [][Symbol.iterator]();
            }
        };
        new Set()[method](other);
        assert(events.join(","), "size,number,has,keys");
    }

    const record = { size: -0.5, has() { return false; },
                     keys() { return [][Symbol.iterator](); } };
    assert(new Set().union(record).size, 0);
    record.size = Infinity;
    assert(new Set().union(record).size, 0);
    record.size = NaN;
    assert_throws(TypeError, () => new Set().union(record));
    record.size = -1;
    assert_throws(RangeError, () => new Set().union(record));

    const saved = Object.getOwnPropertyDescriptor(Number.prototype, "size");
    let reads = 0;
    try {
        Object.defineProperty(Number.prototype, "size", {
            configurable: true,
            get() { reads++; return 0; }
        });
        for (const method of methods)
            assert_throws(TypeError, () => new Set()[method](1));
        assert(reads, 0);
    } finally {
        if (saved)
            Object.defineProperty(Number.prototype, "size", saved);
        else
            delete Number.prototype.size;
    }
}

function test_set_iterator_close()
{
    for (const method of ["isDisjointFrom", "isSupersetOf"]) {
        const receiver = method === "isDisjointFrom" ? new Set([1, 2]) : new Set([1]);
        const yielded = method === "isDisjointFrom" ? 1 : 2;
        const marker = {};
        for (const kind of ["get", "call", "primitive", "noncallable",
                            "object", "null", "undefined"]) {
            let reads = 0, calls = 0;
            const iterator = {
                next() { return { done: false, value: yielded }; },
                get return() {
                    reads++;
                    if (kind === "get")
                        throw marker;
                    if (kind === "noncallable")
                        return 1;
                    if (kind === "null")
                        return null;
                    if (kind === "undefined")
                        return undefined;
                    return function() {
                        calls++;
                        assert(this === iterator, true);
                        assert(arguments.length, 0);
                        if (kind === "call")
                            throw marker;
                        if (kind === "primitive")
                            return 1;
                        return {};
                    };
                }
            };
            const other = { size: 1, has() { return false; },
                            keys() { return iterator; } };
            if (kind === "get" || kind === "call") {
                let caught;
                try {
                    receiver[method](other);
                } catch (error) {
                    caught = error;
                }
                assert(caught === marker, true);
            } else if (kind === "primitive" || kind === "noncallable") {
                assert_throws(TypeError, () => receiver[method](other));
            } else {
                assert(receiver[method](other), false);
            }
            assert(reads, 1);
            assert(calls, kind === "call" || kind === "primitive" || kind === "object" ? 1 : 0);
        }

        let closes = 0;
        const other = {
            size: 1,
            has() { return false; },
            keys() {
                return {
                    next() { return { done: true }; },
                    return() { closes++; return {}; }
                };
            }
        };
        assert(receiver[method](other), true);
        assert(closes, 0);
    }
}

function test_set_iterator_factory()
{
    const methods = ["difference", "intersection", "isDisjointFrom",
                     "isSupersetOf", "symmetricDifference", "union"];
    const saved = Object.getOwnPropertyDescriptor(Number.prototype, "next");
    let reads = 0;
    try {
        Object.defineProperty(Number.prototype, "next", {
            configurable: true,
            get() { reads++; return () => ({ done: true }); }
        });
        for (const method of methods) {
            for (const result of [undefined, null, false, 1, "x", Symbol("x"), 1n]) {
                let calls = 0;
                const other = {
                    size: 0,
                    has() { return false; },
                    keys() {
                        assert(this === other, true);
                        assert(arguments.length, 0);
                        calls++;
                        return result;
                    }
                };
                assert_throws(TypeError, () => new Set([1, 2])[method](other));
                assert(calls, 1);
            }
        }
        assert(reads, 0);
    } finally {
        if (saved)
            Object.defineProperty(Number.prototype, "next", saved);
        else
            delete Number.prototype.next;
    }

    const marker = {};
    for (const method of methods) {
        let closes = 0;
        const other = {
            size: 0,
            has() { return false; },
            keys() {
                return {
                    get next() { throw marker; },
                    return() { closes++; return {}; }
                };
            }
        };
        let caught;
        try {
            new Set([1, 2])[method](other);
        } catch (error) {
            caught = error;
        }
        assert(caught === marker, true);
        assert(closes, 0);
    }
}

function test_iterator_wrapper()
{
    for (const value of [undefined, null, false, 0, "text", Symbol("value"), 1n, {}]) {
        const iterator = {
            next() { assert(this === iterator, true); assert(arguments.length, 0); return value; },
            return() { assert(this === iterator, true); assert(arguments.length, 0); return value; }
        };
        const wrapper = Iterator.from(iterator);
        assert(wrapper.next(1) === value, true);
        assert(wrapper.return(1) === value, true);
    }

    let reads = 0;
    const result = {
        get done() { reads++; throw Error("done read"); },
        get value() { reads++; throw Error("value read"); }
    };
    const iterator = { next() { return result; }, return() { return result; } };
    const wrapper = Iterator.from(iterator);
    assert(wrapper.next() === result, true);
    assert(wrapper.return() === result, true);
    assert(reads, 0);
    iterator.next = () => { throw Error("replacement next"); };
    assert(wrapper.next() === result, true);

    let returnReads = 0;
    Object.defineProperty(iterator, "return", {
        get() { returnReads++; return () => returnReads; }
    });
    assert(wrapper.return(), 1);
    assert(wrapper.return(), 2);
    assert(returnReads, 2);

    for (const method of [undefined, null]) {
        const current = Iterator.from({ next() { return result; }, return: method });
        const first = current.return();
        const second = current.return();
        assert(first === second, false);
        assert(first.done, true);
        assert(first.value, undefined);
        assert(Object.getPrototypeOf(first) === Object.prototype, true);
        assert(current.next() === result, true);
    }

    const marker = {};
    let closes = 0;
    const failed = Iterator.from({
        next() { throw marker; },
        return() { closes++; return {}; }
    });
    let caught;
    try { failed.next(); } catch (error) { caught = error; }
    assert(caught === marker, true);
    assert(closes, 0);
    assert_throws(TypeError, () => wrapper.next.call({}));
    assert_throws(TypeError, () => wrapper.return.call({}));
    assert_throws(TypeError, () => Iterator.from({ next() {}, return: 1 }).return());
    assert_throws(TypeError, () => Iterator.from({ next: 1 }).next());

    const iteratorDescriptor = Object.getOwnPropertyDescriptor(String.prototype, Symbol.iterator);
    const nextDescriptor = Object.getOwnPropertyDescriptor(String.prototype, "next");
    let nextReads = 0;
    try {
        Object.defineProperty(String.prototype, Symbol.iterator, { value: undefined });
        Object.defineProperty(String.prototype, "next", {
            configurable: true,
            get() { nextReads++; return () => ({ done: true }); }
        });
        assert_throws(TypeError, () => Iterator.from("text"));
        assert(nextReads, 0);
    } finally {
        Object.defineProperty(String.prototype, Symbol.iterator, iteratorDescriptor);
        if (nextDescriptor)
            Object.defineProperty(String.prototype, "next", nextDescriptor);
        else
            delete String.prototype.next;
    }
}

function test_iterator_accessors()
{
    const home = Iterator.prototype;
    const constructor = Object.getOwnPropertyDescriptor(home, "constructor");
    assert(constructor.enumerable, false);
    assert(constructor.configurable, true);
    assert(constructor.get === constructor.set, false);
    assert(constructor.get.name, "get constructor");
    assert(constructor.set.name, "set constructor");
    assert(constructor.get.length, 0);
    assert(constructor.set.length, 1);
    assert(constructor.get.call(null, 1) === Iterator, true);
    assert_throws(TypeError, () => new constructor.get());
    assert_throws(TypeError, () => new constructor.set());

    for (const key of ["constructor", Symbol.toStringTag]) {
        const setter = Object.getOwnPropertyDescriptor(home, key).set;
        for (const receiver of [undefined, null, false, 1, "text", Symbol("x")])
            assert_throws(TypeError, () => setter.call(receiver, {}));
        assert_throws(TypeError, () => setter.call(home, {}));
        for (const value of [undefined, null, false, 1, "text", Symbol("x"), {}]) {
            const receiver = Object.create(home);
            assert(setter.call(receiver, value), undefined);
            const descriptor = Object.getOwnPropertyDescriptor(receiver, key);
            assert(descriptor.value === value, true);
            assert(descriptor.writable, true);
            assert(descriptor.enumerable, true);
            assert(descriptor.configurable, true);
        }
        const missing = Object.create(home);
        setter.call(missing);
        assert(Object.hasOwn(missing, key), true);
        assert(missing[key], undefined);
        assert_throws(TypeError, () => setter.call(Object.preventExtensions({}), 1));

        const readOnly = {};
        Object.defineProperty(readOnly, key, { value: 0, configurable: true });
        assert_throws(TypeError, () => setter.call(readOnly, 1));
        const writable = {};
        Object.defineProperty(writable, key, { value: 0, writable: true });
        setter.call(writable, 1);
        const descriptor = Object.getOwnPropertyDescriptor(writable, key);
        assert(descriptor.value, 1);
        assert(descriptor.enumerable, false);
        assert(descriptor.configurable, false);

        let calls = 0;
        const accessor = {};
        Object.defineProperty(accessor, key, {
            get() { throw Error("getter must not run"); },
            set(value) { assert(this === accessor, true); assert(value, 1); calls++; }
        });
        setter.call(accessor, 1);
        assert(calls, 1);
        const getterOnly = {};
        Object.defineProperty(getterOnly, key, { get() { return 0; }, configurable: true });
        assert_throws(TypeError, () => setter.call(getterOnly, 1));

        const events = [];
        const target = {};
        const proxy = new Proxy(target, {
            getOwnPropertyDescriptor(target, property) {
                assert(property, key);
                events.push("descriptor");
                return Reflect.getOwnPropertyDescriptor(target, property);
            },
            defineProperty(target, property, descriptor) {
                events.push("define");
                return Reflect.defineProperty(target, property, descriptor);
            },
            set(target, property, value) {
                assert(property, key);
                assert(value, 2);
                events.push("set");
                return true;
            }
        });
        setter.call(proxy, 1);
        assert(events.join(","), "descriptor,define");
        events.length = 0;
        setter.call(proxy, 2);
        assert(events.join(","), "descriptor,set");
        assert_throws(TypeError, () => setter.call(new Proxy({}, {
            defineProperty() { return false; }
        }), 1));
        assert_throws(TypeError, () => setter.call(new Proxy({ [key]: 0 }, {
            set() { return false; }
        }), 1));
    }
}

function test_iterator_helper_completion()
{
    const makeHelper = [source => source.drop(0), source => source.take(1),
                        source => source.filter(() => true),
                        source => source.flatMap(value => [value]),
                        source => source.map(value => value)];
    const marker = {};
    for (const make of makeHelper) {
        for (const kind of ["next", "primitive", "done", "value"]) {
            let calls = 0, closes = 0;
            const source = Object.assign(Object.create(Iterator.prototype), {
                next() {
                    calls++;
                    if (kind === "next")
                        throw marker;
                    if (kind === "primitive")
                        return 1;
                    return {
                        get done() { if (kind === "done") throw marker; return false; },
                        get value() { throw marker; }
                    };
                },
                return() { closes++; return {}; }
            });
            const helper = make(source);
            let caught;
            try { helper.next(); } catch (error) { caught = error; }
            if (kind === "primitive")
                assert(caught instanceof TypeError, true);
            else
                assert(caught === marker, true);
            assert(helper.next().done, true);
            assert(helper.return().done, true);
            assert(calls, 1);
            assert(closes, 0);
        }
    }

    for (const method of ["map", "filter", "flatMap"]) {
        let calls = 0, closes = 0;
        const source = Object.assign(Object.create(Iterator.prototype), {
            next() { calls++; return { done: false, value: 1 }; },
            return() { closes++; throw {}; }
        });
        const helper = source[method](() => { throw marker; });
        let caught;
        try { helper.next(); } catch (error) { caught = error; }
        assert(caught === marker, true);
        assert(helper.next().done, true);
        assert(helper.return().done, true);
        assert(calls, 1);
        assert(closes, 1);
    }

    let calls = 0, helper;
    const source = Object.assign(Object.create(Iterator.prototype), {
        next() { return { done: calls++ > 0, value: 1 }; }
    });
    helper = source.map(value => {
        assert_throws(TypeError, () => helper.next());
        return value;
    });
    assert(helper.next().value, 1);
    assert(helper.next().done, true);
}

function test_iterator_helper_start_return()
{
    const makeHelper = [source => source.drop(0), source => source.take(1),
                        source => source.filter(() => true),
                        source => source.flatMap(value => [value]),
                        source => source.map(value => value)];
    for (const make of makeHelper) {
        let helper, nexts = 0, reads = 0, closes = 0;
        const source = Object.assign(Object.create(Iterator.prototype), {
            next() { nexts++; return { done: false, value: 1 }; }
        });
        Object.defineProperty(source, "return", {
            get() {
                reads++;
                assert(helper.next().done, true);
                assert(helper.return().done, true);
                return function() {
                    closes++;
                    assert(helper.next().done, true);
                    assert(helper.return().done, true);
                    return {};
                };
            }
        });
        helper = make(source);
        assert(helper.return().done, true);
        assert(helper.next().done, true);
        assert(nexts, 0);
        assert(reads, 1);
        assert(closes, 1);
    }

    for (const kind of ["get", "call"]) {
        const marker = {};
        const source = Object.assign(Object.create(Iterator.prototype), {
            next() { throw Error("must not call next"); }
        });
        Object.defineProperty(source, "return", {
            get() {
                if (kind === "get")
                    throw marker;
                return () => { throw marker; };
            }
        });
        const helper = source.map(value => value);
        let caught;
        try { helper.return(); } catch (error) { caught = error; }
        assert(caught === marker, true);
        assert(helper.next().done, true);
        assert(helper.return().done, true);
    }

    let helper, closes = 0;
    const source = Object.assign(Object.create(Iterator.prototype), {
        next() { return { done: false, value: 1 }; },
        return() {
            closes++;
            assert_throws(TypeError, () => helper.next());
            assert_throws(TypeError, () => helper.return());
            return {};
        }
    });
    helper = source.map(value => value);
    assert(helper.next().value, 1);
    assert(helper.return().done, true);
    assert(closes, 1);
}

function test_iterator_flatmap_close()
{
    let innerCloses = 0, outerCloses = 0;
    const source = Object.assign(Object.create(Iterator.prototype), {
        index: 0,
        next() { return { done: this.index++ > 1, value: this.index }; },
        return() { outerCloses++; return {}; }
    });
    const helper = source.flatMap(value => ({
        next() { return { done: true }; },
        get return() { innerCloses++; throw Error("must not close exhausted inner"); }
    }));
    assert(helper.next().done, true);
    assert(innerCloses, 0);
    assert(outerCloses, 0);

    const marker = {};
    for (const kind of ["get", "call", "primitive", "done", "value"]) {
        let outerNexts = 0, innerNexts = 0;
        innerCloses = outerCloses = 0;
        const outer = Object.assign(Object.create(Iterator.prototype), {
            next() { outerNexts++; return { done: false, value: 1 }; },
            return() { outerCloses++; throw {}; }
        });
        const inner = {
            get next() {
                if (kind === "get")
                    throw marker;
                return function() {
                    innerNexts++;
                    if (kind === "call")
                        throw marker;
                    if (kind === "primitive")
                        return 1;
                    return {
                        get done() { if (kind === "done") throw marker; return false; },
                        get value() { throw marker; }
                    };
                };
            },
            get return() { innerCloses++; throw Error("must not close failed inner"); }
        };
        const current = outer.flatMap(() => inner);
        let caught;
        try { current.next(); } catch (error) { caught = error; }
        if (kind === "primitive")
            assert(caught instanceof TypeError, true);
        else
            assert(caught === marker, true);
        assert(current.next().done, true);
        assert(current.return().done, true);
        assert(outerNexts, 1);
        assert(innerNexts, kind === "get" ? 0 : 1);
        assert(innerCloses, 0);
        assert(outerCloses, 1);
    }

    const events = [];
    const outer = Object.assign(Object.create(Iterator.prototype), {
        next() { return { done: false, value: 1 }; },
        return() { events.push("outer"); return {}; }
    });
    const current = outer.flatMap(() => ({
        next() { return { done: false, value: 2 }; },
        return() { events.push("inner"); return {}; }
    }));
    assert(current.next().value, 2);
    assert(current.return().done, true);
    assert(events.join(","), "inner,outer");
}

function test_iterator_helper_acquisition()
{
    const marker = {};
    for (const [method, argument] of [["drop", 0], ["take", 1],
                                     ["map", value => value], ["filter", () => true],
                                     ["flatMap", value => [value]]]) {
        let closes = 0;
        const source = Object.create(Iterator.prototype);
        Object.defineProperties(source, {
            next: { get() { throw marker; } },
            return: { get() { closes++; throw {}; } }
        });
        let caught;
        try { source[method](argument); } catch (error) { caught = error; }
        assert(caught === marker, true);
        assert(closes, 0);
    }

    for (const method of ["map", "filter", "flatMap", "take", "drop"]) {
        let closes = 0, reads = 0;
        const source = Object.create(Iterator.prototype);
        Object.defineProperties(source, {
            next: { get() { reads++; throw marker; } },
            return: { value() { closes++; throw {}; } }
        });
        if (method === "take" || method === "drop") {
            let caught;
            try { source[method]({ valueOf() { throw marker; } }); }
            catch (error) { caught = error; }
            assert(caught === marker, true);
        } else {
            assert_throws(TypeError, () => source[method](undefined));
        }
        assert(closes, 1);
        assert(reads, 0);
    }
}

function test_iterator_reduce_close()
{
    const marker = {};
    let closes = 0;
    const source = Object.create(Iterator.prototype);
    Object.defineProperties(source, {
        next: { get() { throw marker; } },
        return: { get() { closes++; throw {}; } }
    });
    let caught;
    try { source.reduce((a, b) => a + b); } catch (error) { caught = error; }
    assert(caught === marker, true);
    assert(closes, 0);

    let nexts = 0;
    const empty = Object.assign(Object.create(Iterator.prototype), {
        next() { nexts++; return { done: true }; },
        return() { closes++; throw {}; }
    });
    assert_throws(TypeError, () => empty.reduce((a, b) => a + b));
    assert(empty.reduce((a, b) => a + b, 42), 42);
    assert(nexts, 2);
    assert(closes, 0);

    const abrupt = Object.assign(Object.create(Iterator.prototype), {
        next() { throw marker; },
        return() { closes++; throw {}; }
    });
    caught = undefined;
    try { abrupt.reduce((a, b) => a + b, 0); } catch (error) { caught = error; }
    assert(caught === marker, true);
    assert(closes, 0);

    const callback = Object.assign(Object.create(Iterator.prototype), {
        next() { return { done: false, value: 1 }; },
        return() { closes++; throw {}; }
    });
    caught = undefined;
    try { callback.reduce(() => { throw marker; }, 0); }
    catch (error) { caught = error; }
    assert(caught === marker, true);
    assert(closes, 1);
    assert_throws(TypeError, () => callback.reduce(undefined));
    assert(closes, 2);
}

function test_iterator_constructor_identity()
{
    assert_throws(TypeError, () => Iterator());
    assert_throws(TypeError, () => new Iterator());
    assert_throws(TypeError, () => Reflect.construct(Iterator, [], Iterator));
    class Derived extends Iterator {}
    assert(Object.getPrototypeOf(new Derived()), Derived.prototype);
    const target = new Proxy(Iterator, {});
    const value = Reflect.construct(Iterator, [], target);
    assert(Object.getPrototypeOf(value), Iterator.prototype);
}

function test_iterator_concat_return()
{
    function completed(result) {
        assert(result.done === true && result.value === undefined);
    }
    for (const method of [undefined, null, () => ({ done: false, value: 42 })]) {
        const inner = { next() { return { value: 1, done: false }; }, return: method };
        const iterable = { [Symbol.iterator]() { return inner; } };
        const concat = Iterator.concat(iterable);
        assert(concat.next().value, 1);
        completed(concat.return(99));
        completed(concat.next());
        completed(concat.return());
    }
    for (const kind of ["getter", "call", "primitive", "noncallable"]) {
        let calls = 0;
        const error = Error(kind);
        const inner = { next() { return { value: 1, done: false }; } };
        Object.defineProperty(inner, "return", { get() {
            calls++;
            if (kind === "getter") throw error;
            if (kind === "noncallable") return 1;
            return () => { if (kind === "call") throw error; return 1; };
        }});
        const concat = Iterator.concat({ [Symbol.iterator]() { return inner; } });
        concat.next();
        let actual;
        try { concat.return(); } catch (e) { actual = e; }
        assert(kind === "getter" || kind === "call" ? actual === error
                                                     : actual instanceof TypeError);
        completed(concat.next());
        completed(concat.return());
        assert(calls, 1);
    }
    let opens = 0;
    const unopened = Iterator.concat({ [Symbol.iterator]() { opens++; return {}; } });
    completed(unopened.return());
    completed(unopened.next());
    assert(opens, 0);
    const first = Iterator.concat();
    const a = first.return(), b = first.return();
    completed(a); completed(b); assert(a !== b);
}

function test_iterator_concat_completion()
{
    for (const kind of ["iterator", "next-get", "next-call", "primitive", "done", "value"]) {
        const error = Error(kind);
        let opens = 0, closes = 0;
        const inner = {
            get next() {
                if (kind === "next-get") throw error;
                return () => {
                    if (kind === "next-call") throw error;
                    if (kind === "primitive") return 1;
                    return {
                        get done() { if (kind === "done") throw error; return false; },
                        get value() { if (kind === "value") throw error; return 42; },
                    };
                };
            },
            return() { closes++; return {}; },
        };
        const source = { [Symbol.iterator]() {
            opens++;
            if (kind === "iterator") throw error;
            return inner;
        }};
        const concat = Iterator.concat(source, source);
        let actual;
        try { concat.next(); } catch (e) { actual = e; }
        assert(kind === "primitive" ? actual instanceof TypeError : actual === error);
        const next = concat.next(), result = concat.return();
        assert(next.done === true && next.value === undefined);
        assert(result.done === true && result.value === undefined);
        assert(opens, 1);
        assert(closes, 0);
    }
}

function test_iterator_concat_prototype()
{
    const helper = [1].values().map(x => x);
    const concat = Iterator.concat([2]);
    assert(Object.getPrototypeOf(concat) === Object.getPrototypeOf(helper));
    assert(concat.next === helper.next && concat.return === helper.return);
    assert(Object.prototype.toString.call(concat), "[object Iterator Helper]");
    assert(helper.next.call(concat).value, 2);
    assert(concat.next.call(helper).value, 1);
    const result = helper.return.call(concat);
    assert(result.done === true && result.value === undefined);
    assert_throws(TypeError, () => helper.next.call({}));
    assert_throws(TypeError, () => helper.return.call({}));
}

function test_iterator_limits()
{
    for (const limit of [Number.MAX_SAFE_INTEGER, Infinity]) {
        assert([1, 2, 3].values().take(limit).toArray().join(","), "1,2,3");
        assert([1, 2, 3].values().drop(limit).next().done, true);
    }
    assert([1, 2, 3].values().take(2.9).toArray().join(","), "1,2");
    assert([1, 2, 3].values().drop(2.9).toArray().join(","), "3");
    for (const limit of [0, -0, -0.5, -Number.MIN_VALUE, null, false]) {
        assert([1, 2, 3].values().take(limit).next().done, true);
        assert([1, 2, 3].values().drop(limit).next().value, 1);
    }
    assert([1, 2, 3].values().take("2.9").toArray().join(","), "1,2");
    assert([1, 2, 3].values().drop("2.9").toArray().join(","), "3");

    const marker = {};
    for (const method of ["take", "drop"]) {
        for (const limit of [NaN, -1, -Infinity, Number.MAX_SAFE_INTEGER + 1,
                             2 ** 63, 2 ** 64, Number.MAX_VALUE]) {
            for (const close of ["object", "primitive", "throw"]) {
                const events = [];
                const source = Object.create(Iterator.prototype);
                Object.defineProperties(source, {
                    next: { get() { events.push("next"); throw marker; } },
                    return: { get() {
                        events.push("return-get");
                        return function () {
                            assert(this === source, true);
                            assert(arguments.length, 0);
                            events.push("return-call");
                            if (close === "throw") throw marker;
                            return close === "object" ? {} : 1;
                        };
                    } }
                });
                assert_throws(RangeError, () => source[method]({
                    valueOf() { events.push("number"); return limit; }
                }));
                assert(events.join(","), "number,return-get,return-call");
            }
        }
        for (const limit of [1n, Symbol(), "9007199254740992"]) {
            let reads = 0, closes = 0;
            const source = Object.create(Iterator.prototype);
            Object.defineProperties(source, {
                next: { get() { reads++; throw marker; } },
                return: { value() { closes++; return {}; } }
            });
            assert_throws(typeof limit === "string" ? RangeError : TypeError,
                          () => source[method](limit));
            assert(reads, 0);
            assert(closes, 1);
        }
        const events = [];
        const source = Object.create(Iterator.prototype);
        Object.defineProperties(source, {
            next: { get() { events.push("next"); throw marker; } },
            return: { get() { events.push("return-get"); throw {}; } }
        });
        let caught;
        try { source[method]({ valueOf() {
            events.push("number"); throw marker;
        } }); } catch (error) { caught = error; }
        assert(caught === marker, true);
        assert(events.join(","), "number,return-get");

        events.length = 0;
        assert_throws(TypeError, () => Iterator.prototype[method].call(0, {
            valueOf() { events.push("number"); return 1; }
        }));
        assert(events.length, 0);

        for (const limit of [Number.MAX_SAFE_INTEGER, Infinity]) {
            events.length = 0;
            let nexts = 0;
            const valid = Object.create(Iterator.prototype);
            Object.defineProperties(valid, {
                next: { get() {
                    events.push("next-get");
                    return function () {
                        nexts++;
                        return nexts <= 3 ? { value: nexts, done: false } :
                                          { done: true };
                    };
                } },
                return: { value() { events.push("return"); return {}; } }
            });
            const helper = valid[method]({ valueOf() {
                events.push("number"); return limit;
            } });
            assert(events.join(","), "number,next-get");
            assert(nexts, 0);
            const result = helper.next();
            if (method === "take") {
                assert(result.value, 1);
                assert(result.done, false);
                assert(helper.return().done, true);
                assert(events.join(","), "number,next-get,return");
                assert(nexts, 1);
            } else {
                assert(result.done, true);
                assert(helper.return().done, true);
                assert(events.join(","), "number,next-get");
                assert(nexts, 4);
            }
        }
    }
}

function test_weak_map()
{
    var a, i, n, tab, o, v, n2;
    a = new WeakMap();
    n = 10;
    tab = [];
    for(i = 0; i < n; i++) {
        v = { };
        if (i & 1)
            o = Symbol("x" + i);
        else
            o = { id: i };
        tab[i] = [o, v];
        a.set(o, v);
    }
    o = null;

    n2 = 5;
    for(i = 0; i < n2; i++) {
        a.delete(tab[i][0]);
    }
    for(i = n2; i < n; i++) {
        tab[i][0] = null; /* should remove the object from the WeakMap too */
    }
    std.gc();
    /* the WeakMap should be empty here */
}

function test_weak_map_cycles()
{
    const weak1 = new WeakMap();
    const weak2 = new WeakMap();
    function createCyclicKey() {
        const parent = {};
        const child = {parent};
        parent.child = child;
        return child;
    }
    function testWeakMap() {
        const cyclicKey = createCyclicKey();
        const valueOfCyclicKey = {};
        weak1.set(cyclicKey, valueOfCyclicKey);
        weak2.set(valueOfCyclicKey, 1);
    }
    testWeakMap();
    // Force to free cyclicKey.
    std.gc();
    // Here will cause sigsegv because [cyclicKey] and [valueOfCyclicKey] in [weak1] was free,
    // but weak2's map record was not removed, and it's key refers [valueOfCyclicKey] which is free.
    weak2.get({});
    std.gc();
}

function test_weak_ref()
{
    var w1, w2, o, i;

    for(i = 0; i < 2; i++) {
        if (i == 0)
            o = { };
        else
            o = Symbol("x");
        w1 = new WeakRef(o);
        assert(w1.deref(), o);
        w2 = new WeakRef(o);
        assert(w2.deref(), o);
        
        o = null;
        assert(w1.deref(), undefined);
        assert(w2.deref(), undefined);
        std.gc();
        assert(w1.deref(), undefined);
        assert(w2.deref(), undefined);
    }
}

function test_finalization_registry()
{
    {
        let expected = {};
        let actual;
        let finrec = new FinalizationRegistry(v => { actual = v });
        finrec.register({}, expected);
        os.setTimeout(() => {
            assert(actual, expected);
        }, 0);
    }
    {
        let expected = 42;
        let actual;
        let finrec = new FinalizationRegistry(v => { actual = v });
        finrec.register({}, expected);
        os.setTimeout(() => {
            assert(actual, expected);
        }, 0);
    }
    std.gc();
}

function test_generator()
{
    function *f() {
        var ret;
        yield 1;
        ret = yield 2;
        assert(ret, "next_arg");
        return 3;
    }
    function *f2() {
        yield 1;
        yield 2;
        return "ret_val";
    }
    function *f1() {
        var ret = yield *f2();
        assert(ret, "ret_val");
        return 3;
    }
    function *f3() {
        var ret;
        /* test stack consistency with nip_n to handle yield return +
         * finally clause */
        try {
            ret = 2 + (yield 1);
        } catch(e) {
        } finally {
            ret++;
        }
        return ret;
    }
    var g, v;
    g = f();
    v = g.next();
    assert(v.value === 1 && v.done === false);
    v = g.next();
    assert(v.value === 2 && v.done === false);
    v = g.next("next_arg");
    assert(v.value === 3 && v.done === true);
    v = g.next();
    assert(v.value === undefined && v.done === true);

    g = f1();
    v = g.next();
    assert(v.value === 1 && v.done === false);
    v = g.next();
    assert(v.value === 2 && v.done === false);
    v = g.next();
    assert(v.value === 3 && v.done === true);
    v = g.next();
    assert(v.value === undefined && v.done === true);

    g = f3();
    v = g.next();
    assert(v.value === 1 && v.done === false);
    v = g.next(3);
    assert(v.value === 6 && v.done === true);
}

function rope_concat(n, dir)
{
    var i, s;
    s = "";
    if (dir > 0) {
        for(i = 0; i < n; i++)
            s += String.fromCharCode(i & 0xffff);
    } else {
        for(i = n - 1; i >= 0; i--)
            s = String.fromCharCode(i & 0xffff) + s;
    }
    
    for(i = 0; i < n; i++) {
        /* test before the assert to go faster */
        if (s.charCodeAt(i) != (i & 0xffff)) {
            assert(s.charCodeAt(i), i & 0xffff);
        }
    }
}

function test_rope()
{
    rope_concat(100000, 1);
    rope_concat(100000, -1);
}

function eval_error(eval_str, expected_error, level)
{
    var err = false;
    var expected_pos, tab;

    tab = get_string_pos(eval_str);
    
    try {
        eval(tab[0]);
    } catch(e) {
        err = true;
        if (!(e instanceof expected_error)) {
            throw_error("unexpected exception type");
            return;
        }
        check_error_pos(e, expected_error, tab[1], tab[2], level);
    }
    if (!err) {
        throw_error("expected exception");
    }
}

var poisoned_number = {
    valueOf: function() { throw Error("poisoned number") },
};

function test_line_column_numbers()
{
    var f, e, tab;

    /* The '@' character provides the expected position of the
       error. It is removed before evaluating the string. */
    
    /* parsing */
    eval_error("\n 123 @a ", SyntaxError);
    eval_error("\n  @/*  ", SyntaxError);
    eval_error("function f  @a", SyntaxError);
    /* currently regexp syntax errors point to the start of the regexp */
    eval_error("\n  @/aaa]/u", SyntaxError); 

    /* function definitions */
    
    tab = get_string_pos("\n   @function f() { }; f;");
    e = eval(tab[0]);
    assert(e.lineNumber, tab[1]);
    assert(e.columnNumber, tab[2]);

    /* errors */
    tab = get_string_pos('\n  Error@("hello");');
    e = eval(tab[0]);
    check_error_pos(e, Error, tab[1], tab[2]);
    
    eval_error('\n  throw Error@("hello");', Error);

    /* operators */
    eval_error('\n  1 + 2 @* poisoned_number;', Error, 1);
    eval_error('\n  1 + "café" @* poisoned_number;', Error, 1);
    eval_error('\n  1 + 2 @** poisoned_number;', Error, 1);
    eval_error('\n  2 * @+ poisoned_number;', Error, 1);
    eval_error('\n  2 * @- poisoned_number;', Error, 1);
    eval_error('\n  2 * @~ poisoned_number;', Error, 1);
    eval_error('\n  2 * @++ poisoned_number;', Error, 1);
    eval_error('\n  2 * @-- poisoned_number;', Error, 1);
    eval_error('\n  2 * poisoned_number @++;', Error, 1);
    eval_error('\n  2 * poisoned_number @--;', Error, 1);

    /* accessors */
    eval_error('\n 1 + null@[0];', TypeError); 
    eval_error('\n 1 + null @. abcd;', TypeError); 
    eval_error('\n 1 + null @( 1234 );', TypeError);
    eval_error('var obj = { get a() { throw Error("test"); } }\n 1 + obj @. a;',
               Error, 1);
    eval_error('var obj = { set a(b) { throw Error("test"); } }\n obj @. a = 1;',
               Error, 1);

    /* variables reference */
    eval_error('\n  1 + @not_def', ReferenceError, 0);

    /* assignments */
    eval_error('1 + (@not_def = 1)', ReferenceError, 0);
    eval_error('1 + (@not_def += 2)', ReferenceError, 0);
    eval_error('var a;\n 1 + (a @+= poisoned_number);', Error, 1);
}

function test_group_by_callback_receiver()
{
    for (const groupBy of [Object.groupBy, Map.groupBy]) {
        let calls = 0;
        const groups = groupBy([10, 20], function(value, index) {
            "use strict";
            assert(this, undefined);
            assert(arguments.length, 2);
            assert(value, (index + 1) * 10);
            assert(index, calls++);
            return "group";
        });
        const group = groups instanceof Map ? groups.get("group") : groups.group;
        assert(calls, 2);
        assert(group.length, 2);
        assert(group[0], 10);
        assert(group[1], 20);
    }
}

function test_group_by_own_elements()
{
    for (const groupBy of [Object.groupBy, Map.groupBy]) {
        for (const setter of [true, false]) {
            let writes = 0, group, descriptor;
            const saved = Object.getOwnPropertyDescriptor(Array.prototype, "0");
            try {
                if (setter) {
                    Object.defineProperty(Array.prototype, "0", {
                        configurable: true,
                        set(value) { writes++; },
                    });
                } else {
                    Object.defineProperty(Array.prototype, "0", {
                        configurable: true,
                        writable: false,
                        value: "inherited",
                    });
                }
                const items = {
                    *[Symbol.iterator]() {
                        yield "first";
                        yield "second";
                    },
                };
                const groups = groupBy(items, () => "group");
                group = groups instanceof Map ? groups.get("group") : groups.group;
                descriptor = Object.getOwnPropertyDescriptor(group, "0");
            } finally {
                if (saved)
                    Object.defineProperty(Array.prototype, "0", saved);
                else
                    delete Array.prototype[0];
            }
            assert(writes, 0);
            assert(group.length, 2);
            assert(descriptor !== undefined, true);
            assert(descriptor.value, "first");
            assert(descriptor.writable, true);
            assert(descriptor.enumerable, true);
            assert(descriptor.configurable, true);
            assert(group[1], "second");
        }
    }
}

test();
test_group_by_own_elements();
test_group_by_callback_receiver();
test_function();
test_function_native_fallback();
test_function_initial_name();
test_function_constructor_boundaries();
test_enum();
test_array();
test_array_sort_writeback();
test_string();
test_string_unicode_18();
test_string_normalize();
test_math();
test_number();
test_eval();
test_array_buffer_max_index();
test_shared_array_buffer_atomics();
test_array_buffer_resize_order();
test_array_buffer_transfer_range();
test_array_buffer_slice_shrink();
test_typed_array();
test_typed_array_signed_search();
test_typed_array_with_conversion();
test_typed_array_copywithin_zero();
test_typed_array_set_content();
test_typed_array_constructor_content();
test_typed_array_species_content();
test_typed_array_set_overlap();
test_typed_array_constructor_length();
test_typed_array_slice_resize();
test_empty_array_buffer();
test_empty_typed_array();
test_typed_array_resize_bounds();
test_empty_typed_array_transfer();
test_error_stack();
test_json();
test_date();
test_regexp();
test_regexp_unicode_18();
test_symbol();
test_map();
test_map_computed_reentrancy();
test_set_record();
test_set_iterator_close();
test_set_iterator_factory();
test_iterator_wrapper();
test_iterator_accessors();
test_iterator_helper_completion();
test_iterator_helper_start_return();
test_iterator_flatmap_close();
test_iterator_helper_acquisition();
test_iterator_reduce_close();
test_iterator_limits();
test_iterator_concat_return();
test_iterator_concat_completion();
test_iterator_concat_prototype();
test_iterator_constructor_identity();
test_weak_map();
test_weak_map_cycles();
test_weak_ref();
test_finalization_registry();
test_generator();
test_rope();
test_line_column_numbers();
