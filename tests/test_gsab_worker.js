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
