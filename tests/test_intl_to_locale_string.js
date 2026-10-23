/* Array/TypedArray locale wrappers preserve argument identity and invocation
   order. These checks need no particular non-English locale data. */
import { assert } from "./assert.js";

const locales = {toString() { throw Error("unexpected locale coercion"); }};
const options = {get maximumFractionDigits() { throw Error("unexpected option read"); }};
const cases = [[], [locales], [undefined, options], [locales, options, "ignored"]];
let calls = 0;
const value = {
    toLocaleString(...received) {
        calls++;
        assert(this === value);
        assert(received.length, 2);
        assert(received[0] === current[0]);
        assert(received[1] === current[1]);
        return {toString() { return "converted"; }};
    }
};
let current;
for (const args of cases) {
    current = args;
    assert([value].toLocaleString(...args), "converted");
    assert([null, undefined, , value].toLocaleString(...args), ",,,converted");
}
assert(calls, cases.length * 2);

// Each numeric element's own method receives exactly the first two arguments,
// including explicit undefined when the caller omits either optional argument.
for (const [Constructor, prototype, element] of [
    [Uint8Array, Number.prototype, 7],
    [Float64Array, Number.prototype, 7],
    [BigInt64Array, BigInt.prototype, 7n]
]) {
    const original = Object.getOwnPropertyDescriptor(prototype, "toLocaleString");
    try {
        Object.defineProperty(prototype, "toLocaleString", {configurable: true,
            value(...received) {
                assert(this === element);
                assert(received.length, 2);
                assert(received[0] === current[0]);
                assert(received[1] === current[1]);
                return "numeric";
            }});
        for (const args of cases) {
            current = args;
            assert(new Constructor([element]).toLocaleString(...args), "numeric");
        }
    } finally {
        Object.defineProperty(prototype, "toLocaleString", original);
    }
}

// Convert one result before touching the next element, and stop immediately
// after abrupt completion without invoking a later getter.
const events = [];
const abrupt = {};
const array = {
    length: 2,
    get 0() {
        events.push("get 0");
        return {get toLocaleString() {
            events.push("get method");
            return function(...received) {
                events.push("call");
                assert(received[0] === locales && received[1] === options);
                return {toString() { events.push("convert"); throw abrupt; }};
            };
        }};
    },
    get 1() { throw Error("unexpected later element"); }
};
let caught;
try { Array.prototype.toLocaleString.call(array, locales, options); }
catch (error) { caught = error; }
assert(caught === abrupt);
assert(JSON.stringify(events), JSON.stringify(["get 0", "get method", "call", "convert"]));
assert([1234.5].toLocaleString("en", {useGrouping: false, minimumFractionDigits: 2}), "1234.50");
assert(new Float64Array([1234.5]).toLocaleString("en", {
    useGrouping: false, minimumFractionDigits: 2
}), "1234.50");
