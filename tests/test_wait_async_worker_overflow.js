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

/* Windows native wake plus 64 Worker receive handles. */
import * as os from "os";
import { assert } from "./assert.js";

async function test_worker_overflow()
{
    const workers = [];
    const words = new Int32Array(new SharedArrayBuffer(4));
    let receivePong, fail, phase = "64 Worker startup handshakes";
    const deadline = new Promise((resolve, reject) => { fail = reject; });
    const timer = os.setTimeout(() => {
        fail(Error("timed out waiting for " + phase));
    }, 30000);

    try {
        const ready = [];
        for (let i = 0; i < 64; i++) {
            let receiveReady;
            ready.push(new Promise(resolve => { receiveReady = resolve; }));
            const worker = new os.Worker("./test_wait_async_worker_overflow_module.js");
            workers.push(worker);
            worker.onmessage = ({ data }) => {
                if (data === "ready") {
                    receiveReady();
                } else if (data === "pong" && i === 63) {
                    try {
                        assert(Atomics.notify(words, 0, 1), 1);
                        receivePong(data);
                    } catch (error) {
                        fail(error);
                    }
                } else {
                    fail(Error("unexpected Worker message"));
                }
            };
        }

        /* All 64 ports are serviceable before the native wake consumes a slot.
           Retain every handler after startup, keeping 65 active wait handles. */
        await Promise.race([Promise.all(ready), deadline]);
        phase = "Worker 64 delivery with a pending native wait";
        const wait = Atomics.waitAsync(words, 0, 0, Infinity);
        assert(wait.async, true);
        const pong = new Promise(resolve => { receivePong = resolve; });
        workers[63].postMessage("ping");
        const result = await Promise.race([
            Promise.all([pong, wait.value]), deadline
        ]);
        assert(result[0], "pong");
        assert(result[1], "ok");
    } finally {
        os.clearTimeout(timer);
        /* Release the native wait and child handlers on success or timeout. */
        Atomics.notify(words, 0);
        for (const worker of workers) {
            worker.postMessage("stop");
            worker.onmessage = null;
        }
    }
}

if (os.platform === "win32")
    await test_worker_overflow();
