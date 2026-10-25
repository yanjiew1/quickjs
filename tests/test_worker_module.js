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

/* Worker code for test_worker.js */
import * as std from "std";
import * as os from "os";

var parent = os.Worker.parent;

function handle_msg(e) {
    var ev = e.data;
    //          print("child_recv", JSON.stringify(ev));
    switch(ev.type) {
    case "abort":
        parent.postMessage({ type: "done" });
        parent.onmessage = null; /* terminate the worker */
        break;
    case "sab":
        /* modify the SharedArrayBuffer */
        ev.buf[2] = 10;
        parent.postMessage({ type: "sab_done", buf: ev.buf });
        break;
    }
}

function worker_main() {
    var i;

    parent.onmessage = handle_msg;
    for(i = 0; i < 10; i++) {
        parent.postMessage({ type: "num", num: i });
    }
}

worker_main();
