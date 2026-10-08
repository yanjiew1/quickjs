/* CONFIG_INTL enabled native regression tests; no polyfill or shell tools. */
function same(actual, expected, message) {
    if (!Object.is(actual, expected)) throw Error((message || "mismatch") + ": " + actual + " != " + expected);
}
function throws(type, operation) {
    try { operation(); } catch (error) { if (error instanceof type) return; throw error; }
    throw Error("expected " + type.name);
}
function joined(parts) { return parts.map(p => p.value).join(""); }
function values(parts, type) { return parts.filter(p => p.type === type).map(p => p.value).join(""); }
function nf(options) { return new Intl.NumberFormat("en-US", {useGrouping: false, ...options}); }

if (typeof Intl !== "object") throw Error("test requires CONFIG_INTL=1");
same((9007199254740993n).toLocaleString("en-US", {useGrouping:false}), "9007199254740993");
same((1234.5).toLocaleString("de-DE"), new Intl.NumberFormat("de-DE").format(1234.5));
const silentOptions = {get localeMatcher() {throw Error("must not read options");}};
throws(TypeError, () => Number.prototype.toLocaleString.call({}, undefined, silentOptions));
throws(TypeError, () => BigInt.prototype.toLocaleString.call({}, undefined, silentOptions));
const intrinsic = Intl.NumberFormat;
Intl.NumberFormat = function() {throw Error("global replacement");};
same((1234).toLocaleString("en-US", {useGrouping:false}), "1234");
Intl.NumberFormat = intrinsic;
