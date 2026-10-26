/*
 * Copyright (c) 2026 Yan-Jie Wang
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

import { assert, assertThrows, assertArrayEquals } from "./assert.js";

// ECMA accepts these unknown five-to-eight-letter languages even though the
// selected library's compact Language type cannot store them directly.
for (const tag of ["posix", "abcde-Latn-US", "abcdefgh-123", "en-t-abcdefgh-latn-us"]) {
    assert(Intl.getCanonicalLocales(tag)[0], tag);
    assert(new Intl.Locale(tag).toString(), tag);
}
assert(new Intl.Locale("abcdefgh-Latn-US").maximize().toString(), "abcdefgh-Latn-US");
assert(new Intl.Locale("und-150").maximize().toString(), "en-Latn-150");
assert(new Intl.Locale("und-150").minimize().toString(), "en-150");
assert(new Intl.Locale("und-Qaaa").maximize().toString(), "en-Qaaa-US");
assertThrows(RangeError, () => Intl.getCanonicalLocales("abcdefgh-u"));
const names = new Intl.DisplayNames("en-US", {type:"language"});
assert(names.resolvedOptions().locale, "en-US");
assert(names.of("abcdefgh-Latn-US"), "abcdefgh-Latn-US");
assert(new Intl.DisplayNames("en-US", {type:"language", fallback:"none"})
    .of("abcdefgh"), undefined);
assertThrows(RangeError, () => names.of("abcdefgh-u"));

// The stock library loses these families through root fallback. Preserve
// selected CLDR cardinal/ordinal inheritance without reducing exact operands.
const manx = new Intl.PluralRules("gv");
assertArrayEquals(manx.resolvedOptions().pluralCategories, ["one", "two", "few", "many", "other"]);
for (const [value, category] of [[1,"one"], [2,"two"], [20,"few"], [0.1,"many"], [3,"other"]])
    assert(manx.select(value), category);
assert(manx.selectRange(1, 11), "one"); // Undeclared range pairs retain end-default.
assert(new Intl.PluralRules("en-Dsrt").select(1), "one");
assert(new Intl.PluralRules("en-Dsrt", {type:"ordinal"}).select(2), "two");
assert(new Intl.PluralRules("io").selectRange(2, 1), "other"); // Explicit range rule.
const cornish = new Intl.PluralRules("kw", {type:"ordinal"});
assertArrayEquals(cornish.resolvedOptions().pluralCategories, ["one", "many", "other"]);
assert(cornish.select(5), "many");
