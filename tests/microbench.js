/*
 * Javascript Micro benchmark
 *
 * Copyright (c) 2017-2019 Fabrice Bellard
 * Copyright (c) 2017-2019 Charlie Gordon
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

if (typeof require !== 'undefined') {
    var fs = require('fs');
}

function pad(str, n) {
    str += "";
    while (str.length < n)
        str += " ";
    return str;
}

function pad_left(str, n) {
    str += "";
    while (str.length < n)
        str = " " + str;
    return str;
}

function pad_center(str, n) {
    str += "";
    while (str.length < n) {
        if ((n - str.length) & 1)
            str = str + " ";
        else
            str = " " + str;
    }
    return str;
}

var ref_data;
var log_data;

var heads  = [ "TEST", "N", "TIME (ns)", "REF (ns)", "SCORE (1000)" ];
var widths = [    22,   10,          9,     9,       9 ];
var precs  = [     0,   0,           2,     2,       0 ];
var total  = [     0,   0,           0,     0,       0 ];
var total_score = 0;
var total_scale = 0;

function log_line() {
    var i, n, s, a;
    s = "";
    for (i = 0, n = arguments.length; i < n; i++) {
        if (i > 0)
            s += " ";
        a = arguments[i];
        if (typeof a === "number") {
            total[i] += a;
            a = a.toFixed(precs[i]);
            s += pad_left(a, widths[i]);
        } else {
            s += pad_left(a, widths[i]);
        }
    }
    console.log(s);
}

var clocks_per_sec = 1000;
var max_iterations = 100;
var clock_threshold = 2;  /* favoring short measuring spans */
var min_n_argument = 1;
var get_clock;
if (typeof performance !== "undefined") {
    // use more precise clock on NodeJS
    // need a method call on performance object
    get_clock = () => performance.now();
} else
if (typeof os !== "undefined") {
    // use more precise clock on QuickJS
    get_clock = os.now;
} else {
    // use Date.now and round up to the next millisecond
    get_clock = () => {
        var t0 = Date.now();
        var t;
        while ((t = Date.now()) == t0)
            continue;
        return t;
    }
}

function log_one(text, n, ti) {
    var ref;

    if (ref_data)
        ref = ref_data[text];
    else
        ref = null;

    ti = Math.round(ti * 100) / 100;
    log_data[text] = ti;
    if (typeof ref === "number") {
        log_line(text, n, ti, ref, Math.round(ref * 1000 / ti));
        total_score += ti * 100 / ref;
        total_scale += 100;
    } else {
        log_line(text, n, ti);
        total_score += 100;
        total_scale += 100;
    }
}

function bench(f, text)
{
    var i, j, n, t, ti, nb_its, ref, ti_n, ti_n1;

    nb_its = n = 1;
    if (f.bench) {
        ti_n = f(text);
    } else {
        // measure ti_n: the shortest time for an individual operation
        ti_n = 1000000000;
        for(i = 0; i < 30; i++) {
            // measure ti: the shortest time for max_iterations iterations
            ti = 1000000000;
            for (j = 0; j < max_iterations; j++) {
                t = get_clock();
                nb_its = f(n);
                t = get_clock() - t;
                if (nb_its < 0)
                    return; // test failure
                if (ti > t)
                    ti = t;
            }
            if (ti >= clock_threshold / 10) {
                ti_n1 = ti / nb_its;
                if (ti_n > ti_n1)
                    ti_n = ti_n1;
            }
            if (ti >= clock_threshold && n >= min_n_argument)
                break;

            n = n * [ 2, 2.5, 2 ][i % 3];
        }
        // to use only the best timing from the last loop, uncomment below
        //ti_n = ti / nb_its;
    }
    /* nano seconds per iteration */
    log_one(text, n, ti_n * 1e9 / clocks_per_sec);
}

var global_res; /* to be sure the code is not optimized */

function empty_loop(n) {
    var j;
    for(j = 0; j < n; j++) {
    }
    return n;
}

function empty_down_loop(n) {
    var j;
    for(j = n; j > 0; j--) {
    }
    return n;
}

function empty_down_loop2(n) {
    var j;
    for(j = n; j --> 0;) {
    }
    return n;
}

function empty_do_loop(n) {
    var j = n;
    do { } while (--j > 0);
    return n;
}

function date_now(n) {
    var j;
    for(j = 0; j < n; j++) {
        Date.now();
    }
    return n;
}

function date_parse(n) {
    var x0 = 0, dx = 0;
    var j;
    for(j = 0; j < n; j++) {
        var x1 = x0 - x0 % 1000;
        var x2 = -x0;
        var x3 = -x1;
        var d0 = new Date(x0);
        var d1 = new Date(x1);
        var d2 = new Date(x2);
        var d3 = new Date(x3);
        if (Date.parse(d0.toISOString()) != x0
        ||  Date.parse(d1.toGMTString()) != x1
        ||  Date.parse(d1.toString()) != x1
        ||  Date.parse(d2.toISOString()) != x2
        ||  Date.parse(d3.toGMTString()) != x3
        ||  Date.parse(d3.toString()) != x3) {
            console.log("Date.parse error for " + x0);
            return -1;
        }
        dx = (dx * 1.1 + 1) >> 0;
        x0 = (x0 + dx) % 8.64e15;
    }
    return n * 6;
}

function prop_read(n)
{
    var obj, sum, j;
    obj = {a: 1, b: 2, c:3, d:4 };
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += obj.a;
        sum += obj.b;
        sum += obj.c;
        sum += obj.d;
    }
    global_res = sum;
    return n * 4;
}

function prop_write(n)
{
    var obj, j;
    obj = {a: 1, b: 2, c:3, d:4 };
    for(j = 0; j < n; j++) {
        obj.a = j;
        obj.b = j;
        obj.c = j;
        obj.d = j;
    }
    return n * 4;
}

function prop_update(n)
{
    var obj, j;
    obj = {a: 1, b: 2, c:3, d:4 };
    for(j = 0; j < n; j++) {
        obj.a += j;
        obj.b += j;
        obj.c += j;
        obj.d += j;
    }
    return n * 4;
}

function prop_create(n)
{
    var obj, i, j;
    for(j = 0; j < n; j++) {
        obj = {};
        obj.a = 1;
        obj.b = 2;
        obj.c = 3;
        obj.d = 4;
        obj.e = 5;
        obj.f = 6;
        obj.g = 7;
        obj.h = 8;
        obj.i = 9;
        obj.j = 10;
        for(i = 0; i < 10; i++) {
            obj[i] = i;
        }
    }
    return n * 20;
}

function prop_clone(n)
{
    var ref, obj, j, k;
    ref = { a:1, b:2, c:3, d:4, e:5, f:6, g:7, h:8, i:9, j:10 };
    for(k = 0; k < 10; k++) {
        ref[k] = k;
    }
    for (j = 0; j < n; j++) {
        global_res = { ...ref };
    }
    return n * 20;
}

function prop_delete(n)
{
    var ref, obj, j, k;
    ref = { a:1, b:2, c:3, d:4, e:5, f:6, g:7, h:8, i:9, j:10 };
    for(k = 0; k < 10; k++) {
        ref[k] = k;
    }
    for (j = 0; j < n; j++) {
        obj = { ...ref };
        delete obj.a;
        delete obj.b;
        delete obj.c;
        delete obj.d;
        delete obj.e;
        delete obj.f;
        delete obj.g;
        delete obj.h;
        delete obj.i;
        delete obj.j;
        for(k = 0; k < 10; k++) {
            delete obj[k];
        }
    }
    return n * 20;
}

function array_read(n)
{
    var tab, len, sum, i, j;
    tab = [];
    len = 10;
    for(i = 0; i < len; i++)
        tab[i] = i;
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += tab[0];
        sum += tab[1];
        sum += tab[2];
        sum += tab[3];
        sum += tab[4];
        sum += tab[5];
        sum += tab[6];
        sum += tab[7];
        sum += tab[8];
        sum += tab[9];
    }
    global_res = sum;
    return len * n;
}

function array_write(n)
{
    var tab, len, i, j;
    tab = [];
    len = 10;
    for(i = 0; i < len; i++)
        tab[i] = i;
    for(j = 0; j < n; j++) {
        tab[0] = j;
        tab[1] = j;
        tab[2] = j;
        tab[3] = j;
        tab[4] = j;
        tab[5] = j;
        tab[6] = j;
        tab[7] = j;
        tab[8] = j;
        tab[9] = j;
    }
    return len * n;
}

function array_update(n)
{
    var tab, len, i, j;
    tab = [];
    len = 10;
    for(i = 0; i < len; i++)
        tab[i] = i;
    for(j = 0; j < n; j++) {
        tab[0] += j;
        tab[1] += j;
        tab[2] += j;
        tab[3] += j;
        tab[4] += j;
        tab[5] += j;
        tab[6] += j;
        tab[7] += j;
        tab[8] += j;
        tab[9] += j;
    }
    return len * n;
}

function array_prop_create(n)
{
    var tab, i, j, len;
    len = 1000;
    for(j = 0; j < n; j++) {
        tab = [];
        for(i = 0; i < len; i++)
            tab[i] = i;
    }
    return len * n;
}

function array_slice(n)
{
    var ref, a, i, j, len;
    len = 1000;
    ref = [];
    for(i = 0; i < len; i++)
        ref[i] = i;
    for(j = 0; j < n; j++) {
        ref[0] = j;
        a = ref.slice();
        a[0] = 0;
        global_res = a;
    }
    return len * n;
}

function array_length_read(n)
{
    var tab, sum, j;
    tab = [1, 2, 3];
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += tab.length;
        sum += tab.length;
        sum += tab.length;
        sum += tab.length;
    }
    global_res = sum;
    return n * 4;
}

function array_length_decr(n)
{
    var tab, ref, i, j, len;
    len = 1000;
    ref = [];
    for(i = 0; i < len; i++)
        ref[i] = i;
    for(j = 0; j < n; j++) {
        tab = ref.slice();
        for(i = len; i --> 0;)
            tab.length = i;
    }
    return len * n;
}

function array_hole_length_decr(n)
{
    var tab, ref, i, j, len;
    len = 1000;
    ref = [];
    for(i = 0; i < len; i++) {
        if (i % 10 == 9)
            ref[i] = i;
    }
    for(j = 0; j < n; j++) {
        tab = ref.slice();
        for(i = len; i --> 0;)
            tab.length = i;
    }
    return len * n;
}

function array_push(n)
{
    var tab, i, j, len;
    len = 500;
    for(j = 0; j < n; j++) {
        tab = [];
        for(i = 0; i < len; i++)
            tab.push(i);
    }
    return len * n;
}

function array_pop(n)
{
    var tab, ref, i, j, len, sum;
    len = 500;
    ref = [];
    for(i = 0; i < len; i++)
        ref[i] = i;
    for(j = 0; j < n; j++) {
        tab = ref.slice();
        sum = 0;
        for(i = 0; i < len; i++)
            sum += tab.pop();
        global_res = sum;
    }
    return len * n;
}

function typed_array_read(n)
{
    var tab, len, sum, i, j;
    len = 10;
    tab = new Int32Array(len);
    for(i = 0; i < len; i++)
        tab[i] = i;
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += tab[0];
        sum += tab[1];
        sum += tab[2];
        sum += tab[3];
        sum += tab[4];
        sum += tab[5];
        sum += tab[6];
        sum += tab[7];
        sum += tab[8];
        sum += tab[9];
    }
    global_res = sum;
    return len * n;
}

function typed_array_write(n)
{
    var tab, len, i, j;
    len = 10;
    tab = new Int32Array(len);
    for(i = 0; i < len; i++)
        tab[i] = i;
    for(j = 0; j < n; j++) {
        tab[0] = j;
        tab[1] = j;
        tab[2] = j;
        tab[3] = j;
        tab[4] = j;
        tab[5] = j;
        tab[6] = j;
        tab[7] = j;
        tab[8] = j;
        tab[9] = j;
    }
    return len * n;
}

function arguments_test()
{
    return arguments[0] + arguments[1] + arguments[2];
}

function arguments_read(n)
{
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += arguments_test(j, j, j);
        sum += arguments_test(j, j, j);
        sum += arguments_test(j, j, j);
        sum += arguments_test(j, j, j);
    }
    global_res = sum;
    return n * 4;
}

function arguments_strict_test()
{
    "use strict";
    return arguments[0] + arguments[1] + arguments[2];
}

function arguments_strict_read(n)
{
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += arguments_strict_test(j, j, j);
        sum += arguments_strict_test(j, j, j);
        sum += arguments_strict_test(j, j, j);
        sum += arguments_strict_test(j, j, j);
    }
    global_res = sum;
    return n * 4;
}

var global_var0;

function global_read(n)
{
    var sum, j;
    global_var0 = 0;
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += global_var0;
        sum += global_var0;
        sum += global_var0;
        sum += global_var0;
    }
    global_res = sum;
    return n * 4;
}

function global_write(n)
{
    var j;
    for(j = 0; j < n; j++) {
        global_var0 = j;
        global_var0 = j;
        global_var0 = j;
        global_var0 = j;
    }
    return n * 4;
}

function global_write_strict(n)
{
    "use strict";
    var j;
    for(j = 0; j < n; j++) {
        global_var0 = j;
        global_var0 = j;
        global_var0 = j;
        global_var0 = j;
    }
    return n * 4;
}

function local_destruct(n)
{
    var j, v1, v2, v3, v4;
    var array = [ 1, 2, 3, 4, 5];
    var o = { a:1, b:2, c:3, d:4 };
    var a, b, c, d;
    for(j = 0; j < n; j++) {
        [ v1, v2,, v3, ...v4] = array;
        ({ a, b, c, d } = o);
        ({ a: a, b: b, c: c, d: d } = o);
    }
    return n * 12;
}

var global_v1, global_v2, global_v3, global_v4;
var global_a, global_b, global_c, global_d;

function closure_destruct(n)
{
    var v1, v2, v3, v4, a, b, c, d;
    function run() {
        var array = [1, 2, 3, 4, 5];
        var object = { a: 1, b: 2, c: 3, d: 4 };
        for (var j = 0; j < n; j++) {
            [v1, v2,, v3, ...v4] = array;
            ({ a, b, c, d } = object);
        }
    }
    run();
    global_res = a + b + c + d;
    return n * 8;
}

function global_destruct(n)
{
    var j, v1, v2, v3, v4;
    var array = [ 1, 2, 3, 4, 5 ];
    var o = { a:1, b:2, c:3, d:4 };
    var a, b, c, d;
    for(j = 0; j < n; j++) {
        [ global_v1, global_v2,, global_v3, ...global_v4] = array;
        ({ a: global_a, b: global_b, c: global_c, d: global_d } = o);
    }
    return n * 8;
}

function global_destruct_strict(n)
{
    "use strict";
    var j, v1, v2, v3, v4;
    var array = [ 1, 2, 3, 4, 5 ];
    var o = { a:1, b:2, c:3, d:4 };
    var a, b, c, d;
    for(j = 0; j < n; j++) {
        [ global_v1, global_v2,, global_v3, ...global_v4] = array;
        ({ a: global_a, b: global_b, c: global_c, d: global_d } = o);
    }
    return n * 8;
}

function g(a)
{
    return 1;
}

function global_func_call(n)
{
    var j, sum;
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += g(j);
        sum += g(j);
        sum += g(j);
        sum += g(j);
    }
    global_res = sum;
    return n * 4;
}

function func_call(n)
{
    function f(a)
    {
        return 1;
    }

    var j, sum;
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += f(j);
        sum += f(j);
        sum += f(j);
        sum += f(j);
    }
    global_res = sum;
    return n * 4;
}

function func_closure_call(n)
{
    function f(a)
    {
        sum++;
    }

    var j, sum;
    sum = 0;
    for(j = 0; j < n; j++) {
        f(j);
        f(j);
        f(j);
        f(j);
    }
    global_res = sum;
    return n * 4;
}

function int_arith(n)
{
    var i, j, sum;
    global_res = 0;
    for(j = 0; j < n; j++) {
        sum = 0;
        for(i = 0; i < 1000; i++) {
            sum += i * i;
        }
        global_res += sum;
    }
    return n * 1000;
}

function float_arith(n)
{
    var i, j, sum, a, incr, a0;
    global_res = 0;
    a0 = 0.1;
    incr = 1.1;
    for(j = 0; j < n; j++) {
        sum = 0;
        a = a0;
        for(i = 0; i < 1000; i++) {
            sum += a * a;
            a += incr;
        }
        global_res += sum;
    }
    return n * 1000;
}

function bigint_arith(n, bits)
{
    var i, j, sum, a, incr, a0, sum0;
    sum0 = global_res = BigInt(0);
    a0 = BigInt(1) << BigInt(Math.floor((bits - 10) * 0.5));
    incr = BigInt(1);
    for(j = 0; j < n; j++) {
        sum = sum0;
        a = a0;
        for(i = 0; i < 1000; i++) {
            sum += a * a;
            a += incr;
        }
        global_res += sum;
    }
    return n * 1000;
}

function bigint32_arith(n)
{
    return bigint_arith(n, 32);
}

function bigint64_arith(n)
{
    return bigint_arith(n, 64);
}

function bigint256_arith(n)
{
    return bigint_arith(n, 256);
}

function bigint_to_string(n)
{
    var j, result, value = BigInt(255);
    for(j = 0; j < n; j++) {
        result = value.toString();
    }
    global_res = result;
    if (result !== "255")
        throw Error("incorrect BigInt decimal formatting");
    return n;
}

function bigint_to_string_large(n)
{
    var j, result, value = bigint_to_string_large.value;
    if (value === undefined) {
        value = (BigInt(1) << BigInt(4096)) - BigInt(1);
        bigint_to_string_large.value = value;
        bigint_to_string_large.expected = String(value);
    }
    for(j = 0; j < n; j++) {
        result = value.toString();
    }
    global_res = result;
    if (result !== bigint_to_string_large.expected)
        throw Error("incorrect large BigInt decimal formatting");
    return n;
}

function bigint_to_locale_string(n)
{
    var j, result, value = BigInt(255);
    if (bigint_to_locale_string.expected === undefined) {
        // Keep the measured call unchanged when Intl localizes its output.
        bigint_to_locale_string.expected = typeof Intl !== "undefined" &&
            typeof Intl.NumberFormat === "function" ?
            new Intl.NumberFormat().format(value) : String(value);
    }
    for(j = 0; j < n; j++) {
        result = value.toLocaleString();
    }
    global_res = result;
    if (result !== bigint_to_locale_string.expected)
        throw Error("incorrect BigInt locale fallback");
    return n;
}

function bigint_to_locale_string_large(n)
{
    var j, result, value = bigint_to_locale_string_large.value;
    if (value === undefined) {
        value = (BigInt(1) << BigInt(4096)) - BigInt(1);
        bigint_to_locale_string_large.value = value;
        // ECMA-402 formats this through the default NumberFormat, whose
        // grouping and digits can differ from the locale-independent string.
        bigint_to_locale_string_large.expected = typeof Intl !== "undefined" &&
            typeof Intl.NumberFormat === "function" ?
            new Intl.NumberFormat().format(value) : String(value);
    }
    for(j = 0; j < n; j++) {
        result = value.toLocaleString();
    }
    global_res = result;
    if (result !== bigint_to_locale_string_large.expected)
        throw Error("incorrect large BigInt locale fallback");
    return n;
}

function map_set_string(n)
{
    var s, i, j, len = 1000;
    for(j = 0; j < n; j++) {
        s = new Map();
        for(i = 0; i < len; i++) {
            s.set(String(i), i);
        }
        for(i = 0; i < len; i++) {
            if (!s.has(String(i)))
                throw Error("bug in Map");
        }
    }
    return n * len;
}

function map_set_int(n)
{
    var s, i, j, len = 1000;
    for(j = 0; j < n; j++) {
        s = new Map();
        for(i = 0; i < len; i++) {
            s.set(i, i);
        }
        for(i = 0; i < len; i++) {
            if (!s.has(i))
                throw Error("bug in Map");
        }
    }
    return n * len;
}

function map_set_bigint(n)
{
    var s, i, j, len = 1000;
    for(j = 0; j < n; j++) {
        s = new Map();
        for(i = 0; i < len; i++) {
            s.set(BigInt(i), i);
        }
        for(i = 0; i < len; i++) {
            if (!s.has(BigInt(i)))
                throw Error("bug in Map");
        }
    }
    return n * len;
}

function map_delete(n)
{
    var a, i, j;

    len = 1000;
    for(j = 0; j < n; j++) {
        a = new Map();
        for(i = 0; i < len; i++) {
            a.set(String(i), i);
        }
        for(i = 0; i < len; i++) {
            a.delete(String(i));
        }
    }
    return len * n;
}

function weak_map_set(n)
{
    var a, i, j, tab;

    len = 1000;
    tab = [];
    for(i = 0; i < len; i++) {
        tab.push({ key: i });
    }
    for(j = 0; j < n; j++) {
        a = new WeakMap();
        for(i = 0; i < len; i++) {
            a.set(tab[i], i);
        }
    }
    return len * n;
}

function weak_map_delete(n)
{
    var a, i, j, tab;

    len = 1000;
    for(j = 0; j < n; j++) {
        tab = [];
        for(i = 0; i < len; i++) {
            tab.push({ key: i });
        }
        a = new WeakMap();
        for(i = 0; i < len; i++) {
            a.set(tab[i], i);
        }
        for(i = 0; i < len; i++) {
            tab[i] = null;
        }
    }
    return len * n;
}


function array_for(n)
{
    var r, i, j, sum, len = 100;
    r = [];
    for(i = 0; i < len; i++)
        r[i] = i;
    for(j = 0; j < n; j++) {
        sum = 0;
        for(i = 0; i < len; i++) {
            sum += r[i];
        }
        global_res = sum;
    }
    return n * len;
}

function array_for_in(n)
{
    var r, i, j, sum, len = 100;
    r = [];
    for(i = 0; i < len; i++)
        r[i] = i;
    for(j = 0; j < n; j++) {
        sum = 0;
        for(i in r) {
            sum += r[i];
        }
        global_res = sum;
    }
    return n * len;
}

function array_for_of(n)
{
    var r, i, j, sum, len = 100;
    r = [];
    for(i = 0; i < len; i++)
        r[i] = i;
    for(j = 0; j < n; j++) {
        sum = 0;
        for(i of r) {
            sum += i;
        }
        global_res = sum;
    }
    return n * len;
}

function math_min(n)
{
    var i, j, r;
    r = 0;
    for(j = 0; j < n; j++) {
        for(i = 0; i < 1000; i++)
            r = Math.min(i, 500);
        global_res = r;
    }
    return n * 1000;
}

function regexp_compile(n)
{
    var i, j, r;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 1000; i++)
            r = new RegExp("(?:foo|bar)+(\\d+)");
        global_res = r;
    }
    return n * 1000;
}

var regexp_compile_large_pattern;

function regexp_compile_large(n)
{
    var i, j, r;
    if (regexp_compile_large_pattern === undefined) {
        regexp_compile_large_pattern = Array.from({ length: 500 },
                                                  (_, i) => "word" + i).join("|");
    }
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            r = new RegExp(regexp_compile_large_pattern, "u");
        global_res = r;
    }
    return n * 100;
}

function regexp_ascii(n)
{
    var i, j, r, s;
    s = "the quick brown fox jumped over the lazy dog"
    for(j = 0; j < n; j++) {
        for(i = 0; i < 1000; i++)
            r = /the quick brown fox/.exec(s)
        global_res = r;
    }
    return n * 1000;
}

function regexp_utf16(n)
{
    var i, j, r, s;
    s = "the quick brown ᶠᵒˣ jumped over the lazy ᵈᵒᵍ"
    for(j = 0; j < n; j++) {
        for(i = 0; i < 1000; i++)
            r = /the quick brown ᶠᵒˣ/.exec(s)
        global_res = r;
    }
    return n * 1000;
}

function regexp_replace(n)
{
    var i, j, r, s;
    s = "the quick abc brown fox jumped abc over the lazy dog"
    for(j = 0; j < n; j++) {
        for(i = 0; i < 1000; i++)
            r = s.replace(/abc /g, "-");
        global_res = r;
    }
    return n * 1000;
}

function intl_list_construct(n)
{
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = new Intl.ListFormat("en");
        global_res = result;
    }
    return n * 100;
}

function intl_list_ascii(n)
{
    var formatter = new Intl.ListFormat("en");
    var items = ["alpha", "beta", "gamma"];
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.format(items);
        global_res = result;
    }
    return n * 100;
}

function intl_list_utf16(n)
{
    var formatter = new Intl.ListFormat("en");
    var items = ["\u0100\0", "\ud800", "\ud83e\udd8a"];
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.format(items);
        global_res = result;
    }
    return n * 100;
}

function intl_list_parts(n)
{
    var formatter = new Intl.ListFormat("en");
    var items = ["alpha", "beta", "gamma"];
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.formatToParts(items);
        global_res = result;
    }
    return n * 100;
}

function intl_plural_construct(n)
{
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = new Intl.PluralRules("en");
        global_res = result;
    }
    return n * 100;
}

function intl_plural_select(n)
{
    var formatter = new Intl.PluralRules("en");
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.select(i + 0.125);
        global_res = result;
    }
    return n * 100;
}

function intl_plural_decimal(n)
{
    var formatter = new Intl.PluralRules("en");
    var input = "123456789012345678901234567890.125";
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.select(input);
        global_res = result;
    }
    return n * 100;
}

function intl_plural_range(n)
{
    var formatter = new Intl.PluralRules("en");
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.selectRange(i + 0.125, i + 0.875);
        global_res = result;
    }
    return n * 100;
}

function intl_number_construct(n)
{
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = new Intl.NumberFormat("en");
        global_res = result;
    }
    return n * 100;
}

function intl_number_format(n)
{
    var format = new Intl.NumberFormat("en").format;
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = format(i + 1234.125);
        global_res = result;
    }
    return n * 100;
}

function intl_number_decimal(n)
{
    var format = new Intl.NumberFormat("en").format;
    var input = "123456789012345678901234567890.125";
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = format(input);
        global_res = result;
    }
    return n * 100;
}

function intl_number_bigint(n)
{
    var format = new Intl.NumberFormat("en").format;
    var input = 123456789012345678901234567890n;
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = format(input);
        global_res = result;
    }
    return n * 100;
}

function intl_number_parts(n)
{
    var formatter = new Intl.NumberFormat("en", { style: "currency", currency: "USD" });
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.formatToParts(i + 1234.125);
        global_res = result;
    }
    return n * 100;
}

function intl_number_range(n)
{
    var formatter = new Intl.NumberFormat("en");
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.formatRange(i + 0.125, i + 0.875);
        global_res = result;
    }
    return n * 100;
}

function intl_number_unit(n)
{
    var format = new Intl.NumberFormat("en", {
        style: "unit", unit: "meter-per-second", unitDisplay: "long", notation: "compact"
    }).format;
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = format(i + 1234.125);
        global_res = result;
    }
    return n * 100;
}

/* Locale negotiation and Unicode contexts are separate from ordinary casing.
   Each case reuses its source string while producing an observable result. */
function intl_case_default(n)
{
    var input = "Default I and Greek ΟΣ\u0301", result, i, j;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = input.toLocaleLowerCase("en");
        global_res = result;
    }
    return n * 100;
}

function intl_case_lower(n)
{
    var input = "I\u0323\u0307 I\u0301\u0307 İ I\u034f\u0307", result, i, j;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = input.toLocaleLowerCase("tr");
        global_res = result;
    }
    return n * 100;
}

function intl_case_upper(n)
{
    var input = "i\u0307\u0307 j\u0323\u0307 \u{1d422}\u0307", result, i, j;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = input.toLocaleUpperCase("lt");
        global_res = result;
    }
    return n * 100;
}

function intl_relative_construct(n)
{
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = new Intl.RelativeTimeFormat("en");
        global_res = result;
    }
    return n * 100;
}

function intl_relative_numeric(n)
{
    var formatter = new Intl.RelativeTimeFormat("en");
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.format(i + 1234.125, "day");
        global_res = result;
    }
    return n * 100;
}

function intl_relative_auto(n)
{
    var formatter = new Intl.RelativeTimeFormat("en", { numeric: "auto" });
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.format(i % 3 - 1, "day");
        global_res = result;
    }
    return n * 100;
}

function intl_relative_parts(n)
{
    var formatter = new Intl.RelativeTimeFormat("en");
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.formatToParts(i + 1234.125, "day");
        global_res = result;
    }
    return n * 100;
}

function intl_duration_construct(n)
{
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = new Intl.DurationFormat("en", { style: "digital" });
        global_res = result;
    }
    return n * 100;
}

function intl_duration_text(n)
{
    var formatter = new Intl.DurationFormat("en", { style: "long" });
    var value = { days: 2, hours: 3, minutes: 4, seconds: 5 };
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.format(value);
        global_res = result;
    }
    return n * 100;
}

function intl_duration_digital(n)
{
    var formatter = new Intl.DurationFormat("en", { style: "digital" });
    var value = { hours: 1, minutes: 2, seconds: 3, nanoseconds: 123456789 };
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.format(value);
        global_res = result;
    }
    return n * 100;
}

function intl_duration_parts(n)
{
    var formatter = new Intl.DurationFormat("en", { style: "digital" });
    var value = { years: 1, hours: 2, minutes: 3, seconds: 4, nanoseconds: 1 };
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.formatToParts(value);
        global_res = result;
    }
    return n * 100;
}

function intl_display_construct(n)
{
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = new Intl.DisplayNames("en", { type: "language" });
        global_res = result;
    }
    return n * 100;
}

function intl_display_language(n)
{
    var formatter = new Intl.DisplayNames("en", { type: "language" });
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.of("en-Latn-US-fonipa");
        global_res = result;
    }
    return n * 100;
}

function intl_display_calendar(n)
{
    var formatter = new Intl.DisplayNames("en", { type: "calendar" });
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.of("ethioaa");
        global_res = result;
    }
    return n * 100;
}

function intl_display_currency(n)
{
    var formatter = new Intl.DisplayNames("en", { type: "currency" });
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.of("USD");
        global_res = result;
    }
    return n * 100;
}

// Contrast common short normalization with a disordered nonstarter run.
// Cache source strings per case; fresh JavaScript processes reset the cache.
function string_normalize_short(n)
{
    var source = string_normalize_short.source || (string_normalize_short.source = "cafe\u0301");
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = source.normalize("NFC");
        global_res = result;
    }
    return n * 100;
}

function string_normalize_long(n)
{
    var source = string_normalize_long.source ||
        (string_normalize_long.source = "\u0301\u0300\u0323".repeat(128));
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = source.normalize("NFD");
        global_res = result;
    }
    return n * 100;
}

/* Fixed UTC input keeps date workloads independent of host zone and clock.
   Formatting cases prepare a reusable formatter outside the inner loop. */
function intl_date_construct(n)
{
    var options = { timeZone: "UTC", dateStyle: "medium", timeStyle: "medium" };
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = new Intl.DateTimeFormat("en", options);
        global_res = result;
    }
    return n * 100;
}

function intl_date_format(n)
{
    var format = new Intl.DateTimeFormat("en", {
        timeZone: "UTC", dateStyle: "medium", timeStyle: "medium"
    }).format;
    var base = Date.UTC(2024, 0, 1), i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = format(base + i * 1000);
        global_res = result;
    }
    return n * 100;
}

function intl_date_parts(n)
{
    var formatter = new Intl.DateTimeFormat("en", {
        timeZone: "UTC", dateStyle: "medium", timeStyle: "medium"
    });
    var base = Date.UTC(2024, 0, 1), i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.formatToParts(base + i * 1000);
        global_res = result;
    }
    return n * 100;
}

function intl_date_range_format(n)
{
    var formatter = new Intl.DateTimeFormat("en", {
        timeZone: "UTC", year: "numeric", month: "short", day: "numeric"
    });
    var base = Date.UTC(2024, 0, 1), day = 86400000, i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.formatRange(base + i * day, base + (i + 3) * day);
        global_res = result;
    }
    return n * 100;
}

function intl_date_range_parts(n)
{
    var formatter = new Intl.DateTimeFormat("en", {
        timeZone: "UTC", year: "numeric", month: "short", day: "numeric"
    });
    var base = Date.UTC(2024, 0, 1), day = 86400000, i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = formatter.formatRangeToParts(base + i * day, base + (i + 3) * day);
        global_res = result;
    }
    return n * 100;
}

function intl_date_zone(n)
{
    var format = new Intl.DateTimeFormat("en", {
        timeZone: "UTC", hour: "numeric", minute: "2-digit", timeZoneName: "longOffset"
    }).format;
    var base = Date.UTC(2024, 0, 1), i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = format(base + i * 1000);
        global_res = result;
    }
    return n * 100;
}

function intl_date_locale(n)
{
    var date = new Date(Date.UTC(2024, 0, 1));
    var options = { timeZone: "UTC", dateStyle: "medium", timeStyle: "medium" };
    var i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = date.toLocaleString("en", options);
        global_res = result;
    }
    return n * 100;
}

function intl_date_temporal(n)
{
    var format = new Intl.DateTimeFormat("en", {
        timeZone: "UTC", dateStyle: "medium", timeStyle: "medium"
    }).format;
    var value = new Temporal.PlainDateTime(2024, 1, 1, 12, 34, 56), i, j, result;
    for (j = 0; j < n; j++) {
        for (i = 0; i < 100; i++)
            result = format(value);
        global_res = result;
    }
    return n * 100;
}

function string_length(n)
{
    var str, sum, j;
    str = "abcde";
    sum = 0;
    for(j = 0; j < n; j++) {
        sum += str.length;
        sum += str.length;
        sum += str.length;
        sum += str.length;
    }
    global_res = sum;
    return n * 4;
}

/* incremental string contruction as local var */
function string_build1(n)
{
    var i, j, r;
    for(j = 0; j < n; j++) {
        r = "";
        for(i = 0; i < 1000; i++)
            r += "x";
        global_res = r;
    }
    return n * 1000;
}

/* incremental string contruction using + */
function string_build1x(n)
{
    var i, j, r;
    for(j = 0; j < n; j++) {
        r = "";
        for(i = 0; i < 1000; i++)
            r = r + "x";
        global_res = r;
    }
    return n * 1000;
}

/* incremental string contruction using +2c */
function string_build2c(n)
{
    var i, j;
    for(j = 0; j < n; j++) {
        var r = "";
        for(i = 0; i < 1000; i++)
            r += "xy";
        global_res = r;
    }
    return n * 1000;
}

/* incremental string contruction as arg */
function string_build2(n, r)
{
    var i, j;
    for(j = 0; j < n; j++) {
        r = "";
        for(i = 0; i < 1000; i++)
            r += "x";
        global_res = r;
    }
    return n * 1000;
}

/* incremental string contruction by prepending */
function string_build3(n)
{
    var i, j, r;
    for(j = 0; j < n; j++) {
        r = "";
        for(i = 0; i < 1000; i++)
            r = "x" + r;
        global_res = r;
    }
    return n * 1000;
}

/* incremental string contruction with multiple reference */
function string_build4(n)
{
    var i, j, r, s;
    for(j = 0; j < n; j++) {
        r = "";
        for(i = 0; i < 1000; i++) {
            s = r;
            r += "x";
        }
        global_res = r;
    }
    return n * 1000;
}

/* append */
function string_build_large1(n)
{
    var i, j, r, len = 20000;
    for(j = 0; j < n; j++) {
        r = "";
        for(i = 0; i < len; i++)
            r += "abcdef";
        global_res = r;
    }
    return n * len;
}

/* prepend */
function string_build_large2(n)
{
    var i, j, r, len = 20000;
    for(j = 0; j < n; j++) {
        r = "";
        for(i = 0; i < len; i++)
            r = "abcdef" + r;
        global_res = r;
    }
    return n * len;
}

/* sort bench */

function sort_bench(text) {
    function random(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[(Math.random() * n) >> 0];
    }
    function random8(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[(Math.random() * 256) >> 0];
    }
    function random1(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[(Math.random() * 2) >> 0];
    }
    function hill(arr, n, def) {
        var mid = n >> 1;
        for (var i = 0; i < mid; i++)
            arr[i] = def[i];
        for (var i = mid; i < n; i++)
            arr[i] = def[n - i];
    }
    function comb(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[(i & 1) * i];
    }
    function crisscross(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[(i & 1) ? n - i : i];
    }
    function zero(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[0];
    }
    function increasing(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[i];
    }
    function decreasing(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[n - 1 - i];
    }
    function alternate(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[i ^ 1];
    }
    function jigsaw(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[i % (n >> 4)];
    }
    function incbutone(arr, n, def) {
        for (var i = 0; i < n; i++)
            arr[i] = def[i];
        if (n > 0)
            arr[n >> 2] = def[n];
    }
    function incbutfirst(arr, n, def) {
        if (n > 0)
            arr[0] = def[n];
        for (var i = 1; i < n; i++)
            arr[i] = def[i];
    }
    function incbutlast(arr, n, def) {
        for (var i = 0; i < n - 1; i++)
            arr[i] = def[i + 1];
        if (n > 0)
            arr[n - 1] = def[0];
    }

    var sort_cases = [ random, random8, random1, jigsaw, hill, comb,
                      crisscross, zero, increasing, decreasing, alternate,
                      incbutone, incbutlast, incbutfirst ];

    var n = sort_bench.array_size || 10000;
    var array_type = sort_bench.array_type || Array;
    var def, arr;
    var i, j, x, y;
    var total = 0;

    var save_total_score = total_score;
    var save_total_scale = total_scale;

    // initialize default sorted array (n + 1 elements)
    def = new array_type(n + 1);
    if (array_type == Array) {
        for (i = 0; i <= n; i++) {
            def[i] = i + "";
        }
    } else {
        for (i = 0; i <= n; i++) {
            def[i] = i;
        }
    }
    def.sort();
    for (var f of sort_cases) {
        var ti = 0, tx = 0;
        for (j = 0; j < 100; j++) {
            arr = new array_type(n);
            f(arr, n, def);
            var t1 = get_clock();
            arr.sort();
            t1 = get_clock() - t1;
            tx += t1;
            if (!ti || ti > t1)
                ti = t1;
            if (tx >= clocks_per_sec)
                break;
        }
        total += ti;

        i = 0;
        x = arr[0];
        if (x !== void 0) {
            for (i = 1; i < n; i++) {
                y = arr[i];
                if (y === void 0)
                    break;
                if (x > y)
                    break;
                x = y;
            }
        }
        while (i < n && arr[i] === void 0)
            i++;
        if (i < n) {
            console.log("sort_bench: out of order error for " + f.name +
                        " at offset " + (i - 1) +
                        ": " + arr[i - 1] + " > " + arr[i]);
        }
        if (sort_bench.verbose)
            log_one("sort_" + f.name, 1, ti / 100);
    }
    total_score = save_total_score;
    total_scale = save_total_scale;
    return total / n / 100;
}
sort_bench.bench = true;
sort_bench.verbose = false;

function int_to_string(n)
{
    var s, r, j;
    r = 0;
    for(j = 0; j < n; j++) {
        s = (j % 10) + '';
        s = (j % 100) + '';
        s = (j) + '';
    }
    return n * 3;
}

function int_toString(n)
{
    var s, r, j;
    r = 0;
    for(j = 0; j < n; j++) {
        s = (j % 10).toString();
        s = (j % 100).toString();
        s = (j).toString();
    }
    return n * 3;
}

function float_to_string(n)
{
    var s, r, j;
    r = 0;
    for(j = 0; j < n; j++) {
        s = (j % 10 + 0.1) + '';
        s = (j + 0.1) + '';
        s = (j * 12345678 + 0.1) + '';
    }
    return n * 3;
}

function float_toString(n)
{
    var s, r, j;
    r = 0;
    for(j = 0; j < n; j++) {
        s = (j % 10 + 0.1).toString();
        s = (j + 0.1).toString();
        s = (j * 12345678 + 0.1).toString();
    }
    return n * 3;
}

function float_toFixed(n)
{
    var s, r, j;
    r = 0;
    for(j = 0; j < n; j++) {
        s = (j % 10 + 0.1).toFixed(j % 16);
        s = (j + 0.1).toFixed(j % 16);
        s = (j * 12345678 + 0.1).toFixed(j % 16);
    }
    return n * 3;
}

function float_toPrecision(n)
{
    var s, r, j;
    r = 0;
    for(j = 0; j < n; j++) {
        s = (j % 10 + 0.1).toPrecision(j % 16 + 1);
        s = (j + 0.1).toPrecision(j % 16 + 1);
        s = (j * 12345678 + 0.1).toPrecision(j % 16 + 1);
    }
    return n * 3;
}

function float_toExponential(n)
{
    var s, r, j;
    r = 0;
    for(j = 0; j < n; j++) {
        s = (j % 10 + 0.1).toExponential(j % 16);
        s = (j + 0.1).toExponential(j % 16);
        s = (j * 12345678 + 0.1).toExponential(j % 16);
    }
    return n * 3;
}

function string_to_int(n)
{
    var s, r, j;
    r = 0;
    s = "12345";
    for(j = 0; j < n; j++) {
        r += (s | 0);
    }
    global_res = r;
    return n;
}

function string_to_float(n)
{
    var s, r, j;
    r = 0;
    s = "12345.6";
    for(j = 0; j < n; j++) {
        r -= s;
    }
    global_res = r;
    return n;
}

/* Iterator helper operations count input callbacks, including rejected
   filter items and empty flatMap results. */
function iterator_bench_values()
{
    var values = [];
    for (var i = 0; i < 1000; i++)
        values[i] = i;
    return values;
}

function iterator_map(n)
{
    var values = iterator_bench_values();
    var j, sum = 0;
    function mapper(value, index) { return value + index; }
    for (j = 0; j < n; j++) {
        sum = 0;
        for (var value of values.values().map(mapper))
            sum += value;
    }
    global_res = sum;
    return n * values.length;
}

function iterator_filter(n)
{
    var values = iterator_bench_values();
    var j, sum = 0;
    function predicate(value, index) { return (index & 1) == 0; }
    for (j = 0; j < n; j++) {
        sum = 0;
        for (var value of values.values().filter(predicate))
            sum += value;
    }
    global_res = sum;
    return n * values.length;
}

function iterator_flat_map(n)
{
    var values = iterator_bench_values();
    var j, sum = 0;
    function mapper(value, index) {
        return (index & 1) ? [] : [value, index];
    }
    for (j = 0; j < n; j++) {
        sum = 0;
        for (var value of values.values().flatMap(mapper))
            sum += value;
    }
    global_res = sum;
    return n * values.length;
}

function iterator_every(n)
{
    var values = iterator_bench_values();
    var j, result;
    function predicate(value, index) { return value == index; }
    for (j = 0; j < n; j++)
        result = values.values().every(predicate);
    global_res = result;
    return n * values.length;
}

function iterator_some(n)
{
    var values = iterator_bench_values();
    var j, result;
    function predicate(value, index) { return value != index; }
    for (j = 0; j < n; j++)
        result = values.values().some(predicate);
    global_res = result;
    return n * values.length;
}

function iterator_find(n)
{
    var values = iterator_bench_values();
    var j, result;
    function predicate(value, index) { return value != index; }
    for (j = 0; j < n; j++)
        result = values.values().find(predicate);
    global_res = result;
    return n * values.length;
}

function iterator_for_each(n)
{
    var values = iterator_bench_values();
    var j, sum = 0;
    function procedure(value, index) { sum += value + index; }
    for (j = 0; j < n; j++) {
        sum = 0;
        values.values().forEach(procedure);
    }
    global_res = sum;
    return n * values.length;
}

function iterator_reduce(n)
{
    var values = iterator_bench_values();
    var j, sum = 0;
    function reducer(acc, value, index) { return acc + value + index; }
    for (j = 0; j < n; j++)
        sum = values.values().reduce(reducer, 0);
    global_res = sum;
    return n * values.length;
}

function load_result(filename)
{
    var has_filename = filename;
    var has_error = false;
    var str, res;

    if (!filename)
        filename = "microbench.txt";

    if (typeof fs !== "undefined") {
        // read the file in Node.js
        try {
            str = fs.readFileSync(filename, { encoding: "utf8" });
        } catch {
            has_error = true;
        }
    } else
    if (typeof std !== "undefined") {
        // read the file in QuickJS
        var f = std.open(filename, "r");
        if (f) {
            str = f.readAsString();
            f.close();
        } else {
            has_error = true;
        }
    } else {
        return null;
    }
    if (has_error) {
        if (has_filename) {
            // Should throw exception?
            console.log("cannot load " + filename);
        }
        return null;
    }
    res = JSON.parse(str);
    return res;
}

function save_result(filename, obj)
{
    var str = JSON.stringify(obj, null, 2) + "\n";
    var has_error = false;

    if (typeof fs !== "undefined") {
        // save the file in Node.js
        try {
            str = fs.writeFileSync(filename, str, { encoding: "utf8" });
        } catch {
            has_error = true;
        }
    } else
    if (typeof std !== "undefined") {
        // save the file in QuickJS
        var f = std.open(filename, "w");
        if (f) {
            f.puts(str);
            f.close();
        } else {
            has_error = 'true';
        }
    } else {
        return;
    }
    if (has_error)
        console.log("cannot save " + filename);
}

function main(argc, argv, g)
{
    var test_list = [
        empty_loop,
        empty_down_loop,
        empty_down_loop2,
        empty_do_loop,
        date_now,
        date_parse,
        prop_read,
        prop_write,
        prop_update,
        prop_create,
        prop_clone,
        prop_delete,
        array_read,
        array_write,
        array_update,
        array_prop_create,
        array_slice,
        array_length_read,
        array_length_decr,
        array_hole_length_decr,
        array_push,
        array_pop,
        typed_array_read,
        typed_array_write,
        arguments_read,
        arguments_strict_read,
        global_read,
        global_write,
        global_write_strict,
        local_destruct,
        closure_destruct,
        global_destruct,
        global_destruct_strict,
        global_func_call,
        func_call,
        func_closure_call,
        int_arith,
        float_arith,
        map_set_string,
        map_set_int,
        map_set_bigint,
        map_delete,
        weak_map_set,
        weak_map_delete,
        array_for,
        array_for_in,
        array_for_of,
        math_min,
        regexp_compile,
        regexp_compile_large,
        regexp_ascii,
        regexp_utf16,
        regexp_replace,
        string_length,
        string_build1,
        string_build1x,
        string_build2c,
        string_build2,
        string_build3,
        string_build4,
        string_build_large1,
        string_build_large2,
        int_to_string,
        int_toString,
        float_to_string,
        float_toString,
        float_toFixed,
        float_toPrecision,
        float_toExponential,
        string_to_int,
        string_to_float,
    ];
    var tests = [];
    var i, j, n, f, name, found;
    var ref_file, new_ref_file = "microbench-new.txt";

    if (typeof String.prototype.normalize === "function") {
        test_list.push(string_normalize_short);
        test_list.push(string_normalize_long);
    }

    if (typeof BigInt === "function") {
        /* BigInt test */
        test_list.push(bigint32_arith);
        test_list.push(bigint64_arith);
        test_list.push(bigint256_arith);
        test_list.push(bigint_to_string);
        test_list.push(bigint_to_string_large);
        if (Object.prototype.hasOwnProperty.call(BigInt.prototype,
                                                 "toLocaleString")) {
            test_list.push(bigint_to_locale_string);
            test_list.push(bigint_to_locale_string_large);
        }
    }
    test_list.push(sort_bench);

    /* Additional feature cases do not change the frozen comparison suite. */
    if (typeof Intl === "object") {
        test_list.push(intl_case_default);
        test_list.push(intl_case_lower);
        test_list.push(intl_case_upper);
    }
    if (typeof Intl === "object" && typeof Intl.ListFormat === "function") {
        test_list.push(intl_list_construct);
        test_list.push(intl_list_ascii);
        test_list.push(intl_list_utf16);
        test_list.push(intl_list_parts);
    }
    if (typeof Intl === "object" && typeof Intl.PluralRules === "function") {
        test_list.push(intl_plural_construct);
        test_list.push(intl_plural_select);
        test_list.push(intl_plural_decimal);
        test_list.push(intl_plural_range);
    }
    if (typeof Intl === "object" && typeof Intl.NumberFormat === "function") {
        test_list.push(intl_number_construct);
        test_list.push(intl_number_format);
        test_list.push(intl_number_decimal);
        if (typeof BigInt === "function")
            test_list.push(intl_number_bigint);
        test_list.push(intl_number_parts);
        test_list.push(intl_number_range);
        test_list.push(intl_number_unit);
    }
    if (typeof Intl === "object" && typeof Intl.RelativeTimeFormat === "function") {
        test_list.push(intl_relative_construct);
        test_list.push(intl_relative_numeric);
        test_list.push(intl_relative_auto);
        test_list.push(intl_relative_parts);
    }
    if (typeof Intl === "object" && typeof Intl.DurationFormat === "function") {
        test_list.push(intl_duration_construct);
        test_list.push(intl_duration_text);
        test_list.push(intl_duration_digital);
        test_list.push(intl_duration_parts);
    }
    if (typeof Intl === "object" && typeof Intl.DisplayNames === "function") {
        test_list.push(intl_display_construct);
        test_list.push(intl_display_language);
        test_list.push(intl_display_calendar);
        test_list.push(intl_display_currency);
    }
    if (typeof Intl === "object" && typeof Intl.DateTimeFormat === "function") {
        test_list.push(intl_date_construct);
        test_list.push(intl_date_format);
        test_list.push(intl_date_parts);
        test_list.push(intl_date_range_format);
        test_list.push(intl_date_range_parts);
        test_list.push(intl_date_zone);
        test_list.push(intl_date_locale);
        if (typeof Temporal === "object" && typeof Temporal.PlainDateTime === "function")
            test_list.push(intl_date_temporal);
    }
    if (typeof Iterator === "function") {
        if (typeof Iterator.prototype.map === "function")
            test_list.push(iterator_map);
        if (typeof Iterator.prototype.filter === "function")
            test_list.push(iterator_filter);
        if (typeof Iterator.prototype.flatMap === "function")
            test_list.push(iterator_flat_map);
        if (typeof Iterator.prototype.every === "function")
            test_list.push(iterator_every);
        if (typeof Iterator.prototype.some === "function")
            test_list.push(iterator_some);
        if (typeof Iterator.prototype.find === "function")
            test_list.push(iterator_find);
        if (typeof Iterator.prototype.forEach === "function")
            test_list.push(iterator_for_each);
        if (typeof Iterator.prototype.reduce === "function")
            test_list.push(iterator_reduce);
    }

    for (i = 1; i < argc;) {
        name = argv[i++];
        if (name == "-a") {
            sort_bench.verbose = true;
            continue;
        }
        if (name == "-t") {
            name = argv[i++];
            sort_bench.array_type = g[name];
            if (typeof sort_bench.array_type !== "function") {
                console.log("unknown array type: " + name);
                return 1;
            }
            continue;
        }
        if (name == "-n") {
            sort_bench.array_size = +argv[i++];
            continue;
        }
        if (name == "-r") {
            ref_file = argv[i++];
            continue;
        }
        if (name == "-s") {
            new_ref_file = argv[i++];
            continue;
        }
        for (j = 0, found = false; j < test_list.length; j++) {
            f = test_list[j];
            if (f.name.startsWith(name)) {
                tests.push(f);
                found = true;
            }
        }
        if (!found) {
            console.log("unknown benchmark: " + name);
            return 1;
        }
    }
    if (tests.length == 0)
        tests = test_list;

    ref_data = load_result(ref_file);
    log_data = {};
    log_line.apply(null, heads);
    n = 0;

    for(i = 0; i < tests.length; i++) {
        f = tests[i];
        bench(f, f.name, ref_data, log_data);
        if (ref_data && ref_data[f.name])
            n++;
    }
    if (ref_data)
        log_line("total", "", total[2], total[3], Math.round(total_scale * 1000 / total_score));
    else
        log_line("total", "", total[2]);

    if (tests == test_list && new_ref_file)
        save_result(new_ref_file, log_data);
}

if (typeof scriptArgs === "undefined") {
    if (typeof process !== "undefined" && typeof process.argv === "object") {
        /* node case */
        scriptArgs = process.argv.slice(1);
    } else if (typeof arguments !== "undefined") {
        /* d8 case */
        scriptArgs = arguments;
    } else {
        scriptArgs = [];
    }
}
main(scriptArgs.length, scriptArgs, this);
