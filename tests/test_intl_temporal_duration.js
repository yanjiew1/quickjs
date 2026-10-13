/* Native Stage 4 Temporal/Intl integration; run after both feature stages. */
function same(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw Error((message || "mismatch") + ": " + actual + " != " + expected);
}
function throws(type, operation) {
    try { operation(); } catch (error) {
        if (error instanceof type) return;
        throw error;
    }
    throw Error("expected " + type.name);
}
function joined(parts) { return parts.map(part => part.value).join(""); }
if (typeof Temporal !== "object" || typeof Intl.DurationFormat !== "function")
    throw Error("test requires native Temporal.Duration and Intl.DurationFormat");

const fields = ["years", "months", "weeks", "days", "hours", "minutes",
                "seconds", "milliseconds", "microseconds", "nanoseconds"];
const record = {days:1, hours:2, minutes:3, seconds:4,
                milliseconds:567, microseconds:890, nanoseconds:123};
const options = {style:"digital", fractionalDigits:9};
const formatter = new Intl.DurationFormat("en-US", options);
const expected = formatter.format(record);
const duration = Temporal.Duration.from("P1DT2H3M4.567890123S");
same(formatter.format("P1DT2H3M4.567890123S"), expected);
same(joined(formatter.formatToParts("P1DT2H3M4.567890123S")), expected);
same(formatter.format(duration), expected);
same(joined(formatter.formatToParts(duration)), expected);
same(duration.toLocaleString("en-US", options), expected);

/* Private duration slots precede getters, including subclass instances. */
let getterCalls = 0;
for (const name of [...fields, "toString", "valueOf"]) {
    Object.defineProperty(duration, name, {
        configurable:true,
        get() { getterCalls++; throw Error("unexpected duration getter"); }
    });
}
same(formatter.format(duration), expected);
same(joined(formatter.formatToParts(duration)), expected);
same(duration.toLocaleString("en-US", options), expected);
same(getterCalls, 0);
class DerivedDuration extends Temporal.Duration {}
const derived = new DerivedDuration(0,0,0,0,2,3,4,567,890,123);
Object.defineProperty(derived, "hours", {
    get() { throw Error("unexpected subclass duration getter"); }
});
same(formatter.format(derived), formatter.format({...record, days:0}));

/* Ordinary objects still use the specified alphabetical getter order. */
const calls = [];
formatter.format(new Proxy(record, {
    get(target, key) { calls.push(key); return target[key]; }
}));
same(calls.join(),
     "days,hours,microseconds,milliseconds,minutes,months,nanoseconds,seconds,weeks,years");
const poisonedRecord = {get days() { throw Error("ordinary getter observed"); }};
let observed;
try { formatter.format(poisonedRecord); } catch (error) { observed = error; }
same(observed.message, "ordinary getter observed");

/* Native parser accepts the Temporal grammar and shares invalidity rules. */
for (const text of ["PT1S", "PT1,5S", "-PT0.000000001S", "PT1.5H", "P1W"]) {
    const native = Temporal.Duration.from(text);
    same(formatter.format(text), formatter.format(native));
    same(joined(formatter.formatToParts(text)), formatter.format(native));
}
for (const text of ["", "P", "PT", "P-1D", "P1DT", "PTNaNS",
                    "PT1.1234567890S", "PT9007199254740992S"]) {
    throws(RangeError, () => formatter.format(text));
    throws(RangeError, () => formatter.formatToParts(text));
}
for (const value of [undefined, null, 1, true, Symbol(), 1n]) {
    throws(TypeError, () => formatter.format(value));
    throws(TypeError, () => formatter.formatToParts(value));
}

/* Exact binary64 duration integers survive native-slot conversion. */
const seconds = new Intl.DurationFormat("en-US", {
    seconds:"numeric", fractionalDigits:9
});
const unsafe = new Temporal.Duration(0,0,0,0,0,0,0,0,0,9007199254740994);
same(seconds.format(unsafe), "9007199.254740994");
same(joined(seconds.formatToParts(unsafe)), seconds.format(unsafe));
same(unsafe.toLocaleString("en-US", {seconds:"numeric", fractionalDigits:9}),
     seconds.format(unsafe));

/* Locale method uses the retained intrinsic and brands before options Gets. */
let optionGets = 0;
const poisonOptions = {get localeMatcher() { optionGets++; return "lookup"; }};
throws(TypeError, () => Temporal.Duration.prototype.toLocaleString.call({},
                                                     "en-US", poisonOptions));
same(optionGets, 0);
const intrinsic = Intl.DurationFormat;
try {
    Intl.DurationFormat = function () { throw Error("global constructor used"); };
    same(duration.toLocaleString("en-US", options), expected);
} finally {
    Intl.DurationFormat = intrinsic;
}
