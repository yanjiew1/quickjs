function assert(value, expected) {
    if (arguments.length < 2) expected = true;
    if (!Object.is(value, expected)) throw Error(`${value} !== ${expected}`);
}
const descriptor = Object.getOwnPropertyDescriptor(Temporal.Now, Symbol.toStringTag);
assert(descriptor.value, "Temporal.Now");
assert(descriptor.writable, false);
assert(descriptor.enumerable, false);
assert(descriptor.configurable, true);
assert(Object.getPrototypeOf(Temporal.Now), Object.prototype);
for (const name of ["timeZoneId", "instant", "plainDateTimeISO", "plainDateISO",
                    "plainTimeISO", "zonedDateTimeISO"]) {
    assert(Temporal.Now[name].length, 0);
    assert(Object.getOwnPropertyDescriptor(Temporal.Now[name], "prototype"), undefined);
}
assert(typeof Temporal.Now.timeZoneId(), "string");
const before = Date.now();
const instant = Temporal.Now.instant();
const after = Date.now();
assert(instant instanceof Temporal.Instant);
assert(instant.epochMilliseconds >= before - 1000 && instant.epochMilliseconds <= after + 1000);
assert(Temporal.Now.plainDateISO("UTC") instanceof Temporal.PlainDate);
assert(Temporal.Now.plainTimeISO("UTC") instanceof Temporal.PlainTime);
assert(Temporal.Now.plainDateTimeISO("UTC") instanceof Temporal.PlainDateTime);
assert(Temporal.Now.zonedDateTimeISO("+05:30").timeZoneId, "+05:30");
assert(Temporal.Now.zonedDateTimeISO().calendarId, "iso8601");
assert(Temporal.Now.plainDateISO().calendarId, "iso8601");
assert(Temporal.Now.plainDateTimeISO().calendarId, "iso8601");
