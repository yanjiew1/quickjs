/* Text boundary regression through the existing ICU service frontends. */
import { assert, assertArrayEquals } from "./assert.js";

const samples = ["", "A\0\xffB", "\u0100\u0301", "\ud800", "\udc00",
    "A\0\ud800B\udc00\ud83d\ude00e\u0301",
    "A\0\ud800".repeat(1024) + "B\udc00\ud83d\ude00".repeat(1024)];
const list = new Intl.ListFormat("en");
const segmenter = new Intl.Segmenter("en");
for (const text of samples) {
    assert(list.format([text]), text);
    assertArrayEquals(list.formatToParts([text]).filter(p => p.type === "element")
        .map(p => p.value), [text]);
    const segments = [...segmenter.segment(text)];
    assert(segments.map(p => p.segment).join(""), text);
    let offset = 0;
    for (const part of segments) {
        assert(part.index, offset);
        assert(part.input, text);
        offset += part.segment.length;
    }
    assert(offset, text.length);
}
assert("a\0\ud800b\udc00\ud83d\ude00".toLocaleUpperCase("en"),
       "A\0\ud800B\udc00\ud83d\ude00");
assert("A\0\ud800B\udc00\ud83d\ude00".toLocaleLowerCase("en"),
       "a\0\ud800b\udc00\ud83d\ude00");
let coercions = 0;
const value = { [Symbol.toPrimitive](hint) {
    assert(hint, "string"); coercions++; return samples[5];
} };
assert([...segmenter.segment(value)].map(p => p.segment).join(""), samples[5]);
assert(coercions, 1);
const collator = new Intl.Collator("en");
assert(collator.compare(value, samples[5]), 0);
assert(coercions, 2);
const marker = {};
const throwing = { toString() { coercions++; throw marker; } };
for (const operation of [() => segmenter.segment(throwing),
                         () => collator.compare(throwing, "")]) {
    const before = coercions;
    let caught = false;
    try { operation(); } catch (error) { caught = true; assert(error === marker); }
    assert(caught);
    assert(coercions, before + 1);
}
