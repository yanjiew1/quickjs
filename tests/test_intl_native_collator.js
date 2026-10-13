/* Source-only fixture. Root executes the actual native en/en-US provider. */
import { assert, assertThrows, assertArrayEquals } from "./assert.js";
for (const usage of ["sort", "search"]) {
    for (const tag of ["en", "en-US", "en-GB", "zz"]) {
        const c = new Intl.Collator(tag, {usage});
        const r = c.resolvedOptions();
        assert(r.locale, tag === "en-GB" ? "en" : tag === "zz" ? "en-US" : tag);
        assert(r.usage, usage); assert(r.sensitivity, "variant");
        assert(r.ignorePunctuation, false); assert(r.collation, "default");
        assert(r.numeric, false); assert(r.caseFirst, "false");
        assert(c.compare("a", "b"), -1);
        for (const sensitivity of ["base", "accent", "case", "variant"]) {
            const n = new Intl.Collator(tag, {usage, sensitivity});
            assert(n.compare("\u00e9", "e\u0301"), 0);
            assert(n.compare("a\0b", "a\0c"), -1);
            assert(n.compare("\ud800", "\ud800"), 0);
        }
        assertArrayEquals(Object.keys(r), ["locale", "usage", "sensitivity",
            "ignorePunctuation", "collation", "numeric", "caseFirst"]);
    }
}
const reads = [];
Intl.Collator("en", new Proxy({}, {get(_, name) { reads.push(name); }}));
assertArrayEquals(reads, ["usage", "localeMatcher", "collation", "numeric",
                         "caseFirst", "sensitivity", "ignorePunctuation"]);
assertArrayEquals(Intl.Collator.supportedLocalesOf(["en-US", "en-GB", "fr", "zz"]),
                  ["en-US", "en-GB"]);
let c = Intl.Collator("en-u-co-search-kf-upper-kn", {numeric: true});
assert(c.resolvedOptions().locale, "en-u-kf-upper-kn");
assert(c.resolvedOptions().caseFirst, "upper"); assert(c.resolvedOptions().numeric, true);
assert(c.resolvedOptions().collation, "default");
assert(c.compare("2", "10"), -1);
assert(new Intl.Collator("en-u-kn", {numeric: false}).resolvedOptions().locale, "en");
assert(new Intl.Collator("en-u-kf-upper", {caseFirst: "lower"}).resolvedOptions().locale, "en");
assert(new Intl.Collator("en-u-kn-false").resolvedOptions().locale, "en-u-kn-false");
assert(new Intl.Collator("en-u-kf-unknown-kn-unknown").resolvedOptions().locale, "en");
assert(new Intl.Collator("en", {caseFirst: "upper"}).compare("A", "a"), -1);
assert(new Intl.Collator("en", {caseFirst: "lower"}).compare("A", "a"), 1);
assert(new Intl.Collator("en", {sensitivity: "base"}).compare("a", "A"), 0);
assert(new Intl.Collator("en", {ignorePunctuation: true}).compare("a-b", "ab"), 0);
assert(new Intl.Collator("en", {ignorePunctuation: true}).compare("a+b", "ab") !== 0);
const bound = c.compare;
assert(bound, c.compare); assert(bound.length, 2);
assert(bound.call({}, "2", "10"), -1);
c = null;
assert(bound("\u00e9", "e\u0301"), 0);
const getter = Object.getOwnPropertyDescriptor(Intl.Collator.prototype, "compare").get;
assertThrows(TypeError, () => getter.call({}));
assertThrows(TypeError, () => getter.call(new Proxy(Intl.Collator(), {})));
assertThrows(TypeError, () => Intl.Collator.prototype.resolvedOptions.call({}));
class Sub extends Intl.Collator {}
assert(new Sub("en") instanceof Sub);
const order = [];
const left = {toString() { order.push("left"); return "a"; }};
const right = {toString() { order.push("right"); return "b"; }};
const options = new Proxy({}, {get(_, name) { order.push(name); }});
assert(String.prototype.localeCompare.call(left, right, "en", options), -1);
assertArrayEquals(order, ["left", "right", "usage", "localeMatcher", "collation",
    "numeric", "caseFirst", "sensitivity", "ignorePunctuation"]);
for (const invalid of ["!", "ab", "foo\0bar", "foo--bar"])
    assertThrows(RangeError, () => Intl.Collator("en", {collation: invalid}));
assertThrows(TypeError, () => bound(Symbol(), "a"));
let secondRead = false;
const firstError = Error("first operand");
try { bound({toString() {throw firstError;}}, {toString() {secondRead = true;}}); }
catch (e) {assert(e, firstError);}
assert(secondRead, false);
