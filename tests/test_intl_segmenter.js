/* Segmenter protocols, UTF-16 positions, realms and GC for both profiles. */
import * as std from "std";
import { assert, assertThrows } from "./assert.js";

const Segmenter = Intl.Segmenter;
const own = (object, key) => Object.prototype.hasOwnProperty.call(object, key);
const same = (actual, expected) => assert(JSON.stringify(actual), JSON.stringify(expected));
assert(Segmenter.name, "Segmenter");
assert(Segmenter.length, 0);
assert(Segmenter.supportedLocalesOf.length, 1);
assertThrows(TypeError, () => Segmenter());
assertThrows(RangeError, () => new Segmenter("bad_locale"));
for (const options of [null, true, 1, 1n, "", Symbol()])
    assertThrows(TypeError, () => new Segmenter("en", options));
for (const granularity of [null, false, "letter", "word\0"])
    assertThrows(RangeError, () => new Segmenter("en", {granularity}));
assertThrows(TypeError, () => new Segmenter("en", {granularity: Symbol()}));
assertThrows(RangeError, () => new Segmenter("en", {localeMatcher: "bad"}));
same(Segmenter.supportedLocalesOf({0: "EN", length: 1}), ["en"]);
same(Segmenter.supportedLocalesOf.call(null, ["en-US"]), ["en-US"]);
const grapheme = new Segmenter("en-u-nu-arab");
same(grapheme.resolvedOptions(), {locale: "en", granularity: "grapheme"});
for (const name of ["segment", "resolvedOptions"]) {
    const desc = Object.getOwnPropertyDescriptor(Segmenter.prototype, name);
    assert(desc.writable && desc.configurable && !desc.enumerable);
    assert(desc.value.name, name);
}
assert(Segmenter.prototype.segment.length, 1);
assert(Segmenter.prototype.resolvedOptions.length, 0);
assert(Object.prototype.toString.call(grapheme), "[object Intl.Segmenter]");
class DerivedSegmenter extends Segmenter {}
const derived = new DerivedSegmenter("en");
assert(Object.getPrototypeOf(derived) === DerivedSegmenter.prototype);
same([...derived.segment("ab")].map(x => x.segment), ["a", "b"]);

// Exercise locale argument shapes without assuming non-English availability.
const defaultLocale = new Segmenter().resolvedOptions().locale;
for (const [locales, expected] of [[undefined, defaultLocale], [[], defaultLocale],
        [["xx", "en"], "en"], [["EN", "en"], "en"],
        [{0: "EN", length: 1}, "en"], [{0: "de", length: 0}, defaultLocale]])
    assert(new Segmenter(locales).resolvedOptions().locale, expected);

// Prototype lookup and option getters retain the specified observable order.
{
    const events = [];
    const target = new Proxy(function() {}, {
        get(object, key) {
            if (key === "prototype") events.push("prototype");
            return Reflect.get(object, key);
        }
    });
    const locales = {
        get length() { events.push("length"); return 1; },
        get 0() { events.push("locale"); return "en"; }
    };
    const options = {
        get localeMatcher() { events.push("matcher"); return "lookup"; },
        get granularity() { events.push("granularity"); return "word"; }
    };
    Reflect.construct(Segmenter, [locales, options], target);
    same(events, ["prototype", "length", "locale", "matcher", "granularity"]);
}

const segments = grapheme.segment("a\u0301😀b");
const segmentsProto = Object.getPrototypeOf(segments);
const iterator = segments[Symbol.iterator]();
const iteratorProto = Object.getPrototypeOf(iterator);
assert(Object.getPrototypeOf(segmentsProto) === Object.prototype);
assert(!own(segmentsProto, "constructor") && !own(segmentsProto, Symbol.toStringTag));
assert(Object.getPrototypeOf(iteratorProto) === Iterator.prototype);
assert(!own(iteratorProto, "constructor"));
assert(iterator[Symbol.iterator]() === iterator);
assert(Object.prototype.toString.call(iterator), "[object Segmenter String Iterator]");
assert(segmentsProto.containing.length, 1);
assert(segmentsProto[Symbol.iterator].name, "[Symbol.iterator]");
assert(segmentsProto[Symbol.iterator].length, 0);
assert(iteratorProto.next.name, "next");
assert(iteratorProto.next.length, 0);
const first = iterator.next().value;
same(Object.keys(first), ["segment", "index", "input"]);
same(first, {segment: "a\u0301", index: 0, input: "a\u0301😀b"});
for (const key of Object.keys(first)) {
    const desc = Object.getOwnPropertyDescriptor(first, key);
    assert(desc.writable && desc.enumerable && desc.configurable);
}
for (const index of [0, 1, undefined, NaN, -0.5])
    same(segments.containing(index), first);
for (const index of [2, 3, "3.9"])
    same(segments.containing(index), {segment: "😀", index: 2, input: "a\u0301😀b"});
for (const index of [-1, -Infinity, Infinity, 5, 1e100])
    assert(segments.containing(index), undefined);
for (const index of [1n, Symbol()])
    assertThrows(TypeError, () => segments.containing(index));

// Brand checks precede coercion, including hidden prototypes and empty inputs.
const poison = {toString() { throw Error("unexpected coercion"); },
                valueOf() { throw Error("unexpected coercion"); }};
for (const receiver of [null, undefined, {}, Segmenter.prototype, segments, iterator]) {
    assertThrows(TypeError, () => Segmenter.prototype.segment.call(receiver, poison));
    assertThrows(TypeError, () => Segmenter.prototype.resolvedOptions.call(receiver));
}
for (const receiver of [null, undefined, {}, segmentsProto, grapheme, iterator]) {
    assertThrows(TypeError, () => segmentsProto.containing.call(receiver, poison));
    assertThrows(TypeError, () => segmentsProto[Symbol.iterator].call(receiver));
}
for (const receiver of [null, undefined, {}, iteratorProto, grapheme, segments])
    assertThrows(TypeError, () => iteratorProto.next.call(receiver));
const empty = grapheme.segment("");
let coerced = 0;
assert(empty.containing({valueOf() { coerced++; return 0; }}), undefined);
assert(coerced, 1);
assertThrows(TypeError, () => empty.containing(1n));
same([...grapheme.segment(undefined)].map(x => x.segment), [..."undefined"]);
let converted = 0;
const convertedSegments = grapheme.segment({toString() { converted++; return "xy"; }});
assert(converted, 1);
same([...convertedSegments].map(x => x.input), ["xy", "xy"]);
assert(converted, 1);

// containing() and separate/nested iterators never share a traversal cursor.
const a = segments[Symbol.iterator](), b = segments[Symbol.iterator]();
assert(a.next().value.index, 0);
assert(a.next().value.index, 2);
assert(segments.containing(4).index, 4);
assert(b.next().value.index, 0);
assert(a.next().value.index, 4);
for (let i = 0; i < 2; i++) same(a.next(), {value: undefined, done: true});
let nested = "";
for (const outer of convertedSegments)
    for (const inner of convertedSegments) nested += outer.segment + inner.segment;
assert(nested, "xxxyyxyy");

// Keep original encoding, including both halves of a pair, NUL and lone units.
const unusual = "😀x\ud800\0\udc00";
same([...grapheme.segment(unusual)].map(x => [x.segment, x.index]),
     [["😀",0],["x",2],["\ud800",3],["\0",4],["\udc00",5]]);
assert(grapheme.segment(unusual).containing(1).index, 0);
same([...grapheme.segment("é")].map(x => x.segment), ["é"]);

const words = new Segmenter("en", {granularity: "word"});
same([...words.segment("ab 12.3 _ 😀")].map(x => [x.segment, x.index, x.isWordLike]),
     [["ab",0,true],[" ",2,false],["12.3",3,true],[" ",7,false],
      ["_",8,false],[" ",9,false],["😀",10,false]]);
same(Object.keys(words.segment("a").containing(0)), ["segment","index","input","isWordLike"]);
for (const text of ["中", "あ", "ก", "7"])
    assert(words.segment(text).containing(0).isWordLike);
for (const text of ["_", "!", "\u0301", "😀"])
    assert(!words.segment(text).containing(0).isWordLike);
const sentence = new Segmenter("en", {granularity: "sentence"});
same([...sentence.segment("Hi. Bye!")].map(x => [x.segment, x.index]), [["Hi. ",0],["Bye!",4]]);
assert(!own(sentence.segment("Hi.").containing(0), "isWordLike"));

// Segment data uses own data properties, bypassing inherited setters.
Object.defineProperty(Object.prototype, "index", {configurable: true,
    set() { throw Error("unexpected inherited setter"); }});
try { assert(grapheme.segment("x").containing(0).index, 0); }
finally { delete Object.prototype.index; }

// Borrowed methods create results/prototypes in their own function realm.
const realm = std.createRealm();
const foreign = realm.global;
const foreignSegment = foreign.Intl.Segmenter.prototype.segment;
const foreignSegmentsProto = realm.evalScript("Object.getPrototypeOf(new Intl.Segmenter('en').segment('x'))");
const foreignResult = foreignSegment.call(grapheme, "x");
assert(Object.getPrototypeOf(foreignResult) === foreignSegmentsProto);
const foreignData = foreignSegmentsProto.containing.call(segments, 0);
assert(Object.getPrototypeOf(foreignData) === foreign.Object.prototype);
const foreignIterator = foreignSegmentsProto[Symbol.iterator].call(segments);
assert(Object.getPrototypeOf(foreignIterator) ===
       realm.evalScript("Object.getPrototypeOf(new Intl.Segmenter('en').segment('x')[Symbol.iterator]())"));
const localData = iteratorProto.next.call(foreignIterator).value;
assert(Object.getPrototypeOf(localData) === Object.prototype);
// A non-object prototype falls back to the new target realm's intrinsic.
const foreignTarget = realm.evalScript("(function Target() {})");
foreignTarget.prototype = null;
assert(Object.getPrototypeOf(Reflect.construct(Segmenter, ["en"], foreignTarget)) ===
       foreign.Intl.Segmenter.prototype);

// Iterators own the input and segmenter independently of the Segments wrapper.
const retained = (() => {
    const segmenter = new Segmenter("en");
    const chunks = segmenter.segment("a😀b");
    segmenter.cycle = chunks;
    return chunks[Symbol.iterator]();
})();
std.gc();
same([...retained].map(x => x.segment), ["a", "😀", "b"]);
for (let i = 0; i < 32; i++) {
    const segmenter = new Segmenter("en");
    const chunks = segmenter.segment("abc");
    segmenter.cycle = chunks;
    chunks.cycle = chunks[Symbol.iterator]();
}
std.gc();
