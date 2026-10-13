"use strict";
/* Locale fixtures use the provider's actual admission; semantic checks still run. */
const intlLocaleDataReported = new Set();
function hasLocaleData(C, locale, options) {
    const supported = C.supportedLocalesOf(locale).length !== 0;
    const requested = new C(locale, options).resolvedOptions().locale;
    const fallback = new C(undefined, options).resolvedOptions().locale;
    if (!supported && requested !== fallback)
        throw Error(C.name + ': unsupported locale did not use the default');
    const key = C.name + '/' + locale;
    if (!intlLocaleDataReported.has(key)) {
        print('Intl locale data: ' + key + ': ' +
              (supported ? 'admitted' : 'unavailable; fallback=' + fallback));
        intlLocaleDataReported.add(key);
    }
    return supported;
}

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

/* Shared resolution checks expose option/extension precedence. */
same(new Intl.NumberFormat("en-u-nu-thai", { numberingSystem: "latn" }).resolvedOptions().locale, "en");
const thaiNumber = new Intl.NumberFormat("en-u-nu-thai", { numberingSystem: "thai" });
const thaiOptions = thaiNumber.resolvedOptions();
same(thaiOptions.numberingSystem,
     new Intl.NumberFormat("en", { numberingSystem: "thai" }).resolvedOptions().numberingSystem);
if (thaiOptions.numberingSystem === "thai") {
    same(thaiOptions.locale, "en-u-nu-thai");
} else {
    same(thaiOptions.locale, "en");
    same(thaiOptions.numberingSystem, new Intl.NumberFormat("en").resolvedOptions().numberingSystem);
    print("Intl locale data: NumberFormat/en/nu-thai unavailable; default numbering asserted");
}
same(new Intl.Collator("en-u-kn", { numeric: false }).resolvedOptions().locale, "en");
same(new Intl.Collator("en-u-kn").resolvedOptions().numeric, true);
same(new Intl.DateTimeFormat("en-u-hc-h23", { hour: "numeric", hour12: true }).resolvedOptions().locale, "en");
equalArray(Intl.NumberFormat.supportedLocalesOf(["en-US", "en-US", "fr"]),
           hasLocaleData(Intl.NumberFormat, "fr") ? ["en-US", "fr"] : ["en-US"]);
equalArray(Intl.NumberFormat.supportedLocalesOf(["en"], 0), ["en"]);
