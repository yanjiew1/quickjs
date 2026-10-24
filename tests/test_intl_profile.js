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
