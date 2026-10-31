#! (shebang test)
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

import * as std from "std";
import * as os from "os";

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

// load more elaborate version of assert if available
try { std.loadScript("test_assert.js"); } catch(e) {}

/*----------------*/

function test_printf()
{
    assert(std.sprintf("a=%d s=%s", 123, "abc"), "a=123 s=abc");
    assert(std.sprintf("%010d", 123), "0000000123");
    assert(std.sprintf("%x", -2), "fffffffe");
    assert(std.sprintf("%lx", -2), "fffffffffffffffe");
    assert(std.sprintf("%10.1f", 2.1), "       2.1");
    assert(std.sprintf("%*.*f", 10, 2, -2.13), "     -2.13");
    assert(std.sprintf("%.*s", -1, "abc"), "abc");
    assert(std.sprintf("%.*f", -1, 1.25), "1.250000");
    assert(std.sprintf("%u %lu", -1, -1), "4294967295 18446744073709551615");
    assert(std.sprintf("%.2147483647s", ""), "");
    /* Reject an overflowing field and a truncated synthesized format. */
    for (const [format, args] of [
        ["prefix %999999999999d suffix", [1]],
        ["%2147483648d", [1]],
        ["%4294967296d", [1]],
        ["%.2147483648s", ["x"]],
        ["%*s", [-2147483648, "x"]],
        ["%" + "-".repeat(29) + "*s", [2147483647, "x"]],
        ["%" + "-".repeat(28) + ".*s", [2147483647, "x"]],
    ]) {
        let failed = false;
        try {
            std.sprintf(format, ...args);
        } catch (e) {
            failed = e instanceof TypeError;
        }
        assert(failed);
    }
    assert(std.sprintf("%#lx", 0x7fffffffffffffffn), "0x7fffffffffffffff");

    /* A leading format specifier appends zero bytes to a pristine DynBuf. */
    assert(std.sprintf("%s", "x"), "x");
    assert(std.sprintf("%s", ""), "");
    const str = "x".repeat(4096);
    assert(std.sprintf("%s:%s", str, str), str + ":" + str);
}

function test_file1()
{
    var f, len, str, size, buf, ret, i, str1;

    f = std.tmpfile();
    str = "hello world\n";
    f.puts(str);

    f.seek(0, std.SEEK_SET);
    str1 = f.readAsString();
    assert(str1 === str);

    f.seek(0, std.SEEK_END);
    size = f.tell();
    assert(size === str.length);

    f.seek(0, std.SEEK_SET);

    buf = new Uint8Array(size);
    ret = f.read(buf.buffer, 0, size);
    assert(ret === size);
    for(i = 0; i < size; i++)
        assert(buf[i] === str.charCodeAt(i));

    f.close();
}

function test_file_error_object()
{
    const marker = {};
    const error = { set errno(value) { throw marker; } };
    const calls = [
        () => std.tmpfile(error),
        () => std.open("tests/test_std.js", "r", error),
        () => std.open("missing-file-error-object", "r", error),
        () => std.fdopen(-1, "r", error),
    ];

    for (const call of calls) {
        let caught;
        try {
            const file = call();
            if (file)
                file.close();
        } catch (e) {
            caught = e;
        }
        assert(caught, marker);
    }

    const status = {};
    const file = std.tmpfile(status);
    assert(status.errno, 0);
    file.close();
}

test_file_error_object();

function test_empty_buffer_io()
{
    const f = std.tmpfile();
    const fd = f.fileno();
    const empty = new ArrayBuffer(0);
    assert(f.read(empty, 0, 0), 0);
    assert(f.write(empty, 0, 0), 0);
    assert(os.read(fd, empty, 0, 0), 0);
    assert(os.write(fd, empty, 0, 0), 0);
    assert(os.read(-1, empty, 0, 0) < 0);
    assert(os.write(-1, empty, 0, 0) < 0);

    function check_error(expected, call)
    {
        let error;
        try { call(); } catch(e) { error = e; }
        assert(error instanceof expected);
    }
    for (const call of [
        () => f.read(empty, 1, 0),
        () => f.write(empty, 0, 1),
        () => os.read(fd, empty, 1, 0),
        () => os.write(fd, empty, 0, 1),
    ])
        check_error(RangeError, call);
    const detached = new ArrayBuffer(0);
    detached.transfer(0);
    for (const invalid of [{}, detached]) {
        for (const call of [
            () => f.read(invalid, 0, 0),
            () => f.write(invalid, 0, 0),
            () => os.read(fd, invalid, 0, 0),
            () => os.write(fd, invalid, 0, 0),
        ])
            check_error(TypeError, call);
    }
    f.close();
}

function test_file2()
{
    var f, str, i, size;
    f = std.tmpfile();
    str = "hello world\n";
    size = str.length;
    for(i = 0; i < size; i++)
        f.putByte(str.charCodeAt(i));
    f.seek(0, std.SEEK_SET);
    for(i = 0; i < size; i++) {
        assert(str.charCodeAt(i) === f.getByte());
    }
    assert(f.getByte() === -1);
    f.close();
}

function test_getline()
{
    var f, line, line_count, lines, i;

    lines = ["hello world", "line 1", "line 2" ];
    f = std.tmpfile();
    for(i = 0; i < lines.length; i++) {
        f.puts(lines[i], "\n");
    }

    f.seek(0, std.SEEK_SET);
    assert(!f.eof());
    line_count = 0;
    for(;;) {
        line = f.getline();
        if (line === null)
            break;
        assert(line == lines[line_count]);
        line_count++;
    }
    assert(f.eof());
    assert(line_count === lines.length);

    f.close();
}

function test_popen()
{
    var str, f, fname = "tmp_file.txt";
    var content = "hello world";

    f = std.open(fname, "w");
    f.puts(content);
    f.close();

    /* test loadFile */
    assert(std.loadFile(fname), content);

    /* execute the 'cat' shell command */
    f = std.popen("cat " + fname, "r");
    str = f.readAsString();
    f.close();

    assert(str, content);

    os.remove(fname);
}

function test_ext_json()
{
    var expected, input, obj;
    expected = '{"x":false,"y":true,"z2":null,"a":[1,8,160],"b":"abc\\u000bd","s":"str"}';
    input = `{ "x":false, /*comments are allowed */
               "y":true,  // also a comment
               z2:null, // unquoted property names
               "a":[+1,0o10,0xa0,], // plus prefix, octal, hexadecimal
               "b": "ab\
c\\vd", // multi-line strings, '\v' escape
               "s":'str',} // trailing comma in objects and arrays, single quoted string
            `;
    obj = std.parseExtJSON(input);
    assert(JSON.stringify(obj), expected);

    obj = std.parseExtJSON('[Infinity, +Infinity, -Infinity, NaN, +NaN, -NaN, .1, -.2]');
    assert(obj[0], Infinity);
    assert(obj[1], Infinity);
    assert(obj[2], -Infinity);
    assert(obj[3], NaN);
    assert(obj[4], NaN);
    assert(obj[5], NaN);
    assert(obj[6], 0.1);
    assert(obj[7], -0.2);
}

function test_os()
{
    var fd, fpath, fname, fdir, buf, buf2, i, files, err, fdate, st, link_path;

    const stdinIsTTY = !os.exec(["/bin/sh", "-c", "test -t 0"], { usePath: false });

    assert(os.isatty(0), stdinIsTTY, `isatty(STDIN)`);

    fdir = "test_tmp_dir";
    fname = "tmp_file.txt";
    fpath = fdir + "/" + fname;
    link_path = fdir + "/test_link";

    os.remove(link_path);
    os.remove(fpath);
    os.remove(fdir);

    err = os.mkdir(fdir, 0o755);
    assert(err === 0);

    fd = os.open(fpath, os.O_RDWR | os.O_CREAT | os.O_TRUNC);
    assert(fd >= 0);

    buf = new Uint8Array(10);
    for(i = 0; i < buf.length; i++)
        buf[i] = i;
    assert(os.write(fd, buf.buffer, 0, buf.length) === buf.length);

    assert(os.seek(fd, 0, std.SEEK_SET) === 0);
    buf2 = new Uint8Array(buf.length);
    assert(os.read(fd, buf2.buffer, 0, buf2.length) === buf2.length);

    for(i = 0; i < buf.length; i++)
        assert(buf[i] == buf2[i]);

    if (typeof BigInt !== "undefined") {
        assert(os.seek(fd, BigInt(6), std.SEEK_SET), BigInt(6));
        assert(os.read(fd, buf2.buffer, 0, 1) === 1);
        assert(buf[6] == buf2[0]);
    }

    assert(os.close(fd) === 0);

    [files, err] = os.readdir(fdir);
    assert(err, 0);
    assert(files.indexOf(fname) >= 0);

    fdate = 10000;

    err = os.utimes(fpath, fdate, fdate);
    assert(err, 0);

    [st, err] = os.stat(fpath);
    assert(err, 0);
    assert(st.mode & os.S_IFMT, os.S_IFREG);
    assert(st.mtime, fdate);

    err = os.symlink(fname, link_path);
    assert(err === 0);

    [st, err] = os.lstat(link_path);
    assert(err, 0);
    assert(st.mode & os.S_IFMT, os.S_IFLNK);

    [buf, err] = os.readlink(link_path);
    assert(err, 0);
    assert(buf, fname);

    assert(os.remove(link_path) === 0);

    [buf, err] = os.getcwd();
    assert(err, 0);

    [buf2, err] = os.realpath(".");
    assert(err, 0);

    assert(buf, buf2);

    assert(os.remove(fpath) === 0);

    fd = os.open(fpath, os.O_RDONLY);
    assert(fd < 0);

    assert(os.remove(fdir) === 0);
}

function test_os_exec()
{
    var ret, fds, pid, f, status;

    ret = os.exec(["true"]);
    assert(ret, 0);

    ret = os.exec(["/bin/sh", "-c", "exit 1"], { usePath: false });
    assert(ret, 1);

    fds = os.pipe();
    pid = os.exec(["sh", "-c", "echo $FOO"], {
        stdout: fds[1],
        block: false,
        env: { FOO: "hello" },
    } );
    assert(pid >= 0);
    os.close(fds[1]); /* close the write end (as it is only in the child)  */
    f = std.fdopen(fds[0], "r");
    assert(f.getline(), "hello");
    assert(f.getline(), null);
    f.close();
    [ret, status] = os.waitpid(pid, 0);
    assert(ret, pid);
    assert(status & 0x7f, 0); /* exited */
    assert(status >> 8, 0); /* exit code */

    /* Keep stdin open so cat cannot exit on EOF before receiving SIGTERM. */
    fds = os.pipe();
    pid = os.exec(["cat"], { block: false, stdin: fds[0] } );
    assert(pid >= 0);
    /* The blocked child has no exit status yet; the tuple uses zero. */
    [ret, status] = os.waitpid(pid, os.WNOHANG);
    assert(ret, 0);
    assert(status, 0);
    os.close(fds[0]);
    assert(os.kill(pid, os.SIGTERM), 0);
    [ret, status] = os.waitpid(pid, 0);
    assert(ret, pid);
    assert(status !== 0, true, `expect nonzero exit code (got ${status})`);
    assert(status & 0x7f, os.SIGTERM);
    os.close(fds[1]);
}

function test_os_exec_empty_path_entry()
{
    const oldPath = std.getenv("PATH");

    try {
        std.setenv("PATH", ":/definitely-nonexistent");
        assert(os.exec(["sh", "-c", "exit 0"], { cwd: "/bin" }), 0);
    } finally {
        if (oldPath === undefined)
            std.unsetenv("PATH");
        else
            std.setenv("PATH", oldPath);
    }
}

function test_invalid_environment_names()
{
    for (const name of ["", "QJS_INVALID=NAME"]) {
        let set_failed = false;
        let unset_failed = false;
        try {
            std.setenv(name, "value");
        } catch (e) {
            set_failed = e instanceof TypeError;
        }
        try {
            std.unsetenv(name);
        } catch (e) {
            unset_failed = e instanceof TypeError;
        }
        assert(set_failed && unset_failed);
    }
}

function test_os_exec_overlapping_fds()
{
    const first = os.pipe();
    const second = os.pipe();
    const savedIn = os.dup(0);
    const savedOut = os.dup(1);
    const byte = new Uint8Array(1);

    assert(savedOut >= 0);
    try {
        assert(os.dup2(first[1], 0), 0);
        assert(os.dup2(second[1], 1), 1);
        assert(os.exec(["/bin/sh", "-c", "printf X"], {
            usePath: false, stdin: 1, stdout: 0,
        }), 0);
    } finally {
        if (savedIn >= 0) {
            os.dup2(savedIn, 0);
            os.close(savedIn);
        } else {
            os.close(0);
        }
        os.dup2(savedOut, 1);
        os.close(savedOut);
        os.close(first[1]);
        os.close(second[1]);
    }
    assert(os.read(first[0], byte.buffer, 0, 1), 1);
    assert(byte[0], 88);
    assert(os.read(second[0], byte.buffer, 0, 1), 0);
    os.close(first[0]);
    os.close(second[0]);
}

function test_timer()
{
    var th, i;

    /* just test that a timer can be inserted and removed */
    th = [];
    for(i = 0; i < 3; i++)
        th[i] = os.setTimeout(function () { }, 1000);
    for(i = 0; i < 3; i++)
        os.clearTimeout(th[i]);
}

/* test closure variable handling when freeing asynchronous
   function */
function test_async_gc()
{
    (async function run () {
        let obj = {}

        let done = () => {
            obj
            std.gc();
        }

        Promise.resolve().then(done)

        const p = new Promise(() => {})

        await p
    })();
}

/* check that the promise async rejection handler is not invoked when
   the rejection is handled not too late after the promise
   rejection. */
function test_async_promise_rejection()
{
    var counter = 0;
    var p1, p2, p3;
    p1 = Promise.reject();
    p2 = Promise.reject();
    p3 = Promise.resolve();
    p1.catch(() => counter++);
    p2.catch(() => counter++);
    p3.then(() => counter++)
    os.setTimeout(() => { assert(counter, 3) }, 10);
}

test_printf();
test_file1();
test_empty_buffer_io();
test_file2();
test_getline();
test_popen();
test_os();
test_os_exec();
test_os_exec_empty_path_entry();
test_invalid_environment_names();
test_os_exec_overlapping_fds();
test_timer();
test_ext_json();
test_async_gc();
test_async_promise_rejection();

function test_eval_script_global_references()
{
    const run = "global_reference_route_eval";
    const cases = [
        ["simple", 1, "", "=", 1, 3],
        ["strict", 1, '"use strict";', "=", 1, 3],
        ["compound", 1, "", "+=", 1, 4],
        ["compound_strict", 1, '"use strict";', "+=", 1, 4],
        ["logical", 0, "", "||=", 0, 3],
        ["nullish", null, "", "??=", null, 3],
    ];
    globalThis[run] = code => std.evalScript(code);
    try {
        for (const [suffix, initial, prefix, operator, object, lexical] of cases) {
            const name = "global_reference_route_" + suffix;
            globalThis[name] = initial;
            std.evalScript(`${prefix} ${name} ${operator}
                (globalThis.${run}("let ${name} = 2"), 3);`);
            assert(globalThis[name], object);
            assert(std.evalScript(name), lexical);
            delete globalThis[name];
        }

        const name = "global_reference_route_indirect_eval";
        globalThis[name] = 1;
        std.evalScript(`${name} = ((0, eval)("let ${name} = 2"), 3);`);
        assert(globalThis[name], 3);
        assert(std.evalScript(name), 3);
        delete globalThis[name];

        for (const compound of [false, true]) {
            const suspended = "global_reference_route_suspended_" + compound;
            globalThis[suspended] = 1;
            const operator = compound ? "+=" : "=";
            const iterator = std.evalScript(`(function*() {
                ${suspended} ${operator} yield 9;
            })()`);
            assert(iterator.next().value, 9);
            std.evalScript(`let ${suspended} = 2`);
            std.gc();
            assert(iterator.next(3).done, true);
            assert(globalThis[suspended], 1);
            assert(std.evalScript(suspended), compound ? 4 : 3);
            delete globalThis[suspended];
        }
    } finally {
        delete globalThis[run];
        for (const [suffix] of cases)
            delete globalThis["global_reference_route_" + suffix];
        delete globalThis.global_reference_route_indirect_eval;
        delete globalThis.global_reference_route_suspended_false;
        delete globalThis.global_reference_route_suspended_true;
    }
}

function test_eval_script_global_reference_errors()
{
    const run = "global_reference_route_error_eval";
    const effect = "global_reference_route_error_rhs";
    const names = [];

    function check_error(expected, source)
    {
        let error;
        try { std.evalScript(source); } catch (e) { error = e; }
        assert(error instanceof expected);
    }

    globalThis[run] = code => std.evalScript(code);
    try {
        for (const strict of [false, true]) {
            const prefix = strict ? '"use strict";' : "";
            const suffix = strict ? "strict" : "sloppy";
            const name = "global_reference_route_const_" + suffix;
            names.push(name);
            globalThis[name] = 1;
            check_error(TypeError, `${prefix} ${name} =
                (globalThis.${run}("const ${name} = 2"), 3);`);
            assert(globalThis[name], 1);
            assert(std.evalScript(name), 2);

            const tdz = "global_reference_route_tdz_" + suffix;
            names.push(tdz);
            globalThis[tdz] = 1;
            globalThis[effect] = false;
            check_error(ReferenceError, `${prefix} ${tdz} = (function() {
                try { globalThis.${run}("throw 0; let ${tdz} = 2"); }
                catch (e) {}
                globalThis.${effect} = true;
                return 3;
            })();`);
            assert(globalThis[effect], true);
            assert(globalThis[tdz], 1);
            check_error(ReferenceError, tdz);

            const missing = "global_reference_route_missing_" + suffix;
            names.push(missing);
            delete globalThis[missing];
            globalThis[effect] = false;
            const source = `${prefix} ${missing} =
                (globalThis.${run}("let ${missing} = 2"),
                 globalThis.${effect} = true, 3);`;
            if (strict) {
                check_error(ReferenceError, source);
                assert(Object.hasOwn(globalThis, missing), false);
            } else {
                std.evalScript(source);
                assert(globalThis[missing], 3);
            }
            assert(globalThis[effect], true);
            assert(std.evalScript(missing), 2);
        }

        const name = "global_reference_route_existing_const";
        names.push(name);
        globalThis[name] = 1;
        std.evalScript(`const ${name} = 2`);
        check_error(TypeError, `${name} = 3`);
        assert(globalThis[name], 1);
        assert(std.evalScript(name), 2);

        const tdz = "global_reference_route_existing_tdz";
        names.push(tdz);
        globalThis[tdz] = 1;
        try { std.evalScript(`throw 0; let ${tdz} = 2`); } catch (e) {}
        globalThis[effect] = false;
        check_error(ReferenceError, `${tdz} = (globalThis.${effect} = true, 3)`);
        assert(globalThis[effect], true);
        assert(globalThis[tdz], 1);
        check_error(ReferenceError, tdz);
    } finally {
        delete globalThis[run];
        delete globalThis[effect];
        for (const name of names)
            delete globalThis[name];
    }
}

function test_eval_script_global_reference_environments()
{
    const run = "global_reference_route_environment_eval";
    const effect = "global_reference_route_environment_rhs";
    const old_prototype = Object.getPrototypeOf(globalThis);
    const old_unscopables = Object.getOwnPropertyDescriptor(globalThis,
                                                           Symbol.unscopables);
    const names = [];
    globalThis[run] = code => std.evalScript(code);
    try {
        for (const blocked of [false, true]) {
            const name = "global_reference_route_with_" + blocked;
            names.push(name);
            globalThis[name] = 1;
            Object.defineProperty(globalThis, Symbol.unscopables, {
                value: { [name]: blocked }, configurable: true,
            });
            std.evalScript(`with (globalThis) {
                ${name} = (globalThis.${run}("let ${name} = 2"), 3);
            }`);
            assert(globalThis[name], blocked ? 1 : 3);
            assert(std.evalScript(name), blocked ? 3 : 2);
            delete globalThis[name];
        }
        delete globalThis[Symbol.unscopables];

        for (const mode of ["capture", "capture_tdz", "capture_const", "read",
                            "write", "false", "false_strict", "throw"]) {
            const name = "global_reference_route_proxy_" + mode;
            names.push(name);
            const target = Object.create(old_prototype);
            target[name] = 1;
            const sentinel = new Error("global reference capture");
            let has_count = 0, get_count = 0, set_count = 0;
            Object.setPrototypeOf(globalThis, new Proxy(target, {
                has(object, key) {
                    if (key !== name)
                        return Reflect.has(object, key);
                    has_count++;
                    if (mode === "throw")
                        throw sentinel;
                    if ((mode.startsWith("capture") || mode.startsWith("false")) &&
                        has_count === 1 ||
                        (mode === "read" || mode === "write") && has_count === 2) {
                        if (mode === "capture_tdz") {
                            try { std.evalScript(`throw 0; let ${name} = 2`); }
                            catch (e) {}
                        } else {
                            const declaration = mode === "capture_const" ? "const" : "let";
                            std.evalScript(`${declaration} ${name} = 2`);
                        }
                    }
                    if (mode.startsWith("false"))
                        return false;
                    return Reflect.has(object, key);
                },
                get(object, key, receiver) {
                    if (key === name)
                        get_count++;
                    return Reflect.get(object, key, receiver);
                },
                set(object, key, value, receiver) {
                    if (key === name)
                        set_count++;
                    return Reflect.set(object, key, value, receiver);
                },
            }));
            globalThis[effect] = false;
            const operator = mode.startsWith("capture") || mode === "read" ? "+=" : "=";
            const prefix = mode === "false_strict" ? '"use strict";' : "";
            let error;
            try {
                std.evalScript(`${prefix} ${name} ${operator}
                    (globalThis.${effect} = true, 3);`);
            } catch (e) { error = e; }

            // Inspect counts before later reads can invoke the prototype.
            if (mode === "capture") {
                assert(error, undefined);
                assert(has_count, 1);
                assert(get_count, 0);
                assert(set_count, 0);
                assert(Object.hasOwn(globalThis, name), false);
                assert(std.evalScript(name), 5);
            } else if (mode === "capture_tdz" || mode === "capture_const") {
                assert(error instanceof (mode === "capture_tdz" ? ReferenceError : TypeError));
                assert(globalThis[effect], mode === "capture_const");
                assert(has_count, 1);
                assert(get_count, 0);
                assert(set_count, 0);
                assert(Object.hasOwn(globalThis, name), false);
                if (mode === "capture_const")
                    assert(std.evalScript(name), 2);
            } else if (mode === "read") {
                assert(error, undefined);
                assert(has_count, 2);
                assert(get_count, 1);
                assert(set_count, 0);
                assert(Object.hasOwn(globalThis, name), false);
                assert(std.evalScript(name), 4);
            } else if (mode === "write" || mode === "false") {
                assert(error, undefined);
                assert(has_count, mode === "write" ? 2 : 1);
                assert(get_count, 0);
                assert(set_count, 1);
                assert(globalThis[name], 3);
                assert(std.evalScript(name), 2);
            } else if (mode === "false_strict") {
                assert(error instanceof ReferenceError);
                assert(globalThis[effect], true);
                assert(has_count, 1);
                assert(get_count, 0);
                assert(set_count, 0);
                assert(Object.hasOwn(globalThis, name), false);
                assert(std.evalScript(name), 2);
            } else {
                assert(error, sentinel);
                assert(globalThis[effect], false);
                assert(has_count, 1);
                assert(get_count, 0);
                assert(set_count, 0);
                assert(Object.hasOwn(globalThis, name), false);
            }
            Object.setPrototypeOf(globalThis, old_prototype);
            delete globalThis[name];
        }
    } finally {
        Object.setPrototypeOf(globalThis, old_prototype);
        if (old_unscopables)
            Object.defineProperty(globalThis, Symbol.unscopables, old_unscopables);
        else
            delete globalThis[Symbol.unscopables];
        delete globalThis[run];
        delete globalThis[effect];
        for (const name of names)
            delete globalThis[name];
    }
}

test_eval_script_global_references();
test_eval_script_global_reference_errors();
test_eval_script_global_reference_environments();

function test_assignment_rhs_eval_suspended()
{
    /* The std test is a module; use a sloppy function for leaking eval. */
    const state = Function(`
        let target = 1;
        function* suspended() {
            target = (yield 5, eval("var target = 2"), 3);
            return target;
        }
        return { iterator: suspended(), read: () => target };
    `)();
    const iterator = state.iterator;
    assert(iterator.next().value, 5);
    std.gc();
    assert(iterator.next().value, 2);
    assert(state.read(), 3);
}

test_assignment_rhs_eval_suspended();


function test_eval_script_with_global_reference()
{
    const realm = std.createRealm();
    realm.global.hostEval = source => realm.evalScript(source);
    realm.evalScript(`
        globalThis.global_record_target = 1;
        with ({}) {
            global_record_target = (hostEval("let global_record_target = 2"), 3);
        }
        if (globalThis.global_record_target !== 1 || global_record_target !== 3)
            throw Error("global reference lost its Environment Record");
    `);
}

test_eval_script_with_global_reference();

function test_eval_script_cached_global_tdz()
{
    const names = [];

    function check_reference_error(read)
    {
        let error;
        try { read(); } catch (e) { error = e; }
        assert(error instanceof ReferenceError);
    }

    try {
        for (const strict of [false, true]) {
            const prefix = strict ? '"use strict";' : "";
            for (const declaration of ["let", "const"]) {
                const suffix = declaration + "_" + strict;
                const initialized = "global_tdz_cache_initialized_" + suffix;
                names.push(initialized);
                globalThis[initialized] = 1;
                const initialized_read = std.evalScript(`${prefix}
                    (function() { return ${initialized}; })`);
                assert(initialized_read(), 1);
                std.evalScript(`${declaration} ${initialized} = 2`);
                assert(initialized_read(), 2);
                assert(globalThis[initialized], 1);

                for (const present of [false, true]) {
                    const name = "global_tdz_cache_" + suffix + "_" + present;
                    names.push(name);
                    if (present)
                        globalThis[name] = 1;
                    else
                        delete globalThis[name];
                    const read = std.evalScript(`${prefix}
                        (function() { return ${name}; })`);
                    const read_type = std.evalScript(`${prefix}
                        (function() { return typeof ${name}; })`);
                    if (present)
                        assert(read(), 1);
                    else
                        check_reference_error(read);
                    assert(read_type(), present ? "number" : "undefined");

                    let error;
                    try { std.evalScript(`throw 0; ${declaration} ${name} = 2`); }
                    catch (e) { error = e; }
                    assert(error, 0);
                    check_reference_error(read);
                    check_reference_error(read_type);
                    check_reference_error(() => std.evalScript(prefix + name));
                    check_reference_error(() => std.evalScript(prefix + " typeof " + name));
                    assert(Object.hasOwn(globalThis, name), present);
                    if (present)
                        assert(globalThis[name], 1);
                }
            }
        }
    } finally {
        for (const name of names)
            delete globalThis[name];
    }
}

test_eval_script_cached_global_tdz();
function test_checked_global_compound_resolution()
{
    const realm = std.createRealm();
    let calls = 0;
    realm.global.installLexical = () => {
        calls++;
        realm.evalScript("let checked_compound_global_target = 7");
    };
    realm.evalScript(`
        const original = Object.getPrototypeOf(globalThis);
        Object.setPrototypeOf(globalThis, new Proxy(original, {
            has(target, key) {
                if (key === "checked_compound_global_target") {
                    installLexical();
                    return false;
                }
                return Reflect.has(target, key);
            }
        }));
        try {
            let effects = 0, error;
            try {
                (function() {
                    "use strict";
                    checked_compound_global_target += ++effects;
                })();
            } catch (caught) { error = caught; }
            if (!(error instanceof ReferenceError) || effects !== 0)
                throw Error("compound assignment lost its unresolved reference");
        } finally {
            Object.setPrototypeOf(globalThis, original);
        }
        if (checked_compound_global_target !== 7)
            throw Error("compound assignment wrote the new lexical binding");
    `);
    assert(calls, 1);
}

test_checked_global_compound_resolution();

function test_eval_script_pure_global_store()
{
    for (const strict of [false, true]) {
        for (const resolved of [false, true]) {
            const realm = std.createRealm();
            let has_count = 0;
            Object.setPrototypeOf(realm.global, new Proxy({}, {
                has(target, name) {
                    if (name !== "pure_rhs_binding")
                        return Reflect.has(target, name);
                    if (++has_count === 1)
                        realm.evalScript("let pure_rhs_binding = 2");
                    return resolved;
                },
            }));
            let error;
            try {
                realm.evalScript(`${strict ? '"use strict";' : ''}
                    (function() { pure_rhs_binding = 7; })();`);
            } catch (e) { error = e; }
            assert(has_count, 1);
            if (strict && !resolved)
                assert(error instanceof realm.global.ReferenceError);
            else
                assert(error, undefined);
            assert(realm.evalScript("pure_rhs_binding"), resolved ? 7 : 2);
            assert(realm.global.pure_rhs_binding,
                   !resolved && !strict ? 7 : undefined);
        }
    }
}

test_eval_script_pure_global_store();

function test_eval_script_destructuring_references()
{
    const shapes = {
        named: "({ p: target } = source);",
        computed: "({ [key]: target } = source);",
        array: "[target] = iterable;",
        rest: "[...target] = iterable;",
        object_rest: "({...target} = source);",
        default: "[target = reenter()] = [];",
        suspended: "(function*() { [target = yield 1] = []; })()",
    };
    for (const [shape, body] of Object.entries(shapes)) {
        for (const strict of [false, true]) {
            for (const resolved of [false, true]) {
                const realm = std.createRealm();
                if (resolved)
                    realm.global.target = 1;
                let calls = 0, steps = 0, closed = 0;
                realm.global.reenter = () => {
                    calls++;
                    realm.evalScript("let target = 2");
                    return 7;
                };
                realm.global.source = { get p() { return realm.global.reenter(); } };
                realm.global.key = { toString() { return "p"; } };
                realm.global.iterable = {
                    [Symbol.iterator]() { return this; },
                    next() {
                        return steps++ ? { done: true } :
                            { value: realm.global.reenter(), done: false };
                    },
                    return() { closed++; return {}; }
                };
                let error;
                try {
                    const result = realm.evalScript(`${strict ? '"use strict";' : ''}
                        ${body}`);
                    if (shape === "suspended") {
                        assert(result.next().value, 1);
                        realm.global.reenter();
                        std.gc();
                        assert(result.next(7).done, true);
                    }
                } catch (e) { error = e; }
                assert(calls, 1);
                if (strict && !resolved)
                    assert(error instanceof realm.global.ReferenceError);
                else
                    assert(error, undefined);
                const value = realm.evalScript("target");
                const expected = shape === "rest" ? "7" : 7;
                const normalize = value => {
                    if (shape === "rest" && Array.isArray(value))
                        return value.join(",");
                    if (shape === "object_rest" && value && typeof value === "object")
                        return value.p;
                    return value;
                };
                assert(normalize(value), resolved ? expected : 2);
                assert(normalize(realm.global.target),
                       resolved ? 1 : strict ? undefined : expected);
                assert(closed, shape === "array" ? 1 : 0);
            }
        }
    }

    for (const declaration of ["const target = 1", "throw 0; let target"]) {
        const realm = std.createRealm();
        try { realm.evalScript(declaration); } catch (e) { assert(e, 0); }
        let closed = 0;
        realm.global.iterable = {
            [Symbol.iterator]() { return this; },
            next() { return { value: 7, done: false }; },
            return() { closed++; return {}; }
        };
        let error;
        try { realm.evalScript("[target] = iterable;"); } catch (e) { error = e; }
        const expected = declaration.startsWith("const") ?
            realm.global.TypeError : realm.global.ReferenceError;
        assert(error instanceof expected);
        assert(closed, 1);
    }
}

test_eval_script_destructuring_references();

async function test_extreme_timer_delays()
{
    let fired = false;
    const distant = os.setTimeout(() => { fired = true; }, 9223372036854774784);
    await new Promise(resolve => os.setTimeout(() => {
        os.clearTimeout(distant);
        resolve();
    }, 0));
    assert(fired, false);
    await os.sleepAsync(-9223372036854775808);
}

await test_extreme_timer_delays();

async function test_poll_fd_growth()
{
    if (os.platform === "win32")
        return;
    const pipes = [];
    try {
        const waits = [];
        for (let i = 0; i < 40; i++) {
            const pipe = os.pipe();
            pipes.push(pipe);
            waits.push(new Promise(resolve => {
                os.setReadHandler(pipe[0], () => {
                    const byte = new Uint8Array(1);
                    assert(os.read(pipe[0], byte.buffer, 0, 1), 1);
                    assert(byte[0], i);
                    os.setReadHandler(pipe[0], null);
                    resolve();
                });
            }));
            assert(os.write(pipe[1], new Uint8Array([i]).buffer, 0, 1), 1);
        }
        await Promise.all(waits);
    } finally {
        for (const [read, write] of pipes) {
            os.setReadHandler(read, null);
            os.close(read);
            os.close(write);
        }
    }
}

await test_poll_fd_growth();
