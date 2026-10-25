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

/* Growable SharedArrayBuffer Worker cloning and view freshness. */
import * as os from "os";

function assert(actual, expected) {
    if (actual !== expected)
        throw Error("got " + actual + ", expected " + expected);
}

const worker = new os.Worker("./test_gsab_worker_module.js");
const shared = new SharedArrayBuffer(8, { maxByteLength: 128 });
const tracking = new Uint8Array(shared);
const fixed = new Uint8Array(shared, 0, 8);
const empty = new SharedArrayBuffer(0, { maxByteLength: 0 });

worker.onmessage = event => {
    const message = event.data;
    if (message.type === "ready") {
        shared.grow(64);
        worker.postMessage({ type: "check" });
    } else if (message.type === "race-ready") {
        worker.postMessage({ type: "race" });
        try {
            shared.grow(80);
        } catch (error) {
            if (!(error instanceof RangeError))
                throw error;
        }
    } else if (message.type === "done") {
        assert(shared.byteLength, 96);
        assert(message.shared.byteLength, 96);
        assert(tracking.length, 96);
        assert(message.tracking.length, 96);
        assert(fixed.length, 8);
        assert(message.fixed.length, 8);
        assert(message.tracking[31], 37);
        assert(message.empty.byteLength, 0);
        assert(message.empty.maxByteLength, 0);
        assert(message.empty.growable, true);
        message.empty.grow(0);
        let failed = false;
        try { shared.grow(80); } catch (error) {
            failed = error instanceof RangeError;
        }
        assert(failed, true);
        worker.onmessage = null;
    } else {
        throw Error("unexpected Worker message");
    }
};

worker.postMessage({ type: "start", shared, tracking, fixed, empty });
