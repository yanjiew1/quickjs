/* Native ICU segmentation, ownership, and lazy cursor regressions. */
"use strict";

function same(actual, expected, label) {
    if (!Object.is(actual, expected))
        throw new Error(label + ": " + actual + " !== " + expected);
}
function check(condition, label) {
    if (!condition)
        throw new Error(label);
}
function throws(klass, callback, label) {
    try { callback(); } catch (error) {
        if (error instanceof klass) return;
        throw error;
    }
    throw new Error(label);
}

let segmenter = new Intl.Segmenter("en", { granularity: "grapheme" });
let input = "A\ud83d\ude00e\u0301\ud800\0Z";
let segments = segmenter.segment(input);
let values = [...segments];
same(values.map(value => value.segment).join("|"),
     "A|\ud83d\ude00|e\u0301|\ud800|\0|Z", "Unicode grapheme boundaries");
same(values.map(value => value.index).join(","), "0,1,3,5,6,7", "UTF16 indices");
for (let value of values) {
    same(value.input, input, "input slot retained");
    check(!Object.hasOwn(value, "isWordLike"), "graphemes omit word flag");
    same(Object.keys(value).join(","), "segment,index,input", "data property order");
}
same(segments.containing(1).index, 1, "high surrogate starts its own grapheme");
same(segments.containing(2).index, 1, "low surrogate belongs to whole pair");
same(segments.containing(4).segment, "e\u0301", "combining mark belongs to base");
same(segments.containing(undefined).index, 0, "undefined index becomes zero");
same(segments.containing(NaN).index, 0, "NaN index becomes zero");
same(segments.containing(-0.5).index, 0, "negative fraction truncates to negative zero");
same(segments.containing(1.8).index, 1, "positive fraction truncates");
for (let index of [-1, -Infinity, Infinity, input.length, input.length + 10])
    same(segments.containing(index), undefined, "outside string yields undefined");
throws(TypeError, () => segments.containing(1n), "ToNumber rejects BigInt");
throws(TypeError, () => segments.containing(Symbol()), "ToNumber rejects Symbol");

let first = segments[Symbol.iterator](), second = segments[Symbol.iterator]();
same(first[Symbol.iterator](), first, "iterator inherits identity method");
same(first.next().value.index, 0, "first cursor starts at zero");
same(first.next().value.index, 1, "first cursor advances independently");
same(second.next().value.index, 0, "second cursor has independent ICU state");
same(segments.containing(5).index, 5, "containing can reposition Segments cursor");
same(first.next().value.index, 3, "containing does not move iterator cursor");
let reentrant = {
    valueOf() {
        same(second.next().value.index, 1, "index coercion can reenter another iterator");
        same(segments.containing(0).index, 0, "index coercion can reenter containing");
        return 4;
    }
};
same(segments.containing(reentrant).index, 3, "outer containing uses its own index");
let rest = [...first];
same(rest.map(value => value.index).join(","), "5,6,7", "remaining cursor state");
same(first.next().done, true, "exhausted iterator returns done");
same(first.next().value, undefined, "exhausted iterator remains done");
same(Object.prototype.toString.call(first), "[object Segmenter String Iterator]",
     "iterator tag");
let empty = segmenter.segment("");
let called = false;
same(empty.containing({ valueOf() { called = true; return 0; } }), undefined,
     "empty string containing");
same(called, true, "empty string still coerces index");
same(empty[Symbol.iterator]().next().done, true, "empty iterator is done");

let words = [...new Intl.Segmenter("en", { granularity: "word" }).segment("hi, 42!")];
same(words.map(value => value.segment).join("|"), "hi|,| |42|!", "word boundaries");
same(words.map(value => value.isWordLike).join(","), "true,false,false,true,false",
     "ICU word rule status determines word likeness");
same(Object.keys(words[0]).join(","), "segment,index,input,isWordLike", "word data order");
let sentences = [...new Intl.Segmenter("en", { granularity: "sentence" })
                   .segment("One. Two!")];
same(sentences.map(value => value.segment).join("|"), "One. |Two!", "sentence boundaries");
check(sentences.every(value => !Object.hasOwn(value, "isWordLike")),
      "sentences omit word flag");
let thai = [...new Intl.Segmenter("th", { granularity: "word" }).segment("ภาษาไทย")];
check(thai.length > 1 && thai.every(value => value.isWordLike),
      "Thai dictionary tailoring is used");

let segment = Intl.Segmenter.prototype.segment;
let containing = Object.getPrototypeOf(segments).containing;
let next = Object.getPrototypeOf(first).next;
for (let receiver of [Intl.Segmenter.prototype, {}, new Proxy(segmenter, {})])
    throws(TypeError, () => segment.call(receiver, "x"), "segmenter brand");
for (let receiver of [{}, segmenter, new Proxy(segments, {})])
    throws(TypeError, () => containing.call(receiver, 0), "segments brand");
for (let receiver of [{}, segments, new Proxy(first, {})])
    throws(TypeError, () => next.call(receiver), "iterator brand");
throws(TypeError, () => Intl.Segmenter("en"), "Segmenter requires new");
throws(TypeError, () => new Intl.Segmenter("en", 3), "primitive options rejected");
throws(RangeError, () => new Intl.Segmenter("en", { granularity: "line" }),
       "invalid granularity rejected");
let order = [];
new Intl.Segmenter({
    get length() { order.push("length"); return 1; },
    get 0() { order.push("locale"); return "en"; }
}, new Proxy({}, { get(target, key) { order.push(key); } }));
same(order.join(","), "length,locale,localeMatcher,granularity", "option getter order");
same(Object.keys(segmenter.resolvedOptions()).join(","), "locale,granularity",
     "resolved options table order");
if (typeof std === "object" && typeof std.gc === "function") {
    segmenter = null;
    segments = null;
    std.gc();
    same(second.next().value.index, 3, "iterator owns string and segmenter through GC");
}
