function assert(actual, expected, message) {
    if (arguments.length == 1)
        expected = true;

    if (actual === expected)
        return;

    if (actual !== null && expected !== null
    &&  typeof actual == 'object' && typeof expected == 'object'
    &&  actual.toString() === expected.toString())
        return;

    throw Error("assertion failed: got |" + actual + "|" +
                ", expected |" + expected + "|" +
                (message ? " (" + message + ")" : ""));
}

// load more elaborate version of assert if available
try { __loadScript("test_assert.js"); } catch(e) {}

/*----------------*/

var log_str = "";

function log(str)
{
    log_str += str + ",";
}

function f(a, b, c)
{
    var x = 10;
    log("a="+a);
    function g(d) {
        function h() {
            log("d=" + d);
            log("x=" + x);
        }
        log("b=" + b);
        log("c=" + c);
        h();
    }
    g(4);
    return g;
}

var g1 = f(1, 2, 3);
g1(5);

assert(log_str, "a=1,b=2,c=3,d=4,x=10,b=2,c=3,d=5,x=10,", "closure1");

function test_closure1()
{
    function f2()
    {
        var val = 1;

        function set(a) {
            val = a;
        }
        function get(a) {
            return val;
        }
        return { "set": set, "get": get };
    }

    var obj = f2();
    obj.set(10);
    var r;
    r = obj.get();
    assert(r, 10, "closure2");
}

function test_closure2()
{
    var expr_func = function myfunc1(n) {
        function myfunc2(n) {
            return myfunc1(n - 1);
        }
        if (n == 0)
            return 0;
        else
            return myfunc2(n);
    };
    var r;
    r = expr_func(1);
    assert(r, 0, "expr_func");
}

function test_closure3()
{
    function fib(n)
    {
        if (n <= 0)
            return 0;
        else if (n == 1)
            return 1;
        else
            return fib(n - 1) + fib(n - 2);
    }

    var fib_func = function fib1(n)
    {
        if (n <= 0)
            return 0;
        else if (n == 1)
            return 1;
        else
            return fib1(n - 1) + fib1(n - 2);
    };

    assert(fib(6), 8, "fib");
    assert(fib_func(6), 8, "fib_func");
}

function test_arrow_function()
{
    "use strict";

    function f1() {
        return (() => arguments)();
    }
    function f2() {
        return (() => this)();
    }
    function f3() {
        return (() => eval("this"))();
    }
    function f4() {
        return (() => eval("new.target"))();
    }
    var a;

    a = f1(1, 2);
    assert(a.length, 2);
    assert(a[0] === 1 && a[1] === 2);

    assert(f2.call("this_val") === "this_val");
    assert(f3.call("this_val") === "this_val");
    assert(new f4() === f4);

    var o1 = { f() { return this; } };
    var o2 = { f() {
        return (() => eval("super.f()"))();
    } };
    o2.__proto__ = o1;

    assert(o2.f() === o2);
}

function test_with()
{
    var o1 = { x: "o1", y: "o1" };
    var x = "local";
    eval('var z="var_obj";');
    assert(z === "var_obj");
    with (o1) {
        assert(x === "o1");
        assert(eval("x") === "o1");
        var f = function () {
            o2 = { x: "o2" };
            with (o2) {
                assert(x === "o2");
                assert(y === "o1");
                assert(z === "var_obj");
                assert(eval("x") === "o2");
                assert(eval("y") === "o1");
                assert(eval("z") === "var_obj");
                assert(eval('eval("x")') === "o2");
            }
        };
        f();
    }
}

function test_eval_closure()
{
    var tab;

    tab = [];
    for(let i = 0; i < 3; i++) {
        eval("tab.push(function g1() { return i; })");
    }
    for(let i = 0; i < 3; i++) {
        assert(tab[i]() === i);
    }

    tab = [];
    for(let i = 0; i < 3; i++) {
        let f = function f() {
            eval("tab.push(function g2() { return i; })");
        };
        f();
    }
    for(let i = 0; i < 3; i++) {
        assert(tab[i]() === i);
    }
}

function test_eval_const()
{
    const a = 1;
    var success = false;
    var f = function () {
        eval("a = 1");
    };
    try {
        f();
    } catch(e) {
        success = (e instanceof TypeError);
    }
    assert(success);
}

function test_abrupt_scope_closures()
{
    var closures = [];
    for (let i = 0; i < 2; i++) {
        try {
            let x = i;
            closures.push(() => x);
            throw 0;
        } catch (e) {}
    }
    assert(closures.map(f => f()).join(), "0,1");

    closures = [];
    for (let i = 0; i < 2; i++) {
        try {
            try {
                let x = i;
                { let y = i + 10; closures.push(() => [x, y]); }
                throw 0;
            } finally {
                closures[closures.length - 1]();
            }
        } catch (e) {}
    }
    assert(JSON.stringify(closures.map(f => f())), "[[0,10],[1,11]]");

    closures = [];
    for (let i = 0; i < 2; i++) {
        try {
            try { throw i; } catch (e) {
                let x = e;
                closures.push(() => ++x);
                throw 0;
            }
        } catch (e) {
            assert(closures[i](), i + 1);
        }
    }
    assert(closures.map(f => f()).join(), "2,3");

    function return_from_finally() {
        var values = [];
        for (let i = 0; i < 2; i++) {
            try {
                let x = i;
                values.push(() => x);
                return;
            } finally {
                continue;
            }
        }
        return values.map(f => f()).join();
    }
    assert(return_from_finally(), "0,1");

    function return_from_catch() {
        var values = [];
        for (let i = 0; i < 2; i++) {
            try { throw i; }
            catch (value) {
                values.push(() => value);
                return;
            } finally {
                continue;
            }
        }
        return values.map(f => f()).join();
    }
    assert(return_from_catch(), "0,1");

    closures = [];
    function* suspended_catch() {
        for (let i = 0; i < 2; i++) {
            try { throw i; }
            catch (value) {
                closures.push(() => value);
                yield;
            } finally {
                continue;
            }
        }
    }
    var catch_iter = suspended_catch();
    catch_iter.next();
    catch_iter.return();
    catch_iter.return();
    assert(closures.map(f => f()).join(), "0,1");

    var caught = false;
    try {
        for (let i = 0; i < 2; i++) {
            try { throw {}; }
            catch ({ x = (i ? y : 1), y = 2 }) {
                assert(x, 1);
            }
        }
    } catch (e) {
        assert(e instanceof ReferenceError);
        caught = true;
    }
    assert(caught);

    closures = [];
    function* suspended() {
        for (let i = 0; i < 2; i++) {
            try {
                let x = i;
                closures.push(() => x);
                yield;
            } catch (e) {}
        }
    }
    var iter = suspended();
    iter.next();
    iter.throw(0);
    iter.throw(0);
    assert(closures.map(f => f()).join(), "0,1");
}

test_closure1();
test_closure2();
test_closure3();
test_arrow_function();
test_with();
test_eval_closure();
test_eval_const();

function test_optional_call_cleanup()
{
    for (var spread of [false, true]) {
        var events = [];
        var make = Function("events",
            "return function*() { try { with({ f(x) { return x; } }) { " +
            "return f?.(" + (spread ? "...[yield 1]" : "yield 1") +
            "); } } finally { events.push(1); } };");
        var iter = make(events)();
        var step = iter.next();
        assert(step.value, 1);
        assert(step.done, false);
        step = iter.return(7);
        assert(step.value, 7);
        assert(step.done, true);
        assert(events.join(), "1");
    }
}

test_optional_call_cleanup();
test_abrupt_scope_closures();
