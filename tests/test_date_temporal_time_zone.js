/* Run in a fresh process with TZ=America/New_York or TZ=US/Eastern.
   The engine snapshots the host/embedding default; this unit never mutates it. */
import * as std from "std";

function assert(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw Error(message || `${actual} !== ${expected}`);
}
function assertNear(actual, expected) {
    if (Math.abs(actual - expected) > 1e-10)
        throw Error(`${actual} differs from ${expected}`);
}

const requested = std.getenv("TZ");
assert(requested === "America/New_York" || requested === "US/Eastern", true,
       "this test requires a fresh New York TZ process");
const zone = Temporal.Now.timeZoneId();
assert(zone, "America/New_York");
assert(Temporal.Now.zonedDateTimeISO().timeZoneId, zone);
assert(Temporal.Now.zonedDateTimeISO("US/Eastern").timeZoneId, "US/Eastern");
const alias = new Temporal.ZonedDateTime(0n, "US/Eastern");
assert(alias.equals(new Temporal.ZonedDateTime(0n, zone)), true);
if (typeof Intl !== "undefined")
    assert(new Intl.DateTimeFormat("en").resolvedOptions().timeZone, zone);

function sameFields(date, zoned) {
    assert(date.getFullYear(), zoned.year);
    assert(date.getMonth() + 1, zoned.month);
    assert(date.getDate(), zoned.day);
    assert(date.getHours(), zoned.hour);
    assert(date.getMinutes(), zoned.minute);
    assert(date.getSeconds(), zoned.second);
    assert(date.getMilliseconds(), zoned.millisecond);
    assertNear(date.getTimezoneOffset(), -zoned.offsetNanoseconds / 60000000000);
}
for (const input of ["2024-01-15T12:00:00Z", "2024-07-15T12:00:00Z",
                     "2024-03-10T06:59:59Z", "2024-03-10T07:00:00Z",
                     "2024-11-03T05:30:00Z", "2024-11-03T06:30:00Z",
                     "1883-11-18T16:59:59Z"]) {
    const instant = Temporal.Instant.from(input);
    sameFields(new Date(instant.epochMilliseconds), instant.toZonedDateTimeISO(zone));
}
assert(new Date("2024-01-15T12:00:00Z").getTimezoneOffset(), 300);
assert(new Date("2024-07-15T12:00:00Z").getTimezoneOffset(), 240);
const historical = new Date("1883-11-18T16:59:59Z");
assert(historical.getHours(), 12);
assert(historical.getMinutes(), 3);
assert(historical.getSeconds(), 57);
assertNear(historical.getTimezoneOffset(), 296 + 2 / 60);

for (const [local, fields] of [
    ["2024-03-10T02:30", [2024, 2, 10, 2, 30]],
    ["2024-11-03T01:30", [2024, 10, 3, 1, 30]],
    ["1883-11-18T12:03:57", [1883, 10, 18, 12, 3, 57]]
]) {
    const compatible = Temporal.ZonedDateTime.from(`${local}[${zone}]`);
    assert(new Date(...fields).getTime(), compatible.epochMilliseconds);
    assert(Date.parse(local), compatible.epochMilliseconds);
    const changed = new Date(2024, fields[1], fields[2]);
    changed.setFullYear(fields[0]);
    changed.setHours(fields[3], fields[4] || 0, fields[5] || 0, 0);
    assert(changed.getTime(), compatible.epochMilliseconds);
}

/* Local clock fields may lie just outside the Instant domain. Their UTC
   result can still be exactly the Date/Instant minimum or maximum. */
for (const milliseconds of [-8640000000000000, 8640000000000000]) {
    const date = new Date(milliseconds);
    const zoned = new Temporal.Instant(BigInt(milliseconds) * 1000000n)
        .toZonedDateTimeISO(zone);
    sameFields(date, zoned);
    assert(new Date(zoned.year, zoned.month - 1, zoned.day,
                    zoned.hour, zoned.minute, zoned.second,
                    zoned.millisecond).getTime(), milliseconds);
}
assert(Number.isNaN(new Date(300000, 0, 1).getTime()), true);
assert(Number.isNaN(new Date(2024, 0, 1, 1e100).getTime()), true);
