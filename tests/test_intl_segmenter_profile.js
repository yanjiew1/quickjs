/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Keep explicit native CLDR and Unicode-default profile policies separate
   from the shared API/realm/ownership tests. */
import { assert } from "./assert.js";

const native = scriptArgs[1] === "native";
assert(native || scriptArgs[1] === "minimal");
const same = (actual, expected) => assert(JSON.stringify(actual), JSON.stringify(expected));
const requested = ["en", "en-US", "fr", "ja", "th", "zh"];
same(Intl.Segmenter.supportedLocalesOf(requested), native ? requested : ["en", "en-US"]);

// Unicode18 default GB9c joins a leading linker. The pinned CLDR root rule
// requires a preceding consonant; this test records the selected profile.
const grapheme = new Intl.Segmenter("en");
same([...grapheme.segment("\u094d\u0915")].map(value => value.segment),
     native ? ["\u094d", "\u0915"] : ["\u094d\u0915"]);

if (native) {
    // Dictionary choice follows input script, even with an English locale.
    // The pinned CJK dictionary gives 中国 cost 51, below two unknown edges.
    const words = new Intl.Segmenter("en", {granularity: "word"});
    same([...words.segment("中国")].map(value => [value.segment, value.index, value.isWordLike]),
         [["中国", 0, true]]);

    // Execute complete locale-selected suppression lists, including a multi-
    // part abbreviation. These expectations follow the pinned CLDR lists.
    const english = new Intl.Segmenter("en", {granularity: "sentence"});
    same([...english.segment("L.P. Brown. Next.")].map(value => [value.segment, value.index]),
         [["L.P. Brown. ", 0], ["Next.", 12]]);
    const german = new Intl.Segmenter("de", {granularity: "sentence"});
    assert(german.resolvedOptions().locale, "de");
    same([...german.segment("n. Chr. X")].map(value => [value.segment, value.index]),
         [["n. Chr. X", 0]]);
    const french = new Intl.Segmenter("fr", {granularity: "sentence"});
    assert(french.resolvedOptions().locale, "fr");
    same([...french.segment("M. Dupont. Suite.")].map(value => [value.segment, value.index]),
         [["M. Dupont. ", 0], ["Suite.", 11]]);
}
