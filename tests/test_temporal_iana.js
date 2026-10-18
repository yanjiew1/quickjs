/* Native named-zone behavior, independent of Intl and system zoneinfo. */
import { assert, assertThrows } from "./assert.js";

const ZDT = Temporal.ZonedDateTime;
const hour = 3600000000000n;

function test_identities() {
    const alias = new ZDT(0n, "uS/eAsTeRn");
    assert(alias.timeZoneId, "US/Eastern");
    assert(alias.equals(new ZDT(0n, "America/New_York")), true);
    const utcAlias = new ZDT(0n, "gMt");
    assert(utcAlias.timeZoneId, "GMT");
    assert(utcAlias.equals(new ZDT(0n, "UTC")), true);
    assert(utcAlias.equals(new ZDT(0n, "+00:00")), false);
    assertThrows(RangeError, () => new ZDT(0n, "America/Not_A_Zone"));
    assertThrows(RangeError, () => new ZDT(0n, "America/New_York\0"));
}

function test_gap_and_overlap() {
    const gap = "2024-03-10T02:30[America/New_York]";
    const before = ZDT.from(gap, { disambiguation: "earlier" });
    const after = ZDT.from(gap, { disambiguation: "later" });
    assert(before.hour, 1);
    assert(before.offset, "-05:00");
    assert(after.hour, 3);
    assert(after.offset, "-04:00");
    assert(after.epochNanoseconds - before.epochNanoseconds, hour);
    assert(ZDT.from(gap).equals(after), true);
    assertThrows(RangeError, () => ZDT.from(gap, { disambiguation: "reject" }));

    const overlap = "2024-11-03T01:30[America/New_York]";
    const first = ZDT.from(overlap, { disambiguation: "earlier" });
    const last = ZDT.from(overlap, { disambiguation: "later" });
    assert(first.hour, 1);
    assert(last.hour, 1);
    assert(first.offset, "-04:00");
    assert(last.offset, "-05:00");
    assert(last.epochNanoseconds - first.epochNanoseconds, hour);
    assert(ZDT.from(overlap).equals(first), true);
    assertThrows(RangeError, () => ZDT.from(overlap, { disambiguation: "reject" }));
}

function test_day_arithmetic() {
    const start = ZDT.from("2024-03-10T00:00[America/New_York]");
    assert(start.hoursInDay, 23);
    assert(start.add({ days: 1 }).epochNanoseconds - start.epochNanoseconds, 23n * hour);
    assert(start.add({ hours: 24 }).hour, 1);
    assert(Temporal.Duration.from("P1D").total({ unit: "hour", relativeTo: start }), 23);
    assert(Temporal.Duration.compare("P1D", "PT24H", { relativeTo: start }), -1);
    const autumn = ZDT.from("2024-11-03T00:00[America/New_York]");
    assert(autumn.hoursInDay, 25);
    assert(Temporal.Duration.from("P1D").total({ unit: "hour", relativeTo: autumn }), 25);

    const skipped = new Temporal.PlainDate(2011, 12, 30).toZonedDateTime("Pacific/Apia");
    assert(skipped.toPlainDate().toString(), "2011-12-31");
    assert(skipped.hour, 0);
    assert(skipped.offset, "+14:00");
    assert(skipped.startOfDay().equals(skipped), true);
}

function test_history_and_transitions() {
    const historical = ZDT.from("1883-11-18T16:59:59Z[America/New_York]");
    assert(historical.offset, "-04:56:02");
    assert(historical.offsetNanoseconds, -17762000000000);
    assert(historical.add({ seconds: 1 }).offset, "-05:00");
    const fractional = new Temporal.Instant(-1n).toZonedDateTimeISO("America/New_York");
    assert(fractional.nanosecond, 999);
    assert(fractional.toInstant().epochNanoseconds, -1n);

    const spring = ZDT.from("2024-03-10T07:00Z[America/New_York]");
    assert(spring.subtract({ nanoseconds: 1 }).getTimeZoneTransition("next").equals(spring), true);
    assert(spring.getTimeZoneTransition("next").toInstant().toString(), "2024-11-03T06:00:00Z");
    assert(spring.add({ nanoseconds: 1 }).getTimeZoneTransition("previous").equals(spring), true);
    assert(new ZDT(0n, "Etc/GMT+5").getTimeZoneTransition("next"), null);
    const future = ZDT.from("2424-07-01T00:00[Europe/Dublin]");
    assert(future.offset, "+01:00");
}

test_identities();
test_gap_and_overlap();
test_day_arithmetic();
test_history_and_transitions();

if (scriptArgs.includes("--default-ny")) {
    assert(Temporal.Now.timeZoneId(), "America/New_York");
    assert(new Date("2024-01-01T00:00Z").getTimezoneOffset(), 300);
    assert(new Date("2024-07-01T00:00Z").getTimezoneOffset(), 240);
    assert(new Date(2024, 2, 10, 2, 30).getTime(),
           Number(ZDT.from("2024-03-10T02:30[America/New_York]").epochMilliseconds));
}
