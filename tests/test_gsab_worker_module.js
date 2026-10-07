/* Worker side of test_gsab_worker.js. */
import * as os from "os";

function assert(actual, expected) {
    if (actual !== expected)
        throw Error("got " + actual + ", expected " + expected);
}

const parent = os.Worker.parent;
let shared, tracking, fixed, empty, views, dataView, fixedDataView, atomic;
let typedViews;

parent.onmessage = event => {
    const message = event.data;
    if (message.type === "start") {
        ({ shared, tracking, fixed, empty } = message);
        assert(shared.byteLength, 8);
        assert(tracking.length, 8);
        assert(fixed.length, 8);
        views = {};
        for (const name of ["read", "write", "stringWrite", "keys", "has",
                            "descriptor", "define", "delete", "forin", "iterator"])
            views[name] = new Uint8Array(shared);
        dataView = new DataView(shared);
        fixedDataView = new DataView(shared, 0, 8);
        atomic = new Int32Array(shared);
        typedViews = [Uint8ClampedArray, Uint8Array, Int8Array, Uint16Array,
                      Int16Array, Uint32Array, Int32Array, BigUint64Array,
                      BigInt64Array, Float16Array, Float32Array, Float64Array]
            .map(C => new C(shared));
        empty.grow(0);
        parent.postMessage({ type: "ready" });
    } else if (message.type === "check") {
        assert(shared.byteLength, 64);
        assert(views.read[20], 0);
        views.write[20] = 33;
        assert(views.read[20], 33);
        views.stringWrite["21"] = 34;
        assert(views.read[21], 34);
        assert(Object.keys(views.keys).length, 64);
        assert(20 in views.has, true);
        assert(Object.getOwnPropertyDescriptor(views.descriptor, "20").value, 33);
        Object.defineProperty(views.define, "22", { value: 35 });
        assert(views.read[22], 35);
        assert(Reflect.deleteProperty(views.delete, "20"), false);
        let count = 0;
        for (const key in views.forin)
            count++;
        assert(count, 64);
        assert([...views.iterator].length, 64);
        dataView.setUint8(31, 37);
        assert(dataView.getUint8(31), 37);
        assert(dataView.byteLength, 64);
        assert(fixedDataView.byteLength, 8);
        Atomics.store(atomic, 12, 36);
        assert(Atomics.load(atomic, 12), 36);
        for (const view of typedViews) {
            const C = view.constructor;
            const length = 64 / C.BYTES_PER_ELEMENT;
            const value = C === BigInt64Array || C === BigUint64Array ? 1n : 1;
            view[length - 1] = value;
            assert(view[length - 1], value);
            assert(view.length, length);
        }
        assert(tracking.length, 64);
        assert(fixed.length, 8);
        let failed = false;
        try { shared.grow(32); } catch (error) {
            failed = error instanceof RangeError;
        }
        assert(failed, true);
        parent.postMessage({ type: "race-ready" });
    } else if (message.type === "race") {
        shared.grow(96);
        assert(shared.byteLength, 96);
        assert(tracking.length, 96);
        assert(dataView.byteLength, 96);
        assert(tracking[95], 0);
        parent.postMessage({ type: "done", shared, tracking, fixed, empty });
        parent.onmessage = null;
    } else {
        throw Error("unexpected parent message");
    }
};
