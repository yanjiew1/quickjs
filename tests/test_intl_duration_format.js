/* CONFIG_INTL enabled native regression tests; no polyfill or shell tools. */
function same(actual, expected, message) {
    if (!Object.is(actual, expected)) throw Error((message || "mismatch") + ": " + actual + " != " + expected);
}
function throws(type, operation) {
    try { operation(); } catch (error) { if (error instanceof type) return; throw error; }
    throw Error("expected " + type.name);
}
function joined(parts) { return parts.map(p => p.value).join(""); }
function values(parts, type) { return parts.filter(p => p.type === type).map(p => p.value).join(""); }
function nf(options) { return new Intl.NumberFormat("en-US", {useGrouping: false, ...options}); }

if (typeof Intl !== "object" || typeof Intl.DurationFormat !== "function") throw Error("test requires Intl.DurationFormat");
same(Intl.DurationFormat.length, 0);
same(new Intl.DurationFormat().resolvedOptions().style, "short");
same(Intl.DurationFormat.supportedLocalesOf([]).length, 0);
same(Intl.DurationFormat.supportedLocalesOf().length, 0);
throws(TypeError, () => new Intl.DurationFormat().format());
throws(TypeError, () => new Intl.DurationFormat().formatToParts());
throws(TypeError, () => Intl.DurationFormat());
const digital = new Intl.DurationFormat("en-US", {style:"digital", fractionalDigits:3});
same(digital.format({hours:1, minutes:2, seconds:3, milliseconds:456}), "1:02:03.456");
same(digital.format({hours:0, minutes:-2, seconds:-3}), "-0:02:03.000");
same(joined(digital.formatToParts({hours:1, minutes:2, seconds:3, microseconds:1})), digital.format({hours:1, minutes:2, seconds:3, microseconds:1}));
same(digital.formatToParts({hours:1, minutes:2, seconds:3}).filter(p => p.type === "literal").every(p => !("unit" in p)), true);
same(digital.formatToParts({hours:1, minutes:2, seconds:3}).some(p => p.unit === "hour"), true);
same(new Intl.DurationFormat("da", {style:"digital"}).format({hours:1,minutes:2,seconds:3}), "1.02.03");
const short = new Intl.DurationFormat("en-US", {style:"short"});
same(short.format({years:0}), "");
same(joined(short.formatToParts({years:1,days:2,hours:3})), short.format({years:1,days:2,hours:3}));
same(new Intl.DurationFormat("en-US", {seconds:"numeric",fractionalDigits:9}).format({seconds:0, nanoseconds:1}), "0.000000001");
same(new Intl.DurationFormat("en-US", {milliseconds:"long", microseconds:"numeric", fractionalDigits:3}).format({milliseconds:1,microseconds:2345}), "3.345 milliseconds");
for (const input of [undefined, null, 1, true, Symbol(), 1n]) throws(TypeError, () => short.format(input));
same(short.format("PT1S"), short.format({seconds:1}));
throws(TypeError, () => short.format({}));
for (const input of [{seconds:1.5},{seconds:Infinity},{seconds:1,milliseconds:-1},{years:4294967296},{seconds:9007199254740992}])
    throws(RangeError, () => short.format(input));
throws(TypeError, () => short.format({seconds:1n}));
throws(RangeError, () => new Intl.DurationFormat("en", {milliseconds:"numeric",millisecondsDisplay:"always"}));
throws(RangeError, () => new Intl.DurationFormat("en", {hours:"numeric",minutes:"long"}));
const durationGetters = [];
short.format(new Proxy({seconds:1}, {get(target,key) {durationGetters.push(key);return target[key];}}));
same(durationGetters.join(), "days,hours,microseconds,milliseconds,minutes,months,nanoseconds,seconds,weeks,years");
const durationOptionGetters = [];
new Intl.DurationFormat("en", new Proxy({}, {get(target,key) {durationOptionGetters.push(key);return undefined;}}));
same(durationOptionGetters.join(), ["localeMatcher","numberingSystem","style",...Object.keys({years:0,months:0,weeks:0,days:0,hours:0,minutes:0,seconds:0,milliseconds:0,microseconds:0,nanoseconds:0}).flatMap(k => [k,k+"Display"]),"fractionalDigits"].join());
/* Exact validation near the normative 2^53-second boundary, including a
   remainder carried from unsafe nanosecond integers. */
const limit = 9007199254740992n * 1000000000n;
const below = Number(limit - 1000000000n);
if (BigInt(below) < limit) short.format({nanoseconds:below});
const above = Number(limit + 1000000000n);
if (BigInt(above) >= limit) throws(RangeError, () => short.format({nanoseconds:above}));
short.format({seconds:9007199254740991, nanoseconds:999999999});
throws(RangeError, () => short.format({seconds:9007199254740991, nanoseconds:1000000000}));
for (const method of ["format", "formatToParts", "resolvedOptions"])
    throws(TypeError, () => Intl.DurationFormat.prototype[method].call({}, {seconds:1}));
const durationKeys = Object.keys(digital.resolvedOptions());
same(durationKeys.slice(0,3).join(), "locale,numberingSystem,style");
same(durationKeys[durationKeys.length-1], "fractionalDigits");
