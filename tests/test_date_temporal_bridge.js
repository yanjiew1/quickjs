/* Native Date bridge regression. */
function same(actual, expected) {
    if (!Object.is(actual, expected)) throw Error("unexpected value");
}
function throws(type, callback) {
    let caught;
    try { callback(); } catch (error) { caught = error; }
    if (!(caught instanceof type)) throw Error("unexpected completion");
}
same(new Date(-1).toTemporalInstant().epochNanoseconds, -1000000n);
throws(RangeError, () => new Date(NaN).toTemporalInstant());
throws(TypeError, () => Date.prototype.toTemporalInstant.call({}));
same(Date.prototype.toTemporalInstant.length, 0);
let date = new Date(42);
Object.defineProperty(date, "valueOf", { get() { throw Error("read valueOf"); } });
same(date.toTemporalInstant().epochNanoseconds, 42000000n);
