import * as std from "std";

let realm = std.createRealm();
const Instant = realm.global.Temporal.Instant;
const expected = Instant.prototype;
const original = new Instant(17n);
realm = null;
std.gc();
const copy = Instant.from(original);
if (copy === original || Object.getPrototypeOf(copy) !== expected ||
    expected === Temporal.Instant.prototype || copy.epochNanoseconds !== 17n)
    throw Error("Temporal.Instant must use its native function realm");
std.gc();
if (Instant.from(copy).epochNanoseconds !== 17n)
    throw Error("escaped Temporal intrinsics must retain their realm");
