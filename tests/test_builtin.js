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

function test_copy_data_property_reentrancy()
{
    const copies = [
        source => Object.assign({}, source),
        source => ({ ...source }),
        source => { const { ...result } = source; return result; },
    ];
    for (const copy of copies) {
        const hidden = {
            get a() {
                Object.defineProperty(this, "b", { enumerable: false });
                return 1;
            },
            b: 2,
        };
        const hidden_result = copy(hidden);
        assert(hidden_result.a, 1);
        assert(Object.hasOwn(hidden_result, "b"), false);

        const visible = {
            get a() {
                Object.defineProperty(this, "b", { enumerable: true });
                this.c = 3;
                return 1;
            },
        };
        Object.defineProperty(visible, "b", {
            configurable: true, value: 2,
        });
        const visible_result = copy(visible);
        assert(visible_result.b, 2);
        assert(Object.hasOwn(visible_result, "c"), false);

        let inherited_reads = 0;
        const deleted = {
            get a() { delete this.b; return 1; },
            b: 2,
        };
        Object.setPrototypeOf(deleted, {
            get b() { inherited_reads++; return 3; },
        });
        const deleted_result = copy(deleted);
        assert(Object.hasOwn(deleted_result, "b"), false);
        assert(inherited_reads, 0);

        let getter_calls = 0;
        const changed = {
            get a() {
                Object.defineProperty(this, "b", {
                    enumerable: true,
                    get() { getter_calls++; return 4; },
                });
                return 1;
            },
            b: 2,
        };
        assert(copy(changed).b, 4);
        assert(getter_calls, 1);

        const symbol = Symbol("later");
        const symbols = {
            get a() {
                Object.defineProperty(this, symbol, { enumerable: true });
                return 1;
            },
        };
        Object.defineProperty(symbols, symbol, { configurable: true, value: 5 });
        assert(copy(symbols)[symbol], 5);

        let proxy_gets = 0, proxy_descriptors = 0;
        const proxy = new Proxy({ a: "stored" }, {
            getOwnPropertyDescriptor(target, key) {
                proxy_descriptors++;
                return { configurable: true, enumerable: true,
                         writable: true, value: "descriptor" };
            },
            get(target, key) { proxy_gets++; return "actual"; },
        });
        assert(copy(proxy).a, "actual");
        assert(proxy_descriptors, 1);
        assert(proxy_gets, 1);
    }
    let later_reads = 0;
    const source = { a: 1, get b() { later_reads++; return 2; } };
    const target = {
        set a(value) {
            assert(value, 1);
            Object.defineProperty(source, "b", { enumerable: false });
        },
    };
    Object.assign(target, source);
    assert(later_reads, 0);
    assert(Object.hasOwn(target, "b"), false);

    const proto = { marker: true };
    const special = {};
    Object.defineProperty(special, "__proto__", { enumerable: true, value: proto });
    assert(Object.getPrototypeOf(Object.assign({}, special)), proto);
    for (const copy of copies.slice(1)) {
        const result = copy(special);
        assert(Object.getPrototypeOf(result), Object.prototype);
        assert(Object.hasOwn(result, "__proto__"), true);
        assert(result.__proto__, proto);
    }
    const events = [];
    const proxy = new Proxy({ skip: 1, keep: 2 }, {
        ownKeys() { events.push("keys"); return ["skip", "keep"]; },
        getOwnPropertyDescriptor(target, key) {
            events.push("descriptor " + key);
            if (key === "skip")
                throw new Error("excluded descriptor");
            return Reflect.getOwnPropertyDescriptor(target, key);
        },
        get(target, key) { events.push("get " + key); return target[key]; },
    });
    const { skip, ...rest } = proxy;
    assert(skip, 1);
    assert(rest.keep, 2);
    assert(Object.hasOwn(rest, "skip"), false);
    assert(events.join(","), "get skip,keys,descriptor keep,get keep");
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

function test_array_constructor_own_elements()
{
    for (const values of [["first"], ["first", "second"]]) {
        for (const setter of [true, false]) {
            let writes = 0;
            function Constructor() {}
            const proto = {};
            Constructor.prototype = proto;
            for (let i = 0; i < values.length; i++) {
                if (setter) {
                    Object.defineProperty(proto, i, {
                        set(value) { writes++; },
                    });
                } else {
                    Object.defineProperty(proto, i, {
                        writable: false, value: "inherited",
                    });
                }
            }
            const array = Reflect.construct(Array, values, Constructor);
            assert(Array.isArray(array), true);
            assert(Object.getPrototypeOf(array), proto);
            assert(writes, 0);
            assert(array.length, values.length);
            for (let i = 0; i < values.length; i++) {
                const descriptor = Object.getOwnPropertyDescriptor(array, i);
                assert(descriptor !== undefined, true);
                assert(descriptor.value, values[i]);
                assert(descriptor.writable, true);
                assert(descriptor.enumerable, true);
                assert(descriptor.configurable, true);
            }
            const holes = Reflect.construct(Array, [2], Constructor);
            assert(holes.length, 2);
            assert(Object.hasOwn(holes, "0"), false);
            assert(writes, 0);
        }
    }
    function Constructor() {}
    Object.defineProperty(Constructor.prototype, "0", {
        set(value) { throw new Error("inherited setter"); },
    });
    const array = Reflect.construct(Array, ["value"], Constructor);
    assert(array[0], "value");
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

function test_bigint_to_locale_string()
{
    const descriptor = Object.getOwnPropertyDescriptor(BigInt.prototype,
                                                       "toLocaleString");
    assert(descriptor !== undefined);
    const toLocaleString = descriptor.value;
    assert(typeof toLocaleString, "function");
    assert(descriptor.writable, true);
    assert(descriptor.enumerable, false);
    assert(descriptor.configurable, true);
    assert(toLocaleString !== Object.prototype.toLocaleString);
    assert(toLocaleString !== BigInt.prototype.toString);
    for (const [key, value] of [["name", "toLocaleString"], ["length", 0]]) {
        const property = Object.getOwnPropertyDescriptor(toLocaleString, key);
        assert(property.value, value);
        assert(property.writable, false);
        assert(property.enumerable, false);
        assert(property.configurable, true);
    }

    /* Decimal formatting applies to primitive and boxed BigInts. */
    const values = [
        [0n, "0"], [-0n, "0"], [1n, "1"], [-1n, "-1"],
        [255n, "255"], [-255n, "-255"],
        [12345678901234567890123456789012345678901234567890n,
         "12345678901234567890123456789012345678901234567890"],
        [-12345678901234567890123456789012345678901234567890n,
         "-12345678901234567890123456789012345678901234567890"],
    ];
    for (const [value, expected] of values) {
        assert(toLocaleString.call(value), expected);
        assert(value.toLocaleString(), expected);
        assert(Object(value).toLocaleString(), expected);
    }

    let observed = 0;
    function fail() {
        observed++;
        throw Error("BigInt locale conversion observed user code");
    }

    /* The method requires a BigInt or an object with [[BigIntData]]. */
    const invalid = [
        undefined, null, false, true, 0, 1.5, NaN, Infinity, "1", Symbol("1"),
        {}, [], function() {}, Object(1), Object("1"), Object(Symbol("1")),
        BigInt.prototype, Object.create(BigInt.prototype),
        { valueOf: fail, toString: fail, [Symbol.toPrimitive]: fail },
        { [Symbol.toStringTag]: "BigInt", valueOf() { return 1n; } },
        new Proxy(Object(1n), { get: fail }),
    ];
    for (const receiver of invalid) {
        assert_throws(TypeError, () => toLocaleString.call(receiver));
    }
    const revoked = Proxy.revocable(Object(1n), {});
    revoked.revoke();
    assert_throws(TypeError, () => toLocaleString.call(revoked.proxy));

    /* ECMA-262 reserves both optional parameters for ECMA-402. */
    const hostile = {
        get [Symbol.toPrimitive]() { return fail(); },
        valueOf: fail,
        toString: fail,
        get length() { return fail(); },
        get 0() { return fail(); },
        get [Symbol.iterator]() { return fail(); },
        get localeMatcher() { return fail(); },
        get style() { return fail(); },
        get useGrouping() { return fail(); },
    };
    const proxy = new Proxy({}, {
        get: fail, has: fail, ownKeys: fail, getOwnPropertyDescriptor: fail,
    });
    const arguments_list = [
        [undefined, undefined], ["en-US", {}],
        [["fr", "zh-TW"], { useGrouping: true }],
        [2, { style: "currency" }], [36, undefined],
        [0, undefined], [1, undefined], [null, null],
        [Symbol("locale"), Symbol("options")],
        [hostile, hostile], [proxy, proxy], [revoked.proxy, revoked.proxy],
    ];
    for (const [locales, options] of arguments_list) {
        assert(toLocaleString.call(255n, locales, options), "255");
        assert(toLocaleString.call(Object(-255n), locales, options), "-255");
    }

    /* Formatting must not look up an overridden toString property. */
    const boxed = Object(123456789n);
    boxed.toString = fail;
    assert(boxed.toLocaleString(), "123456789");
    Object.defineProperty(boxed, "toString", { get: fail });
    assert(boxed.toLocaleString(), "123456789");
    const saved = Object.getOwnPropertyDescriptor(BigInt.prototype, "toString");
    try {
        BigInt.prototype.toString = fail;
        assert((123n).toLocaleString(), "123");
        Object.defineProperty(BigInt.prototype, "toString", { get: fail });
        assert((123n).toLocaleString(), "123");
        assert(boxed.toLocaleString(), "123456789");
    } finally {
        Object.defineProperty(BigInt.prototype, "toString", saved);
    }
    assert(observed, 0);
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

function test_atomics_initial_oob()
{
    if (typeof Atomics === "undefined")
        return;
    const methods = ["load", "store", "add", "sub", "and", "or", "xor",
                     "exchange", "compareExchange", "notify"];
    for (const C of [Int32Array, BigInt64Array]) {
        const size = C.BYTES_PER_ELEMENT;
        for (const tracking of [false, true]) {
            const buffer = new ArrayBuffer(2 * size, { maxByteLength: 2 * size });
            const view = tracking ? new C(buffer, size) : new C(buffer, size, 1);
            buffer.resize(0);
            for (const method of methods) {
                const calls = [];
                const index = { valueOf() { calls.push("index"); return 0; } };
                const value = { valueOf() { calls.push("value"); return C === BigInt64Array ? 0n : 0; } };
                const replacement = { valueOf() { calls.push("replacement"); return C === BigInt64Array ? 0n : 0; } };
                assert_throws(TypeError, () => Atomics[method](view, index, value, replacement));
                assert(calls.length, 0);
            }
            if (tracking) {
                buffer.resize(size);
                for (const method of methods) {
                    const calls = [];
                    const index = { valueOf() { calls.push("index"); return 0; } };
                    const value = { valueOf() { calls.push("value"); return C === BigInt64Array ? 0n : 0; } };
                    assert_throws(RangeError, () => Atomics[method](view, index, value, value));
                    assert(calls.join(","), "index");
                }
            }
        }
        const buffer = new ArrayBuffer(2 * size, { maxByteLength: 2 * size });
        const view = new C(buffer, 0, 2);
        let calls = 0;
        assert_throws(TypeError, () => Atomics.load(view, {
            valueOf() { calls++; buffer.resize(0); return 0; }
        }));
        assert(calls, 1);
    }
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

function test_typed_array_from_constructor_order()
{
    const from = Uint8Array.from;
    const marker = new Error("source read");
    const receivers = [undefined, null, 1, "x", {}, Math.max, () => {},
                       new Proxy(() => {}, {})];
    for (const receiver of receivers) {
        let iterator_reads = 0, length_reads = 0;
        const iterable = {
            get [Symbol.iterator]() { iterator_reads++; throw marker; },
        };
        const arrayLike = {
            get length() { length_reads++; throw marker; },
        };
        assert_throws(TypeError, () => from.call(receiver, iterable));
        assert_throws(TypeError, () => from.call(receiver, arrayLike));
        assert(iterator_reads, 0);
        assert(length_reads, 0);
    }
    const order = [];
    function Constructor(length) {
        order.push("construct");
        assert(length, 1);
        return new Uint8Array(length);
    }
    const iterable = {
        *[Symbol.iterator]() {
            order.push("iterate");
            yield 7;
            order.push("done");
        },
    };
    const result = from.call(Constructor, iterable, value => {
        order.push("map");
        return value + 1;
    });
    assert(order.join(","), "iterate,done,construct,map");
    assert(result.length, 1);
    assert(result[0], 8);
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

function test_dataview_oob_error_order()
{
    for (const tracking of [false, true]) {
        const buffer = new ArrayBuffer(8, { maxByteLength: 8 });
        const view = tracking ? new DataView(buffer, 2) : new DataView(buffer, 2, 4);
        buffer.resize(1);
        for (const width of [8, 16]) {
            const get = "getUint" + width;
            const set = "setUint" + width;
            for (const position of [0, 32]) {
                const calls = [];
                const index = { valueOf() { calls.push("index"); return position; } };
                const value = { valueOf() { calls.push("value"); return 1; } };
                assert_throws(TypeError, () => view[get](index));
                assert(calls.join(","), "index");
                calls.length = 0;
                assert_throws(TypeError, () => view[set](index, value));
                assert(calls.join(","), "index,value");
            }
            let calls = 0;
            assert_throws(RangeError, () => view[set](-1, {
                valueOf() { calls++; return 1; }
            }));
            assert(calls, 0);
            const marker = {};
            try {
                view[set](0, { valueOf() { throw marker; } });
                assert(false);
            } catch (e) {
                assert(e, marker);
            }
        }
    }
    const buffer = new ArrayBuffer(8, { maxByteLength: 8 });
    const fixed = new DataView(buffer, 2, 4);
    buffer.resize(5);
    assert_throws(TypeError, () => fixed.getUint8(32));
    assert_throws(TypeError, () => fixed.setUint8(32, 1));
    const empty = new DataView(buffer, 5);
    assert_throws(RangeError, () => empty.getUint8(0));
    assert_throws(RangeError, () => empty.setUint8(0, 1));
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

function test_iterator_includes_values()
{
    const includes = Iterator.prototype.includes;
    const desc = Object.getOwnPropertyDescriptor(Iterator.prototype, "includes");
    assert(typeof includes, "function");
    assert(includes.name, "includes");
    assert(includes.length, 1);
    assert(desc.writable, true);
    assert(desc.configurable, true);
    assert(desc.enumerable, false);
    assert_throws(TypeError, () => new includes());
    assert([].values().includes(undefined), false);
    assert([undefined].values().includes(), true);
    assert(Array(1).values().includes(undefined), true);
    assert([NaN].values().includes(NaN), true);
    assert([0].values().includes(-0), true);
    assert([-0].values().includes(0), true);
    assert([1n].values().includes(1n), true);
    assert([1n].values().includes(1), false);
    const object = {}, symbol = Symbol("item");
    assert([object].values().includes(object), true);
    assert([object].values().includes({}), false);
    assert([symbol].values().includes(symbol), true);
    assert([symbol].values().includes(Symbol("item")), false);
    const identity = new Proxy({}, { get() { throw {}; } });
    assert([identity].values().includes(identity), true);
    assert([identity].values().includes({}), false);
    const prefix = "abcdefgh".repeat(32);
    assert([prefix + "suffix"].values().includes(prefix + "suffix"), true);
    assert(["a\0b"].values().includes("a\0b"), true);
    assert("a\uD83D\uDE00b"[Symbol.iterator]().includes("\uD83D\uDE00"), true);
    assert("a\uD83D\uDE00b"[Symbol.iterator]().includes("\uD83D"), false);
    for (const skip of [undefined, 0, -0])
        assert([1, 2].values().includes(1, skip), true);
    assert([1, 2, 1].values().includes(1, 1), true);
    assert([1, 2].values().includes(1, 1), false);
    assert([1, 2].values().includes(2, 2), false);
    assert([1, 2].values().includes(2, Number.MAX_SAFE_INTEGER), false);
    const remaining = [1, 2].values();
    assert(remaining.includes(1), true);
    assert(remaining.next().value, 2);
    let closed = false;
    function* values() {
        try { yield 1; yield 2; }
        finally { closed = true; }
    }
    const generator = values();
    assert(generator.includes(1), true);
    assert(closed, true);
    assert(generator.next().done, true);
    const callable = function() {};
    let called = false;
    callable.next = () => called ? { done: true } :
        (called = true, { done: false, value: 7 });
    assert(includes.call(callable, 7), true);
}

function test_iterator_includes_validation()
{
    const includes = Iterator.prototype.includes;
    let coercions = 0;
    const coercible = {
        [Symbol.toPrimitive]() { coercions++; return 0; },
        valueOf() { coercions++; return 0; },
        toString() { coercions++; return "0"; }
    };
    const proxyArgument = new Proxy({}, {
        get() { coercions++; throw {}; }
    });
    const invalid = [NaN, 0.5, -0.5, null, true, "0", 1n, Symbol(),
                     {}, [], new Number(0), coercible, proxyArgument];
    const cases = invalid.map(value => [value, TypeError]);
    for (const value of [-1, -Infinity, Number.MAX_SAFE_INTEGER + 1,
                         Number.MAX_VALUE])
        cases.push([value, RangeError]);
    for (const [value, ErrorType] of cases) {
        const events = [];
        let receiver, argumentCount;
        const source = new Proxy({}, {
            get(target, key) {
                events.push("get " + String(key));
                if (key !== "return")
                    throw {};
                return function() {
                    receiver = this;
                    argumentCount = arguments.length;
                    events.push("call return");
                    return {};
                };
            }
        });
        assert_throws(ErrorType, () => includes.call(source, 0, value));
        assert(events.join(","), "get return,call return");
        assert(receiver === source, true);
        assert(argumentCount, 0);
    }
    assert(coercions, 0);
    for (const receiver of [undefined, null, true, 0, "", Symbol(), 1n])
        assert_throws(TypeError, () => includes.call(receiver, 0, -1));

    for (const mode of ["get", "noncallable", "call", "primitive"])
        for (const [skip, ErrorType] of [[NaN, TypeError], [-1, RangeError]]) {
            let returnReads = 0, returnCalls = 0, nextReads = 0;
            const marker = {};
            const source = {
                get next() { nextReads++; throw marker; },
                get return() {
                    returnReads++;
                    if (mode === "get")
                        throw marker;
                    if (mode === "noncallable")
                        return 1;
                    return function() {
                        returnCalls++;
                        if (mode === "call")
                            throw marker;
                        return 1;
                    };
                }
            };
            assert_throws(ErrorType, () => includes.call(source, 0, skip));
            assert(nextReads, 0);
            assert(returnReads, 1);
            assert(returnCalls, mode === "call" || mode === "primitive" ? 1 : 0);
        }
}

function test_iterator_includes_protocol()
{
    const includes = Iterator.prototype.includes;
    const marker = {};
    for (const mode of ["next get", "next call", "noncallable", "result",
                        "done", "value"])
        for (const skip of [0, 1, Infinity]) {
            let closes = 0, nextCalls = 0;
            const source = {
                get next() {
                    if (mode === "next get")
                        throw marker;
                    if (mode === "noncallable")
                        return 1;
                    return function() {
                        if (++nextCalls > 1)
                            throw {};
                        if (mode === "next call")
                            throw marker;
                        if (mode === "result")
                            return 1;
                        return {
                            get done() {
                                if (mode === "done")
                                    throw marker;
                                return false;
                            },
                            get value() { throw marker; }
                        };
                    };
                },
                get return() { closes++; throw {}; }
            };
            let caught;
            try { includes.call(source, 1, skip); }
            catch (error) { caught = error; }
            if (mode === "noncallable" || mode === "result")
                assert(caught instanceof TypeError, true);
            else
                assert(caught === marker, true);
            assert(closes, 0);
            assert(nextCalls, mode === "next get" || mode === "noncallable" ? 0 : 1);
        }

    const events = [];
    let index = 0, nextReads = 0;
    const target = {
        get next() {
            nextReads++;
            return function() {
                assert(this === source, true);
                assert(arguments.length, 0);
                events.push("next");
                Object.defineProperty(target, "next", {
                    value() { throw marker; }, configurable: true
                });
                const value = ++index * 10;
                return new Proxy({ done: value > 20, value }, {
                    get(result, key) {
                        events.push(String(key) + value);
                        return result[key];
                    }
                });
            };
        },
        get return() {
            return function() {
                assert(this === source, true);
                assert(arguments.length, 0);
                events.push("return");
                return new Proxy({}, { get() { throw marker; } });
            };
        }
    };
    const source = new Proxy(target, {
        get(object, key, receiver) {
            events.push("get " + String(key));
            return Reflect.get(object, key, receiver);
        }
    });
    assert(includes.call(source, 20, 1), true);
    assert(nextReads, 1);
    assert(events.join(","),
           "get next,next,done10,value10,next,done20,value20,get return,return");

    for (const skip of [0, 1, Infinity]) {
        let closes = 0, values = 0;
        const exhausted = {
            next() {
                return { done: true, get value() { values++; throw marker; } };
            },
            get return() { closes++; throw marker; }
        };
        assert(includes.call(exhausted, undefined, skip), false);
        assert(values, 0);
        assert(closes, 0);
    }
    events.length = 0;
    index = 0;
    const infiniteSkip = {
        next() {
            const current = index++;
            events.push("next" + current);
            return {
                get done() {
                    events.push("done" + current);
                    return current === 2;
                },
                get value() { events.push("value" + current); return 1; }
            };
        },
        get return() { throw marker; }
    };
    assert(includes.call(infiniteSkip, 1, Infinity), false);
    assert(events.join(","),
           "next0,done0,value0,next1,done1,value1,next2,done2");
}

function test_iterator_includes_close()
{
    const includes = Iterator.prototype.includes;
    const marker = {};
    for (const mode of ["absent", "undefined", "null", "object",
                        "noncallable", "primitive", "get", "call"]) {
        let returnReads = 0, returnCalls = 0, receiver, argumentCount;
        let nextCalls = 0;
        const source = {
            next() {
                return ++nextCalls === 1 ? { done: false, value: 1 } : { done: true };
            }
        };
        if (mode !== "absent")
            Object.defineProperty(source, "return", {
                get() {
                    returnReads++;
                    if (mode === "get")
                        throw marker;
                    if (mode === "undefined")
                        return undefined;
                    if (mode === "null")
                        return null;
                    if (mode === "noncallable")
                        return 1;
                    return function() {
                        returnCalls++;
                        receiver = this;
                        argumentCount = arguments.length;
                        if (mode === "call")
                            throw marker;
                        return mode === "primitive" ? 1 : {};
                    };
                }
            });
        if (mode === "get" || mode === "call") {
            let caught;
            try { includes.call(source, 1); }
            catch (error) { caught = error; }
            assert(caught === marker, true);
        } else if (mode === "noncallable" || mode === "primitive") {
            assert_throws(TypeError, () => includes.call(source, 1));
        } else {
            assert(includes.call(source, 1), true);
        }
        assert(nextCalls, 1);
        assert(returnReads, mode === "absent" ? 0 : 1);
        assert(returnCalls, ["object", "primitive", "call"].includes(mode) ? 1 : 0);
        if (returnCalls) {
            assert(receiver === source, true);
            assert(argumentCount, 0);
        }
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

function test_iterator_join()
{
    const join = Iterator.prototype.join;
    const descriptor = Object.getOwnPropertyDescriptor(Iterator.prototype, "join");
    assert(descriptor.value === join, true);
    assert(descriptor.writable, true);
    assert(descriptor.enumerable, false);
    assert(descriptor.configurable, true);
    for (const [key, value] of [["name", "join"], ["length", 1]]) {
        const property = Object.getOwnPropertyDescriptor(join, key);
        assert(property.value, value);
        assert(property.writable, false);
        assert(property.enumerable, false);
        assert(property.configurable, true);
    }
    assert_throws(TypeError, () => new join());
    assert_throws(TypeError, () => Reflect.construct(join, []));

    let conversions = 0;
    const separator = { toString() { conversions++; return "&"; } };
    for (const receiver of [undefined, null, false, 1, "text", Symbol("x"), 1n])
        assert_throws(TypeError, () => join.call(receiver, separator));
    assert(conversions, 0);
    assert([].values().join(separator), "");
    assert([1].values().join(separator), "1");
    assert(conversions, 2);
    assert([1, 2, 3].values().join(), "1,2,3");
    assert([1, 2, 3].values().join(undefined), "1,2,3");
    assert([1, 2].values().join(null), "1null2");
    assert([1, 2].values().join(""), "12");
    assert([null, undefined, 1, null].values().join(), ",,1,");
    assert([null, undefined].values().join("|"), "|");
    assert([0, false, -0, NaN, Infinity, 12n].values().join(":"),
           "0:false:0:NaN:Infinity:12");

    const log = [];
    const item = {
        toString() { log.push("toString"); return {}; },
        valueOf() { log.push("valueOf"); return 42; }
    };
    assert([item].values().join(), "42");
    assert(log.join(","), "toString,valueOf");
    function receiver() {}
    receiver.next = () => ({ done: true });
    assert(join.call(receiver), "");
}

function test_iterator_join_order()
{
    const log = [];
    let index = 0;
    const source = {
        get [Symbol.iterator]() { throw Error("iterator must not be read"); },
        get return() { throw Error("exhaustion must not close"); },
        get next() {
            log.push("next:get");
            return function() {
                assert(this === source, true);
                assert(arguments.length, 0);
                const current = index++;
                log.push("next:" + current);
                Object.defineProperty(source, "next", {
                    configurable: true,
                    value() { throw Error("cached next must be used"); }
                });
                return {
                    get done() {
                        log.push("done:" + current);
                        return current === 2 ? {
                            [Symbol.toPrimitive]() { throw Error("done is not coerced"); }
                        } : false;
                    },
                    get value() {
                        log.push("value:" + current);
                        if (current === 2)
                            throw Error("completed value must not be read");
                        if (current === 1)
                            return null;
                        return {
                            get [Symbol.toPrimitive]() {
                                log.push("item:primitive:get");
                                return hint => {
                                    log.push("item:" + hint);
                                    return "one";
                                };
                            }
                        };
                    }
                };
            };
        }
    };
    const separator = {
        get [Symbol.toPrimitive]() {
            log.push("separator:primitive:get");
            return hint => {
                log.push("separator:" + hint);
                return "&";
            };
        }
    };
    assert(Iterator.prototype.join.call(source, separator), "one&");
    assert(log.join(","), "separator:primitive:get,separator:string,next:get," +
           "next:0,done:0,value:0,item:primitive:get,item:string," +
           "next:1,done:1,value:1,next:2,done:2");

    let nexts = 0;
    const changed = { next() { throw Error("old next must not be called"); } };
    const changingSeparator = {
        toString() {
            changed.next = () => ++nexts === 1 ? { value: "new" } : { done: true };
            return ":";
        }
    };
    assert(Iterator.prototype.join.call(changed, changingSeparator), "new");
    assert(nexts, 2);
}

function test_iterator_join_protocol_errors()
{
    const marker = {};
    function failure(setup, expected) {
        let closes = 0;
        const source = {
            get return() { closes++; throw Error("protocol error must not close"); }
        };
        setup(source);
        let caught;
        try { Iterator.prototype.join.call(source); }
        catch (error) { caught = error; }
        if (expected === TypeError)
            assert(caught instanceof TypeError, true);
        else
            assert(caught === expected, true);
        assert(closes, 0);
    }
    failure(source => {
        Object.defineProperty(source, "next", { get() { throw marker; } });
    }, marker);
    for (const next of [undefined, null, false, 1, "next", Symbol("next"), {}])
        failure(source => { source.next = next; }, TypeError);
    failure(source => { source.next = () => { throw marker; }; }, marker);
    for (const result of [undefined, null, false, 1, "result", Symbol("result")])
        failure(source => { source.next = () => result; }, TypeError);
    failure(source => {
        source.next = () => ({ get done() { throw marker; } });
    }, marker);
    failure(source => {
        source.next = () => ({ done: false, get value() { throw marker; } });
    }, marker);

    const completed = {
        next() {
            return { done: true, get value() { throw Error("value must not be read"); } };
        },
        get return() { throw Error("exhaustion must not close"); }
    };
    assert(Iterator.prototype.join.call(completed), "");
}

function test_iterator_join_coercion_close()
{
    const marker = {}, closeMarker = {};
    for (const stage of ["separator", "item"]) {
        for (const mode of ["missing", "undefined", "null", "noncallable",
                            "getter-throw", "call-throw", "primitive", "object"]) {
            let nextReads = 0, nextCalls = 0, returnReads = 0, returnCalls = 0;
            const poison = {
                [Symbol.toPrimitive](hint) {
                    assert(hint, "string");
                    throw marker;
                }
            };
            const source = {
                get next() {
                    nextReads++;
                    return function() {
                        assert(this === source, true);
                        assert(arguments.length, 0);
                        nextCalls++;
                        return { value: poison, done: false };
                    };
                }
            };
            if (mode !== "missing") {
                Object.defineProperty(source, "return", {
                    get() {
                        assert(this === source, true);
                        returnReads++;
                        if (mode === "getter-throw")
                            throw closeMarker;
                        if (mode === "undefined")
                            return undefined;
                        if (mode === "null")
                            return null;
                        if (mode === "noncallable")
                            return 1;
                        return function() {
                            assert(this === source, true);
                            assert(arguments.length, 0);
                            returnCalls++;
                            if (mode === "call-throw")
                                throw closeMarker;
                            return mode === "primitive" ? 1 : {};
                        };
                    }
                });
            }
            let caught;
            try {
                Iterator.prototype.join.call(source, stage === "separator" ? poison : "|");
            } catch (error) { caught = error; }
            assert(caught === marker, true);
            assert(nextReads, stage === "separator" ? 0 : 1);
            assert(nextCalls, stage === "separator" ? 0 : 1);
            assert(returnReads, mode === "missing" ? 0 : 1);
            assert(returnCalls, ["call-throw", "primitive", "object"].includes(mode) ? 1 : 0);
        }
    }

    for (const stage of ["separator", "item"]) {
        let nexts = 0, closes = 0;
        const source = {
            next() { nexts++; return { value: Symbol("item"), done: false }; },
            return() { closes++; throw closeMarker; }
        };
        assert_throws(TypeError, () => Iterator.prototype.join.call(source,
                      stage === "separator" ? Symbol("separator") : undefined));
        assert(nexts, stage === "separator" ? 0 : 1);
        assert(closes, 1);
    }

    let closes = 0;
    const source = {
        next() {
            return { value: { get [Symbol.toPrimitive]() { throw marker; } } };
        },
        return() { closes++; return {}; }
    };
    let caught;
    try { Iterator.prototype.join.call(source); }
    catch (error) { caught = error; }
    assert(caught === marker, true);
    assert(closes, 1);
    source.next = () => ({ value: { [Symbol.toPrimitive]() { return {}; } } });
    assert_throws(TypeError, () => Iterator.prototype.join.call(source));
    assert(closes, 2);
}

function test_iterator_join_reentrancy()
{
    const join = Iterator.prototype.join;
    let index = 0, nexts = 0, closes = 0, nested;
    const source = {
        next() {
            nexts++;
            return index < 3 ? { value: ["a", "b", "c"][index++] } : { done: true };
        },
        return() { closes++; return {}; }
    };
    const separator = {
        toString() { nested = join.call(source, "|"); return ":"; }
    };
    assert(join.call(source, separator), "");
    assert(nested, "a|b|c");
    assert(nexts, 5);
    assert(closes, 0);

    index = 0;
    nexts = 0;
    const item = {
        [Symbol.toPrimitive](hint) {
            assert(hint, "string");
            return "inner:" + join.call(source, "|");
        }
    };
    source.next = () => {
        nexts++;
        return index < 3 ? { value: [item, "b", "c"][index++] } : { done: true };
    };
    assert(join.call(source, ":"), "inner:b|c");
    assert(nexts, 5);
    assert(closes, 0);

    const marker = {};
    const changed = {
        next() {
            return { value: { toString() {
                changed.return = function() { closes++; return {}; };
                throw marker;
            } } };
        },
        return() { throw Error("old return must not be called"); }
    };
    let caught;
    try { join.call(changed); } catch (error) { caught = error; }
    assert(caught === marker, true);
    assert(closes, 1);
}

function test_iterator_join_strings()
{
    let narrow = "", wide = "", separator = "";
    for (let i = 0; i < 128; i++) {
        narrow += "abc\0\u00e9";
        wide += "\u0100\ud800\udfff";
        separator += "x\0";
    }
    for (const sep of ["", "\0", "\u0100", "\ud800", separator]) {
        assert([narrow, wide, narrow].values().join(sep),
               narrow + sep + wide + sep + narrow);
    }
    const wrapped = { [Symbol.toPrimitive]() { return wide; } };
    assert([wrapped, narrow].values().join(separator), wide + separator + narrow);
    const values = [];
    for (let i = 0; i < 512; i++)
        values.push(i & 1 ? "\u0100" : "a\0");
    assert(values.values().join("\0"), values.join("\0"));
}

function test_iterator_buffer_helper(method)
{
    const proto = Iterator.prototype;
    const marker = {};
    const make = source => proto[method].call(source, 2);
    function source(values) {
        let index = 0;
        return {
            next() {
                return index < values.length ? { value: values[index++], done: false }
                                             : { done: true };
            }
        };
    }
    function caught(fn) {
        let error, threw = false;
        try { fn(); } catch (value) { error = value; threw = true; }
        assert(threw, true);
        return error;
    }

    assert(proto[method].length, 1);
    assert(proto[method].name, method);
    assert_throws(TypeError, () => new proto[method](2));
    for (const receiver of [undefined, null, true, 1, "x", 1n, Symbol()])
        assert_throws(TypeError, () => proto[method].call(receiver, 0));
    for (const [values, errorType] of [
        [[undefined, null, true, "2", 2n, Symbol(), {}, new Number(2),
          NaN, Infinity, -Infinity, 1.5, -1.5, 4294967296.5], TypeError],
        [[0, -0, -1, 4294967296, Number.MAX_VALUE], RangeError]
    ]) {
        for (const size of values) {
            const events = [];
            const input = {
                get next() { events.push("next"); throw marker; },
                get return() {
                    events.push("return");
                    return () => { events.push("close"); throw marker; };
                }
            };
            assert(caught(() => proto[method].call(input, size)) instanceof errorType,
                   true);
            assert(events.join(","), "return,close");
        }
    }
    let conversions = 0, closes = 0;
    assert_throws(TypeError, () => proto[method].call({
        return() { closes++; return 0; }
    }, { valueOf() { conversions++; return 2; } }));
    assert(conversions, 0);
    assert(closes, 1);

    for (const thrown of [marker, undefined, null]) {
        let closes = 0;
        assert(caught(() => make({
            get next() { throw thrown; },
            return() { closes++; return {}; }
        })) === thrown, true);
        assert(closes, 0);
    }
    for (const kind of ["call", "primitive", "done", "value", "noncallable"]) {
        let reads = 0, nexts = 0, closes = 0;
        const input = {
            get next() {
                reads++;
                if (kind === "noncallable")
                    return 1;
                return function() {
                    assert(this === input, true);
                    assert(arguments.length, 0);
                    nexts++;
                    if (kind === "call") throw marker;
                    if (kind === "primitive") return 1;
                    return {
                        get done() { if (kind === "done") throw marker; return false; },
                        get value() { throw marker; }
                    };
                };
            },
            return() { closes++; throw {}; }
        };
        const helper = make(input);
        assert(reads, 1);
        assert(nexts, 0);
        const error = caught(() => helper.next());
        assert(kind === "primitive" || kind === "noncallable"
               ? error instanceof TypeError : error === marker, true);
        assert(helper.next().done, true);
        assert(helper.return().done, true);
        assert(reads, 1);
        assert(nexts, kind === "noncallable" ? 0 : 1);
        assert(closes, 0);
    }
    const events = [];
    let index = 0;
    const input = {
        get next() {
            events.push("get-next");
            return function() {
                events.push("next");
                if (index++ === 2)
                    return { done: true, get value() { throw marker; } };
                return {
                    get done() { events.push("done"); return false; },
                    get value() { events.push("value"); return index; }
                };
            };
        },
        get [Symbol.iterator]() { throw marker; },
        get return() { throw marker; }
    };
    const helper = make(input);
    Object.defineProperty(input, "next", { value: undefined });
    assert(events.join(","), "get-next");
    assert(JSON.stringify(helper.next().value), "[1,2]");
    assert(helper.next().done, true);
    assert(events.join(","), "get-next,next,done,value,next,done,value,next");

    for (const thrown of [marker, undefined, null]) {
        let helper, index = 0, closes = 0;
        const value = {};
        helper = make({
            next() {
                if (index++ === 0) {
                    value.helper = helper;
                    return { value, done: false };
                }
                if (typeof std !== "undefined") std.gc();
                throw thrown;
            },
            return() { closes++; return {}; }
        });
        assert(caught(() => helper.next()) === thrown, true);
        assert(helper.next().done, true);
        assert(helper.return().done, true);
        assert(closes, 0);
    }

    for (const started of [false, true]) {
        for (const closeKind of ["normal", "get", "call", "primitive", "noncallable", "null"]) {
            let helper, reads = 0, calls = 0;
            const input = source([1, 2, 3]);
            Object.defineProperty(input, "return", {
                get() {
                    reads++;
                    if (started) {
                        assert_throws(TypeError, () => helper.next());
                        assert_throws(TypeError, () => helper.return());
                    } else {
                        assert(helper.next().done, true);
                        assert(helper.return().done, true);
                    }
                    if (closeKind === "get") throw marker;
                    if (closeKind === "noncallable") return 1;
                    if (closeKind === "null") return null;
                    return function() {
                        calls++;
                        assert(this === input, true);
                        assert(arguments.length, 0);
                        if (started) {
                            assert_throws(TypeError, () => helper.next());
                            assert_throws(TypeError, () => helper.return());
                        } else {
                            assert(helper.next().done, true);
                            assert(helper.return().done, true);
                        }
                        if (closeKind === "call") throw marker;
                        return closeKind === "primitive" ? 1 : {};
                    };
                }
            });
            helper = make(input);
            if (started)
                assert(JSON.stringify(helper.next().value), "[1,2]");
            if (closeKind === "normal" || closeKind === "null") {
                assert(helper.return(9).done, true);
            } else {
                const error = caught(() => helper.return());
                assert(closeKind === "primitive" || closeKind === "noncallable"
                       ? error instanceof TypeError : error === marker, true);
            }
            assert(helper.next().done, true);
            assert(helper.return().done, true);
            assert(reads, 1);
            assert(calls, closeKind === "normal" || closeKind === "call" ||
                          closeKind === "primitive" ? 1 : 0);
        }
    }
    for (const stage of ["next", "done", "value"]) {
        let helper;
        function reenter() {
            assert_throws(TypeError, () => helper.next());
            assert_throws(TypeError, () => helper.return());
        }
        helper = make({
            next() {
                if (stage === "next") reenter();
                return {
                    get done() { if (stage === "done") reenter(); return false; },
                    get value() { if (stage === "value") reenter(); return 1; }
                };
            }
        });
        assert(JSON.stringify(helper.next().value), "[1,1]");
        assert(helper.return().done, true);
    }
    assert(make(source([])).next().done, true);
    const many = proto[method].call(source([undefined, null, true, 1, "x", 1n,
                                          Symbol.for("chunk"), marker]), 1);
    for (const value of [undefined, null, true, 1, "x", 1n, Symbol.for("chunk"), marker]) {
        const row = many.next().value;
        assert(row.length, 1);
        assert(row[0] === value, true);
    }
    assert(many.next().done, true);

    let arrayCloses = 0;
    Object.defineProperty(Array.prototype, "return", {
        configurable: true,
        get() { arrayCloses++; throw Error("arrays are held values"); }
    });
    try {
        const helper = make(source([[1], [2], [3]]));
        assert(helper.next().value.length, 2);
        assert(helper.return().done, true);
    } finally {
        delete Array.prototype.return;
    }
    assert(arrayCloses, 0);

    let gcHelper, gcItem = {};
    const gcSource = {
        next() {
            if (!gcItem) return { done: true };
            const value = gcItem;
            gcItem = null;
            value.helper = gcHelper;
            return { value, done: false };
        }
    };
    gcHelper = proto[method].call(gcSource, 2);
    if (typeof std !== "undefined") std.gc();
    const gcResult = gcHelper.next();
    if (method === "chunks") {
        assert(gcResult.value[0].helper === gcHelper, true);
        gcResult.value.length = 0;
    } else {
        assert(gcResult.done, true);
    }
    gcHelper = null;
    if (typeof std !== "undefined") std.gc();
}

function test_iterator_chunks()
{
    test_iterator_buffer_helper("chunks");
    function collect(values, size) {
        return JSON.stringify(values.values().chunks(size).toArray());
    }
    assert(collect([1, 2, 3, 4], 2), "[[1,2],[3,4]]");
    assert(collect([1, 2, 3, 4, 5], 2), "[[1,2],[3,4],[5]]");
    assert(collect([1, 2, 3], 4294967295), "[[1,2,3]]");
    const source = [1, 2, 3, 4, 5].values();
    const helper = source.chunks(2);
    const first = helper.next().value;
    first[0] = 99;
    first.push(99);
    assert(source.next().value, 3);
    const second = helper.next().value;
    assert(first !== second, true);
    assert(JSON.stringify(second), "[4,5]");
    assert(helper.next().done, true);

    for (const length of [1, 2]) {
        let index = 0, closes = 0;
        const helper = Iterator.prototype.chunks.call({
            next() { return { done: index >= length, value: index++ }; },
            get return() { closes++; return () => ({}); }
        }, 2);
        assert(helper.next().done, false);
        assert(helper.return().done, true);
        assert(closes, length === 1 ? 0 : 1);
    }
    let finallyCalls = 0;
    function* input() {
        try { yield 1; yield 2; yield 3; } finally { finallyCalls++; }
    }
    const external = input(), remaining = external.chunks(2);
    assert(JSON.stringify(remaining.next().value), "[1,2]");
    external.return();
    assert(remaining.next().done, true);
    assert(finallyCalls, 1);

    let setters = 0, row;
    Object.defineProperty(Array.prototype, "0", {
        configurable: true, set() { setters++; }
    });
    try { row = [1, 2].values().chunks(2).next().value; }
    finally { delete Array.prototype[0]; }
    assert(setters, 0);
    assert(JSON.stringify(row), "[1,2]");
    const desc = Object.getOwnPropertyDescriptor(row, "0");
    assert(desc.writable && desc.enumerable && desc.configurable, true);
}

function test_iterator_windows()
{
    test_iterator_buffer_helper("windows");
    const windows = Iterator.prototype.windows;
    function collect(values, size, undersized) {
        return JSON.stringify(values.values().windows(size, undersized).toArray());
    }
    assert(collect([1, 2, 3, 4], 2), "[[1,2],[2,3],[3,4]]");
    assert(collect([1, 2, 3, 4], 3), "[[1,2,3],[2,3,4]]");
    assert(collect([1, 2, 3], 1), "[[1],[2],[3]]");
    assert(collect([1, 2], 3), "[]");
    assert(collect([1, 2], 3, "only-full"), "[]");
    assert(collect([1, 2], 3, "allow-partial"), "[[1,2]]");
    assert(collect([1, 2, 3, 4], 3, "allow-partial"), "[[1,2,3],[2,3,4]]");
    assert(collect([], 3, "allow-partial"), "[]");
    assert(collect([1, 2], 4294967295), "[]");
    assert(collect([1, 2], 4294967295, "allow-partial"), "[[1,2]]");
    assert(collect([1], 2, "allow-" + "partial"), "[[1]]");
    assert(collect([1], 2, ("x" + "allow-partial").slice(1)), "[[1]]");

    for (const option of [null, 1, 1n, true, Symbol(), {}, new String("only-full"),
                          "", "only-full\0", "allow-partial\0", "ONLY-FULL"]) {
        const events = [];
        const input = {
            get next() { events.push("next"); throw {}; },
            return() { events.push("return"); throw {}; }
        };
        assert_throws(TypeError, () => windows.call(input, 2, option));
        assert(events.join(","), "return");
    }
    let coerced = 0, closes = 0, nexts = 0;
    const input = {
        get next() { nexts++; return () => ({ done: true }); },
        return() { closes++; return {}; }
    };
    const option = { toString() { coerced++; return "only-full"; } };
    assert_throws(RangeError, () => windows.call(input, 0, option));
    assert_throws(TypeError, () => windows.call(input, 2, option));
    assert(coerced, 0);
    assert(closes, 2);
    assert(nexts, 0);

    const source = [1, 2, 3, 4, 5].values();
    const helper = source.windows(2);
    const first = helper.next().value;
    first[0] = 99;
    first.length = 0;
    first.push(99);
    assert(source.next().value, 3);
    const second = helper.next().value;
    assert(first !== second, true);
    assert(JSON.stringify(second), "[2,4]");
    assert(JSON.stringify(helper.next().value), "[4,5]");
    assert(helper.next().done, true);

    const ring = Array.from({ length: 40 }, (_, i) => i).values().windows(9);
    for (let i = 0; i < 32; i++) {
        const row = ring.next().value;
        assert(row.length, 9);
        for (let j = 0; j < 9; j++) assert(row[j], i + j);
    }
    assert(ring.next().done, true);

    for (const length of [1, 2, 3]) {
        let index = 0, closes = 0;
        const helper = windows.call({
            next() { return { done: index >= length, value: index++ }; },
            get return() { closes++; return () => ({}); }
        }, 2, "allow-partial");
        assert(helper.next().done, false);
        assert(helper.return().done, true);
        assert(closes, length === 1 ? 0 : 1);
    }
    let finallyCalls = 0;
    function* inputGenerator() {
        try { yield 1; yield 2; yield 3; } finally { finallyCalls++; }
    }
    const external = inputGenerator(), remaining = external.windows(2);
    assert(JSON.stringify(remaining.next().value), "[1,2]");
    external.return();
    assert(remaining.next().done, true);
    assert(finallyCalls, 1);

    const ordinary = [1, 2, 3].values().map(x => x);
    const chunks = [1, 2, 3].values().chunks(2);
    const window = [1, 2, 3].values().windows(2);
    const next = ordinary.next, close = window.return;
    assert(JSON.stringify(next.call(chunks).value), "[1,2]");
    assert(JSON.stringify(next.call(window).value), "[1,2]");
    assert(next.call(ordinary).value, 1);
    assert(close.call(chunks).done, true);
    assert(close.call(ordinary).done, true);
    assert(close.call(window).done, true);
    const concat = Iterator.concat([1]);
    assert(next.call(concat).value, 1);
    assert(close.call(concat).done, true);

    let setters = 0, row;
    Object.defineProperty(Array.prototype, "0", {
        configurable: true, set() { setters++; }
    });
    try { row = [1, 2, 3].values().windows(2).next().value; }
    finally { delete Array.prototype[0]; }
    assert(setters, 0);
    assert(JSON.stringify(row), "[1,2]");
    const desc = Object.getOwnPropertyDescriptor(row, "0");
    assert(desc.writable && desc.enumerable && desc.configurable, true);

    let live, index = 0, captured = {};
    const sourceCycle = {
        next() {
            if (++index === 3) {
                captured.helper = live;
                return { value: captured, done: false };
            }
            return { value: index, done: false };
        }
    };
    live = windows.call(sourceCycle, 3);
    let retained = live.next().value;
    assert(retained[2] === captured, true);
    captured = null;
    retained = null;
    retained = live.next().value;
    assert(retained[1].helper === live, true);
    retained = null;
    if (typeof std !== "undefined") std.gc();
    retained = live.next().value;
    assert(retained[0].helper === live, true);
    assert(retained[1], 4);
    assert(retained[2], 5);
    retained = null;
    assert(JSON.stringify(live.next().value), "[4,5,6]");
    assert(live.return().done, true);
    live = null;
    if (typeof std !== "undefined") std.gc();
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

function test_array_iterator_length()
{
    function check(result, kind, index, value) {
        assert(result.done, false);
        if (kind === "keys") {
            assert(result.value, index);
        } else if (kind === "values") {
            assert(result.value, value);
        } else {
            assert(result.value.length, 2);
            assert(result.value[0], index);
            assert(result.value[1], value);
        }
    }
    for (const kind of ["keys", "values", "entries"]) {
        for (const length of [-1, -0.5, NaN]) {
            const iterator = Array.prototype[kind].call({ 0: "zero", length });
            assert(iterator.next().done, true);
        }
        for (const length of [2 ** 32, 2 ** 32 + 1, Infinity]) {
            let reads = 0, conversions = 0;
            const iterator = Array.prototype[kind].call({
                0: "zero",
                1: "one",
                get length() {
                    reads++;
                    return { valueOf() { conversions++; return length; } };
                },
            });
            check(iterator.next(), kind, 0, "zero");
            assert(reads, 1);
            assert(conversions, 1);
            check(iterator.next(), kind, 1, "one");
            assert(reads, 2);
            assert(conversions, 2);
        }
        let iterator, inner, entered = false;
        iterator = Array.prototype[kind].call({
            0: "zero",
            1: "one",
            get length() {
                if (!entered) {
                    entered = true;
                    inner = iterator.next();
                }
                return 2;
            },
        });
        const outer = iterator.next();
        check(inner, kind, 0, "zero");
        check(outer, kind, 0, "zero");
        check(iterator.next(), kind, 1, "one");
        assert(iterator.next().done, true);

        entered = false;
        iterator = Array.prototype[kind].call({
            0: "zero",
            get length() {
                if (entered)
                    return 0;
                entered = true;
                inner = iterator.next();
                return 1;
            },
        });
        const last = iterator.next();
        assert(inner.done, true);
        check(last, kind, 0, "zero");
        assert(iterator.next().done, true);
    }
}

function test_object_from_entries_close()
{
    const marker = new Error("entry failure");
    const close_error = new Error("close failure");
    function failure(setup, expected, close) {
        let reads = 0, calls = 0, caught;
        const iterator = {
            get return() {
                reads++;
                return function() {
                    calls++;
                    throw close_error;
                };
            },
        };
        setup(iterator);
        const items = { [Symbol.iterator]() { return iterator; } };
        try {
            Object.fromEntries(items);
        } catch (error) {
            caught = error;
        }
        if (expected === TypeError)
            assert(caught instanceof TypeError, true);
        else
            assert(caught, expected);
        assert(reads, close ? 1 : 0);
        assert(calls, close ? 1 : 0);
    }
    failure(iterator => {
        Object.defineProperty(iterator, "next", { get() { throw marker; } });
    }, marker, false);
    failure(iterator => { iterator.next = 1; }, TypeError, false);
    failure(iterator => { iterator.next = () => { throw marker; }; }, marker, false);
    failure(iterator => { iterator.next = () => 1; }, TypeError, false);
    failure(iterator => {
        iterator.next = () => ({ get done() { throw marker; } });
    }, marker, false);
    failure(iterator => {
        iterator.next = () => ({ done: false, get value() { throw marker; } });
    }, marker, false);
    failure(iterator => {
        iterator.next = () => ({ done: false, value: 1 });
    }, TypeError, true);
    failure(iterator => {
        iterator.next = () => ({ done: false, value: {
            get 0() { throw marker; },
        } });
    }, marker, true);
    failure(iterator => {
        iterator.next = () => ({ done: false, value: {
            0: "key", get 1() { throw marker; },
        } });
    }, marker, true);
    failure(iterator => {
        const key = { [Symbol.toPrimitive]() { throw marker; } };
        iterator.next = () => ({ done: false, value: [key, "value"] });
    }, marker, true);
}

function test_aggregate_error_iterator_close()
{
    const marker = new Error("iterator failure");
    function failure(setup, expected) {
        let reads = 0, calls = 0, caught;
        const iterator = {
            get return() {
                reads++;
                return function() {
                    calls++;
                    throw new Error("unexpected close");
                };
            },
        };
        setup(iterator);
        const items = { [Symbol.iterator]() { return iterator; } };
        try {
            new AggregateError(items);
        } catch (error) {
            caught = error;
        }
        if (expected === TypeError)
            assert(caught instanceof TypeError, true);
        else
            assert(caught, expected);
        assert(reads, 0);
        assert(calls, 0);
    }
    failure(iterator => {
        Object.defineProperty(iterator, "next", { get() { throw marker; } });
    }, marker);
    failure(iterator => { iterator.next = 1; }, TypeError);
    failure(iterator => { iterator.next = () => { throw marker; }; }, marker);
    failure(iterator => { iterator.next = () => 1; }, TypeError);
    failure(iterator => {
        iterator.next = () => ({ get done() { throw marker; } });
    }, marker);
    failure(iterator => {
        iterator.next = () => ({ done: false, get value() { throw marker; } });
    }, marker);
    const errors = new AggregateError([marker, 42]).errors;
    assert(errors.length, 2);
    assert(errors[0], marker);
    assert(errors[1], 42);
}

function iterator_zip_throws_value(fn, expected)
{
    let threw = false, caught;
    try { fn(); } catch (error) { threw = true; caught = error; }
    assert(threw, true);
    assert(caught, expected);
}

function iterator_zip_source(values, name, log)
{
    let index = 0;
    const source = {
        get next() {
            log.push("get " + name);
            return function() {
                assert(this === source, true);
                assert(arguments.length, 0);
                log.push("next " + name);
                if (index === values.length) return { done: true };
                return { value: values[index++], done: false };
            };
        },
        return() {
            assert(this === source, true);
            assert(arguments.length, 0);
            log.push("close " + name);
            return {};
        },
    };
    return source;
}

function iterator_zip_row(value)
{
    if (Array.isArray(value)) return value;
    return Reflect.ownKeys(value).map(key => value[key]);
}

function test_iterator_zip_common(make)
{
    const marker = {}, close_marker = {};
    let log, a, b, c, helper, result;

    // Sources are acquired now; stepping is deferred and uses cached next.
    log = [];
    a = iterator_zip_source([1, 2], "a", log);
    b = iterator_zip_source([3], "b", log);
    helper = make([a, b]);
    assert(log.join(), "get a,get b");
    Object.defineProperty(a, "next", { value() { throw marker; } });
    result = helper.next(42);
    assert(result.done, false);
    assert(iterator_zip_row(result.value).join(), "1,3");
    assert(log.join(), "get a,get b,next a,next b");
    result = helper.next();
    assert(result.done, true);
    assert(result.value, undefined);
    assert(log.join(), "get a,get b,next a,next b,next a,next b,close a");
    assert(helper.next().done, true);
    assert(helper.return(42).value, undefined);

    // Shortest mode stops immediately and closes remaining records in reverse.
    log = [];
    a = iterator_zip_source([1], "a", log);
    b = iterator_zip_source([], "b", log);
    c = iterator_zip_source([3], "c", log);
    helper = make([a, b, c], { mode: "shortest" });
    log.length = 0;
    assert(helper.next().done, true);
    assert(log.join(), "next a,next b,close c,close a");

    // Longest mode retires exhausted records; padding cannot add a final row.
    log = [];
    a = iterator_zip_source([], "a", log);
    b = iterator_zip_source([1, 2], "b", log);
    helper = make([a, b], { mode: "longest" });
    log.length = 0;
    result = iterator_zip_row(helper.next().value);
    assert(result[0], undefined);
    assert(result[1], 1);
    result = iterator_zip_row(helper.next().value);
    assert(result[0], undefined);
    assert(result[1], 2);
    assert(helper.next().done, true);
    assert(helper.return().done, true);
    assert(log.join(), "next a,next b,next b,next b");
    log = [];
    a = iterator_zip_source([], "a", log);
    b = iterator_zip_source([1, 2], "b", log);
    helper = make([a, b], { mode: "longest" });
    helper.next();
    log.length = 0;
    helper.return();
    assert(log.join(), "close b");

    // Strict mode's final probes read done and deliberately omit value.
    log = [];
    a = iterator_zip_source([], "a", log);
    b = { next() {
        log.push("probe b");
        return { done: true, get value() { throw marker; } };
    }, return() { throw marker; } };
    c = { next() {
        log.push("probe c");
        return { done: true, get value() { throw marker; } };
    }, return() { throw marker; } };
    helper = make([a, b, c], { mode: "strict" });
    log.length = 0;
    assert(helper.next().done, true);
    assert(log.join(), "next a,probe b,probe c");
    log = [];
    a = iterator_zip_source([], "a", log);
    b = { next() {
        log.push("probe b");
        return { done: false, get value() { throw marker; } };
    }, return() { log.push("close b"); throw close_marker; } };
    c = iterator_zip_source([], "c", log);
    helper = make([a, b, c], { mode: "strict" });
    log.length = 0;
    assert_throws(TypeError, () => helper.next());
    assert(log.join(), "next a,probe b,close c,close b");
    assert(helper.next().done, true);
    log = [];
    a = iterator_zip_source([1], "a", log);
    b = iterator_zip_source([], "b", log);
    c = iterator_zip_source([3], "c", log);
    helper = make([a, b, c], { mode: "strict" });
    log.length = 0;
    assert_throws(TypeError, () => helper.next());
    assert(log.join(), "next a,next b,close c,close a");
    log = [];
    helper = make([[1, 2], [3, 4]], { mode: "strict" });
    assert(iterator_zip_row(helper.next().value).join(), "1,3");
    assert(iterator_zip_row(helper.next().value).join(), "2,4");
    assert(helper.next().done, true);

    // Every source protocol failure excludes that record from close-all.
    const failures = [
        () => { throw marker; },
        () => 1,
        () => ({ get done() { throw marker; } }),
        () => ({ done: false, get value() { throw marker; } }),
    ];
    for (let position = 0; position < 3; position++) {
        for (let n = 0; n < failures.length; n++) {
            log = [];
            const sources = ["a", "b", "c"].map(name =>
                iterator_zip_source([1], name, log));
            Object.defineProperty(sources[position], "next", {
                value: failures[n], configurable: true,
            });
            helper = make(sources);
            log.length = 0;
            if (n === 1) assert_throws(TypeError, () => helper.next());
            else iterator_zip_throws_value(() => helper.next(), marker);
            const expected = ["a", "b", "c"].slice(0, position)
                .map(name => "next " + name).concat(["c", "b", "a"]
                    .filter((name, index) => 2 - index !== position)
                    .map(name => "close " + name));
            assert(log.join(), expected.join());
            const length = log.length;
            assert(helper.next().done, true);
            assert(helper.return().done, true);
            assert(log.length, length);
        }
    }
    // Strict IteratorStep must also exclude a failed final probe.
    log = [];
    a = iterator_zip_source([], "a", log);
    b = { next() { throw marker; }, return() { throw close_marker; } };
    c = iterator_zip_source([3], "c", log);
    helper = make([a, b, c], { mode: "strict" });
    log.length = 0;
    iterator_zip_throws_value(() => helper.next(), marker);
    assert(log.join(), "next a,close c");
    // An uncallable cached next fails lazily.
    log = [];
    helper = make([iterator_zip_source([1], "a", log),
                   { next: null, return() { throw marker; } }]);
    assert_throws(TypeError, () => helper.next());
    assert(log.join(), "get a,next a,close a");

    // A throw completion survives all closing getters, calls and bad results.
    for (const thrown of [undefined, null, false, 0, "failure", marker]) {
        log = [];
        helper = make([
            { next() { throw thrown; }, return() { throw close_marker; } },
            { next() { return { value: 1 }; }, get return() {
                log.push("get b return"); throw close_marker;
            } },
            { next() { return { value: 2 }; }, return() {
                log.push("close c"); return 0;
            } },
        ]);
        iterator_zip_throws_value(() => helper.next(), thrown);
        assert(log.join(), "close c,get b return");
    }

    // Return from suspended-start completes before callbacks and closes all.
    for (const started of [false, true]) {
        log = [];
        const sources = ["a", "b", "c"].map(name => ({
            next() { return { value: 1, done: false }; },
            return() {
                log.push("close " + name);
                if (started) {
                    assert_throws(TypeError, () => helper.next());
                    assert_throws(TypeError, () => helper.return());
                } else {
                    assert(helper.next().done, true);
                    assert(helper.return().done, true);
                }
                return {};
            },
        }));
        helper = make(sources);
        if (started) helper.next();
        assert(helper.return("ignored").done, true);
        assert(log.join(), "close c,close b,close a");
        assert(helper.next().done, true);
        assert(helper.return().done, true);
        assert(log.length, 3);
    }
    for (const started of [false, true]) {
        log = [];
        helper = make([
            { next() { return { value: 1 }; }, return() {
                log.push("a"); throw marker;
            } },
            { next() { return { value: 2 }; }, return() {
                log.push("b"); throw close_marker;
            } },
            { next() { return { value: 3 }; }, return() {
                log.push("c"); return {};
            } },
        ]);
        if (started) helper.next();
        iterator_zip_throws_value(() => helper.return(), close_marker);
        assert(log.join(), "c,b,a");
        assert(helper.next().done, true);
    }
    for (const badReturn of [1, () => 1, () => null]) {
        log = [];
        helper = make([
            iterator_zip_source([1], "a", log),
            { next() { return { value: 2 }; }, return: badReturn },
        ]);
        assert_throws(TypeError, () => helper.return());
        assert(log.join(), "get a,close a");
    }
    for (const noReturn of [undefined, null]) {
        helper = make([{ next() { return { value: 1 }; }, return: noReturn }]);
        assert(helper.return().done, true);
    }
    // Duplicate objects are separate records with separate cached next methods.
    log = [];
    const duplicate = iterator_zip_source([1, 2, 3], "duplicate", log);
    helper = make([duplicate, duplicate]);
    assert(iterator_zip_row(helper.next().value).join(), "1,2");
    assert(helper.next().done, true);
    assert(log.join(), "get duplicate,get duplicate,next duplicate,next duplicate,"
           + "next duplicate,next duplicate,close duplicate");
    let duplicateCloses = 0;
    const duplicateStart = { next() { throw marker; }, return() {
        duplicateCloses++; return {};
    } };
    helper = make([duplicateStart, duplicateStart]);
    helper.return();
    assert(duplicateCloses, 2);
    // A shortest normal close failure becomes the first reverse-order throw.
    log = [];
    helper = make([
        { next() { return { done: true }; }, return() { throw marker; } },
        { next() { throw marker; }, return() { log.push("b"); throw marker; } },
        { next() { throw marker; }, return() {
            log.push("c"); throw close_marker;
        } },
    ]);
    iterator_zip_throws_value(() => helper.next(), close_marker);
    assert(log.join(), "c,b");

    // Reentry from next, done and value observes the single helper state.
    for (const phase of ["next", "done", "value"]) {
        const source = { next() {
            if (phase === "next") reenter();
            return {
                get done() { if (phase === "done") reenter(); return false; },
                get value() { if (phase === "value") reenter(); return 7; },
            };
        } };
        function reenter() {
            assert_throws(TypeError, () => helper.next());
            assert_throws(TypeError, () => helper.return());
        }
        helper = make([source]);
        assert(iterator_zip_row(helper.next().value)[0], 7);
        helper.return();
    }

    // Shared helper prototype and borrowed methods retain their native brands.
    helper = make([[1, 2]]);
    const map = Iterator.from([3]).map(value => value);
    const concat = Iterator.concat([4]);
    const proto = Object.getPrototypeOf(map);
    assert(Object.getPrototypeOf(helper) === proto, true);
    assert(helper[Symbol.iterator]() === helper, true);
    assert(Object.prototype.toString.call(helper), "[object Iterator Helper]");
    assert(iterator_zip_row(proto.next.call(helper).value)[0], 1);
    assert(proto.next.call(map).value, 3);
    assert(proto.next.call(concat).value, 4);
    assert(proto.return.call(helper).done, true);
    for (const receiver of [undefined, null, 1, {}, Iterator.prototype]) {
        assert_throws(TypeError, () => proto.next.call(receiver));
        assert_throws(TypeError, () => proto.return.call(receiver));
    }
    for (const mode of ["shortest", "longest", "strict"]) {
        helper = make([], { mode });
        assert(helper.next().done, true);
        assert(helper.return().done, true);
    }
    const symbol = Symbol(), object = {};
    const values = [undefined, null, false, NaN, -0, 1n, symbol, object];
    helper = make([values], { mode: "strict" });
    for (const value of values)
        assert(iterator_zip_row(helper.next().value)[0], value);
    assert(helper.next().done, true);

    // Temporary row values and padding survive GC inside later next calls.
    let held = {}, gcHelper;
    const heldSource = { next() {
        if (!held) return { done: true };
        const value = held;
        held = null;
        value.helper = gcHelper;
        return { value };
    } };
    const gcSource = { next() {
        if (typeof std !== "undefined") std.gc();
        return { value: 2 };
    } };
    gcHelper = make([heldSource, gcSource]);
    assert(iterator_zip_row(gcHelper.next().value)[0].helper === gcHelper, true);
    gcHelper.return();
    gcHelper = null;
    if (typeof std !== "undefined") std.gc();
}

function test_iterator_zip_options(method)
{
    let reads = 0;
    const options = { get mode() { reads++; throw 42; } };
    for (const primitive of [undefined, null, false, 1, 1n, "abc", Symbol()])
        assert_throws(TypeError, () => method(primitive, options));
    assert(reads, 0);
    for (const option of [null, false, 1, 1n, "", Symbol()])
        assert_throws(TypeError, () => method([], option));
    for (const mode of [null, false, 1, 1n, Symbol(), "", "short", "strict\0",
                        "longest\0", new String("shortest"), {
        [Symbol.toPrimitive]() { throw 42; },
    }]) {
        assert_throws(TypeError, () => method([], {
            mode, get padding() { throw 42; },
        }));
    }
    for (const mode of [undefined, "shortest", "strict"])
        assert(method([], { mode, get padding() { throw 42; } }).next().done, true);
    for (const padding of [null, false, 1, 1n, "", Symbol()])
        assert_throws(TypeError, () => method([], { mode: "longest", padding }));
    for (const mode of ["shortest", "longest", "strict"])
        assert(method([], { mode: (mode + "!").slice(0, -1) }).next().done, true);
    iterator_zip_throws_value(() => method([], options), 42);
    assert(reads, 1);
    iterator_zip_throws_value(() => method([], {
        mode: "longest", get padding() { throw 42; },
    }), 42);
    const oldMode = Object.getOwnPropertyDescriptor(Object.prototype, "mode");
    Object.defineProperty(Object.prototype, "mode", {
        configurable: true, get() { throw 42; },
    });
    let completed;
    try { completed = method([]).next().done; }
    finally {
        if (oldMode) Object.defineProperty(Object.prototype, "mode", oldMode);
        else delete Object.prototype.mode;
    }
    assert(completed, true);
    function callableOptions() {}
    callableOptions.mode = "strict";
    assert(method([], callableOptions).next().done, true);
}

function test_iterator_zip_acquisition()
{
    const marker = {}, close_marker = {};
    let log = [], calls = 0;
    const input = {
        [Symbol.iterator]() {
            assert(this === input, true);
            assert(arguments.length, 0);
            log.push("open input");
            return this;
        },
        get next() {
            log.push("get input next");
            return function() {
                assert(this === input, true);
                assert(arguments.length, 0);
                log.push("next input");
                return calls++ ? { done: true } : { value: inner };
            };
        },
        return() { throw marker; },
    };
    const inner = {
        [Symbol.iterator]: null,
        get next() { log.push("get inner next"); return () => ({ value: 9 }); },
    };
    let helper = Iterator.zip(input, {
        get mode() { log.push("mode"); return undefined; },
    });
    assert(log.join(), "mode,open input,get input next,next input,get inner next,next input");
    assert(helper.next().value[0], 9);
    helper.return();
    assert_throws(TypeError, () => Iterator.zip({ next() { return { done: true }; } }));
    assert(Iterator.zip([new String("xy")]).toArray().map(row => row[0]).join(), "x,y");
    const failures = [
        "primitive", null, 1,
        { get [Symbol.iterator]() { throw marker; } },
        { [Symbol.iterator]: 1 },
        { [Symbol.iterator]() { throw marker; } },
        { [Symbol.iterator]() { return 1; } },
        { get next() { throw marker; }, return() { throw close_marker; } },
    ];
    for (const failure of failures) {
        log = [];
        const a = iterator_zip_source([1], "a", log);
        const b = iterator_zip_source([2], "b", log);
        const values = [a, b, failure];
        let index = 0;
        const outer = {
            [Symbol.iterator]() { return this; },
            next() { return { value: values[index++] }; },
            return() { log.push("close input"); throw close_marker; },
        };
        let threw = false, caught;
        try { Iterator.zip(outer); } catch (error) { threw = true; caught = error; }
        assert(threw, true);
        assert(caught === marker || caught instanceof TypeError, true);
        assert(log.join(), "get a,get b,close b,close a,close input");
    }
    // Input IteratorStepValue errors close records but omit the failed input.
    for (const phase of ["call", "result", "done", "value"]) {
        log = [];
        let index = 0;
        const source = iterator_zip_source([1], "a", log);
        const outer = {
            [Symbol.iterator]() { return this; },
            next() {
                if (!index++) return { value: source };
                if (phase === "call") throw marker;
                if (phase === "result") return 1;
                if (phase === "done") return { get done() { throw marker; } };
                return { get value() { throw marker; } };
            },
            return() { log.push("unexpected input close"); return {}; },
        };
        if (phase === "result") assert_throws(TypeError, () => Iterator.zip(outer));
        else iterator_zip_throws_value(() => Iterator.zip(outer), marker);
        assert(log.join(), "get a,close a");
    }
    let closes = 0;
    const outer = { [Symbol.iterator]() { return this; },
        get next() { throw marker; }, return() { closes++; return {}; } };
    iterator_zip_throws_value(() => Iterator.zip(outer), marker);
    assert(closes, 0);
}

function test_iterator_zip_padding()
{
    const marker = {};
    for (let count = 0; count <= 3; count++) {
        for (let length = 0; length <= 5; length++) {
            let steps = 0, closes = 0;
            const padding = {
                [Symbol.iterator]() { assert(this === padding, true); return this; },
                next() {
                    assert(this === padding, true);
                    assert(arguments.length, 0);
                    return steps++ < length ? { value: steps } : { done: true };
                },
                return() {
                    assert(this === padding, true);
                    assert(arguments.length, 0);
                    closes++; return {};
                },
            };
            const helper = Iterator.zip(Array(count).fill([]), { mode: "longest", padding });
            assert(steps, Math.min(count, length + 1));
            assert(closes, count <= length ? 1 : 0);
            assert(helper.next().done, true);
        }
    }
    let helper = Iterator.zip([[], [1, 2], []], {
        mode: "longest", padding: ["a"],
    });
    let row = helper.next().value;
    assert(row[0], "a"); assert(row[1], 1); assert(row[2], undefined);
    row = helper.next().value;
    assert(row[0], "a"); assert(row[1], 2); assert(row[2], undefined);
    assert(helper.next().done, true);
    for (const phase of ["iterator", "next-get", "call", "done", "value", "close"]) {
        const log = [];
        const a = iterator_zip_source([1], "a", log);
        const b = iterator_zip_source([2], "b", log);
        const padding = {
            get [Symbol.iterator]() {
                if (phase === "iterator") throw marker;
                return function() { return this; };
            },
            get next() {
                if (phase === "next-get") throw marker;
                return function() {
                    if (phase === "call") throw marker;
                    if (phase === "done") return { get done() { throw marker; } };
                    if (phase === "value") return { get value() { throw marker; } };
                    return { value: 0 };
                };
            },
            return() { log.push("close padding"); throw marker; },
        };
        iterator_zip_throws_value(() => Iterator.zip([a, b], { mode: "longest", padding }), marker);
        assert(log.join(), "get a,get b," + (phase === "close" ? "close padding," : "")
               + "close b,close a");
    }
    assert_throws(TypeError, () => Iterator.zip([], { mode: "longest", padding: {} }));
    const held = { cycle: null };
    helper = Iterator.zip([[], [1, 2]], { mode: "longest", padding: [held] });
    held.cycle = helper;
    if (typeof std !== "undefined") std.gc();
    assert(helper.next().value[0] === held, true);
    assert(helper.next().value[0] === held, true);
    helper.return();
    held.cycle = null;
}

function test_iterator_zip_results()
{
    assert(Iterator.zip.name, "zip");
    assert(Iterator.zip.length, 1);
    const desc = Object.getOwnPropertyDescriptor(Iterator, "zip");
    assert(desc.writable, true); assert(desc.enumerable, false); assert(desc.configurable, true);
    assert_throws(TypeError, () => new Iterator.zip([]));
    const helper = Iterator.zip([[1, 2], [3, 4]]);
    const first = helper.next().value;
    first[0] = 42;
    const second = helper.next().value;
    assert(first !== second, true);
    assert(second.join(), "2,4");
    assert(Object.getPrototypeOf(second) === Array.prototype, true);
    const element = Object.getOwnPropertyDescriptor(second, "0");
    assert(element.writable, true); assert(element.enumerable, true); assert(element.configurable, true);
    let setterCalls = 0, row;
    const old = Object.getOwnPropertyDescriptor(Array.prototype, "0");
    Object.defineProperty(Array.prototype, "0", { configurable: true,
        set() { setterCalls++; throw 42; } });
    try { row = Iterator.zip([[5]]).next().value; }
    finally {
        if (old) Object.defineProperty(Array.prototype, "0", old);
        else delete Array.prototype[0];
    }
    assert(setterCalls, 0); assert(row[0], 5);
}

function iterator_zip_keyed_make(iterables, options)
{
    const keyed = {};
    for (let i = 0; i < iterables.length; i++) keyed["k" + i] = iterables[i];
    return Iterator.zipKeyed(keyed, options);
}

function test_iterator_zip_keyed_results()
{
    assert(Iterator.zipKeyed.name, "zipKeyed");
    assert(Iterator.zipKeyed.length, 1);
    const desc = Object.getOwnPropertyDescriptor(Iterator, "zipKeyed");
    assert(desc.writable, true); assert(desc.enumerable, false); assert(desc.configurable, true);
    assert_throws(TypeError, () => new Iterator.zipKeyed({}));
    const symbol = Symbol("key");
    const input = Object.create({ inherited: [42] });
    input.b = [2, 3];
    input[2] = [4, 5];
    input[1] = [6, 7];
    input[symbol] = [8, 9];
    input.absent = undefined;
    Object.defineProperty(input, "hidden", { value: [42] });
    Object.defineProperty(input, "__proto__", {
        value: [10, 11], enumerable: true,
    });
    const helper = Iterator.zipKeyed(input);
    const first = helper.next().value;
    assert(Object.getPrototypeOf(first), null);
    const keys = Reflect.ownKeys(first);
    assert(keys.length, 5);
    assert(keys[0], "1"); assert(keys[1], "2"); assert(keys[2], "b");
    assert(keys[3], "__proto__"); assert(keys[4], symbol);
    assert(first[1], 6); assert(first[2], 4); assert(first.b, 2);
    assert(first.__proto__, 10); assert(first[symbol], 8);
    assert(Object.hasOwn(first, "hidden"), false);
    assert(Object.hasOwn(first, "inherited"), false);
    assert(Object.hasOwn(first, "absent"), false);
    for (const key of keys) {
        const property = Object.getOwnPropertyDescriptor(first, key);
        assert(property.writable, true); assert(property.enumerable, true);
        assert(property.configurable, true);
    }
    first.b = 42; delete first[symbol];
    const second = helper.next().value;
    assert(second !== first, true);
    assert(second.b, 3); assert(second[symbol], 9);
    assert(helper.next().done, true);
    const arrayResult = Iterator.zipKeyed([[12], [13]]).next().value;
    assert(Object.getPrototypeOf(arrayResult), null);
    assert(arrayResult[0], 12); assert(arrayResult[1], 13);
    assert(Object.hasOwn(arrayResult, "length"), false);
    const stringRows = Iterator.zipKeyed({ a: new String("xy") }).toArray();
    assert(stringRows.length, 2); assert(stringRows[0].a, "x"); assert(stringRows[1].a, "y");
    let setters = 0;
    const old = Object.getOwnPropertyDescriptor(Object.prototype, "zipOutput");
    Object.defineProperty(Object.prototype, "zipOutput", {
        configurable: true, set() { setters++; throw 42; },
    });
    let result;
    try { result = Iterator.zipKeyed({ zipOutput: [14] }).next().value; }
    finally {
        if (old) Object.defineProperty(Object.prototype, "zipOutput", old);
        else delete Object.prototype.zipOutput;
    }
    assert(setters, 0); assert(result.zipOutput, 14);
    const zip = Iterator.zip([[1]]), keyed = Iterator.zipKeyed({ a: [2] });
    const proto = Object.getPrototypeOf(zip);
    assert(Object.getPrototypeOf(keyed) === proto, true);
    assert(proto.next.call(keyed).value.a, 2);
    assert(proto.return.call(keyed).done, true);
}

function test_iterator_zip_keyed_acquisition()
{
    const marker = {}, symbol = Symbol("s");
    let log = [];
    const a = iterator_zip_source([1], "a", log);
    const b = iterator_zip_source([2], "b", log);
    const proxy = new Proxy({}, {
        ownKeys() { log.push("keys"); return [symbol, "b", "a", "skip", "hidden", "gone"]; },
        getOwnPropertyDescriptor(target, key) {
            log.push("desc " + String(key));
            if (key === "gone") return undefined;
            return { configurable: true, enumerable: key !== "hidden" };
        },
        get(target, key) {
            log.push("get " + String(key));
            if (key === "skip") return undefined;
            if (key === symbol) return b;
            if (key === "b") return a;
            if (key === "a") return [3];
            throw marker;
        },
    });
    let helper = Iterator.zipKeyed(proxy, {
        get mode() { log.push("mode"); return "strict"; },
    });
    assert(log.join(), "mode,keys,desc Symbol(s),get Symbol(s),get b,desc b,get b,get a,"
           + "desc a,get a,desc skip,get skip,desc hidden,desc gone");
    const row = helper.next().value;
    assert(row[symbol], 2); assert(row.b, 1); assert(row.a, 3);
    assert(Reflect.ownKeys(row)[0], "b");
    assert(Reflect.ownKeys(row)[1], "a");
    assert(Reflect.ownKeys(row)[2], symbol);
    helper.return();

    // A single key snapshot still observes later deletion and descriptor edits.
    log = [];
    const input = {
        get a() {
            delete input.b;
            Object.defineProperty(input, "c", { enumerable: false });
            input.added = [99];
            return [4];
        },
        b: [5], c: [6], absent: undefined,
    };
    helper = Iterator.zipKeyed(input);
    const value = helper.next().value;
    assert(Reflect.ownKeys(value).join(), "a"); assert(value.a, 4);
    assert(helper.next().done, true);
    // Options are read before taking the key snapshot.
    const changed = {};
    helper = Iterator.zipKeyed(changed, { get mode() {
        changed.a = [7]; return undefined;
    } });
    assert(helper.next().value.a, 7);

    // Descriptor/get/acquisition errors close only acquired records, in reverse.
    for (const phase of ["desc", "get", "iterator", "next"]) {
        log = [];
        const first = iterator_zip_source([1], "a", log);
        const second = iterator_zip_source([2], "b", log);
        const bad = {
            get [Symbol.iterator]() { if (phase === "iterator") throw marker; },
            get next() { throw marker; },
            return() { log.push("unexpected bad close"); return {}; },
        };
        const iterables = new Proxy({}, {
            ownKeys() { return ["a", "b", "bad"]; },
            getOwnPropertyDescriptor(target, key) {
                if (key === "bad" && phase === "desc") throw marker;
                return { enumerable: true, configurable: true };
            },
            get(target, key) {
                if (key === "a") return first;
                if (key === "b") return second;
                if (phase === "get") throw marker;
                return bad;
            },
        });
        iterator_zip_throws_value(() => Iterator.zipKeyed(iterables), marker);
        assert(log.join(), "get a,get b,close b,close a");
    }
    const badKeys = new Proxy({}, { ownKeys() { throw marker; } });
    iterator_zip_throws_value(() => Iterator.zipKeyed(badKeys), marker);
    for (const bad of [null, false, 1, 1n, "abc", Symbol()])
        assert_throws(TypeError, () => Iterator.zipKeyed({ a: bad }));
    assert(Iterator.zipKeyed({ a: undefined }).next().done, true);
}

function test_iterator_zip_keyed_padding()
{
    const marker = {}, symbol = Symbol("pad");
    let log = [], gets = 0;
    const padding = Object.create({ a: "inherited" });
    Object.defineProperty(padding, symbol, { get() { gets++; return "symbol"; } });
    Object.defineProperty(padding, "skip", { get() { throw marker; } });
    Object.defineProperty(padding, "hidden", { get() { throw marker; } });
    Object.defineProperty(padding, Symbol.iterator, { get() { throw marker; } });
    let helper = Iterator.zipKeyed({ a: [], b: [1, 2], [symbol]: [], skip: undefined }, {
        mode: "longest", padding,
    });
    assert(gets, 1);
    let row = helper.next().value;
    assert(row.a, "inherited"); assert(row.b, 1); assert(row[symbol], "symbol");
    row = helper.next().value;
    assert(row.a, "inherited"); assert(row.b, 2); assert(row[symbol], "symbol");
    assert(helper.next().done, true);
    assert(gets, 1);
    // Pads are read for every included key during construction, after acquisition.
    const a = iterator_zip_source([1], "a", log);
    const b = iterator_zip_source([2], "b", log);
    const options = {
        get mode() { log.push("mode"); return "longest"; },
        get padding() {
            log.push("padding");
            return new Proxy({}, { get(target, key) {
                log.push("pad " + key); return 0;
            } });
        },
    };
    helper = Iterator.zipKeyed({ a, b }, options);
    assert(log.join(), "mode,padding,get a,get b,pad a,pad b");
    helper.return();
    for (const key of ["a", "b"]) {
        log = [];
        const iterables = {
            a: iterator_zip_source([1], "a", log),
            b: iterator_zip_source([2], "b", log),
        };
        const throwing = new Proxy({}, { get(target, property) {
            log.push("pad " + property);
            if (property === key) throw marker;
            return 0;
        } });
        iterator_zip_throws_value(() => Iterator.zipKeyed(iterables, {
            mode: "longest", padding: throwing,
        }), marker);
        assert(log.join(), "get a,get b,pad a," + (key === "b" ? "pad b," : "")
               + "close b,close a");
    }
    gets = 0;
    const unused = new Proxy({}, { get() { gets++; throw marker; } });
    assert(Iterator.zipKeyed({}, { mode: "longest", padding: unused }).next().done, true);
    assert(gets, 0);
    const held = {};
    helper = Iterator.zipKeyed({ a: [], b: [1, 2] }, {
        mode: "longest", padding: { a: held },
    });
    held.helper = helper;
    if (typeof std !== "undefined") std.gc();
    assert(helper.next().value.a === held, true);
    assert(helper.next().value.a === held, true);
    helper.return();
    held.helper = null;
    // Array-shaped padding is read by keys, with no iterable protocol access.
    helper = Iterator.zipKeyed({ 0: [], 1: [3] }, { mode: "longest", padding: [4] });
    assert(helper.next().value[0], 4);
}

test();
test_iterator_zip_common(iterator_zip_keyed_make);
test_iterator_zip_options(Iterator.zipKeyed);
test_iterator_zip_keyed_results();
test_iterator_zip_keyed_acquisition();
test_iterator_zip_keyed_padding();
test_iterator_zip_common((iterables, options) => Iterator.zip(iterables, options));
test_iterator_zip_options(Iterator.zip);
test_iterator_zip_acquisition();
test_iterator_zip_padding();
test_iterator_zip_results();
test_aggregate_error_iterator_close();
test_object_from_entries_close();
test_array_iterator_length();
test_group_by_own_elements();
test_group_by_callback_receiver();
test_function();
test_function_native_fallback();
test_function_initial_name();
test_function_constructor_boundaries();
test_enum();
test_copy_data_property_reentrancy();
test_array();
test_array_constructor_own_elements();
test_array_sort_writeback();
test_string();
test_string_unicode_18();
test_string_normalize();
test_math();
test_number();
test_bigint_to_locale_string();
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
test_atomics_initial_oob();
test_typed_array_constructor_content();
test_typed_array_from_constructor_order();
test_typed_array_species_content();
test_typed_array_set_overlap();
test_typed_array_constructor_length();
test_typed_array_slice_resize();
test_empty_array_buffer();
test_empty_typed_array();
test_dataview_oob_error_order();
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
test_iterator_includes_values();
test_iterator_includes_validation();
test_iterator_includes_protocol();
test_iterator_includes_close();
test_iterator_reduce_close();
test_iterator_join();
test_iterator_join_order();
test_iterator_join_protocol_errors();
test_iterator_join_coercion_close();
test_iterator_join_reentrancy();
test_iterator_join_strings();
test_iterator_limits();
test_iterator_concat_return();
test_iterator_concat_completion();
test_iterator_concat_prototype();
test_iterator_chunks();
test_iterator_windows();
test_iterator_constructor_identity();
test_weak_map();
test_weak_map_cycles();
test_weak_ref();
test_finalization_registry();
test_generator();
test_rope();
test_line_column_numbers();

function test_disposal_symbols()
{
    for (const name of ["dispose", "asyncDispose"]) {
        const symbol = Symbol[name];
        assert(typeof symbol, "symbol");
        assert(symbol.description, "Symbol." + name);
        const descriptor = Object.getOwnPropertyDescriptor(Symbol, name);
        assert(descriptor.value === symbol, true);
        assert(descriptor.writable, false);
        assert(descriptor.enumerable, false);
        assert(descriptor.configurable, false);
        assert(Symbol.keyFor(symbol), undefined);
        assert(Symbol.for("Symbol." + name) === symbol, false);
        const object = { [symbol]: name };
        assert(object[symbol], name);
        assert(Object.getOwnPropertySymbols(object)[0] === symbol, true);
    }
    assert(Symbol.dispose === Symbol.asyncDispose, false);
}

test_disposal_symbols();

function test_suppressed_error()
{
    const first = {}, second = {};
    for (const error of [SuppressedError(first, second),
                        new SuppressedError(first, second)]) {
        assert(error instanceof SuppressedError, true);
        assert(error instanceof Error, true);
        assert(Error.isError(error), true);
        assert(Object.prototype.toString.call(error), "[object Error]");
        assert(error.error === first, true);
        assert(error.suppressed === second, true);
        assert(Object.hasOwn(error, "message"), false);
        assert(Object.hasOwn(error, "cause"), false);
        for (const key of ["error", "suppressed"]) {
            const descriptor = Object.getOwnPropertyDescriptor(error, key);
            assert(descriptor.writable, true);
            assert(descriptor.enumerable, false);
            assert(descriptor.configurable, true);
        }
    }
    const missing = SuppressedError();
    assert(Object.hasOwn(missing, "error"), true);
    assert(Object.hasOwn(missing, "suppressed"), true);
    assert(missing.error, undefined);
    assert(missing.suppressed, undefined);
    assert(SuppressedError.length, 3);
    assert(SuppressedError.name, "SuppressedError");
    assert(Object.getPrototypeOf(SuppressedError) === Error, true);
    assert(Object.getPrototypeOf(SuppressedError.prototype) === Error.prototype,
           true);
    assert(Error.isError(SuppressedError.prototype), false);
    assert(SuppressedError.prototype.name, "SuppressedError");
    assert(SuppressedError.prototype.message, "");
    const text = { toString() { return "message"; } };
    const error = new SuppressedError(first, second, text, {
        get cause() { throw Error("options must be ignored"); }
    });
    const message = Object.getOwnPropertyDescriptor(error, "message");
    assert(message.value, "message");
    assert(message.writable, true);
    assert(message.enumerable, false);
    assert(message.configurable, true);
    assert(error.toString(), "SuppressedError: message");
    class Derived extends SuppressedError {}
    const derived = new Derived(first, second, "subclass");
    assert(derived instanceof Derived, true);
    assert(derived.error === first, true);
    const prototype = {};
    let order = "";
    const target = new Proxy(function() {}, {
        get(target, key) {
            if (key === "prototype") {
                order += "prototype;";
                return prototype;
            }
            return Reflect.get(target, key);
        }
    });
    const custom = Reflect.construct(SuppressedError,
        [first, second, { toString() { order += "message;"; return "x"; } }],
        target);
    assert(Object.getPrototypeOf(custom) === prototype, true);
    assert(order, "prototype;message;");
    assert(Error.isError(custom), true);
    const sentinel = {};
    let caught;
    try {
        SuppressedError(first, second, { toString() { throw sentinel; } });
    } catch (error) {
        caught = error;
    }
    assert(caught === sentinel, true);
}

test_suppressed_error();

function test_disposable_stack_registration()
{
    const prototype = DisposableStack.prototype;
    assert(DisposableStack.length, 0);
    assert(DisposableStack.name, "DisposableStack");
    assert(prototype[Symbol.dispose] === prototype.dispose, true);
    assert(Object.prototype.toString.call(new DisposableStack()),
           "[object DisposableStack]");
    assert(Object.getPrototypeOf(prototype) === Object.prototype, true);
    const disposed = Object.getOwnPropertyDescriptor(prototype, "disposed");
    assert(disposed.set, undefined);
    assert(disposed.enumerable, false);
    assert(disposed.configurable, true);
    for (const [name, length] of [["use", 1], ["adopt", 2], ["defer", 1],
                                 ["move", 0], ["dispose", 0]]) {
        assert(prototype[name].length, length);
        assert(prototype[name].name, name);
        assert(Object.hasOwn(prototype[name], "prototype"), false);
        const descriptor = Object.getOwnPropertyDescriptor(prototype, name);
        assert(descriptor.writable, true);
        assert(descriptor.enumerable, false);
        assert(descriptor.configurable, true);
    }
    assert_throws(TypeError, () => DisposableStack());
    class Derived extends DisposableStack {}
    assert(new Derived() instanceof Derived, true);
    const custom = Reflect.construct(DisposableStack, [],
                                    function Custom() {});
    assert(custom.disposed, undefined);
    assert(disposed.get.call(custom), false);
    const stack = new DisposableStack();
    const order = [];
    const resource = {
        [Symbol.dispose]() {
            "use strict";
            assert(this === resource, true);
            assert(arguments.length, 0);
            order.push("use");
            return { get then() { throw Error("sync result is ignored"); } };
        }
    };
    const adopted = {};
    assert(stack.disposed, false);
    assert(stack.use(resource) === resource, true);
    assert(stack.adopt(adopted, function(value) {
        "use strict";
        assert(this, undefined);
        assert(value === adopted, true);
        assert(arguments.length, 1);
        order.push("adopt");
    }) === adopted, true);
    assert(stack.defer(function() {
        "use strict";
        assert(this, undefined);
        assert(arguments.length, 0);
        order.push("defer");
    }), undefined);
    resource[Symbol.dispose] = () => { throw Error("method must be cached"); };
    assert(stack.use(null), null);
    assert(stack.use(undefined), undefined);
    assert(stack.dispose(), undefined);
    assert(stack.disposed, true);
    assert(order.join(","), "defer,adopt,use");
    assert(stack.dispose(), undefined);
    assert(order.length, 3);
    for (const receiver of [null, undefined, 1, {}, prototype,
                            new Proxy(new DisposableStack(), {})]) {
        assert_throws(TypeError, () => prototype.use.call(receiver, resource));
        assert_throws(TypeError, () => prototype.adopt.call(receiver, 0, () => {}));
        assert_throws(TypeError, () => prototype.defer.call(receiver, () => {}));
        assert_throws(TypeError, () => prototype.move.call(receiver));
        assert_throws(TypeError, () => prototype.dispose.call(receiver));
        assert_throws(TypeError, () => disposed.get.call(receiver));
    }
    const pending = new DisposableStack();
    for (const value of [1, "x", true, Symbol(), 1n, {},
                         { [Symbol.dispose]: null },
                         { [Symbol.dispose]: 1 }]) {
        assert_throws(TypeError, () => pending.use(value));
    }
    assert_throws(TypeError, () => pending.defer(1));
    assert_throws(TypeError, () => pending.adopt({}, null));
    const sentinel = {};
    let reads = 0;
    const throwing = {
        get [Symbol.dispose]() { reads++; throw sentinel; }
    };
    let caught;
    try { pending.use(throwing); } catch (error) { caught = error; }
    assert(caught === sentinel, true);
    assert(reads, 1);
    pending.dispose();
    assert_throws(ReferenceError, () => pending.use(throwing));
    assert(reads, 1);
    assert_throws(ReferenceError, () => pending.adopt({}, 1));
    assert_throws(ReferenceError, () => pending.defer(1));
    assert_throws(ReferenceError, () => pending.move());
}

function test_disposable_stack_move_and_reentrancy()
{
    class Derived extends DisposableStack {}
    const source = new Derived();
    const calls = [];
    source.defer(() => calls.push("first"));
    source.constructor = { get [Symbol.species]() {
        throw Error("move does not consult species");
    } };
    const moved = source.move();
    assert(Object.getPrototypeOf(moved) === DisposableStack.prototype, true);
    assert(moved instanceof Derived, false);
    assert(source.disposed, true);
    assert(moved.disposed, false);
    assert(source.dispose(), undefined);
    assert(calls.length, 0);
    moved.defer(() => calls.push("second"));
    moved.dispose();
    assert(calls.join(","), "second,first");

    const getterSource = new DisposableStack();
    let getterMoved, reads = 0;
    const resource = {
        get [Symbol.dispose]() {
            reads++;
            getterMoved = getterSource.move();
            return function() {
                assert(this === resource, true);
                calls.push("getter move");
            };
        }
    };
    assert(getterSource.use(resource) === resource, true);
    assert(reads, 1);
    assert(getterSource.disposed, true);
    getterSource.dispose();
    assert(calls.length, 2);
    getterMoved.dispose();
    assert(calls[2], "getter move");

    const recursive = new DisposableStack();
    let count = 0;
    recursive.defer(() => {
        count++;
        assert(recursive.disposed, true);
        assert(recursive.dispose(), undefined);
        assert_throws(ReferenceError, () => recursive.defer(() => {}));
    });
    recursive.dispose();
    assert(count, 1);

    const getterDispose = new DisposableStack();
    getterDispose.defer(() => calls.push("existing"));
    getterDispose.use({
        get [Symbol.dispose]() {
            getterDispose.dispose();
            return () => { throw Error("late resource must not be traversed"); };
        }
    });
    assert(getterDispose.disposed, true);
    assert(calls[3], "existing");
    getterDispose.dispose();

    const nested = new DisposableStack();
    nested.use({
        get [Symbol.dispose]() {
            nested.defer(() => calls.push("nested"));
            return () => calls.push("outer");
        }
    });
    nested.dispose();
    assert(calls.slice(-2).join(","), "outer,nested");
}

function collect_disposable_stack_garbage()
{
    if (typeof std !== "undefined" && typeof std.gc === "function")
        std.gc();
    else if (typeof gc === "function")
        gc();
}

function test_disposable_stack_failures_and_gc()
{
    const first = {}, second = {}, third = {};
    const stack = new DisposableStack();
    stack.defer(() => { throw third; });
    stack.defer(() => { throw second; });
    stack.defer(() => { throw first; });
    const intrinsic = SuppressedError;
    globalThis.SuppressedError = function() {
        throw Error("suppression must use the intrinsic");
    };
    let caught;
    try {
        try { stack.dispose(); } catch (error) { caught = error; }
    } finally {
        globalThis.SuppressedError = intrinsic;
    }
    assert(caught instanceof intrinsic, true);
    assert(caught.error === third, true);
    assert(caught.suppressed instanceof intrinsic, true);
    assert(caught.suppressed.error === second, true);
    assert(caught.suppressed.suppressed === first, true);
    assert(stack.disposed, true);
    const undefinedFailure = new DisposableStack();
    undefinedFailure.defer(() => { throw undefined; });
    let threw = false;
    try { undefinedFailure.dispose(); } catch (error) {
        threw = true;
        assert(error, undefined);
    }
    assert(threw, true);
    const gcStack = new DisposableStack();
    let called = 0;
    const resource = {
        stack: gcStack,
        [Symbol.dispose]() { called++; }
    };
    gcStack.use(resource);
    gcStack.defer(() => {
        collect_disposable_stack_garbage();
    });
    collect_disposable_stack_garbage();
    gcStack.dispose();
    assert(called, 1);
    for (let index = 0; index < 100; index++) {
        const cycle = new DisposableStack();
        cycle.use({ stack: cycle, [Symbol.dispose]() {} });
    }
    collect_disposable_stack_garbage();
}

test_disposable_stack_registration();
test_disposable_stack_move_and_reentrancy();
test_disposable_stack_failures_and_gc();
