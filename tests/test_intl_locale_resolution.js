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

/* Shared resolution checks expose option/extension precedence. */
same(new Intl.NumberFormat("en-u-nu-thai", { numberingSystem: "latn" }).resolvedOptions().locale, "en");
same(new Intl.NumberFormat("en-u-nu-thai", { numberingSystem: "thai" }).resolvedOptions().locale, "en-u-nu-thai");
same(new Intl.Collator("en-u-kn", { numeric: false }).resolvedOptions().locale, "en");
same(new Intl.Collator("en-u-kn").resolvedOptions().numeric, true);
same(new Intl.DateTimeFormat("en-u-hc-h23", { hour: "numeric", hour12: true }).resolvedOptions().locale, "en");
equalArray(Intl.NumberFormat.supportedLocalesOf(["en-US", "en-US", "fr"]), ["en-US", "fr"]);
equalArray(Intl.NumberFormat.supportedLocalesOf(["en"], 0), ["en"]);
