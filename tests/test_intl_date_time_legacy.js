/* CONFIG_INTL_LEGACY enabled only; complete optional Date chaining. */
function same(a, b) { if (!Object.is(a, b)) throw Error("not same"); }
function throws(C, f) { try { f(); } catch (e) { if (e instanceof C) return; throw e; } throw Error("missing exception"); }
const proto = Intl.DateTimeFormat.prototype;
const legacy = Object.create(proto);
same(Intl.DateTimeFormat.call(legacy, "en", { timeZone: "UTC" }), legacy);
const symbols = Object.getOwnPropertySymbols(legacy);
same(symbols.length, 1);
same(symbols[0].description, "IntlLegacyConstructedSymbol");
const descriptor = Object.getOwnPropertyDescriptor(legacy, symbols[0]);
same(descriptor.writable, false);
same(descriptor.enumerable, false);
same(descriptor.configurable, false);
same(typeof legacy.format(0), "string");
same(legacy.format, legacy.format);
same(legacy.resolvedOptions().timeZone, "UTC");
throws(TypeError, () => legacy.formatToParts(0));
throws(TypeError, () => legacy.formatRange(0, 1));
throws(TypeError, () => legacy.formatRangeToParts(0, 1));
throws(TypeError, () => Intl.DateTimeFormat.call(legacy));
const frozen = Object.freeze(Object.create(proto));
throws(TypeError, () => Intl.DateTimeFormat.call(frozen));
const initialized = new Intl.DateTimeFormat("en", { timeZone: "UTC" });
same(Intl.DateTimeFormat.call(initialized, "de", { timeZone: "+01" }), initialized);
same(initialized.resolvedOptions().timeZone, "UTC");
/* Cross-realm and fallback-symbol equality with NumberFormat are official
   Test262 focus gates; this standalone test does not provide a realm host. */
