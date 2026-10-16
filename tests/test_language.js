function assert(actual, expected, message) {
    if (arguments.length == 1)
        expected = true;

    if (Object.is(actual, expected))
        return;

    if (actual !== null && expected !== null
    &&  typeof actual == 'object' && typeof expected == 'object'
    &&  actual.toString() === expected.toString())
        return;

    throw Error("assertion failed: got |" + actual + "|" +
                ", expected |" + expected + "|" +
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
            throw Error("unexpected exception type");
        }
    }
    if (!err) {
        throw Error("expected exception");
    }
}

// load more elaborate version of assert if available
try { __loadScript("test_assert.js"); } catch(e) {}

/*----------------*/

function test_arrow_for_in_grammar()
{
    for (const arrow of ["x =>", "(x) =>", "async x =>", "async (x) =>",
                         "x => y =>", "async x => y =>"]) {
        assert_throws(SyntaxError,
                      () => Function("for (" + arrow + " 0 in 1;;) break;"));
        assert_throws(SyntaxError,
                      () => Function("for (let f = " + arrow + " 0 in 1;;) break;"));
        Function("for (let f = " + arrow + " (0 in {}); false;) break;");
        Function("for (let f = (" + arrow + " 0 in {}); false;) break;");
    }
    Function("for (let f = (x = (0 in {})) => x; false;) break;");
    Function("for (let f = x => { return 0 in {}; }; false;) break;");
    Function("for (let f = async x => { return 0 in {}; }; false;) break;");
    const value = x => 0 in {};
    assert(value(), false);
}

function test_eval_call_receiver()
{
    function parameter(value = eval(`
        function method() { "use strict"; return this; }
        with ({}) { method(); }
    `)) {
        return value;
    }
    assert(parameter(), undefined);
    function body(value = eval(`
        function method() { "use strict"; return this; }
    `)) {
        return eval("method()");
    }
    assert(body(), undefined);
    const object = { method() { "use strict"; return this; } };
    let result;
    with (object) { result = method(); }
    assert(result === object);
    function method() { "use strict"; return this; }
    object[Symbol.unscopables] = { method: true };
    with (object) { result = method(); }
    assert(result, undefined);
}

function test_op1()
{
    var r, a;
    r = 1 + 2;
    assert(r, 3, "1 + 2 === 3");

    r = 1 - 2;
    assert(r, -1, "1 - 2 === -1");

    r = -1;
    assert(r, -1, "-1 === -1");

    r = +2;
    assert(r, 2, "+2 === 2");

    r = 2 * 3;
    assert(r, 6, "2 * 3 === 6");

    r = 4 / 2;
    assert(r, 2, "4 / 2 === 2");

    r = 4 % 3;
    assert(r, 1, "4 % 3 === 3");

    r = 4 << 2;
    assert(r, 16, "4 << 2 === 16");

    r = 1 << 0;
    assert(r, 1, "1 << 0 === 1");

    r = 1 << 31;
    assert(r, -2147483648, "1 << 31 === -2147483648");

    r = 1 << 32;
    assert(r, 1, "1 << 32 === 1");

    r = (1 << 31) < 0;
    assert(r, true, "(1 << 31) < 0 === true");

    r = -4 >> 1;
    assert(r, -2, "-4 >> 1 === -2");

    r = -4 >>> 1;
    assert(r, 0x7ffffffe, "-4 >>> 1 === 0x7ffffffe");

    r = 1 & 1;
    assert(r, 1, "1 & 1 === 1");

    r = 0 | 1;
    assert(r, 1, "0 | 1 === 1");

    r = 1 ^ 1;
    assert(r, 0, "1 ^ 1 === 0");

    r = ~1;
    assert(r, -2, "~1 === -2");

    r = !1;
    assert(r, false, "!1 === false");

    assert((1 < 2), true, "(1 < 2) === true");

    assert((2 > 1), true, "(2 > 1) === true");

    assert(('b' > 'a'), true, "('b' > 'a') === true");

    assert(2 ** 8, 256, "2 ** 8 === 256");
}

function test_cvt()
{
    assert((NaN | 0) === 0);
    assert((Infinity | 0) === 0);
    assert(((-Infinity) | 0) === 0);
    assert(("12345" | 0) === 12345);
    assert(("0x12345" | 0) === 0x12345);
    assert(((4294967296 * 3 - 4) | 0) === -4);

    assert(("12345" >>> 0) === 12345);
    assert(("0x12345" >>> 0) === 0x12345);
    assert((NaN >>> 0) === 0);
    assert((Infinity >>> 0) === 0);
    assert(((-Infinity) >>> 0) === 0);
    assert(((4294967296 * 3 - 4) >>> 0) === (4294967296 - 4));
    assert((19686109595169230000).toString() === "19686109595169230000");
}

function test_eq()
{
    assert(null == undefined);
    assert(undefined == null);
    assert(true == 1);
    assert(0 == false);
    assert("" == 0);
    assert("123" == 123);
    assert("122" != 123);
    assert((new Number(1)) == 1);
    assert(2 == (new Number(2)));
    assert((new String("abc")) == "abc");
    assert({} != "abc");
}

function test_inc_dec()
{
    var a, r;

    a = 1;
    r = a++;
    assert(r === 1 && a === 2, true, "++");

    a = 1;
    r = ++a;
    assert(r === 2 && a === 2, true, "++");

    a = 1;
    r = a--;
    assert(r === 1 && a === 0, true, "--");

    a = 1;
    r = --a;
    assert(r === 0 && a === 0, true, "--");

    a = {x:true};
    a.x++;
    assert(a.x, 2, "++");

    a = {x:true};
    a.x--;
    assert(a.x, 0, "--");

    a = [true];
    a[0]++;
    assert(a[0], 2, "++");

    a = {x:true};
    r = a.x++;
    assert(r === 1 && a.x === 2, true, "++");

    a = {x:true};
    r = a.x--;
    assert(r === 1 && a.x === 0, true, "--");

    a = [true];
    r = a[0]++;
    assert(r === 1 && a[0] === 2, true, "++");

    a = [true];
    r = a[0]--;
    assert(r === 1 && a[0] === 0, true, "--");
}

function F(x)
{
    this.x = x;
}

function test_op2()
{
    var a, b;
    a = new Object;
    a.x = 1;
    assert(a.x, 1, "new");
    b = new F(2);
    assert(b.x, 2, "new");

    a = {x : 2};
    assert(("x" in a), true, "in");
    assert(("y" in a), false, "in");

    a = {};
    assert((a instanceof Object), true, "instanceof");
    assert((a instanceof String), false, "instanceof");

    assert((typeof 1), "number", "typeof");
    assert((typeof Object), "function", "typeof");
    assert((typeof null), "object", "typeof");
    assert((typeof unknown_var), "undefined", "typeof");

    a = {x: 1, if: 2, async: 3};
    assert(a.if === 2);
    assert(a.async === 3);
}

function test_delete()
{
    var a, err;

    a = {x: 1, y: 1};
    assert((delete a.x), true, "delete");
    assert(("x" in a), false, "delete");

    /* the following are not tested by test262 */
    assert(delete "abc"[100], true);

    err = false;
    try {
        delete null.a;
    } catch(e) {
        err = (e instanceof TypeError);
    }
    assert(err, true, "delete");

    err = false;
    try {
        a = { f() { delete super.a; } };
        a.f();
    } catch(e) {
        err = (e instanceof ReferenceError);
    }
    assert(err, true, "delete");
}

function test_constructor()
{
    function *G() {}
    let ex
    try { new G() } catch (ex_) { ex = ex_ }
    assert(ex instanceof TypeError)
    assert(ex.message, "G is not a constructor")
}

function test_prototype()
{
    var f = function f() { };
    assert(f.prototype.constructor, f, "prototype");

    var g = function g() { };
    /* QuickJS bug */
    Object.defineProperty(g, "prototype", { writable: false });
    assert(g.prototype.constructor, g, "prototype");
}

function test_arguments()
{
    function f2() {
        assert(arguments.length, 2, "arguments");
        assert(arguments[0], 1, "arguments");
        assert(arguments[1], 3, "arguments");
    }
    f2(1, 3);
}

function test_super_base_order()
{
    const first = { value: "first", call() { return "first"; } };
    const second = { value: "second", call() { return "second"; } };
    const home = {
        __proto__: first,
        read(key) { return super[key()]; },
        call(key, arg) { return super[key()](arg()); },
        put(key, rhs) { return super[key()] = rhs(); },
    };
    assert(home.read(() => { Object.setPrototypeOf(home, second); return "value"; }),
           "second");
    Object.setPrototypeOf(home, first);
    assert(home.read(() => ({ toString() {
        Object.setPrototypeOf(home, second); return "value";
    }})), "first");
    Object.setPrototypeOf(home, first);
    assert(home.call(() => "call", () => Object.setPrototypeOf(home, second)),
           "first");
    const order = [];
    Object.defineProperty(first, "target", { set(v) { order.push("first:" + v); } });
    Object.defineProperty(second, "target", { set(v) { order.push("second:" + v); } });
    Object.setPrototypeOf(home, first);
    home.put(() => {
        order.push("key");
        return { toString() { order.push("coerce"); return "target"; } };
    }, () => {
        order.push("rhs"); Object.setPrototypeOf(home, second); return 42;
    });
    assert(JSON.stringify(order), '["key","rhs","coerce","first:42"]');
    Object.setPrototypeOf(home, first);
    assert_throws(TypeError, () => home.read(() => {
        Object.setPrototypeOf(home, null); return "value";
    }));
    let evaluated = false;
    class Uninitialized extends Object {
        constructor() { super[(evaluated = true, "value")]; }
    }
    assert_throws(ReferenceError, () => new Uninitialized());
    assert(evaluated, false);
}

function test_super_null_key_coercion()
{
    let coerces = 0, rhs = 0;
    const key = { toString() { coerces++; throw Error("unexpected key coercion"); } };
    const home = {
        __proto__: null,
        read() { return super[key]; },
        prefix() { return ++super[key]; },
        postfix() { return super[key]++; },
        compound() { return super[key] += 1; },
        assign() { super[key] = (++rhs); },
    };
    for (const method of [home.read, home.prefix, home.postfix, home.compound]) {
        assert_throws(TypeError, () => method.call(home));
        assert(coerces, 0);
    }
    assert_throws(TypeError, () => home.assign());
    assert(rhs, 1);
    assert(coerces, 0);
    const error = Error("key conversion");
    Object.setPrototypeOf(home, {});
    const throwing = { toString() { throw error; } };
    const ordinary = { read(k) { return super[k]; } };
    let actual;
    try { ordinary.read(throwing); } catch (e) { actual = e; }
    assert(actual === error);
}

function test_annex_if_function_scopes()
{
    function choose(flag) {
        if (flag) function selected() { return "left"; }
        else function selected() { return "right"; }
        return selected();
    }
    assert(choose(true), "left");
    assert(choose(false), "right");
    function before() {
        let kind;
        if ((kind = typeof hidden, false)) function hidden() {}
        return kind;
    }
    assert(before(), "undefined");
    function blocked(flag) {
        let hidden = "lexical";
        if (flag) function hidden() { return "branch"; }
        return hidden;
    }
    assert(blocked(true), "lexical");
    assert(blocked(false), "lexical");
    function nested(a, b) {
        if (a) if (b) function inner() { return 42; }
        return typeof inner;
    }
    assert(nested(true, true), "function");
    assert(nested(true, false), "undefined");
    assert(nested(false, true), "undefined");
    for (const source of [
        "'use strict'; if (true) function f() {}",
        "if (true) function* f() {}",
        "if (true) async function f() {}",
        "if (true) label: function f() {}",
    ])
        assert_throws(SyntaxError, () => Function(source));
}

function test_annex_function_identity()
{
    function single() {
        let saved;
        {
            saved = local;
            function local() { return local; }
            assert(local === saved);
            assert(local() === saved);
        }
        assert(local === saved);
        return local;
    }
    const first = single(), second = single();
    assert(first !== second);
    function duplicate() {
        let saved;
        {
            saved = local;
            assert(local(), 2);
            function local() { return 1; }
            assert(local === saved);
            function local() { return 2; }
            assert(local === saved);
        }
        assert(local === saved);
        assert(local(), 2);
    }
    duplicate();
    let saved;
    const outer = Function("save", `{
        save(local);
        function local() { return 42; }
    }
    return local;`)(value => { saved = value; });
    assert(outer === saved);
}

function test_annex_duplicate_binding()
{
    function outer() {
        let saved;
        const read = () => local;
        const write = value => { local = value; };
        {
            function local() { return 1; }
            assert(local(), 2);
            write(() => 99);
            assert(read()(), 99);
            function local() { return 2; }
            saved = local;
            assert(read() === saved);
        }
        assert(read() === saved);
    }
    outer();
    function blocked() {
        let local = "outer";
        {
            function local() { return 1; }
            function local() { return 2; }
            assert(local(), 2);
        }
        return local;
    }
    assert(blocked(), "outer");
    for (const body of [
        "'use strict'; { function f() {} function f() {} }",
        "{ let f; function f() {} }",
        "{ function f() {} const f = 1; }",
        "{ function* f() {} function f() {} }",
    ])
        assert_throws(SyntaxError, () => Function(body));
}

function test_eval_catch_var_declaration()
{
    globalThis.evalCatchGlobal = "global";
    function created() {
        let caught;
        try { throw "caught"; } catch (evalCatchGlobal) {
            eval("var evalCatchGlobal = 'initializer';");
            caught = evalCatchGlobal;
        }
        assert(caught, "initializer");
        evalCatchGlobal = "local";
        return evalCatchGlobal;
    }
    assert(created(), "local");
    assert(globalThis.evalCatchGlobal, "global");
    function parameter(value) {
        try { throw 99; } catch (value) {
            eval("var value = 42;");
            assert(value, 42);
        }
        return value;
    }
    assert(parameter(7), 7);
    function uninitialized() {
        try { throw 99; } catch (createdByEval) {
            eval("var createdByEval;");
            assert(createdByEval, 99);
        }
        return createdByEval;
    }
    assert(uninitialized(), undefined);
    delete globalThis.evalCatchGlobal;
}

function test_annex_eval_variable_target()
{
    function local() {
        function target() { return "outer"; }
        const object = { target() { return "with"; } };
        const original = object.target;
        with (object) {
            eval("{ function target() { return 'eval'; } }");
        }
        assert(target(), "eval");
        assert(object.target === original);
        assert(object.target(), "with");
        with (object) {
            eval("target = 42;");
        }
        assert(object.target, 42);
        assert(target(), "eval");
    }
    local();
    function nested() {
        var target = "outer";
        const first = { target: "first" }, second = { target: "second" };
        with (first) {
            with (second) {
                eval("{ function target() { return 42; } }");
            }
        }
        assert(target(), 42);
        assert(first.target, "first");
        assert(second.target, "second");
    }
    nested();
    function caught() {
        var target = "outer";
        try { throw "caught"; } catch (target) {
            eval("{ function target() { return 42; } }");
            assert(target, "caught");
        }
        assert(target(), 42);
    }
    caught();
    function created() {
        const object = { newBinding: 42 };
        with (object) {
            eval("{ function newBinding() { return 7; } }");
        }
        assert(object.newBinding, 42);
        assert(newBinding(), 7);
        assert(delete newBinding);
    }
    created();
    function lexical() {
        const object = { value: 42 };
        with (object) {
            eval("{ let value = 7; assert(value, 7); }");
            eval("var value = 99;");
        }
        assert(object.value, 99);
        assert(value, undefined);
    }
    lexical();
}

function test_parameter_arguments_binding()
{
    function assigned(read = () => arguments) {
        var arguments = 0;
        assert(arguments, 0);
        assert(typeof read(), "object");
        assert(read() !== arguments);
        return read;
    }
    function shared(read = () => arguments) {
        arguments = 23;
        assert(read(), 23);
        (() => { arguments = 29; })();
        assert(read(), 29);
    }
    shared();
    function evalShared(read = () => arguments) {
        eval("arguments = 31;");
        assert(read(), 31);
        (() => eval("arguments = 37;"))();
        assert(read(), 37);
    }
    evalShared();
    const first = assigned(), second = assigned();
    assert(first() !== second());
    function uninitialized(read = () => arguments) {
        var arguments;
        assert(read() === arguments);
        arguments = 0;
        assert(typeof read(), "object");
        assert(arguments, 0);
    }
    uninitialized();
    function hoisted(read = () => arguments) {
        function arguments() { return 42; }
        assert(arguments(), 42);
        assert(typeof read(), "object");
    }
    hoisted();
    function nested(factory = () => () => arguments) {
        var arguments = 7;
        assert(typeof factory()(), "object");
        assert(arguments, 7);
    }
    nested();
    function strict(read = () => arguments) {
        const original = arguments;
        assert(read() === original);
        return read;
    }
    const strictRead = Function(`"use strict";
        return function (read = () => arguments) {
            const original = arguments;
            assert(read() === original);
            return read;
        };`)()();
    assert(typeof strictRead(), "object");
    strict();
    function formal(read = () => arguments, arguments = 7) {
        assert(read(), 7);
        var arguments = 9;
        assert(read(), 7);
        assert(arguments, 9);
        eval("var arguments = 15;");
        assert(arguments, 15);
        assert(read(), 7);
    }
    formal();
    function destructured(read = () => arguments, { arguments } = { arguments: 11 }) {
        assert(read(), 11);
        var arguments = 13;
        assert(read(), 11);
        assert(arguments, 13);
        eval("var arguments = 17;");
        assert(arguments, 17);
        assert(read(), 11);
    }
    destructured();
    function evaluated(read = eval("() => arguments")) {
        var arguments = 17;
        assert(typeof read(), "object");
        assert(arguments, 17);
    }
    evaluated();
    function bodyEval(read = () => arguments) {
        eval("var arguments = 19;");
        assert(typeof read(), "object");
        assert(arguments, 19);
    }
    bodyEval();
}

function test_annex_deferred_applicability()
{
    function later() {
        let saved;
        { function target() { return 1; } saved = target; }
        { { function target() { return 2; } } let target; }
        { let target; { function target() { return 3; } } }
        assert(target === saved);
    }
    later();
    function absent() {
        { { function onlyBlocked() {} } const onlyBlocked = 42; }
        assert(typeof onlyBlocked, "undefined");
        assert_throws(ReferenceError, () => onlyBlocked);
    }
    absent();
    function explicit() {
        var target = 42;
        { { function target() {} } let target; }
        assert(target, 42);
    }
    explicit();
    function formal({target}) {
        { function target() {} }
        assert(target, 42);
    }
    formal({ target: 42 });
    function evalConst() {
        eval("{ function target() {} }");
        const target = 42;
        assert(target, 42);
    }
    evalConst();
    function evalLet() {
        eval("{ function target() {} }");
        let target = 42;
        assert(target, 42);
    }
    evalLet();
    function evalBlocked() {
        let target = 42;
        eval("{ function target() {} }");
        assert(target, 42);
    }
    evalBlocked();
    function catchAllowed() {
        try { throw 42; } catch (target) {
            { function target() { return 7; } }
        }
        assert(target(), 7);
    }
    catchAllowed();
    function evalExplicit() {
        eval("assert(target(), 42); { function target() {} } function target() { return 42; }");
        assert(target(), undefined);
    }
    evalExplicit();
    assert_throws(SyntaxError, () => Function("{ var target; } let target;"));
}

function test_annex_arguments_binding()
{
    function simple() {
        assert(typeof arguments, "object");
        let saved;
        { saved = arguments; function arguments() { return 42; } }
        assert(arguments === saved);
        assert(arguments(), 42);
    }
    simple();
    function evaluated() {
        eval("42");
        { function arguments() { return 42; } }
        assert(arguments(), 42);
    }
    evaluated();
    function formal(arguments) {
        { function arguments() {} }
        assert(arguments, 7);
    }
    formal(7);
    function defaultFormal(arguments = 7) {
        { function arguments() {} }
        assert(arguments, 7);
    }
    defaultFormal();
    function separate(read = () => arguments) {
        arguments = 1;
        assert(read(), 1);
        const bodyRead = () => arguments;
        let saved;
        { saved = arguments; function arguments() { return 42; } }
        assert(arguments === saved);
        assert(bodyRead() === saved);
        assert(read(), 1);
        assert(arguments(), 42);
    }
    separate();
    function skipped(read = () => arguments) {
        arguments = 3;
        if (false) { function arguments() {} }
        assert(arguments, 3);
        assert(read(), 3);
    }
    skipped();
    function explicit(read = () => arguments) {
        var arguments;
        const initial = arguments;
        { function arguments() { return 42; } }
        assert(arguments(), 42);
        assert(read() === initial);
    }
    explicit();
    function nestedEval(read = () => arguments) {
        arguments = 5;
        eval("{ function arguments() { return 42; } }");
        assert(arguments(), 42);
        assert(read(), 5);
    }
    nestedEval();
    assert_throws(SyntaxError, () => Function("'use strict'; { function arguments() {} }"));
    const strictFunction = Function(`"use strict"; return function(read = () => arguments) {
        const before = arguments;
        { function local() {} }
        assert(arguments === before);
        assert(read() === before);
    };`)();
    strictFunction();
}

function test_class_lexical_strictness()
{
    const readonly = Object.defineProperty({}, "value", { value: 0 });
    const accessor = { get value() { return 0; } };
    const frozen = Object.preventExtensions({});
    const proxy = new Proxy({}, { set() { return false; } });
    for (const object of [readonly, accessor, frozen, proxy]) {
        assert_throws(TypeError, () => { class C { [object.value = 1]() {} } });
        assert_throws(TypeError, () => { const C = class extends (object.value = 1, Object) {}; });
        assert_throws(TypeError, () => { class C { [object["value"] = 1]() {} } });
    }
    const noDelete = Object.defineProperty({}, "value", { value: 0 });
    assert_throws(TypeError, () => { class C { [delete noDelete.value]() {} } });
    assert_throws(TypeError, () => { class C { [delete noDelete?.value]() {} } });
    delete globalThis.classStrictMissing;
    assert_throws(ReferenceError, () => { class C { [classStrictMissing = 1]() {} } });
    assert(!Object.hasOwn(globalThis, "classStrictMissing"));
    function direct() {
        class C { [eval("var classEvalLocal = 42; 'method'")]() {} }
        assert(typeof classEvalLocal, "undefined");
        class D { [eval(...["var classSpreadLocal = 42; 'method'"])]() {} }
        assert(typeof classSpreadLocal, "undefined");
    }
    direct();
    function indirect() {
        class C { [(0, eval)("classIndirectGlobal = 42; 'method'")]() {} }
        assert(globalThis.classIndirectGlobal, 42);
    }
    indirect();
    delete globalThis.classIndirectGlobal;
    function sloppy() { readonly.value = 1; return "method"; }
    class C { [sloppy()]() {} }
    assert(readonly.value, 0);
    assert_throws(TypeError, () => (function self() {
        class C extends (self = 1, Object) {}
    })());
    const object = Object.defineProperty({}, "value", { value: 0, writable: false });
    with (object) {
        assert_throws(TypeError, () => { class C { [value = 1]() {} } });
    }
    assert(object.value, 0);
    function afterException() {
        try { class C { [readonly.value = 1]() {} } } catch (error) {}
        readonly.value = 1;
        return 42;
    }
    assert(afterException(), 42);
    (function self() {
        with ({}) {
            class C { [self ||= "unused"]() {} }
            let effects = 0;
            assert_throws(TypeError, () => {
                class D { [self = (++effects, "method")]() {} }
            });
            assert(effects, 1);
            assert_throws(TypeError, () => {
                class D { [self &&= (++effects, "method")]() {} }
            });
            assert(effects, 2);
        }
    })();
    let probes = 0;
    const scope = new Proxy({}, {
        has(target, key) {
            return key === "classStrictFunction" && ++probes === 1;
        }
    });
    assert_throws(ReferenceError, () => {
        with (scope) {
            class C { [classStrictFunction()]() {} }
        }
    });
    assert(probes, 2);
    probes = 0;
    assert_throws(ReferenceError, () => {
        with (scope) {
            class C { [classStrictFunction]() {} }
        }
    });
    assert(probes, 2);
    let effects = 0;
    assert_throws(TypeError, () => (function self() {
        class C { [eval("self = (++effects, 1)")]() {} }
    })());
    assert(effects, 1);
    assert_throws(TypeError, () => (function self() {
        class C { [(() => { self = (++effects, 1); })()]() {} }
    })());
    assert(effects, 2);
    assert_throws(TypeError, () => (function self() {
        eval("'use strict'; self = (++effects, 1)");
    })());
    assert(effects, 3);
    let calls = 0;
    const environment = {
        fn() { assert(this === environment); calls++; return "method"; },
        missing: null
    };
    with (environment) {
        class C { [fn?.()]() {} }
        class D { [fn?.(...[])]() {} }
        class E { [missing?.()]() {} }
        fn?.();
        missing?.(...[]);
    }
    assert(calls, 3);
}

const global_const_reference = 1;

function test_const_write_tdz()
{
    let effects = 0;
    assert_throws(ReferenceError, function() {
        binding = ++effects;
        const binding = 1;
    });
    assert(effects, 1);
    effects = 0;
    assert_throws(ReferenceError, function() {
        const binding = (binding = ++effects);
    });
    assert(effects, 1);
    effects = 0;
    assert_throws(ReferenceError, function() {
        "use strict";
        binding = ++effects;
        const binding = 1;
    });
    assert(effects, 1);
    effects = 0;
    assert_throws(ReferenceError, function() {
        const run = () => { binding = ++effects; };
        run();
        const binding = 1;
    });
    assert(effects, 1);
    effects = 0;
    assert_throws(ReferenceError, function() {
        const run = () => () => { binding = ++effects; };
        run()();
        const binding = 1;
    });
    assert(effects, 1);
    effects = 0;
    assert_throws(ReferenceError, function() {
        eval("binding = ++effects");
        const binding = 1;
    });
    assert(effects, 1);
    effects = 0;
    assert_throws(ReferenceError, function() {
        const run = () => eval("binding = ++effects");
        run();
        const binding = 1;
    });
    assert(effects, 1);
    const rhsError = new Error("RHS");
    function abrupt() { throw rhsError; }
    try {
        const binding = (binding = abrupt());
        assert(false);
    } catch (e) {
        assert(e, rhsError);
    }
    effects = 0;
    const initialized = 1;
    assert_throws(TypeError, () => { initialized = ++effects; });
    assert(effects, 1);
    assert(initialized, 1);
    effects = 0;
    assert_throws(ReferenceError, () => { global_const_write_tdz = ++effects; });
    assert(effects, 1);
    globalThis.const_write_effects = 0;
    assert_throws(ReferenceError, () => {
        (0, eval)("global_const_write_tdz = ++globalThis.const_write_effects");
    });
    assert(globalThis.const_write_effects, 1);
    delete globalThis.const_write_effects;
}

function test_const_reference_timing()
{
    let effects = 0;
    const truthy = 1;
    const falsy = 0;
    const absent = null;
    with ({}) {
        assert(truthy ||= ++effects, 1);
        assert(truthy ??= ++effects, 1);
        assert(falsy &&= ++effects, 0);
        assert(effects, 0);
        assert_throws(TypeError, () => { truthy &&= ++effects; });
        assert(effects, 1);
        assert_throws(TypeError, () => { falsy ||= ++effects; });
        assert(effects, 2);
        assert_throws(TypeError, () => { absent ??= ++effects; });
        assert(effects, 3);
        assert_throws(TypeError, () => { truthy = ++effects; });
        assert(effects, 4);
        assert(truthy, 1);
    }
    /* The lexical binding can precede the with object in the scope chain. */
    with ({}) {
        const local = 1;
        assert(local ||= ++effects, 1);
        assert_throws(TypeError, () => { local = ++effects; });
        assert(effects, 5);
        assert(local, 1);
    }
    function captured() {
        const binding = 1;
        return function() {
            with ({}) {
                assert(binding ||= ++effects, 1);
                assert_throws(TypeError, () => { binding = ++effects; });
                assert(binding, 1);
            }
        };
    }
    captured()();
    assert(effects, 6);
    eval("with ({}) { truthy ||= ++effects; }");
    assert(effects, 6);
    assert_throws(TypeError, () => {
        eval("with ({}) { truthy = ++effects; }");
    });
    assert(effects, 7);
    with ({}) {
        assert(global_const_reference ||= ++effects, 1);
        assert_throws(TypeError, () => { global_const_reference = ++effects; });
        assert(effects, 8);
    }
    Object.defineProperty(globalThis, "readonly_global_reference", {
        value: 1, configurable: true
    });
    (0, eval)("var readonly_global_reference;" +
              " with ({}) { readonly_global_reference = 2; }");
    assert(globalThis.readonly_global_reference, 1);
    delete globalThis.readonly_global_reference;
    effects = 0;
    assert_throws(ReferenceError, function() {
        with ({}) { binding = ++effects; }
        const binding = 1;
    });
    assert(effects, 1);
    effects = 0;
    assert_throws(ReferenceError, function() {
        with ({}) { binding ||= ++effects; }
        const binding = 1;
    });
    assert(effects, 0);
    assert_throws(ReferenceError, function() {
        with ({}) { binding = ++effects; }
        let binding;
    });
    assert(effects, 1);
    function capturedTDZ(write) {
        const run = function() {
            with ({}) {
                if (write)
                    binding = ++effects;
                else
                    binding ||= ++effects;
            }
        };
        run();
        const binding = 1;
    }
    effects = 0;
    assert_throws(ReferenceError, () => capturedTDZ(true));
    assert(effects, 1);
    effects = 0;
    assert_throws(ReferenceError, () => capturedTDZ(false));
    assert(effects, 0);
}

function test_parameter_environment_bindings()
{
    function shared(a = 1, read = () => a) {
        a = 2;
        assert(read(), 2);
        eval("a = 3");
        assert(read(), 3);
        const write = () => { a = 4; };
        write();
        assert(read(), 4);
        return a;
    }
    assert(shared(), 4);
    function separate(a = 1, read = () => a) {
        var a;
        a = 2;
        assert(read(), 1);
        eval("a = 3");
        assert(read(), 1);
        return a;
    }
    assert(separate(), 3);
    function current(a = 1, b = (a = 7), read = () => a) {
        var a;
        assert(a, 7);
        a = 8;
        assert(read(), 7);
    }
    current();
    function destructured({ a } = { a: 1 }, read = () => a) {
        a = 2;
        assert(read(), 2);
        assert(eval("a"), 2);
    }
    destructured();
    function destructuredSeparate({ a } = { a: 1 }, b = (a = 7), read = () => a) {
        var a;
        assert(a, 7);
        a = 8;
        assert(read(), 7);
    }
    destructuredSeparate();
    function dynamic(a = 1, read = () => a) {
        const bodyRead = () => a;
        const bodyEval = () => eval("a");
        assert(bodyRead(), 1);
        assert(bodyEval(), 1);
        eval("var a = 9");
        assert(bodyRead(), 9);
        assert(bodyEval(), 9);
        assert(read(), 1);
    }
    dynamic();
    function hoisted(a = 1, read = () => a) {
        function a() { return 42; }
        assert(a(), 42);
        assert(read(), 1);
    }
    hoisted();
    const arrow = (a = 1, read = () => a) => { a = 2; return read(); };
    assert(arrow(), 2);
    function* generator(a = 1, read = () => a) { a = 2; yield read(); }
    assert(generator().next().value, 2);
    const rest = (a = 1, ...values) => { a = 2; return [a, values.length]; };
    assert(rest()[0], 2);
    assert(rest()[1], 0);
}

function test_computed_parameter_environment()
{
    let read;
    function key(fn) { read = fn; return "value"; }
    function separate({ [key(() => a)]: a }) {
        var a;
        a = 2;
        return read();
    }
    assert(separate({ value: 1 }), 1);
    function shared({ [key(() => a)]: a }) {
        a = 2;
        return read();
    }
    assert(shared({ value: 1 }), 2);
    function nested([{ [key(() => a)]: a }]) {
        var a;
        a = 2;
        return read();
    }
    assert(nested([{ value: 1 }]), 1);
    function later({ ignored, [key(() => a)]: a }) {
        var a;
        a = 2;
        return read();
    }
    assert(later({ value: 1 }), 1);
    const arrow = ({ [key(() => a)]: a }) => {
        var a;
        a = 2;
        return read();
    };
    assert(arrow({ value: 1 }), 1);
    function* generator({ [key(() => a)]: a }) {
        var a;
        a = 2;
        yield read();
    }
    assert(generator({ value: 1 }).next().value, 1);
    const obj = {
        method({ [key(() => a)]: a }) {
            var a;
            a = 2;
            return read();
        }
    };
    assert(obj.method({ value: 1 }), 1);
    let a = "value";
    function uninitialized({ [a]: a }) {}
    assert_throws(ReferenceError, () => uninitialized({ value: 1 }));
    function future({ [b]: a }, b) {}
    assert_throws(ReferenceError, () => future({ value: 1 }, "value"));
    function evaluated({ [eval("a")]: a }) {}
    assert_throws(ReferenceError, () => evaluated({ value: 1 }));
    function plainObject({ first: [a], nested: { b } }) {
        var a, b;
        a++;
        b++;
        assert(eval("a + b"), 5);
    }
    plainObject({ first: [1], nested: { b: 2 } });
    function plainArray([a, { b }]) {
        var a, b;
        a++;
        b++;
        assert(eval("a + b"), 5);
    }
    plainArray([1, { b: 2 }]);
}

function test_class_element_line_terminators()
{
    for (const separator of ["\n", "\r", "\r\n", "\u2028", "\u2029", "/*\n*/"]) {
        const Getter = eval("(class { get" + separator + "x() { return 7; } })");
        const getter = new Getter();
        assert(getter.x, 7);
        assert(Object.hasOwn(getter, "get"), false);

        const Setter = eval("(class { set" + separator + "x(value) { this.y = value; } })");
        const setter = new Setter();
        setter.x = 42;
        assert(setter.y, 42);
        assert(Object.hasOwn(setter, "set"), false);

        const StaticName = eval("(class { static" + separator + "() { return 3; } })");
        assert(new StaticName().static(), 3);
        const StaticField = eval("(class { static" + separator + "= 5; })");
        assert(new StaticField().static, 5);

        for (const name of ["get", "set"]) {
            const Generator = eval("(class { " + name + separator + "*x() { yield 9; } })");
            const generator = new Generator();
            assert(Object.hasOwn(generator, name), true);
            assert(generator.x().next().value, 9);
            assert_throws(SyntaxError, () => eval("class C { " + name + separator + "x; }"));
            assert_throws(SyntaxError, () => eval("class C { " + name + " *x() {} }"));
        }
    }
}

function test_class()
{
    var o;
    class C {
        constructor() {
            this.x = 10;
        }
        f() {
            return 1;
        }
        static F() {
            return -1;
        }
        get y() {
            return 12;
        }
    };
    class D extends C {
        constructor() {
            super();
            this.z = 20;
        }
        g() {
            return 2;
        }
        static G() {
            return -2;
        }
        h() {
            return super.f();
        }
        static H() {
            return super["F"]();
        }
    }

    assert(C.F() === -1);
    assert(Object.getOwnPropertyDescriptor(C.prototype, "y").get.name === "get y");

    o = new C();
    assert(o.f() === 1);
    assert(o.x === 10);

    assert(D.F() === -1);
    assert(D.G() === -2);
    assert(D.H() === -1);

    o = new D();
    assert(o.f() === 1);
    assert(o.g() === 2);
    assert(o.x === 10);
    assert(o.z === 20);
    assert(o.h() === 1);

    /* test class name scope */
    var E1 = class E { static F() { return E; } };
    assert(E1 === E1.F());

    class S {
        static x = 42;
        static y = S.x;
        static z = this.x;
    }
    assert(S.x === 42);
    assert(S.y === 42);
    assert(S.z === 42);

    class P {
        get = () => "123";
        static() { return 42; }
    }
    assert(new P().get() === "123");
    assert(new P().static() === 42);
};

function test_class_strict_region_control_flow()
{
    const frozen = Object.freeze({});
    let caught = 0;
    try {
        class Outer { [(class Inner {}, frozen.x = 1)]() {} }
    } catch (e) {
        assert(e instanceof TypeError);
        frozen.x = 2;
        eval("var after_catch = 3");
        caught++;
    }
    assert(caught, 1);
    assert(after_catch, 3);
    assert(frozen.x, undefined);

    const Named = class {};
    const object = { Named: class {} };
    assert(Named.name, "Named");
    assert(object.Named.name, "Named");
    const key = Symbol("computed");
    const computed = {
        [key]: class { [eval("'method'")]() {} },
        plain: { [key]: class {} },
        nested: class { ["inner"] = { [key]: class {} }; },
    };
    assert(computed[key].name, "[computed]");
    assert(computed.plain[key].name, "[computed]");
    assert(new computed.nested().inner[key].name, "[computed]");
    class Plain { method() { "use strict"; frozen.x = 1; } }
    assert_throws(TypeError, () => new Plain().method());
    frozen.x = 2;

    /* A strict enclosing function must retain its mode after the catch. */
    (function() {
        "use strict";
        try {
            class C extends (frozen.x = 1) {}
        } catch (e) {
            assert(e instanceof TypeError);
            assert_throws(TypeError, () => { frozen.x = 2; });
            eval("var strict_catch_only = 1");
            assert(typeof strict_catch_only, "undefined");
        }
    })();

    /* Class cleanup must not consume an outer catch marker when the
       generator is returned from, or replace its finally. */
    for (const source of [
        "class C { [(yield 1)]() {} }",
        "class C extends (yield 1) {}",
        "class C { [(yield 1, eval('0'))]() {} }",
    ]) {
        const iterator = Function("frozen", `
            return (function* () {
                try { ${source} }
                finally {
                    frozen.x = 2;
                    eval("var restored = 3");
                    yield restored;
                }
            })();
        `)(frozen);
        assert(iterator.next().value, 1);
        assert(iterator.return(4).value, 3);
        assert(iterator.next().value, 4);
    }

    for (const completion of ["next", "return", "throw"]) {
        let closed = 0, finalized = 0;
        const sentinel = {};
        const iterable = {
            [Symbol.iterator]() { return this; },
            next() { return { value: 1, done: false }; },
            return() { closed++; return {}; }
        };
        function* suspended() {
            try {
                for (const value of iterable) {
                    class C {
                        [(yield 1, eval("var class_only = 1"), "key")]() {}
                    }
                    assert(typeof class_only, "undefined");
                    frozen.x = 3;
                    break;
                }
            } finally {
                frozen.x = 4;
                eval("var after_finally = 5");
                assert(after_finally, 5);
                finalized++;
            }
        }
        const iterator = suspended();
        assert(iterator.next().value, 1);
        if (completion === "throw") {
            let error;
            try { iterator.throw(sentinel); } catch (e) { error = e; }
            assert(error, sentinel);
        } else {
            assert(iterator[completion](2).done, true);
        }
        assert(closed, 1);
        assert(finalized, 1);
    }
}

function test_template()
{
    var a, b;
    b = 123;
    a = `abc${b}d`;
    assert(a, "abc123d");

    a = String.raw `abc${b}d`;
    assert(a, "abc123d");

    a = "aaa";
    b = "bbb";
    assert(`aaa${a, b}ccc`, "aaabbbccc");
}

function test_template_skip()
{
    var a = "Bar";
    var { b = `${a + `a${a}` }baz` } = {};
    assert(b, "BaraBarbaz");
}

function test_template_coercion()
{
    var log = [];
    var value = {
        [Symbol.toPrimitive](hint) {
            log.push(hint);
            return "first";
        }
    };
    assert(`${value}${(log.push("next"), 2)}end`, "first2end");
    assert(log.join(), "string,next");
    assert(`${value}${value}`, "firstfirst");
    var concat = Object.getOwnPropertyDescriptor(String.prototype, "concat");
    Object.defineProperty(String.prototype, "concat", {
        get() { throw new Error("template read concat"); },
        configurable: true
    });
    try {
        assert(`a${1}b${2}`, "a1b2");
        assert(`${""}${""}`, "");
    } finally {
        Object.defineProperty(String.prototype, "concat", concat);
    }
    log = [];
    value = { toString() { throw 37; } };
    try {
        `${value}${log.push("unreachable")}`;
        assert(false);
    } catch (e) {
        assert(e, 37);
    }
    assert(log.length, 0);
    try {
        `${Symbol()}${log.push("unreachable")}`;
        assert(false);
    } catch (e) {
        assert(e instanceof TypeError);
    }
    assert(log.length, 0);
    function tag(strings, first, second) {
        assert(first, value);
        assert(second, symbol);
        return strings.raw.join("|");
    }
    var symbol = Symbol();
    assert(tag`a${value}b${symbol}c`, "a|b|c");
}

function test_negated_condition()
{
    function branch(value) {
        if (!value)
            return 1;
        return 2;
    }
    var falsy = [false, 0, -0, NaN, "", null, undefined, 0n];
    var truthy = [true, 1, -1, "x", 1n, Symbol(), {},
        { [Symbol.toPrimitive]() { throw new Error("unexpected coercion"); } }];
    for (var value of falsy)
        assert(branch(value), 1);
    for (var value of truthy)
        assert(branch(value), 2);
    var calls = 0, iterations = 0;
    function done() { return ++calls == 3; }
    do {
        iterations++;
    } while (!done());
    assert(calls, 3);
    assert(iterations, 3);
    assert(!truthy[truthy.length - 1], false);
}

function test_object_literal()
{
    var x = 0, get = 1, set = 2, async = 3;
    a = { get: 2, set: 3, async: 4, get a(){ return this.get} };
    assert(JSON.stringify(a), '{"get":2,"set":3,"async":4,"a":2}');
    assert(a.a === 2);

    a = { x, get, set, async };
    assert(JSON.stringify(a), '{"x":0,"get":1,"set":2,"async":3}');
}

function test_regexp_skip()
{
    var a, b;
    [a, b = /abc\(/] = [1];
    assert(a === 1);

    [a, b =/abc\(/] = [2];
    assert(a === 2);
}

function test_labels()
{
    do x: { break x; } while(0);
    if (1)
        x: { break x; }
    else
        x: { break x; }
    with ({}) x: { break x; };
    while (0) x: { break x; };
}

function test_labels2()
{
    while (1) label: break
    var i = 0
    while (i < 3) label: {
        if (i > 0)
            break
        i++
    }
    assert(i, 1)
    for (;;) label: break
    for (i = 0; i < 3; i++) label: {
        if (i > 0)
            break
    }
    assert(i, 1)
}

function test_destructuring_property_colon()
{
    const invalid = [
        "({''})",
        "({''} = {})",
        "var {''};",
        "var {'', a} = {a: 0};",
        "var {'bad'};",
        "({'bad'} = {bad: 0});",
        "var {'if'};",
        "function f({''}) {}",
        "function f({a, 'bad', c}) {}",
        "let {'quoted', target} = {};",
        "const {'quoted', target} = {};",
        "({'quoted', target} = {});",
        "function f({'quoted', target}) {}",
        "({'quoted', target}) => target;",
        "let {nested: {'quoted', target}} = {};",
        "for (let {'quoted', target} of []) {}",
        "try {} catch ({'quoted', target}) {}",
        "var {0, target} = {};",
        "var {['computed'], target} = {};",
    ];
    for (const source of invalid) {
        assert_throws(SyntaxError, () => Function(source));
        assert_throws(SyntaxError, () => Function('"use strict"; ' + source));
    }

    const source = {'': 1, if: 2, 0: 3, computed: 4, quoted: 5};
    var {'': empty, 'if': keyword, 0: numeric,
         ['computed']: computed} = source;
    assert(empty, 1);
    assert(keyword, 2);
    assert(numeric, 3);
    assert(computed, 4);
    let {'quoted': alias, ...rest} = source;
    assert(alias, 5);
    assert(rest.quoted, undefined);
    assert(rest[''], 1);
    const {nested: {'quoted': nested}} = {nested: source};
    assert(nested, 5);
    function binding({'quoted': value = 6}) { return value; }
    assert(binding(source), 5);
    assert(binding({}), 6);
    assert((({'quoted': value}) => value)(source), 5);
    let assigned;
    ({'quoted': assigned} = source);
    assert(assigned, 5);
    ({['computed']: assigned} = source);
    assert(assigned, 4);
    const shorthand = 7;
    const {shorthand: ordinary} = {shorthand};
    assert(ordinary, 7);
    function shorthand_binding({shorthand}) { return shorthand; }
    assert(shorthand_binding({shorthand}), 7);
}

function test_destructuring()
{
    function * g () { return 0; };
    var [x] = g();
    assert(x, void 0);
}

function test_spread()
{
    var x;
    x = [1, 2, ...[3, 4]];
    assert(x.toString(), "1,2,3,4");

    x = [ ...[ , ] ];
    assert(Object.getOwnPropertyNames(x).toString(), "0,length");
}

function test_function_length()
{
    assert( ((a, b = 1, c) => {}).length, 1);
    assert( (([a,b]) => {}).length, 1);
    assert( (({a,b}) => {}).length, 1);
    assert( ((c, [a,b] = 1, d) => {}).length, 1);
}

function test_argument_scope()
{
    var f;
    var c = "global";

    (function() {
        "use strict";
        // XXX: node only throws in strict mode
        f = function(a = eval("var arguments")) {};
        assert_throws(SyntaxError, f);
    })();

    f = function(a = eval("1"), b = arguments[0]) { return b; };
    assert(f(12), 12);

    f = function(a, b = arguments[0]) { return b; };
    assert(f(12), 12);

    f = function(a, b = () => arguments) { return b; };
    assert(f(12)()[0], 12);

    f = function(a = eval("1"), b = () => arguments) { return b; };
    assert(f(12)()[0], 12);

    (function() {
        "use strict";
        f = function(a = this) { return a; };
        assert(f.call(123), 123);

        f = function f(a = f) { return a; };
        assert(f(), f);

        f = function f(a = eval("f")) { return a; };
        assert(f(), f);
    })();

    f = (a = eval("var c = 1"), probe = () => c) => {
        var c = 2;
        assert(c, 2);
        assert(probe(), 1);
    }
    f();

    f = (a = eval("var arguments = 1"), probe = () => arguments) => {
        var arguments = 2;
        assert(arguments, 2);
        assert(probe(), 1);
    }
    f();

    f = function f(a = eval("var c = 1"), b = c, probe = () => c) {
        assert(b, 1);
        assert(c, 1);
        assert(probe(), 1)
    }
    f();

    assert(c, "global");
    f = function f(a, b = c, probe = () => c) {
        eval("var c = 1");
        assert(c, 1);
        assert(b, "global");
        assert(probe(), "global")
    }
    f();
    assert(c, "global");

    f = function f(a = eval("var c = 1"), probe = (d = eval("c")) => d) {
        assert(probe(), 1)
    }
    f();
}

function test_function_expr_name()
{
    var f;

    /* non strict mode test : assignment to the function name silently
       fails */

    f = function myfunc() {
        myfunc = 1;
        return myfunc;
    };
    assert(f(), f);

    f = function myfunc() {
        myfunc = 1;
        (() => {
            myfunc = 1;
        })();
        return myfunc;
    };
    assert(f(), f);

    f = function myfunc() {
        eval("myfunc = 1");
        return myfunc;
    };
    assert(f(), f);

    /* strict mode test : assignment to the function name raises a
       TypeError exception */

    f = function myfunc() {
        "use strict";
        myfunc = 1;
    };
    assert_throws(TypeError, f);

    f = function myfunc() {
        "use strict";
        (() => {
            myfunc = 1;
        })();
    };
    assert_throws(TypeError, f);

    f = function myfunc() {
        "use strict";
        eval("myfunc = 1");
    };
    assert_throws(TypeError, f);
}

function test_parse_semicolon()
{
    /* 'yield' or 'await' may not be considered as a token if the
       previous ';' is missing */
    function *f()
    {
        function func() {
        }
        yield 1;
        var h = x => x + 1
        yield 2;
    }
    async function g()
    {
        function func() {
        }
        await 1;
        var h = x => x + 1
        await 2;
    }
}

function test_parse_arrow_function()
{
    assert(typeof eval("() => {}\n() => {}"), "function");
    assert(eval("() => {}\n+1"), 1);
    assert(typeof eval("x => {}\n() => {}"), "function");
    assert(typeof eval("async () => {}\n() => {}"), "function");
    assert(typeof eval("async x => {}\n() => {}"), "function");
}

/* optional chaining tests not present in test262 */
function test_optional_chaining()
{
    var a, z;
    z = null;
    a = { b: { c: 2 } };
    assert(delete z?.b.c, true);
    assert(delete a?.b.c, true);
    assert(JSON.stringify(a), '{"b":{}}', "optional chaining delete");

    a = { b: { c: 2 } };
    assert(delete z?.b["c"], true);
    assert(delete a?.b["c"], true);
    assert(JSON.stringify(a), '{"b":{}}');

    a = {
        b() { return this._b; },
        _b: { c: 42 }
    };

    assert((a?.b)().c, 42);

    assert((a?.["b"])().c, 42);
}

function test_unicode_ident()
{
    var Ãµ = 3;
    assert(typeof õ, "undefined");

    const starts = [
        0x125a8, 0x1264b, 0x18e00, 0x19191, 0x191a0, 0x191d2,
        0x2b81e, 0x3d000, 0x3fc3f,
    ];
    for (const code of starts) {
        const character = String.fromCodePoint(code);
        const escaped = "\\u{" + code.toString(16) + "}";
        for (const prefix of ["", "a"]) {
            for (const name of [prefix + character, prefix + escaped]) {
                assert(Function("let " + name + " = 42; return " + name + ";")(), 42);
            }
        }
        const match = new RegExp("(?<" + character + ">.)\\k<" + character + ">", "u").exec("xx");
        assert(match.groups[character], "x");
    }
    for (const code of [0x20c2, 0x20c3, 0x20c4, 0x19192, 0x191d3,
                        0x2b81f, 0x3fc40]) {
        const character = String.fromCodePoint(code);
        const escaped = "\\u{" + code.toString(16) + "}";
        for (const prefix of ["", "a"]) {
            for (const name of [prefix + character, prefix + escaped]) {
                assert_throws(SyntaxError, () => Function("let " + name + " = 42;"));
            }
        }
    }
}

/* check global variable optimization */
function test_global_var_opt()
{
    var v2;
    (1, eval)('var gvar1'); /* create configurable global variables */

    gvar1 = 1;
    Object.defineProperty(globalThis, "gvar1", { writable: false });
    gvar1 = 2;
    assert(gvar1, 1);

    Object.defineProperty(globalThis, "gvar1", { get: function() { return "hello" },
                                                 set: function(v) { v2 = v; } });
    assert(gvar1, "hello");
    gvar1 = 3;
    assert(v2, 3);

    Object.defineProperty(globalThis, "gvar1", { value: 4, writable: true, configurable: true });
    assert(gvar1, 4);
    gvar1 = 6;
    
    delete gvar1;
    assert_throws(ReferenceError, function() { return gvar1 });
    gvar1 = 5;
    assert(gvar1, 5);
}

function test_number_literals()
{
    assert(0.1.a, undefined);
    assert(0x1.a, undefined);
    assert(0b1.a, undefined);
    assert(01.a, undefined);
    assert(0o1.a, undefined);
    assert_throws(SyntaxError, () => eval('0.a'));
}

test_op1();
test_arrow_for_in_grammar();
test_eval_call_receiver();
test_cvt();
test_eq();
test_inc_dec();
test_op2();
test_constructor();
test_delete();
test_prototype();
test_arguments();
test_class();
test_class_element_line_terminators();
test_super_base_order();
test_super_null_key_coercion();
test_annex_if_function_scopes();
test_annex_function_identity();
test_annex_duplicate_binding();
test_eval_catch_var_declaration();
test_annex_eval_variable_target();
test_parameter_arguments_binding();
test_annex_deferred_applicability();
test_annex_arguments_binding();
test_class_lexical_strictness();
test_class_strict_region_control_flow();
test_const_write_tdz();
const global_const_write_tdz = 1;
test_const_reference_timing();
test_parameter_environment_bindings();
test_computed_parameter_environment();
test_template();
test_template_skip();
test_template_coercion();
test_negated_condition();
test_object_literal();
test_regexp_skip();
test_labels();
test_labels2();
test_destructuring_property_colon();
test_destructuring();
test_spread();
test_function_length();
test_argument_scope();
test_function_expr_name();
test_parse_semicolon();
test_optional_chaining();
test_parse_arrow_function();
test_unicode_ident();
test_global_var_opt();
test_number_literals();




function test_assignment_rhs_eval_abrupt()
{
    let target = 13;
    const sentinel = {};
    function abrupt() {
        target = (eval("var target = 50"), (() => { throw sentinel; })());
    }
    try { abrupt(); throw Error("expected RHS exception"); }
    catch (error) { assert(error, sentinel); }
    assert(target, 13);
}

function test_assignment_rhs_eval_parameter_environments()
{
    let target = 1;
    function parameter(value = (target = (eval("var target = 2"), 3))) {
        return [target, value];
    }
    assert(parameter().join(","), "2,3");
    assert(target, 3);
    function body(value = 0) {
        target = (eval("var target = 4"), 5);
        return target;
    }
    assert(body(), 4);
    assert(target, 5);
}

function test_assignment_rhs_eval_later_parent()
{
    let target = 0;
    function parent() {
        function child() {
            target = (eval("var target = 3"), 4);
            return target;
        }
        eval("var target = 2");
        return [child(), target];
    }
    assert(parent().join(","), "3,4");
    assert(target, 0);
    function strictParent() {
        function child() {
            "use strict";
            target += (eval("var target = 9"), 2);
            return target;
        }
        eval("var target = 6");
        return [child(), target];
    }
    assert(strictParent().join(","), "8,8");
    assert(target, 0);
}

function test_assignment_rhs_eval_stable_strict_stores()
{
    let local = 1;
    assert(local += (local = 10, 2), 3);
    assert(local, 3);
    const immutable = 1;
    let rhs = false;
    assert_throws(TypeError, () => {
        immutable = (rhs = true, 2);
    });
    assert(rhs, true);
    class StrictScope {
        static {
            assert_throws(ReferenceError, () => {
                __rhs_eval_strict_missing = 1;
            });
        }
    }
    assert("__rhs_eval_strict_missing" in globalThis, false);
}

test_assignment_rhs_eval_abrupt();
test_assignment_rhs_eval_parameter_environments();
test_assignment_rhs_eval_later_parent();
test_assignment_rhs_eval_stable_strict_stores();


function test_assignment_reference_capture()
{
    let outer = 1;
    function simple() {
        outer = (eval("var outer = 2"), 3);
        return outer;
    }
    assert(simple(), 2);
    assert(outer, 3);

    outer = 1;
    function compound() {
        outer += (eval("var outer = 2"), 3);
        return outer;
    }
    assert(compound(), 2);
    assert(outer, 4);

    outer = 1;
    function removed() {
        eval("var outer = 2");
        outer = (eval("delete outer"), 3);
        return outer;
    }
    assert(removed(), 3);
    assert(outer, 1);

    outer = 1;
    function suspended() {
        function* writer() { outer += yield 0; }
        const iterator = writer();
        assert(iterator.next().value, 0);
        eval("var outer = 2");
        assert(iterator.next(3).done, true);
        return outer;
    }
    assert(suspended(), 2);
    assert(outer, 4);

    delete globalThis.reference_capture_global;
    function missing() {
        reference_capture_global =
            (eval("var reference_capture_global = 2"), 3);
        return reference_capture_global;
    }
    assert(missing(), 2);
    assert(globalThis.reference_capture_global, 3);
    delete globalThis.reference_capture_global;

    let effects = 0;
    assert_throws(ReferenceError, () => {
        reference_capture_global += ++effects;
    });
    assert(effects, 0);
    assert_throws(ReferenceError, () => {
        (function() {
            "use strict";
            reference_capture_global =
                (globalThis.reference_capture_global = 1, 2);
        })();
    });
    assert(globalThis.reference_capture_global, 1);
    delete globalThis.reference_capture_global;

    assert_throws(ReferenceError, () => {
        class C {
            [reference_capture_global =
                (globalThis.reference_capture_global = 1, 2)]() {}
        }
    });
    assert(globalThis.reference_capture_global, 1);
    delete globalThis.reference_capture_global;

    globalThis.reference_capture_global = 1;
    assert_throws(ReferenceError, () => {
        (function() {
            "use strict";
            reference_capture_global =
                (delete globalThis.reference_capture_global, 2);
        })();
    });
    assert(Object.hasOwn(globalThis, "reference_capture_global"), false);

    const rhs_error = new Error("RHS");
    function abrupt() { throw rhs_error; }
    try {
        (function() { "use strict"; reference_capture_global = abrupt(); })();
        assert(false);
    } catch (e) {
        assert(e, rhs_error);
    }
    assert(Object.hasOwn(globalThis, "reference_capture_global"), false);

    const old_prototype = Object.getPrototypeOf(globalThis);
    const order = [];
    try {
        Object.setPrototypeOf(globalThis, new Proxy({}, {
            has(target, name) {
                if (name === "reference_capture_inherited") {
                    order.push("has");
                    return true;
                }
                return Reflect.has(target, name);
            },
            set(target, name, value, receiver) {
                if (name === "reference_capture_inherited")
                    order.push("set");
                return Reflect.set(target, name, value, receiver);
            }
        }));
        reference_capture_inherited = (order.push("rhs"), 7);
        assert(order.join(","), "has,rhs,has,set");
        assert(globalThis.reference_capture_inherited, 7);
    } finally {
        Object.setPrototypeOf(globalThis, old_prototype);
        delete globalThis.reference_capture_inherited;
    }

    let key = 0;
    class C { [key = 1]() {} [key += 2]() {} }
    assert(key, 3);
    assert(typeof C.prototype[1], "function");
    assert(typeof C.prototype[3], "function");
}

test_assignment_reference_capture();



function test_logical_assignment_reference_capture()
{
    let outer, effects = 0;

    function or_taken() {
        const result = outer ||= (eval("var outer = 2"), 3);
        return [result, outer];
    }
    function and_taken() {
        const result = outer &&= (eval("var outer = 2"), 3);
        return [result, outer];
    }
    function nullish_taken() {
        const result = outer ??= (eval("var outer = 2"), 3);
        return [result, outer];
    }
    function or_short() {
        const result = outer ||= (++effects, eval("var outer = 2"), 3);
        return [result, outer];
    }
    function and_short() {
        const result = outer &&= (++effects, eval("var outer = 2"), 3);
        return [result, outer];
    }
    function nullish_short() {
        const result = outer ??= (++effects, eval("var outer = 2"), 3);
        return [result, outer];
    }

    outer = 0;
    assert(or_taken().join(","), "3,2");
    assert(outer, 3);
    outer = 1;
    assert(and_taken().join(","), "3,2");
    assert(outer, 3);
    outer = null;
    assert(nullish_taken().join(","), "3,2");
    assert(outer, 3);

    outer = 7;
    assert(or_short().join(","), "7,7");
    assert(outer, 7);
    outer = 0;
    assert(and_short().join(","), "0,0");
    assert(outer, 0);
    outer = 5;
    assert(nullish_short().join(","), "5,5");
    assert(outer, 5);
    assert(effects, 0);
}

function test_destructuring_reference_capture()
{
    let outer = 1;
    function array_default() {
        [outer = (eval("var outer = 2"), 3)] = [];
        return outer;
    }
    function object_default() {
        ({ value: outer = (eval("var outer = 2"), 3) } = {});
        return outer;
    }
    assert(array_default(), 2);
    assert(outer, 3);
    outer = 1;
    assert(object_default(), 2);
    assert(outer, 3);

    outer = 100;
    function array_getter() {
        eval("var outer = 1");
        const source = [];
        Object.defineProperty(source, "0", {
            get() { eval("delete outer"); return 3; }
        });
        [outer] = source;
        return outer;
    }
    function object_getter() {
        eval("var outer = 1");
        ({ value: outer } = {
            get value() { eval("delete outer"); return 3; }
        });
        return outer;
    }
    assert(array_getter(), 3);
    assert(outer, 100);
    assert(object_getter(), 3);
    assert(outer, 100);

    const name = "destructuring_capture_missing";
    let reads = 0;
    delete globalThis[name];
    try {
        assert_throws(ReferenceError, () => {
            (function() {
                "use strict";
                ({ value: destructuring_capture_missing } = {
                    get value() {
                        reads++;
                        globalThis[name] = 1;
                        return 3;
                    }
                });
            })();
        });
        assert(reads, 1);
        assert(globalThis[name], 1);
        delete globalThis[name];

        assert_throws(ReferenceError, () => {
            (function() {
                "use strict";
                const source = [];
                Object.defineProperty(source, "0", {
                    get() {
                        reads++;
                        globalThis[name] = 1;
                        return 3;
                    }
                });
                [destructuring_capture_missing] = source;
            })();
        });
        assert(reads, 2);
        assert(globalThis[name], 1);
        delete globalThis[name];

        assert_throws(ReferenceError, () => {
            (function() {
                "use strict";
                [destructuring_capture_missing =
                    (globalThis[name] = 1, 3)] = [];
            })();
        });
        assert(globalThis[name], 1);
        delete globalThis[name];

        assert_throws(ReferenceError, () => {
            (function() {
                "use strict";
                ({ value: destructuring_capture_missing =
                    (globalThis[name] = 1, 3) } = {});
            })();
        });
        assert(globalThis[name], 1);
    } finally {
        delete globalThis[name];
    }
}

function test_update_reference_capture()
{
    let outer = 100;
    const declaration = "var outer = { valueOf() { " +
        "eval('delete outer'); return numeric; } }";
    function post_increment(numeric) {
        eval(declaration);
        const result = outer++;
        return [result, outer];
    }
    function pre_increment(numeric) {
        eval(declaration);
        const result = ++outer;
        return [result, outer];
    }
    function post_decrement(numeric) {
        eval(declaration);
        const result = outer--;
        return [result, outer];
    }
    function pre_decrement(numeric) {
        eval(declaration);
        const result = --outer;
        return [result, outer];
    }
    const operations = [
        [post_increment, false, true], [pre_increment, true, true],
        [post_decrement, false, false], [pre_decrement, true, false]
    ];
    for (const numeric of [2, 2n]) {
        const one = typeof numeric === "bigint" ? 1n : 1;
        for (const [update, prefix, increment] of operations) {
            const next = increment ? numeric + one : numeric - one;
            const result = update(numeric);
            assert(result[0], prefix ? next : numeric);
            assert(result[1], next);
            assert(outer, 100);
        }
    }
}

function test_iteration_assignment_reference_capture()
{
    const name = "iteration_capture_global";
    const old_prototype = Object.getPrototypeOf(globalThis);
    const prototype = Object.create(old_prototype);
    const order = [];
    const writers = [
        [function() { for (iteration_capture_global of [7]) {} }, 7],
        [function() { for (iteration_capture_global in { key: 0 }) {} }, "key"]
    ];
    try {
        Object.setPrototypeOf(globalThis, new Proxy(prototype, {
            has(target, key) {
                if (key === name)
                    order.push("has");
                return Reflect.has(target, key);
            },
            set(target, key, value, receiver) {
                if (key === name)
                    order.push("set");
                return Reflect.set(target, key, value, receiver);
            }
        }));
        for (const [write, value] of writers) {
            delete globalThis[name];
            prototype[name] = 0;
            order.length = 0;
            write();
            assert(order.join(","), "has,has,set");
            assert(globalThis[name], value);
        }
    } finally {
        Object.setPrototypeOf(globalThis, old_prototype);
        delete globalThis[name];
    }
}

test_logical_assignment_reference_capture();
test_destructuring_reference_capture();
test_update_reference_capture();
test_iteration_assignment_reference_capture();

function test_assignment_reference_write_order()
{
    const name = "reference_write_order_global";
    const old_prototype = Object.getPrototypeOf(globalThis);
    const prototype = Object.create(old_prototype);
    const order = [];
    const has_error = new Error("write-time has");
    let rhs_seen = false, throw_live_has = false;

    function reset(resolved) {
        delete globalThis[name];
        delete prototype[name];
        if (resolved)
            prototype[name] = 1;
        order.length = 0;
        rhs_seen = false;
        throw_live_has = false;
    }

    function rhs() {
        order.push("rhs");
        rhs_seen = true;
        return 7;
    }

    const sloppy_writers = [
        function(value) { reference_write_order_global = value(); },
        // A transparent with object also exercises saved destructuring
        // references when ordinary targets use the direct store path.
        function(value) {
            with ({})
                ({ value: reference_write_order_global = value() } = {});
        },
        function(value) {
            with ({})
                [reference_write_order_global = value()] = [];
        },
    ];
    const strict_writers = [
        function(value) {
            "use strict";
            reference_write_order_global = value();
        },
        function(value) {
            class C { [reference_write_order_global = value()]() {} }
        },
    ];

    try {
        Object.setPrototypeOf(globalThis, new Proxy(prototype, {
            has(target, key) {
                if (key === name) {
                    order.push("has");
                    if (rhs_seen && throw_live_has)
                        throw has_error;
                }
                return Reflect.has(target, key);
            },
            set(target, key, value, receiver) {
                if (key === name)
                    order.push("set");
                return Reflect.set(target, key, value, receiver);
            },
        }));

        for (const write of sloppy_writers) {
            reset(false);
            write(rhs);
            assert(order.join(","), "has,rhs,set");
            assert(globalThis[name], 7);

            // A second has would throw; PutValue must skip that lookup.
            reset(false);
            throw_live_has = true;
            write(rhs);
            assert(order.join(","), "has,rhs,set");
            assert(globalThis[name], 7);

            reset(true);
            write(rhs);
            assert(order.join(","), "has,rhs,has,set");
            assert(globalThis[name], 7);

            reset(true);
            write(() => {
                const value = rhs();
                delete prototype[name];
                return value;
            });
            assert(order.join(","), "has,rhs,has,set");
            assert(globalThis[name], 7);

            // Resolved bindings must propagate the live has exception.
            reset(true);
            throw_live_has = true;
            let caught;
            try { write(rhs); } catch (e) { caught = e; }
            assert(caught, has_error);
            assert(order.join(","), "has,rhs,has");
            assert(Object.hasOwn(globalThis, name), false);
        }

        for (const write of strict_writers) {
            reset(false);
            assert_throws(ReferenceError, () => write(rhs));
            assert(order.join(","), "has,rhs");
            assert(Object.hasOwn(globalThis, name), false);

            // RHS creation cannot resolve a saved missing strict reference.
            reset(false);
            assert_throws(ReferenceError, () => write(() => {
                const value = rhs();
                Object.defineProperty(globalThis, name, {
                    value: 4, writable: true, configurable: true,
                });
                return value;
            }));
            assert(order.join(","), "has,rhs");
            assert(globalThis[name], 4);

            reset(true);
            write(rhs);
            assert(order.join(","), "has,rhs,has,set");
            assert(globalThis[name], 7);

            reset(true);
            assert_throws(ReferenceError, () => write(() => {
                const value = rhs();
                delete prototype[name];
                return value;
            }));
            assert(order.join(","), "has,rhs,has");
            assert(Object.hasOwn(globalThis, name), false);
        }
    } finally {
        Object.setPrototypeOf(globalThis, old_prototype);
        delete globalThis[name];
        delete prototype[name];
    }
}
test_assignment_reference_write_order();
function test_guarded_global_pure_store()
{
    const name = "__guarded_global_store";
    const events = [];
    function write(value) { __guarded_global_store = value; }
    function strictWrite(value) { "use strict"; __guarded_global_store = value; }
    Object.defineProperty(globalThis, name, {
        value: 0, writable: true, configurable: true
    });
    try {
        write(3);
        assert(globalThis[name], 3);
        strictWrite(4);
        assert(globalThis[name], 4);
        const object = {};
        write(object);
        assert(globalThis[name], object);

        Object.defineProperty(globalThis, name, {
            configurable: true,
            set(value) { events.push(value); }
        });
        write(5);
        strictWrite(6);
        assert(events.join(","), "5,6");
        Object.defineProperty(globalThis, name, {
            value: 7, writable: false, configurable: true
        });
        write(8);
        assert(globalThis[name], 7);
        assert_throws(TypeError, () => strictWrite(9));
        assert(globalThis[name], 7);
    } finally {
        delete globalThis[name];
    }
}

function test_guarded_global_pure_store_resolution_effects()
{
    const oldPrototype = Object.getPrototypeOf(globalThis);
    const target = Object.create(oldPrototype);
    const events = [];
    var local = 1;
    Object.defineProperty(target, "__guarded_global_inherited", {
        value: 0, writable: true, configurable: true
    });
    const prototype = new Proxy(target, {
        has(object, name) {
            if (name === "__guarded_global_inherited") {
                events.push("has");
                local = 9;
            }
            if (name === "__guarded_global_missing") {
                events.push("create");
                globalThis[name] = 17;
                return false;
            }
            return Reflect.has(object, name);
        }
    });
    Object.setPrototypeOf(globalThis, prototype);
    try {
        __guarded_global_inherited = local;
        assert(globalThis.__guarded_global_inherited, 9);
        assert(events.join(","), "has,has");
        function strictMissing() {
            "use strict";
            __guarded_global_missing = 0;
        }
        assert_throws(ReferenceError, strictMissing);
        assert(events.join(","), "has,has,create");
        assert(globalThis.__guarded_global_missing, 17);
    } finally {
        Object.setPrototypeOf(globalThis, oldPrototype);
        delete globalThis.__guarded_global_inherited;
        delete globalThis.__guarded_global_missing;
    }
}
test_guarded_global_pure_store();
test_guarded_global_pure_store_resolution_effects();
function test_using_registration_and_bindings()
{
    const events = [];
    let reads = 0, check;
    const resource = {
        get [Symbol.dispose]() {
            reads++;
            assert_throws(ReferenceError, check);
            return function() {
                assert(this, resource);
                assert(arguments.length, 0);
                events.push("dispose");
                return { get then() { throw Error("result must be ignored"); } };
            };
        }
    };
    {
        check = () => value;
        assert_throws(ReferenceError, check);
        using value = resource;
        assert(value, resource);
        assert_throws(TypeError, () => { value = null; });
        Object.defineProperty(resource, Symbol.dispose, {
            value() { throw Error("method must be cached"); }
        });
        events.push("body");
    }
    assert(reads, 1);
    assert(events.join(","), "body,dispose");
    assert(check(), resource);
    assert(eval("4; { using empty = null; }"), 4);
    assert(eval("6; { using first = null, second = undefined; }"), 6);
    let readDisposed, disposed = 0;
    {
        using current = { [Symbol.dispose]() { disposed++; } };
        readDisposed = eval("() => current");
        assert(readDisposed(), current);
        assert(eval("7; { using nested = null; }"), 7);
    }
    assert(disposed, 1);
    assert(typeof readDisposed()[Symbol.dispose], "function");
    {
        using empty = null, absent = undefined;
        assert(empty, null);
        assert(absent, undefined);
    }
    for (const value of [0, "", true, Symbol(), 0n, {}]) {
        assert_throws(TypeError, () => { using resource = value; });
    }
    assert_throws(TypeError, () => {
        using resource = { [Symbol.dispose]: 1 };
    });
    {
        using Named = class { static [Symbol.dispose]() {} };
        assert(Named.name, "Named");
    }
}

function test_using_completions_and_suppression()
{
    const events = [];
    function resource(name, error, shouldThrow = false) {
        return { [Symbol.dispose]() {
            events.push(name);
            if (shouldThrow)
                throw error;
        } };
    }
    const initial = {}, firstError = {}, secondError = {};
    let caught;
    try {
        using first = resource("first", firstError, true),
              second = resource("second", secondError, true);
        throw initial;
    } catch (error) { caught = error; }
    assert(events.join(","), "second,first");
    assert(caught instanceof SuppressedError, true);
    assert(caught.error, firstError);
    assert(caught.suppressed.error, secondError);
    assert(caught.suppressed.suppressed, initial);

    events.length = 0;
    caught = false;
    try {
        using value = resource("undefined", undefined, true);
        throw undefined;
    } catch (error) {
        caught = true;
        assert(error instanceof SuppressedError, true);
        assert(error.error, undefined);
        assert(error.suppressed, undefined);
    }
    assert(caught, true);
    assert(events.join(","), "undefined");

    events.length = 0;
    try {
        using first = resource("first"),
              second = (() => { throw initial; })();
    } catch (error) { assert(error, initial); }
    assert(events.join(","), "first");

    events.length = 0;
    function returns() {
        using value = resource("return");
        try { return initial; } finally { events.push("finally"); }
    }
    assert(returns(), initial);
    assert(events.join(","), "finally,return");

    events.length = 0;
    outer: for (let i = 0; i < 3; i++) {
        using value = resource(String(i));
        if (i < 2)
            continue outer;
        break outer;
    }
    assert(events.join(","), "0,1,2");
    events.length = 0;
    exit: {
        using outer = resource("outer");
        {
            using inner = resource("inner");
            break exit;
        }
    }
    assert(events.join(","), "inner,outer");

    events.length = 0;
    const iterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: resource("resource"), done: false }; },
                return() { events.push("iterator"); return {}; }
            };
        }
    };
    for (const value of iterable) {
        using current = value;
        break;
    }
    assert(events.join(","), "resource,iterator");

    const captured = [];
    for (let i = 0; i < 2; i++) {
        try {
            using value = {
                id: i,
                [Symbol.dispose]() { throw initial; }
            };
            captured.push(() => value.id);
        } catch (error) { assert(error, initial); }
    }
    assert(captured.map(get => get()).join(","), "0,1");

    let after;
    {
        let changed = 0;
        using value = { [Symbol.dispose]() { changed++; } };
        after = () => changed;
    }
    assert(after(), 1);
}

function test_using_contextual_grammar()
{
    let using = [], value = 0;
    using[0] = 1;
    us\u0069ng[1] = 2;
    using
    value = 3;
    assert(using.join(","), "1,2");
    assert(value, 3);
    Function("using", "value", "using [value] = null;")(using, "computed");
    assert(using.computed, null);
    for (const source of [
        "using value;", "using first = null, second;",
        "using let = null;", "using first = null, [value] = null;",
        "using first = null, {value} = null;",
        "using {value} = null;", "using value = null; let value;",
        "let value; using value = null;",
        "if (true) using value = null;",
        "label: using value = null;",
        "switch (1) { case 1: using value = null; }",
        "switch (1) { default: using value = null; }",
        "us\\u0069ng value = null;"
    ]) {
        assert_throws(SyntaxError, () => Function(source));
    }
    assert_throws(SyntaxError, () => eval("using value = null;"));
    assert_throws(SyntaxError, () => (0, eval)("using value = null;"));
    Function("switch (1) { case 1: { using value = null; } }")();
    Function("var using = 1, let; using\nlet = 2;")();
    assert(Function("{ using us\\u0069ng = null; return using; }")(), null);
    assert_throws(SyntaxError,
        () => Function("class C { static { using await = null; } }"));
    Function("class C { static { (() => { using await = null; }); } }")();
}

function test_using_generator_cleanup()
{
    const events = [], error = {};
    function* generator() {
        using value = { [Symbol.dispose]() { events.push("dispose"); } };
        yield value;
    }
    const first = generator();
    assert(first.next().done, false);
    assert(events.length, 0);
    assert(first.return(42).value, 42);
    assert(events.join(","), "dispose");
    events.length = 0;
    const second = generator();
    assert(second.next().done, false);
    if (typeof gc === "function")
        gc();
    try {
        second.throw(error);
        assert(false);
    } catch (caught) { assert(caught, error); }
    assert(events.join(","), "dispose");
}

function test_using_intrinsic_cleanup()
{
    const Stack = Object.getOwnPropertyDescriptor(globalThis, "DisposableStack");
    const Suppressed = SuppressedError;
    const first = {}, second = {};
    globalThis.DisposableStack = function() {
        throw Error("syntax must not construct a public stack");
    };
    globalThis.SuppressedError = function() {
        throw Error("cleanup must use the intrinsic error prototype");
    };
    try {
        try {
            using resource = { [Symbol.dispose]() { throw second; } };
            throw first;
        } catch (error) {
            assert(error instanceof Suppressed, true);
            assert(error.error, second);
            assert(error.suppressed, first);
        }
    } finally {
        if (Stack)
            Object.defineProperty(globalThis, "DisposableStack", Stack);
        else
            delete globalThis.DisposableStack;
        globalThis.SuppressedError = Suppressed;
    }
}

test_using_registration_and_bindings();
test_using_completions_and_suppression();
test_using_contextual_grammar();
test_using_generator_cleanup();
test_using_intrinsic_cleanup();

function test_using_classic_head_lifetime()
{
    const events = [];
    let i = 0, before, after;
    const resource = { [Symbol.dispose]() {
        events.push("dispose");
        assert(before(), resource);
        assert(after(), resource);
    } };
    for (using value = (() => {
             before = () => value;
             assert_throws(ReferenceError, before);
             events.push("init");
             return resource;
         })();
         (events.push("test" + i), i < 2);
         (events.push("update" + i), i++)) {
        after = () => value;
        assert(value, resource);
        assert_throws(TypeError, () => { value = null; });
        events.push("body" + i);
        continue;
    }
    assert(events.join(","),
           "init,test0,body0,update0,test1,body1,update1,test2,dispose");
    assert(before(), resource);
    assert(after(), resource);

    events.length = 0;
    for (using Named = class { static [Symbol.dispose]() {
             assert(this.name, "Named");
             events.push("named");
         } }; false;) {
        assert(false);
    }
    assert(events.join(","), "named");

    events.length = 0;
    const error = {};
    let caught;
    try {
        for (using first = { [Symbol.dispose]() { events.push("first"); } },
                   second = (() => { throw error; })();;) {}
    } catch (e) { caught = e; }
    assert(caught, error);
    assert(events.join(","), "first");

    events.length = 0;
    for (using first = { [Symbol.dispose]() { events.push("first"); } },
               second = { [Symbol.dispose]() { events.push("second"); } };;)
        break;
    assert(events.join(","), "second,first");
}

function test_using_classic_head_abrupt()
{
    const events = [], initial = {}, disposeError = {};
    function resource(name, shouldThrow = false) {
        return { [Symbol.dispose]() {
            events.push(name);
            if (shouldThrow)
                throw disposeError;
        } };
    }
    function returns() {
        for (using head = resource("head");;) {
            try {
                using body = resource("body");
                return initial;
            } finally { events.push("finally"); }
        }
    }
    assert(returns(), initial);
    assert(events.join(","), "body,finally,head");

    for (const position of ["test", "update", "body"]) {
        events.length = 0;
        let caught;
        function failAt(name) {
            if (position == name)
                throw initial;
            return true;
        }
        try {
            for (using head = resource("head", true);
                 failAt("test"); failAt("update")) {
                failAt("body");
            }
        } catch (e) { caught = e; }
        assert(caught instanceof SuppressedError, true);
        assert(caught.error, disposeError);
        assert(caught.suppressed, initial);
        assert(events.join(","), "head");
    }

    events.length = 0;
    outer: for (let i = 0; i < 2; i++) {
        for (using head = resource("head" + i);;) {
            using body = resource("body" + i);
            continue outer;
        }
    }
    assert(events.join(","), "body0,head0,body1,head1");

    function replacedReturn() {
        for (using head = resource("head", true);;)
            return initial;
    }
    try {
        replacedReturn();
        assert(false);
    } catch (e) { assert(e, disposeError); }
}

function test_using_for_of_lifetime_and_capture()
{
    const events = [], captures = [], initial = {}, resources = [];
    for (let i = 0; i < 3; i++) {
        resources.push({ id: i, [Symbol.dispose]() {
            events.push("dispose" + this.id);
        } });
    }
    let next = 0;
    const iterable = {
        [Symbol.iterator]() {
            return {
                next() {
                    events.push("next" + next);
                    return next < resources.length ?
                        { value: resources[next++], done: false } : { done: true };
                },
                return() { events.push("close"); return {}; }
            };
        }
    };
    for (using value of iterable) {
        assert_throws(TypeError, () => { value = null; });
        captures.push(() => value.id);
        events.push("body" + value.id);
        continue;
    }
    assert(events.join(","),
           "next0,body0,dispose0,next1,body1,dispose1,next2,body2,dispose2,next3");
    assert(captures.map(get => get()).join(","), "0,1,2");

    events.length = 0;
    next = 0;
    for (using value of iterable) {
        using nested = { [Symbol.dispose]() { events.push("nested"); } };
        break;
    }
    assert(events.join(","), "next0,nested,dispose0,close");

    events.length = 0;
    next = 0;
    function returns() {
        for (using value of iterable) {
            try { return initial; } finally { events.push("finally"); }
        }
    }
    assert(returns(), initial);
    assert(events.join(","), "next0,finally,dispose0,close");

    events.length = 0;
    outer: for (let i = 0; i < 2; i++) {
        next = 0;
        for (using value of iterable)
            continue outer;
    }
    assert(events.join(","), "next0,dispose0,close,next0,dispose0,close");

    let headTDZ;
    for (using value of (headTDZ = () => value, resources)) {}
    assert_throws(ReferenceError, headTDZ);
    assert_throws(ReferenceError, () => {
        let value = resources[0];
        for (using value of [value]) {}
    });

    const descendants = [];
    for (let i = 0; i < 3; i++) {
        try {
            for (using value of [{ id: i, [Symbol.dispose]() { throw initial; } }]) {
                {
                    let child = i;
                    descendants.push(() => [value.id, child].join(":"));
                    if (i == 0)
                        break;
                    if (i == 1)
                        continue;
                    throw undefined;
                }
            }
        } catch (e) {
            if (i < 2)
                assert(e, initial);
            else {
                assert(e instanceof SuppressedError, true);
                assert(e.error, initial);
                assert(e.suppressed, undefined);
            }
        }
    }
    assert(descendants.map(get => get()).join(","), "0:0,1:1,2:2");
}

function test_using_for_of_close_precedence()
{
    const initial = {}, disposeError = {}, closeError = {}, acquireError = {};
    for (const mode of ["break", "continue", "return", "throw", "normal"]) {
        for (const throwDispose of [false, true]) {
            const events = [];
            let caught, count = 0;
            const iterable = {
                [Symbol.iterator]() {
                    return {
                        next() {
                            events.push("next");
                            if (count++)
                                return { done: true };
                            return { done: false, value: { [Symbol.dispose]() {
                                events.push("dispose");
                                if (throwDispose)
                                    throw disposeError;
                            } } };
                        },
                        return() { events.push("close"); throw closeError; }
                    };
                }
            };
            function run() {
                for (using value of iterable) {
                    if (mode == "break")
                        break;
                    if (mode == "continue")
                        continue;
                    if (mode == "return")
                        return initial;
                    if (mode == "throw")
                        throw initial;
                }
            }
            try { run(); } catch (e) { caught = e; }
            if (throwDispose && mode == "throw") {
                assert(caught instanceof SuppressedError, true);
                assert(caught.error, disposeError);
                assert(caught.suppressed, initial);
            } else if (throwDispose) {
                assert(caught, disposeError);
            } else if (mode == "throw") {
                assert(caught, initial);
            } else if (mode == "break" || mode == "return") {
                assert(caught, closeError);
            } else {
                assert(caught, undefined);
            }
            assert(events.join(","),
                   throwDispose || mode == "break" || mode == "return" || mode == "throw" ?
                   "next,dispose,close" : "next,dispose,next");
        }
    }

    for (const closeFailure of ["getter", "call", "primitive"]) {
        const events = [];
        const iterable = {
            [Symbol.iterator]() {
                return {
                    next() { return { value: { [Symbol.dispose]() {
                        events.push("dispose");
                    } }, done: false }; },
                    get return() {
                        events.push("return-get");
                        if (closeFailure == "getter")
                            throw closeError;
                        return function() {
                            events.push("return-call");
                            if (closeFailure == "call")
                                throw closeError;
                            return 1;
                        };
                    }
                };
            }
        };
        let caught;
        try { for (using value of iterable) break; } catch (e) { caught = e; }
        if (closeFailure == "primitive")
            assert(caught instanceof TypeError, true);
        else
            assert(caught, closeError);
        assert(events.join(","), closeFailure == "getter" ?
               "dispose,return-get" : "dispose,return-get,return-call");
        events.length = 0;
        try { for (using value of iterable) throw initial; } catch (e) { caught = e; }
        assert(caught, initial);
    }

    for (const failure of ["getter", "non-callable", "primitive"]) {
        const events = [];
        const value = failure == "primitive" ? 1 : {
            get [Symbol.dispose]() {
                events.push("get");
                if (failure == "getter")
                    throw acquireError;
                return 1;
            }
        };
        const iterable = {
            [Symbol.iterator]() {
                return {
                    next() { return { value, done: false }; },
                    return() { events.push("close"); throw closeError; }
                };
            }
        };
        let caught;
        try { for (using current of iterable) assert(false); } catch (e) { caught = e; }
        if (failure == "getter")
            assert(caught, acquireError);
        else
            assert(caught instanceof TypeError, true);
        assert(events.join(","), failure == "primitive" ? "close" : "get,close");
    }

    for (const failure of ["next", "result", "done", "value"]) {
        const events = [];
        const iterable = {
            [Symbol.iterator]() {
                return {
                    next() {
                        if (failure == "next")
                            throw initial;
                        if (failure == "result")
                            return 1;
                        return {
                            get done() { if (failure == "done") throw initial; return false; },
                            get value() { throw initial; }
                        };
                    },
                    return() { events.push("close"); return {}; }
                };
            }
        };
        let caught;
        try { for (using current of iterable) assert(false); } catch (e) { caught = e; }
        if (failure == "result")
            assert(caught instanceof TypeError, true);
        else
            assert(caught, initial);
        assert(events.length, 0);
    }
}

function test_using_loop_head_grammar_and_generators()
{
    for (const source of [
        "for (using value;;) {}", "for (using value = null, other;;) {}",
        "for (using let = null;;) {}", "for (using value in {}) {}",
        "for (using value = null of []) {}", "for (using first, second of []) {}",
        "for (using let of []) {}", "for (using value of []) { var value; }",
        "for (using value = null;;) { var value; }",
        "for (us\\u0069ng value = null;;) {}",
        "for (us\\u0069ng value of []) {}",
        "for (using\nvalue of []) {}", "for (using\nvalue = null;;) {}"
    ]) {
        assert_throws(SyntaxError, () => Function(source));
    }
    const resources = [null, undefined];
    for (using value of resources)
        assert(value == null, true);
    Function("var using; for (using\nof [null]) {}")();
    let using, of = [[9], [8], [7]], result = [];
    for (using of of [0, 1, 2])
        result.push(using);
    assert(result.join(","), "7");
    Function("for (using o\\u0066 of [null]) {}")();
    Function("for (using /* comment */ value of [null]) {}")();
    Function("var using = []; for (using[0] of [1]) {}")();

    const events = [], error = {};
    function* generator() {
        for (using value of [{ [Symbol.dispose]() { events.push("dispose"); } }])
            yield value;
    }
    const first = generator();
    assert(first.next().done, false);
    assert(first.return(42).value, 42);
    assert(events.join(","), "dispose");
    events.length = 0;
    const second = generator();
    second.next();
    try { second.throw(error); assert(false); } catch (e) { assert(e, error); }
    assert(events.join(","), "dispose");

    events.length = 0;
    function* beforeIterator() {
        for (using value of (yield "before", [null])) {}
    }
    const third = beforeIterator();
    assert(third.next().value, "before");
    assert(third.return(12).value, 12);
    assert(events.length, 0);
}

test_using_classic_head_lifetime();
test_using_classic_head_abrupt();
test_using_for_of_lifetime_and_capture();
test_using_for_of_close_precedence();
test_using_loop_head_grammar_and_generators();

/* Source-only synchronous prerequisite regression additions. */
function test_using_for_of_catch_baseline()
{
    const original = {}, disposalError = {}, events = [];
    const resource = { [Symbol.dispose]() { events.push("dispose"); throw disposalError; } };
    const iterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: resource, done: false }; },
                return() { events.push("close"); return {}; }
            };
        }
    };
    let caught;
    try {
        for (using value of iterable) {
            assert(value, resource);
            throw original;
        }
    } catch (error) { caught = error; }
    assert(caught instanceof SuppressedError, true);
    assert(caught.error, disposalError);
    assert(caught.suppressed, original);
    assert(events.join(","), "dispose,close");

    events.length = 0;
    Object.defineProperty(resource, Symbol.dispose, {
        get() { events.push("lookup"); throw original; }
    });
    caught = undefined;
    try {
        for (using value of iterable) assert(false);
    } catch (error) { caught = error; }
    assert(caught, original);
    assert(events.join(","), "lookup,close");
}

test_using_for_of_catch_baseline();

/* Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */

function test_using_return_disposal_failure_detaches_inner_capture()
{
    const captured = [], events = [], failure = {};
    function exercise() {
        for (let i = 0; i < 2; i++) {
            try {
                using resource = {
                    [Symbol.dispose]() { events.push(i); throw failure; }
                };
                {
                    let value = i;
                    captured.push(() => value);
                    return "discarded";
                }
            } catch (error) { assert(error, failure); }
        }
        return "caught";
    }
    assert(exercise(), "caught");
    assert(events.join(","), "0,1");
    assert(captured.map(read => read()).join(","), "0,1");
}

function test_using_abrupt_inner_capture_preserves_disposer_mutation()
{
    const captured = [], failure = {};
    for (let i = 0; i < 2; i++) {
        let update;
        try {
            using resource = { [Symbol.dispose]() { update(); } };
            {
                let value = i;
                captured.push(() => value);
                update = () => { value += 10; };
                throw failure;
            }
        } catch (error) { assert(error, failure); }
    }
    assert(captured.map(read => read()).join(","), "10,11");
}

function test_using_inner_capture_registration_failure()
{
    const captured = [], failure = {};
    for (let i = 0; i < 2; i++) {
        try {
            using outer = { [Symbol.dispose]() {} };
            {
                let value = i;
                captured.push(() => value);
                using inner = {
                    get [Symbol.dispose]() { throw failure; }
                };
            }
        } catch (error) { assert(error, failure); }
    }
    assert(captured.map(read => read()).join(","), "0,1");
}

function test_using_descendant_cleanup_preserves_outer_capture()
{
    const captured = [], failure = {};
    let outer = 0;
    const readOuter = () => outer;
    for (let i = 0; i < 2; i++) {
        try {
            using resource = { [Symbol.dispose]() { outer++; } };
            {
                let value = i;
                captured.push(() => value);
                throw failure;
            }
        } catch (error) { assert(error, failure); }
    }
    assert(captured.map(read => read()).join(","), "0,1");
    assert(readOuter(), 2);
    outer = 3;
    assert(readOuter(), 3);
}

test_using_return_disposal_failure_detaches_inner_capture();
test_using_abrupt_inner_capture_preserves_disposer_mutation();
test_using_inner_capture_registration_failure();
test_using_descendant_cleanup_preserves_outer_capture();

function test_assignment_rhs_eval_resource_disposal()
{
    let target = 1;
    const events = [];
    function write() {
        using resource = { [Symbol.dispose]() { events.push("dispose"); } };
        target = (events.push("rhs"), eval("var target = 2"), 3);
        return target;
    }
    assert(write(), 2);
    assert(target, 3);
    assert(events.join(","), "rhs,dispose");
}

test_assignment_rhs_eval_resource_disposal();



function test_stable_assignment_reference_depth()
{
    let first = 1, second = 2, tail;
    const events = [];
    const source = {
        get first() {
            events.push("first");
            first = 40;
            return 3;
        },
        get second() {
            events.push("second");
            second = 50;
            return undefined;
        }
    };
    const result = ({ first, second = (events.push("default"), 4) } = source);
    assert(result, source);
    assert(first, 3);
    assert(second, 4);
    assert(events.join(","), "first,second,default");

    let step = 0, closed = 0;
    const iterable = {
        [Symbol.iterator]() { return this; },
        next() {
            first = 60 + step;
            return { value: ++step, done: step > 3 };
        },
        return() {
            closed++;
            return {};
        }
    };
    [first, second, ...tail] = iterable;
    assert(second, 2);
    assert(tail.join(","), "3");
    assert(first, 63);
    assert(closed, 0);
    [first] = iterable;
    assert(first, undefined);
    assert(closed, 0);

    let truthy = 1, falsy = 0, nullish = null;
    assert(truthy ||= (events.push("unexpected"), 2), 1);
    assert(falsy &&= (events.push("unexpected"), 2), 0);
    assert(truthy &&= (events.push("and"), truthy = 9, 5), 5);
    assert(falsy ||= (events.push("or"), falsy = 9, 6), 6);
    assert(nullish ??= (events.push("nullish"), nullish = 9, 7), 7);
    assert(events.join(","), "first,second,default,and,or,nullish");
    assert(truthy, 5);
    assert(falsy, 6);
    assert(nullish, 7);
}

function test_stable_parameter_assignment_reference_depth()
{
    function mapped(value) {
        const source = { get value() { value = 4; return 7; } };
        ({ value } = source);
        assert(value, 7);
        assert(arguments[0], 7);
        [value] = [8];
        assert(value, 8);
        assert(arguments[0], 8);
        assert(value &&= 9, 9);
        assert(arguments[0], 9);
    }
    function unmapped(value) {
        "use strict";
        const source = { get value() { value = 4; return 7; } };
        ({ value } = source);
        assert(value, 7);
        assert(arguments[0], 1);
        [value] = [8];
        assert(value, 8);
        assert(arguments[0], 1);
        assert(value &&= 9, 9);
        assert(arguments[0], 1);
    }
    mapped(1);
    unmapped(1);
}

function test_stable_assignment_reference_depth_errors()
{
    let next = 0, closed = 0;
    const iterable = {
        [Symbol.iterator]() { return this; },
        next() {
            next++;
            return { value: 1, done: false };
        },
        return() {
            closed++;
            return {};
        }
    };
    assert_throws(ReferenceError, function() {
        let uninitialized = ([uninitialized] = iterable);
    });
    assert(next, 1);
    assert(closed, 1);

    const fixed = 0;
    assert_throws(TypeError, function() { [fixed] = iterable; });
    assert(next, 2);
    assert(closed, 2);
    assert(fixed, 0);
}

test_stable_assignment_reference_depth();
test_stable_parameter_assignment_reference_depth();
test_stable_assignment_reference_depth_errors();




function test_compact_named_assignment_reference()
{
    let first = 1, second = 2;
    const events = [];
    const source = {
        get p() { events.push("p"); first = 40; return 3; },
        get q() { events.push("q"); assert(first, 3); first = 50; return 4; }
    };
    const result = ({ p: first, q: second } = source);
    assert(result, source);
    assert(first, 50);
    assert(second, 4);
    assert(events.join(","), "p,q");

    function mapped(value) {
        const source = { get p() { value = 4; return 7; } };
        ({ p: value } = source);
        assert(value, 7);
        assert(arguments[0], 7);
    }
    function unmapped(value) {
        "use strict";
        const source = { get p() { value = 4; return 7; } };
        ({ p: value } = source);
        assert(value, 7);
        assert(arguments[0], 1);
    }
    mapped(1);
    unmapped(1);
}

function test_compact_named_assignment_reference_abrupt()
{
    const events = [];
    const sentinel = new Error("named extraction");
    let value = 1, error;
    try {
        ({ p: value } = { get p() { events.push("get"); throw sentinel; } });
    } catch (e) {
        error = e;
    }
    assert(error, sentinel);
    assert(value, 1);
    assert(events.join(","), "get");

    assert_throws(ReferenceError, function() {
        let uninitialized = ({ p: uninitialized } = {
            get p() { events.push("tdz-get"); return 3; }
        });
    });
    const fixed = 0;
    assert_throws(TypeError, function() {
        ({ p: fixed } = { get p() { events.push("const-get"); return 3; } });
    });
    assert(events.join(","), "get,tdz-get,const-get");

    events.length = 0;
    const iterable = {
        [Symbol.iterator]() { return this; },
        next() {
            events.push("next");
            return { value: { get p() { events.push("get"); throw sentinel; } },
                     done: false };
        },
        return() { events.push("close"); return {}; }
    };
    error = undefined;
    try { [{ p: value }] = iterable; } catch (e) { error = e; }
    assert(error, sentinel);
    assert(value, 1);
    assert(events.join(","), "next,get,close");
}

test_compact_named_assignment_reference();
test_compact_named_assignment_reference_abrupt();

function test_checked_strict_global_assignment()
{
    let effects = 0;
    delete globalThis.checked_strict_global_target;
    function write() {
        "use strict";
        return checked_strict_global_target =
            (++effects, globalThis.checked_strict_global_target = 1, 2);
    }
    assert_throws(ReferenceError, write);
    assert(effects, 1);
    assert(globalThis.checked_strict_global_target, 1);
    assert(write(), 2);
    assert(effects, 2);
    assert(globalThis.checked_strict_global_target, 2);
    delete globalThis.checked_strict_global_target;

    const sentinel = {};
    function abrupt() {
        "use strict";
        return checked_strict_global_target = (() => { throw sentinel; })();
    }
    try { abrupt(); assert(false); }
    catch (error) { assert(error, sentinel); }
    assert(Object.hasOwn(globalThis, "checked_strict_global_target"), false);

    assert_throws(ReferenceError, () => {
        class StrictScope {
            static {
                checked_strict_global_target =
                    (globalThis.checked_strict_global_target = 3, 4);
            }
        }
    });
    assert(globalThis.checked_strict_global_target, 3);
    delete globalThis.checked_strict_global_target;
}

test_checked_strict_global_assignment();

function test_checked_global_class_mode()
{
    const name = "checked_class_mode_target";
    const sentinel = {};
    delete globalThis[name];
    try {
        /* A checked global store must keep the enclosing class strict. */
        assert_throws(ReferenceError, () => {
            return class { [(checked_class_mode_target = 1)]() {} };
        });
        assert(Object.hasOwn(globalThis, name), false);

        Object.defineProperty(globalThis, name, {
            value: 1, writable: false, configurable: true
        });
        assert_throws(TypeError, () => {
            return class { [(checked_class_mode_target = 2)]() {} };
        });
        assert(checked_class_mode_target = 3, 3);
        assert(globalThis[name], 1);
        delete globalThis[name];

        /* Suspension retains both the saved resolution and strict mode. */
        function* create() {
            return class { [(checked_class_mode_target = yield 0)]() {} };
        }
        const generator = create();
        assert(generator.next().value, 0);
        globalThis[name] = 4;
        assert_throws(ReferenceError, () => generator.next(5));
        assert(globalThis[name], 4);
        delete globalThis[name];

        /* Unwinding out of a class restores the enclosing sloppy mode. */
        try {
            (class { [(checked_class_mode_target = (() => { throw sentinel; })())]() {} });
            assert(false);
        } catch (error) {
            assert(error, sentinel);
        }
        checked_class_mode_target = 6;
        assert(globalThis[name], 6);
    } finally {
        delete globalThis[name];
    }
}

test_checked_global_class_mode();

function test_assignment_reference_pure_rhs()
{
    const old_prototype = Object.getPrototypeOf(globalThis);
    const order = [];
    let mutate, resolved;
    const writers = [
        function() { pure_rhs_global_reference = -1; return -1; },
        function() { pure_rhs_global_reference = 7; return 7; },
        function() {
            let value = 1;
            mutate = () => { value = 7; };
            pure_rhs_global_reference = value;
            return value;
        },
        function(value) {
            mutate = () => { value = 7; };
            pure_rhs_global_reference = value;
            return value;
        },
    ];
    try {
        Object.setPrototypeOf(globalThis, new Proxy({}, {
            has(target, name) {
                if (name !== "pure_rhs_global_reference")
                    return Reflect.has(target, name);
                order.push("has");
                if (order.length === 1)
                    mutate();
                return resolved;
            },
            set(target, name, value, receiver) {
                if (name === "pure_rhs_global_reference")
                    order.push("set");
                return Reflect.set(target, name, value, receiver);
            },
        }));
        for (resolved of [false, true]) {
            for (const write of writers) {
                order.length = 0;
                mutate = () => {};
                const expected = write(1);
                assert(order.join(","), resolved ? "has,has,set" : "has,set");
                assert(globalThis.pure_rhs_global_reference, expected);
                delete globalThis.pure_rhs_global_reference;
            }
        }
    } finally {
        Object.setPrototypeOf(globalThis, old_prototype);
        delete globalThis.pure_rhs_global_reference;
    }
}

test_assignment_reference_pure_rhs();

function test_checked_sloppy_global_assignment()
{
    const name = "checked_sloppy_global_target";
    const old_prototype = Object.getPrototypeOf(globalThis);
    delete globalThis[name];
    try {
        function created() {
            return checked_sloppy_global_target =
                (globalThis[name] = 1, 2);
        }
        assert(created(), 2);
        assert(globalThis[name], 2);

        function removed() {
            return checked_sloppy_global_target =
                (delete globalThis[name], 3);
        }
        assert(removed(), 3);
        assert(globalThis[name], 3);

        Object.defineProperty(globalThis, name, { writable: false });
        assert(created(), 2);
        assert(globalThis[name], 3);

        delete globalThis[name];
        const sentinel = {};
        function abrupt() {
            checked_sloppy_global_target = (() => { throw sentinel; })();
        }
        try { abrupt(); assert(false); }
        catch (error) { assert(error, sentinel); }
        assert(Object.hasOwn(globalThis, name), false);

        Object.setPrototypeOf(globalThis, new Proxy(Object.create(old_prototype), {
            has(target, key) {
                return key === name ? false : Reflect.has(target, key);
            },
            set(target, key, value, receiver) {
                if (key === name)
                    throw sentinel;
                return Reflect.set(target, key, value, receiver);
            },
        }));
        function rejected() { checked_sloppy_global_target = {}; }
        try { rejected(); assert(false); }
        catch (error) { assert(error, sentinel); }
    } finally {
        Object.setPrototypeOf(globalThis, old_prototype);
        delete globalThis[name];
    }
}

test_checked_sloppy_global_assignment();

function test_closure_destructuring_lowering()
{
    let first = 0, second = 0, tail;
    const events = [];
    const key = { toString() { events.push("key"); return "q"; } };
    const source = {
        get p() { events.push("p"); first = 40; return 3; },
        get q() { events.push("q"); assert(first, 3); second = 50; return 4; }
    };
    function object() { return ({ p: first, [key]: second } = source); }
    assert(object(), source);
    assert(first, 3);
    assert(second, 4);
    assert(events.join(","), "p,key,q");

    function defaults() {
        ({ p: first = (first = 90, 5) } = {});
        [second = (second = 90, 6), ...tail] = [];
    }
    defaults();
    assert(first, 5);
    assert(second, 6);
    assert(tail.length, 0);

    function logical() {
        assert(first ||= 7, 5);
        assert(second &&= 8, 8);
        second = null;
        assert(second ??= 9, 9);
    }
    logical();

    function* suspended() { [first = yield 1] = []; return first; }
    const iterator = suspended();
    assert(iterator.next().value, 1);
    first = 99;
    assert(iterator.next(7).value, 7);
    assert(first, 7);

    let next = 0, closed = 0;
    const iterable = {
        [Symbol.iterator]() { return this; },
        next() { next++; return { value: 1, done: false }; },
        return() { closed++; return {}; }
    };
    function before_initialization() { [uninitialized] = iterable; }
    assert_throws(ReferenceError, before_initialization);
    assert(next, 1);
    assert(closed, 1);
    let uninitialized;

    first = 3;
    assert_throws(ReferenceError, function() {
        let local = ({ [key]: local } = source);
    });
    assert(events.slice(-2).join(","), "key,q");
}

test_closure_destructuring_lowering();

/* Lookahead must tolerate a provisional RegExp error for division after
   contextual identifiers, without affecting ordinary syntax diagnostics. */
function test_lookahead_division()
{
    function f(of, x = of / 2) { return x; }
    function g(yield, x = yield / 2) { return x; }
    assert(f(4), 2);
    assert(g(6), 3);
    assert_throws(SyntaxError, () => eval("function broken(a = ) {}"));
}

test_lookahead_division();

function test_large_function_bindings()
{
    for (const count of [16, 31, 32, 33, 47, 48, 49, 95, 96, 97, 257]) {
        const declarations = Array.from({ length: count }, (_, i) =>
            "var v" + i + "=" + i + ";").join("");
        const last = "v" + (count - 1);
        const total = Array.from({ length: count }, (_, i) => "v" + i).join("+");
        assert(Function(declarations + "return " + total)(), count * (count - 1) / 2);
        assert(Function(declarations + "return " + last)(), count - 1);
        assert(Function("a", "a", declarations + "var a; return a+" + last)(1, 2),
               count + 1);
        assert(Function("a=3", declarations + "var a; return a+" + last)(), count + 2);
        assert(Function("{let x=2;}" + declarations +
                        "var x=9; {let x=3;} return x+" + last)(), count + 8);
        assert(Function(declarations + "eval('v0=17'); return (()=>v0)();")(), 17);
        assert(Function(declarations + "with({v0:9}){v0++;} return v0;")(), 0);
        assert_throws(SyntaxError, () => Function(declarations + "let v0;"));
        const siblings = "{ let x=2; }".repeat(count);
        assert(Function(siblings + "var x=7; return x;")(), 7);
    }
}

test_large_function_bindings();

function test_typeof_comparison_alternatives()
{
    for (const op of ["==", "!=", "===", "!=="]) {
        const compare = Function("value", "return typeof value " + op + " 'string';");
        for (const value of [undefined, null, 1, "s", {}, function() {}, Symbol()]) {
            const equal = typeof value === "string";
            assert(compare(value), op === "==" || op === "===" ? equal : !equal);
        }
    }
}

test_typeof_comparison_alternatives();
