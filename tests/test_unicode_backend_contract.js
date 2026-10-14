/* Shared normative fixtures; no Unicode17/18 character coverage assumption. */
"use strict";
function check(value, message) {
    if (!value) throw new Error(message);
}
function same(actual, expected, message) {
    check(Object.is(actual, expected), message);
}
function syntax(pattern, flags) {
    try { new RegExp(pattern, flags); }
    catch (error) { if (error instanceof SyntaxError) return; throw error; }
    throw new Error("Accepted invalid Unicode property: " + pattern);
}
same("\u00df".toUpperCase(), "SS", "Full uppercase expansion");
same("\u0130".toLowerCase(), "i\u0307", "Root lowercase expansion");
same("A\u0301\u03a3".toLowerCase(), "a\u0301\u03c2", "Final sigma context");
same("A\u03a3B".toLowerCase(), "a\u03c3b", "Medial sigma context");
same("\ud800A\udc00".toLowerCase(), "\ud800a\udc00", "Lone surrogates preserved");
check(/\u1fd3/iu.test("\u0390"), "Distinct simple and full folds");
check(/\ufb05/iu.test("\ufb06"), "Ligature simple fold");
check(!/\u00df/iu.test("ss"), "RegExp never full string folding");
check(!/k/i.test("\u212a") && /k/iu.test("\u212a"), "Legacy ASCII restriction");
check(!/s/i.test("\u017f") && /s/iu.test("\u017f"), "Legacy long s restriction");
check(/[\u{10400}]/iu.test("\u{10428}"), "Astral range canonicalization");
check(/\P{Lowercase_Letter}/iu.test("a"), "u complements before folding");
check(!/\P{Lowercase_Letter}/iv.test("a"), "v folds before complement");
same("\u212b".normalize("NFC"), "\u00c5", "Canonical composition");
same("\uac00".normalize("NFD"), "\u1100\u1161", "Hangul decomposition");
same("\ufb03".normalize("NFKC"), "ffi", "Compatibility decomposition");
same("A\u0315\u0300".normalize("NFD"), "A\u0300\u0315", "Combining order");
same("\ud800e\u0301\udc00".normalize(), "\ud800\u00e9\udc00", "Normalize surrogates");
same("a\0e\u0301".normalize(), "a\0\u00e9", "Normalize embedded NUL");
check(/\p{General_Category=Lu}/u.test("A"), "Exact GC alias");
check(/\p{Script=Latin}/u.test("A"), "Exact Script long name");
check(!/\p{Script=Hiragana}/u.test("\u30fc") &&
      /\p{Script_Extensions=Hiragana}/u.test("\u30fc"), "sc and scx differ");
check(/\p{White_Space}/u.test("\u0085") && !/\s/u.test("\u0085"), "ES whitespace differs");
check(!/\p{White_Space}/u.test("\ufeff") && /\s/u.test("\ufeff"), "ES BOM whitespace");
same("\ufeff x \ufeff".trim(), "x", "Trim ECMAScript whitespace");
same("\u0085x\u0085".trim(), "\u0085x\u0085", "Trim excludes NEL");
check(/\p{Cs}/u.test("\ud800"), "Surrogate category");
for (const pattern of ["\\p{script=Latin}", "\\p{Script=latin}",
                       "\\p{gc=uppercase_letter}", "\\p{GeneralCategory=Lu}",
                       "\\p{IsGreek}", "\\p{Block=Basic_Latin}", "\\p{Age=1.1}",
                       "\\p{IDS_Unary_Operator}", "\\p{IDSU}",
                       "\\p{Modifier_Combining_Mark}", "\\p{MCM}",
                       "\\p{ID_Compat_Math_Start}", "\\p{InCB}"])
    syntax(pattern, "u");
check(/\p{RGI_Emoji}/v.test("\ud83c\uddfa\ud83c\uddf8"), "v string property");
syntax("\\p{RGI_Emoji}", "u");
syntax("\\P{RGI_Emoji}", "v");
same(Function("let \u{10400}=7; return \u{10400}")(), 7, "Astral identifier");
same(Function("let a\u200c=9; return a\u200c")(), 9, "ZWNJ identifier continuation");
