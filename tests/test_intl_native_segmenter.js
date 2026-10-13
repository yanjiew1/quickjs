/* Source-only fixture. Indices and containing positions are UTF16 units. */
import { assert, assertThrows, assertArrayEquals } from "./assert.js";
const gets = [];
new Intl.Segmenter("en", new Proxy({}, {get(_, name) {gets.push(name);}}));
assertArrayEquals(gets, ["localeMatcher", "granularity"]);
assertThrows(TypeError, () => Intl.Segmenter("en"));
assertThrows(TypeError, () => new Intl.Segmenter("en", null));
assertThrows(RangeError, () => new Intl.Segmenter("en", {granularity: "line"}));
for (const granularity of ["grapheme", "word", "sentence"]) {
    for (const [tag, resolved] of [["en", "en"], ["en-US", "en-US"],
                                  ["en-GB", "en"], ["zz", "en-US"]]) {
        const s = new Intl.Segmenter(tag, {granularity});
        assert(s.resolvedOptions().locale, resolved);
        assert(s.resolvedOptions().granularity, granularity);
        assertArrayEquals(Object.keys(s.resolvedOptions()), ["locale", "granularity"]);
        for (const input of ["", "a\0b", "a\ud800b\udc00", "\ud83d\ude00",
            "e\u0301", "\r\n", "hello world.", "\ud83d\udc69\u200d\ud83d\udcbb"]) {
            const parts = [...s.segment(input)];
            assert(parts.map(p => p.segment).join(""), input);
            let offset = 0;
            for (const p of parts) {
                assert(p.index, offset); assert(p.input, input);
                assertArrayEquals(Object.keys(p), granularity === "word" ?
                    ["segment", "index", "input", "isWordLike"] : ["segment", "index", "input"]);
                for (let i = offset; i < offset + p.segment.length; i++)
                    assert(s.segment(input).containing(i).segment, p.segment);
                offset += p.segment.length;
            }
        }
    }
}
assertArrayEquals(Intl.Segmenter.supportedLocalesOf(["en-US", "en-GB", "fr", "zz"]),
                  ["en-US", "en-GB"]);
const g = new Intl.Segmenter("en");
const input = "\ud83d\ude00e\u0301\ud800\0";
const segments = g.segment(input);
assertArrayEquals([...segments].map(p => p.segment), ["\ud83d\ude00", "e\u0301", "\ud800", "\0"]);
assert(segments.containing(1).index, 0); assert(segments.containing(3).index, 2);
for (const index of [undefined, NaN, -0, -0.5, 0.9]) assert(segments.containing(index).index, 0);
for (const index of [-1, Infinity, -Infinity, input.length, 1e100]) assert(segments.containing(index), undefined);
assert(g.segment("").containing(0), undefined);
const a = segments[Symbol.iterator](), b = segments[Symbol.iterator]();
assert(a.next().value.index, 0); assert(a.next().value.index, 2);
assert(b.next().value.index, 0);
assert(segments.containing({valueOf() { assert(b.next().value.index, 2); return 1; }}).index, 0);
assert(a.next().value.index, 4); assert(b.next().value.index, 4);
assert(a.next().value.index, 5); assert(a.next().done, true); assert(a.next().done, true);
let calls = 0;
const outer = g.segment({toString() {calls++; assert([...g.segment("x")][0].segment, "x"); return input;}});
assert(calls, 1); assert([...outer].map(p => p.segment).join(""), input);
const poison = {toString() {throw Error("must not coerce wrong receiver");}};
assertThrows(TypeError, () => Intl.Segmenter.prototype.segment.call({}, poison));
assertThrows(TypeError, () => Intl.Segmenter.prototype.segment.call(new Proxy(g, {}), poison));
assertThrows(TypeError, () => Object.getPrototypeOf(segments).containing.call({}, {valueOf() {throw Error();}}));
assertThrows(TypeError, () => Object.getPrototypeOf(a).next.call({}));
assertThrows(TypeError, () => segments.containing(1n));
assertThrows(TypeError, () => g.segment(Symbol()));
class Sub extends Intl.Segmenter {}
assert(new Sub("en") instanceof Sub);
assertArrayEquals([...new Intl.Segmenter("en", {granularity: "word"}).segment("Hi 12!")]
    .map(p => [p.segment, p.isWordLike].join(":")), ["Hi:true", " :false", "12:true", "!:false"]);
assertArrayEquals([...new Intl.Segmenter("en", {granularity: "sentence"}).segment("Hi. Bye!")]
    .map(p => p.segment), ["Hi. ", "Bye!"]);
/* UAX29 revision49 GB9c starts at a Linker, without prior Consonant. */
assertArrayEquals([...g.segment("\u094d\u0301\u0915")].map(p => p.segment), ["\u094d\u0301\u0915"]);
