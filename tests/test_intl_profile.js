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

/* Profile availability changes matching, not valid language-tag identities. */
import { assert } from "./assert.js";

const minimal = scriptArgs[1] === "minimal";
assert(minimal || scriptArgs[1] === "native");
for (const name of ["Collator", "ListFormat", "PluralRules", "NumberFormat",
                    "RelativeTimeFormat", "DurationFormat", "DisplayNames",
                    "DateTimeFormat", "Segmenter"]) {
    const Constructor = Intl[name];
    const options = name === "DisplayNames" ? {type: "language"} : undefined;
    const formatter = new Constructor("en-US", options);
    const resolved = formatter.resolvedOptions();
    assert(resolved.locale, "en-US");
    assert(Object.getPrototypeOf(resolved) === Object.prototype);
    assert(resolved !== formatter.resolvedOptions());
    const descriptor = Object.getOwnPropertyDescriptor(resolved, "locale");
    assert(descriptor.value, resolved.locale);
    assert(descriptor.writable);
    assert(descriptor.enumerable);
    assert(descriptor.configurable);
    assert(new Constructor("en", options).resolvedOptions().locale, "en");
    // SupportedLocales retains the requested tag even when prefix matching
    // selects a shorter locale for the actual formatter.
    assert(Constructor.supportedLocalesOf(["en-US", "en-GB"]).join(","), "en-US,en-GB");
    if (minimal) {
        assert(new Constructor("en-GB", options).resolvedOptions().locale, "en");
        assert(Constructor.supportedLocalesOf(["fr", "ja"]).length, 0);
    }
}
assert(new Intl.Locale("en-US").toString(), "en-US");
assert(new Intl.Locale("en-GB").region, "GB");
assert(Intl.getCanonicalLocales("EN-us").join(","), "en-US");
assert(new Intl.NumberFormat("en-US-u-nu-arab").resolvedOptions().locale,
       "en-US-u-nu-arab");
