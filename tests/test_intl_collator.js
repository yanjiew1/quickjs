/* Native ICU Collator regressions. Run only in CONFIG_INTL builds. */
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
        if (error instanceof klass)
            return;
        throw error;
    }
    throw new Error(label);
}

let base = new Intl.Collator("en", { sensitivity: "base" });
same(base.compare("a", "A"), 0, "base ignores case");
same(base.compare("a", "\u00e1"), 0, "base ignores accents");
check(new Intl.Collator("sv", { sensitivity: "base" }).compare("a", "\u00e4") < 0,
      "Swedish tailoring treats umlaut as a different base letter");
same(new Intl.Collator("de-u-co-phonebk", { sensitivity: "base" })
     .compare("\u00e4", "ae"), 0, "German phonebook expansion");
same(new Intl.Collator("en").compare("\u00e9", "e\u0301"), 0,
     "canonical equivalence");
let punctuation = new Intl.Collator("en", { ignorePunctuation: true });
same(punctuation.compare("a-b", "ab"), 0, "punctuation ignored");
check(punctuation.compare("a$", "a") !== 0, "symbols remain significant");
same(new Intl.Collator("th").resolvedOptions().ignorePunctuation, true,
     "Thai punctuation default comes from locale data");
check(new Intl.Collator("en", { numeric: true }).compare("2", "10") < 0,
      "numeric option uses numeric collation");
same(new Intl.Collator("en-u-kn").resolvedOptions().numeric, true,
     "empty kn extension means true");
let numericOverride = new Intl.Collator("en-u-kn", { numeric: false }).resolvedOptions();
same(numericOverride.numeric, false, "option overrides numeric extension");
check(!numericOverride.locale.includes("-kn"), "overridden key removed from locale");
same(new Intl.Collator("de", { usage: "search", collation: "phonebk" })
     .resolvedOptions().collation, "default", "search uses SearchLocaleData");
same(new Intl.Collator("en").compare("a\0b", "a\0c"), -1,
     "embedded NUL does not truncate comparison");
same(base.compare("\ud800", "\ud800"), 0, "lone surrogate accepted");

let compare = base.compare;
same(compare, base.compare, "bound compare is cached");
same(compare.call(null, "a", "A"), 0, "compare retains collator");
same(compare.name, "", "bound compare name");
same(compare.length, 2, "bound compare length");
same(Object.getOwnPropertyNames(compare).join(","), "length,name",
     "bound compare property order");
check(!Object.hasOwn(compare, "prototype"), "bound compare is not a constructor");
throws(TypeError, () => new compare(), "bound compare cannot construct");
same(Object.keys(base.resolvedOptions()).join(","),
     "locale,usage,sensitivity,ignorePunctuation,collation,numeric,caseFirst",
     "resolved option table order");
let compareGetter = Object.getOwnPropertyDescriptor(Intl.Collator.prototype,
                                                   "compare").get;
for (let receiver of [Intl.Collator.prototype, {}, new Proxy(base, {})])
    throws(TypeError, () => compareGetter.call(receiver), "collator brand checked");

let order = [];
let optionNames = ["usage", "localeMatcher", "collation", "numeric", "caseFirst",
                   "sensitivity", "ignorePunctuation"];
let options = new Proxy({}, { get(target, key) { order.push(key); } });
let locales = {
    get length() { order.push("length"); return 1; },
    get 0() { order.push("locale"); return "en"; }
};
let target = new Proxy(function Target() {}, {
    get(object, key, receiver) {
        if (key === "prototype") order.push("prototype");
        return Reflect.get(object, key, receiver);
    }
});
Reflect.construct(Intl.Collator, [locales, options], target);
same(order.join(","), ["prototype", "length", "locale", ...optionNames].join(","),
     "prototype and locale coercion precede exactly one ordered option lookup");
order.length = 0;
compare({ toString() { order.push("x"); return "a"; } },
        { toString() { order.push("y"); return "A"; } });
same(order.join(","), "x,y", "compare coerces x before y");
let sentinel = {};
try {
    new Intl.Collator("en", { get numeric() { throw sentinel; } });
    throw new Error("numeric getter exception omitted");
} catch (error) { same(error, sentinel, "getter exception retained"); }
throws(RangeError, () => new Intl.Collator("en", { collation: "ab" }),
       "collation Unicode type grammar validated");
throws(RangeError, () => new Intl.Collator("en", { collation: "phonebk\0x" }),
       "collation does not truncate embedded NUL");
throws(RangeError, () => new Intl.Collator("en", { usage: "sort\0x" }),
       "enum option requires the whole string");
throws(TypeError, () => new Intl.Collator("en", null), "null options rejected");
check(Intl.Collator("en") instanceof Intl.Collator, "Collator callable without new");

order.length = 0;
let left = { toString() { order.push("left"); return "2"; } };
let right = { toString() { order.push("right"); return "10"; } };
let localeCompareOptions = { get numeric() { order.push("numeric"); return true; } };
check(String.prototype.localeCompare.call(left, right, "en", localeCompareOptions) < 0,
      "localeCompare forwards ICU options");
same(order.join(","), "left,right,numeric", "localeCompare coercion order");
throws(TypeError, () => String.prototype.localeCompare.call(null, "x"),
       "localeCompare requires coercible receiver");
let savedCollator = Intl.Collator;
try {
    Intl.Collator = () => { throw new Error("global constructor must not be called"); };
    same("\u00e9".localeCompare("e\u0301", "en"), 0, "intrinsic constructor retained");
} finally { Intl.Collator = savedCollator; }

if (typeof $262 === "object" && typeof $262.createRealm === "function") {
    let realm = $262.createRealm();
    let foreign = realm.global.Intl.Collator;
    let foreignGetter = Object.getOwnPropertyDescriptor(foreign.prototype, "compare").get;
    let local = new Intl.Collator("en");
    let foreignCompare = foreignGetter.call(local);
    check(Object.getPrototypeOf(foreignCompare) === realm.global.Function.prototype,
          "borrowed getter creates compare in its own realm");
    throws(realm.global.TypeError, () => foreignCompare(Symbol(), "x"),
           "bound compare conversion error comes from creation realm");
}
