"use strict";
/* Native locale semantics. Execution deferred to the root's approved gate. */
function same(actual, expected) {
    if (!Object.is(actual, expected)) throw Error(`expected ${expected}, got ${actual}`);
}
function equalArray(actual, expected) {
    same(actual.length, expected.length);
    actual.forEach((value, index) => same(value, expected[index]));
}
function throws(kind, body) {
    try { body(); } catch (error) { if (error instanceof kind) return; throw error; }
    throw Error(`expected ${kind.name}`);
}

equalArray(Intl.getCanonicalLocales("en"), ["en"]);
equalArray(Intl.getCanonicalLocales("en-u-foo"), ["en-u-foo"]);
equalArray(Intl.getCanonicalLocales([]), []);

equalArray(Intl.getCanonicalLocales(["IW", "he", "en-us"]), ["he", "en-US"]);
same(Intl.getCanonicalLocales("und-Armn-SU")[0], "und-Armn-AM");
same(Intl.getCanonicalLocales("ja-Latn-hepburn-heploc")[0], "ja-Latn-alalc97");
same(Intl.getCanonicalLocales("en-u-foo-foo-kn-yes-ca-islamicc-ca-gregory")[0],
     "en-u-foo-ca-islamic-civil-kn");
same(Intl.getCanonicalLocales("und-t-iw-m0-names")[0], "und-t-he-m0-prprname");
same(Intl.getCanonicalLocales("en-u-rg-fi01")[0], "en-u-rg-axzzzz");
for (const tag of ["root", "x-private", "en_US", "en-GB-oed", "en-u-12",
                   "en-1994-1994", "en-t-en-1994-1994", "en-a-aa-a-bb",
                   "en-t-m0", "en\0-US", "en-\ud800"]) {
    throws(RangeError, () => Intl.getCanonicalLocales(tag));
}
const access = [];
const locales = new Proxy({ length: 3, 0: "en", 2: "fr" }, {
    get(target, key) { access.push(`get:${key}`); return Reflect.get(target, key); },
    has(target, key) { access.push(`has:${key}`); return Reflect.has(target, key); }
});
equalArray(Intl.getCanonicalLocales(locales), ["en", "fr"]);
equalArray(access, ["get:length", "has:0", "get:0", "has:1", "has:2", "get:2"]);
throws(TypeError, () => Intl.getCanonicalLocales([1]));
throws(TypeError, () => Intl.getCanonicalLocales(null));

const loc = new Intl.Locale("und-Armn-SU", { language: "ru" });
same(loc.toString(), "ru-Armn-AM");
same(loc.language, "ru"); same(loc.script, "Armn"); same(loc.region, "AM");
same(new Intl.Locale("en-US", { variants: "ROZAJ-BISKE-1994" }).variants,
     "1994-biske-rozaj");
same(new Intl.Locale("en", { firstDayOfWeek: 7 }).firstDayOfWeek, "sun");
same(new Intl.Locale("en", { numeric: true }).toString(), "en-u-kn");
same(new Intl.Locale("en", { numeric: false }).numeric, false);
const canonicalLocale = new Intl.Locale("fr");
canonicalLocale.toString = () => { throw Error("Locale internal slot was ignored"); };
equalArray(Intl.getCanonicalLocales(canonicalLocale), ["fr"]);
same(new Intl.Locale(canonicalLocale).toString(), "fr");

const order = [];
new Intl.Locale("en", new Proxy({}, {
    get(target, key) { order.push(key); return undefined; }
}));
equalArray(order, ["language", "script", "region", "variants", "calendar", "collation",
                   "firstDayOfWeek", "hourCycle", "caseFirst", "numeric", "numberingSystem"]);
const sentinel = {};
throws(RangeError, () => new Intl.Locale("bad_locale", { get language() { throw sentinel; } }));
throws(TypeError, () => Intl.Locale("en"));
class Derived extends Intl.Locale {}
same(Object.getPrototypeOf(new Derived("en")), Derived.prototype);
for (const method of ["toString", "maximize", "minimize", "getCalendars", "getCollations",
                      "getHourCycles", "getNumberingSystems", "getTimeZones", "getTextInfo", "getWeekInfo"]) {
    throws(TypeError, () => Intl.Locale.prototype[method].call({}));
    throws(TypeError, () => Intl.Locale.prototype[method].call(new Proxy(loc, {})));
}
same(new Intl.Locale("zh-TW-u-ca-chinese").maximize().toString(), "zh-Hant-TW-u-ca-chinese");
same(new Intl.Locale("zh-Hant-TW-u-ca-chinese").minimize().toString(), "zh-TW-u-ca-chinese");
equalArray(new Intl.Locale("en-u-ca-foobar").getCalendars(), ["foobar"]);
equalArray(new Intl.Locale("en-u-co-foobar").getCollations(), ["foobar"]);
equalArray(new Intl.Locale("en-u-hc-foobar").getHourCycles(), ["foobar"]);
equalArray(new Intl.Locale("en-u-nu-foobar").getNumberingSystems(), ["foobar"]);
same(new Intl.Locale("ar").getTextInfo().direction, "rtl");
same(new Intl.Locale("en").getTextInfo().direction, "ltr");
same(new Intl.Locale("en").getTimeZones(), undefined);
same(new Intl.Locale("en-US").getWeekInfo().firstDay, 7);
same(new Intl.Locale("en-US-u-rg-zzzzzz").getWeekInfo().firstDay, 7);
same(new Intl.Locale("en-US-u-rg-gbzzzz").getWeekInfo().firstDay, 1);
same(new Intl.Locale("en-GB-u-rg-uszzzz").getWeekInfo().firstDay, 7);
same(new Intl.Locale("en-ZZ-u-rg-zzzzzz").getWeekInfo().firstDay, 1);
same(new Intl.Locale("en-US-u-fw-mon-rg-zzzzzz").getWeekInfo().firstDay, 1);
const week = new Intl.Locale("en-GB-u-fw-sun").getWeekInfo();
same(week.firstDay, 7); equalArray(week.weekend, [6, 7]);
equalArray(Object.keys(week), ["firstDay", "weekend"]);
const weekAgain = new Intl.Locale("en-GB-u-fw-sun").getWeekInfo();
same(week !== weekAgain, true); same(week.weekend !== weekAgain.weekend, true);

for (const key of ["calendar", "collation", "currency", "numberingSystem", "timeZone", "unit"]) {
    const values = Intl.supportedValuesOf(key);
    equalArray(values, [...new Set(values)].sort());
    same(values !== Intl.supportedValuesOf(key), true);
}
const calendars = Intl.supportedValuesOf("calendar");
same(calendars.includes("islamic-civil"), true);
same(calendars.includes("islamicc"), false);
same(new Intl.Locale("en-u-ca-islamicc").calendar, "islamic-civil");
throws(RangeError, () => Intl.supportedValuesOf("timeZone\0"));
throws(TypeError, () => Intl.supportedValuesOf(Symbol()));
const zones = Intl.supportedValuesOf("timeZone");
same(zones.includes("UTC"), true);
same(zones.includes("Asia/Kolkata"), true);
same(zones.includes("Asia/Calcutta"), false);
same(zones.includes("ACT"), false);
same(zones.includes("Etc/Unknown"), false);
for (const key of ["calendar", "collation", "currency", "numberingSystem", "timeZone", "unit"]) {
    const values = Intl.supportedValuesOf(key); values.length = 0;
    same(Intl.supportedValuesOf(key).length > 0, true);
}

/* Script metadata can specify neither left-to-right nor right-to-left. */
for (const script of ["Zyyy", "Zinh", "Zzzz", "Brai", "Zxxx"]) {
    for (const language of ["und", "en", "ar", "tlh", "qfz"]) {
        const info = new Intl.Locale(language + "-" + script).getTextInfo();
        same(info.direction, undefined);
        same(Object.getPrototypeOf(info), Object.prototype);
        equalArray(Object.keys(info), ["direction"]);
        const descriptor = Object.getOwnPropertyDescriptor(info, "direction");
        same(descriptor.writable, true);
        same(descriptor.enumerable, true);
        same(descriptor.configurable, true);
    }
}
for (const [tag, direction] of [["ar-Latn", "ltr"], ["en-Arab", "rtl"],
                                ["en-Hebr", "rtl"], ["en", "ltr"],
                                ["ar", "rtl"]])
    same(new Intl.Locale(tag).getTextInfo().direction, direction);
const nastaliqDirection = new Intl.Locale("ur-Aran").getTextInfo().direction;
same(nastaliqDirection === "rtl" || nastaliqDirection === undefined, true);
