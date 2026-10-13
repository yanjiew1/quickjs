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
test_const_reference_timing();
test_parameter_environment_bindings();
test_computed_parameter_environment();
test_template();
test_template_skip();
test_object_literal();
test_regexp_skip();
test_labels();
test_labels2();
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
