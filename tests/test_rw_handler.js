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

function handle_read(fd_r, fd_w, i)
{
    var buf = new Uint32Array(1);
    var val, len;
    len = os.read(fd_r, buf.buffer, 0, 4);
    os.setReadHandler(fd_r, null);
    val = buf[0];
//    print("read fd=", fd_r, "val=", val, "len=", len);
    assert(val, i);
}

function handle_write(fd_r, fd_w, i)
{
    var buf = new Uint32Array(1);
    buf[0] = i;
    os.write(fd_w, buf.buffer, 0, 4);
    os.setWriteHandler(fd_w, null);
}

function test_rw_handlers(n)
{
    var tab, fd_r, fd_w, i;
    tab = [];
    for(i = 0; i < n; i++) {
        tab[i] = os.pipe();
        fd_r = tab[i][0];
        fd_w = tab[i][1];
        os.setReadHandler(fd_r, handle_read.bind(null, fd_r, fd_w, i));
    }
    for(i = n - 1; i >= 0; i--) {
        fd_r = tab[i][0];
        fd_w = tab[i][1];
        os.setWriteHandler(fd_w, handle_write.bind(null, fd_r, fd_w, i));
    }
}

test_rw_handlers(100);
