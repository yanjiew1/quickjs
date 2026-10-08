/* Native locale casing and Array/TypedArray optional argument forwarding. */
function same(a, b) { if (!Object.is(a, b)) throw Error(JSON.stringify(a) + " !== " + JSON.stringify(b)); }
function throws(C, f) { try { f(); } catch (e) { if (e instanceof C) return; throw e; } throw Error("missing exception"); }
same("I\u0130".toLocaleLowerCase("tr"), "\u0131i");
same("i\u0131".toLocaleUpperCase("az"), "\u0130I");
same("I\u0301".toLocaleLowerCase("lt"), "i\u0307\u0301");
same("ΟΣ".toLocaleLowerCase("el"), "ος");
same("ά".toLocaleUpperCase("el"), "Α");
same("I\0I\ud800".toLocaleLowerCase("tr"), "\u0131\0\u0131\ud800");
same("I".toLocaleLowerCase(["zz", "tr"]), "i", "only first locale participates");
throws(RangeError, () => "I".toLocaleLowerCase(["tr", "bad_locale"]));
throws(TypeError, () => String.prototype.toLocaleLowerCase.call(null));
let order = [];
const receiver = { toString() { order.push("string"); return "I"; } };
const locales = { length: 1, get 0() { order.push("locale"); return "tr"; } };
same(String.prototype.toLocaleLowerCase.call(receiver, locales), "\u0131");
same(order.join(","), "string,locale");
/* Case conversion must not read a formatter options argument. */
same("I".toLocaleLowerCase("tr", new Proxy({}, { get() { throw Error("options"); } })), "\u0131");
const markerLocale = {}, markerOptions = {};
let calls = [];
const first = { toLocaleString(...args) { calls.push(args); return "first"; } };
const second = { toLocaleString(...args) { calls.push(args); return { toString() { return "second"; } }; } };
const joined = [first, null, undefined, , second].toLocaleString(markerLocale, markerOptions);
same(calls.length, 2);
for (const args of calls) {
    same(args.length, 2);
    same(args[0], markerLocale);
    same(args[1], markerOptions);
}
same(joined.startsWith("first"), true);
same(joined.endsWith("second"), true);
/* Host policy selects comma as its implementation-defined list separator. */
same(joined, "first,,,,second");
calls = [];
[first].toLocaleString();
same(calls[0].length, 2);
same(calls[0][0], undefined);
same(calls[0][1], undefined);
const abrupt = {};
try { [{ toLocaleString() { throw abrupt; } }].toLocaleString(markerLocale, markerOptions); throw Error("missing abrupt completion"); }
catch (e) { same(e, abrupt); }
const originalNumber = Object.getOwnPropertyDescriptor(Number.prototype, "toLocaleString");
const originalBigInt = Object.getOwnPropertyDescriptor(BigInt.prototype, "toLocaleString");
try {
    Object.defineProperty(Number.prototype, "toLocaleString", {
        configurable: true,
        value(...args) { calls.push([Number(this), args]); return "n"; }
    });
    calls = [];
    same(new Uint16Array([7, 9]).toLocaleString(markerLocale, markerOptions), "n,n");
    same(calls.length, 2);
    same(calls[0][0], 7);
    same(calls[1][0], 9);
    for (const call of calls) {
        same(call[1].length, 2);
        same(call[1][0], markerLocale);
        same(call[1][1], markerOptions);
    }
    Object.defineProperty(BigInt.prototype, "toLocaleString", {
        configurable: true,
        value(...args) { calls.push([this.valueOf(), args]); return "b"; }
    });
    calls = [];
    same(new BigInt64Array([7n, 9n]).toLocaleString(markerLocale, markerOptions), "b,b");
    same(calls[0][0], 7n);
    same(calls[0][1][0], markerLocale);
    same(calls[0][1][1], markerOptions);
} finally {
    Object.defineProperty(Number.prototype, "toLocaleString", originalNumber);
    Object.defineProperty(BigInt.prototype, "toLocaleString", originalBigInt);
}
