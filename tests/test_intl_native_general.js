/* Native general operations, independent observable assertions. */
function assert(value, message) { if (!value) throw Error(message || "assertion failed"); }
function same(a, b) { assert(Object.is(a, b), JSON.stringify(a) + " != " + JSON.stringify(b)); }
function array(a, b) { same(JSON.stringify(a), JSON.stringify(b)); }
function throws(type, f) { let caught = false; try { f(); } catch (e) { assert(e instanceof type); caught = true; } assert(caught); }
const keys = ["calendar", "collation", "currency", "numberingSystem", "timeZone", "unit"];
for (const key of keys) {
    const a = Intl.supportedValuesOf(key), b = Intl.supportedValuesOf(key);
    assert(Array.isArray(a)); same(Object.getPrototypeOf(a), Array.prototype);
    assert(a !== b); array(a, b); array(a, [...a].sort()); same(new Set(a).size, a.length);
    a.push("mutation"); assert(!b.includes("mutation"));
}
array(Intl.supportedValuesOf("calendar"), ["buddhist", "chinese", "coptic", "dangi", "ethioaa", "ethiopic", "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla", "islamic-umalqura", "iso8601", "japanese", "persian", "roc"]);
assert(Intl.supportedValuesOf("timeZone").includes("UTC"));
assert(!Intl.supportedValuesOf("timeZone").includes("Etc/UTC"));
assert(!Intl.supportedValuesOf("collation").includes("standard"));
assert(!Intl.supportedValuesOf("collation").includes("search"));
for (const currency of Intl.supportedValuesOf("currency")) {
    assert(/^[A-Z]{3}$/.test(currency));
    same(new Intl.NumberFormat("en", {style:"currency", currency}).resolvedOptions().currency, currency);
}
for (const unit of Intl.supportedValuesOf("unit")) {
    assert(!unit.includes("-per-"));
    const nf = new Intl.NumberFormat("en", {style:"unit", unit});
    same(nf.resolvedOptions().unit, unit); assert(nf.format(3).length);
}
for (const numberingSystem of Intl.supportedValuesOf("numberingSystem")) {
    same(new Intl.NumberFormat("en", {numberingSystem}).resolvedOptions().numberingSystem, numberingSystem);
}
for (const key of [undefined, null, "", "Calendar", "calendar\0", "timeZone\0suffix", "invalid"])
    throws(RangeError, () => Intl.supportedValuesOf(key));
throws(TypeError, () => Intl.supportedValuesOf(Symbol()));
let conversions = 0;
array(Intl.supportedValuesOf({toString(){ conversions++; return "unit"; }}), Intl.supportedValuesOf("unit"));
same(conversions, 1);
const sentinel = {};
try { Intl.supportedValuesOf({toString(){throw sentinel;}}); assert(false); } catch (e) { same(e, sentinel); }

same("Straßeﬃ\ud801\udc28".toLocaleUpperCase("und"), "STRASSEFFI\ud801\udc00");
same("ΟΣ".toLocaleLowerCase("und"), "ος");
same("ΟΣ'".toLocaleLowerCase("und"), "ος'");
same("ΟΣ'Α".toLocaleLowerCase("und"), "οσ'α");
same("Σ".toLocaleLowerCase("und"), "σ");
same("\u0345Σ".toLocaleLowerCase("und"), "\u0345σ");
same("İ".toLocaleLowerCase("und"), "i\u0307");
for (const locale of ["tr", "az", "tr-Latn-TR-u-co-search", "az-Cyrl-AZ"]) {
    same("Iİiı".toLocaleLowerCase(locale), "ıiiı");
    same("Iİiı".toLocaleUpperCase(locale), "IİİI");
    same("I\u0323\u0307".toLocaleLowerCase(locale), "i\u0323");
    same("I\u0301\u0307".toLocaleLowerCase(locale), "ı\u0301\u0307");
    same("i\u0307".toLocaleLowerCase(locale), "i\u0307");
}
same("I\u0323\u0301J\u0301Į\u0301".toLocaleLowerCase("lt-LT"), "i\u0307\u0323\u0301j\u0307\u0301į\u0307\u0301");
same("ÌÍĨ".toLocaleLowerCase("lt"), "i\u0307\u0300i\u0307\u0301i\u0307\u0303");
same("i\u0323\u0307".toLocaleUpperCase("lt"), "I\u0323");
same("i\u0301\u0307".toLocaleUpperCase("lt"), "I\u0301\u0307");
same("I\u0307".toLocaleUpperCase("lt"), "I\u0307");
same("\ud835\udc22\ud800\uddfd\u0307".toLocaleUpperCase("lt"), "\ud835\udc22\ud800\uddfd");
same("a\0\ud800I\udc00".toLocaleLowerCase("tr"), "a\0\ud800ı\udc00");
same("a\0\ud800i\udc00".toLocaleUpperCase("tr"), "A\0\ud800İ\udc00");
same("I".toLocaleLowerCase(["und", "tr"]), "i");
same("I".toLocaleLowerCase(["tr", "und"]), "ı");
same("I".toLocaleLowerCase("zz-TR"), "i");
same("I".toLocaleLowerCase(new Intl.Locale("tr")), "ı");
for (const method of ["toLocaleUpperCase", "toLocaleLowerCase"]) {
    throws(RangeError, () => ""[method](["tr", "invalid_tag"]));
    throws(TypeError, () => String.prototype[method].call(null, {get length(){throw sentinel;}}));
    const order = [];
    const receiver = {toString(){order.push("string"); return "i";}};
    const locales = {get length(){order.push("length"); return 2;},
        get 0(){order.push("0"); "I".toLocaleLowerCase("tr"); return "lt";},
        get 1(){order.push("1"); return "und";}};
    String.prototype[method].call(receiver, locales);
    array(order, ["string", "length", "0", "1"]);
    let touched = false;
    String.prototype[method].call("i", "tr", {get localeMatcher(){touched=true; throw sentinel;}});
    same(touched, false);
}
assert(new Intl.Locale("en").getCollations().every(x => Intl.supportedValuesOf("collation").includes(x)));
array(new Intl.Locale("zz").getCollations(), ["emoji", "eor"]);
array(new Intl.Locale("zz").getNumberingSystems(), ["latn"]);
array(new Intl.Locale("en-u-nu-arab").getNumberingSystems(), ["arab"]);
array(new Intl.Locale("en-u-co-unknown").getCollations(), ["unknown"]);
assert(new Intl.Locale("en").getCalendars().includes("gregory"));
array(new Intl.Locale("en-u-ca-unknown").getCalendars(), ["unknown"]);
same(new Intl.Locale("en").getTimeZones(), undefined);
const us = new Intl.Locale("en-US").getTimeZones();
assert(us.length > 0); array(us, [...us].sort()); same(new Set(us).size, us.length);
assert(us.every(x => Intl.supportedValuesOf("timeZone").includes(x)));
