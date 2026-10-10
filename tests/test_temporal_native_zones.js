/* Temporal is optional; named zones require no Intl service. */
if (typeof Temporal !== "undefined" &&
    typeof Temporal.ZonedDateTime === "function" &&
    typeof Temporal.ZonedDateTime.prototype.getTimeZoneTransition === "function") {
    /* Named zones work with Intl disabled and with filesystem-free fallback. */
    function assert(actual, expected) {
        if (actual !== expected) throw new Error(String(actual) + " !== " + String(expected));
    }
    function rejected(callback) {
        try { callback(); } catch (error) { if (error instanceof RangeError) return; throw error; }
        throw new Error("expected RangeError");
    }
    assert(Temporal.ZonedDateTime.from("2024-01-01T00:00[aMeRiCa/NeW_yOrK]").timeZoneId, "America/New_York");
    assert(Temporal.ZonedDateTime.from("2024-01-01T00:00[US/Eastern]").timeZoneId, "US/Eastern");
    const gap = "2024-03-10T02:30[America/New_York]";
    assert(Temporal.ZonedDateTime.from(gap, { disambiguation: "earlier" }).hour, 1);
    assert(Temporal.ZonedDateTime.from(gap, { disambiguation: "later" }).hour, 3);
    assert(Temporal.ZonedDateTime.from(gap).hour, 3);
    rejected(() => Temporal.ZonedDateTime.from(gap, { disambiguation: "reject" }));
    const overlap = "2024-11-03T01:30[America/New_York]";
    const earlier = Temporal.ZonedDateTime.from(overlap, { disambiguation: "earlier" });
    const later = Temporal.ZonedDateTime.from(overlap, { disambiguation: "later" });
    assert(later.epochNanoseconds - earlier.epochNanoseconds, 3600000000000n);
    assert(earlier.equals(earlier.withTimeZone("US/Eastern")), true);
    rejected(() => Temporal.ZonedDateTime.from(overlap, { disambiguation: "reject" }));
    const spring = Temporal.ZonedDateTime.from("2024-03-10T00:00[America/New_York]");
    assert(spring.getTimeZoneTransition("next").epochNanoseconds, 1710054000000000000n);
    assert(spring.getTimeZoneTransition("next").add({ nanoseconds: 1 }).getTimeZoneTransition("previous").epochNanoseconds,
           1710054000000000000n);
    assert(new Temporal.ZonedDateTime(8640000000000000000000n, "America/New_York").offset.length > 0, true);
    assert(new Temporal.ZonedDateTime(-8640000000000000000000n, "America/New_York").offset.length > 0, true);
    assert(Temporal.ZonedDateTime.from("2011-12-30T12:00[Pacific/Apia]").day, 31);
    assert(Temporal.ZonedDateTime.from("2018-11-04T12:00[America/Sao_Paulo]").startOfDay().hour, 1);
    rejected(() => new Temporal.ZonedDateTime(0n, "../../etc/passwd"));
    rejected(() => new Temporal.ZonedDateTime(0n, "Not/AZone"));

    /* The exact historical dates below are tz 2026e fixture checks, not dates
       prescribed by Temporal. See source-notes.txt in the source packet. */
    function transitionAt(zone, seconds, calendar = "iso8601") {
        return new Temporal.ZonedDateTime(BigInt(seconds) * 1000000000n, zone, calendar);
    }
    function epochResult(value) {
        if (value === null) return null;
        const before = new Temporal.ZonedDateTime(value.epochNanoseconds - 1n,
                                                  value.timeZoneId, value.calendarId);
        assert(value.offsetNanoseconds !== before.offsetNanoseconds, true);
        return value.epochNanoseconds;
    }
    function transitionBoundary(zone, seconds, previous, next, deltaSeconds, calendar = "iso8601") {
        const at = transitionAt(zone, seconds, calendar);
        const t = at.epochNanoseconds;
        const before = new Temporal.ZonedDateTime(t - 1n, zone, calendar);
        const after = new Temporal.ZonedDateTime(t + 1n, zone, calendar);
        assert(epochResult(before.getTimeZoneTransition("next")), t);
        assert(epochResult(after.getTimeZoneTransition("previous")), t);
        assert(epochResult(at.getTimeZoneTransition("previous")),
               previous === null ? null : BigInt(previous) * 1000000000n);
        assert(epochResult(at.getTimeZoneTransition("next")),
               next === null ? null : BigInt(next) * 1000000000n);
        assert(epochResult(before.getTimeZoneTransition("previous")),
               previous === null ? null : BigInt(previous) * 1000000000n);
        assert(epochResult(after.getTimeZoneTransition("next")),
               next === null ? null : BigInt(next) * 1000000000n);
        assert(at.offsetNanoseconds - before.offsetNanoseconds,
               deltaSeconds * 1000000000);
        const repeated = before.getTimeZoneTransition({ direction: "next" });
        assert(repeated.epochNanoseconds, t);
        assert(repeated.timeZoneId, before.timeZoneId);
        assert(repeated.calendarId, before.calendarId);
    }
    function offsetPreservingCandidate(zone, seconds, previous, next, offsetSeconds) {
        const at = transitionAt(zone, seconds);
        const t = at.epochNanoseconds;
        for (const ns of [t - 1n, t, t + 1n]) {
            const input = new Temporal.ZonedDateTime(ns, zone);
            assert(input.offsetNanoseconds, offsetSeconds * 1000000000);
            assert(epochResult(input.getTimeZoneTransition("previous")),
                   BigInt(previous) * 1000000000n);
            assert(epochResult(input.getTimeZoneTransition("next")),
                   BigInt(next) * 1000000000n);
        }
    }

    /* northamerica:346-351: LMT -> EST at 1883 Nov 18 17:00u, then
       US:182-183 supplies 1918 Mar lastSun 02:00 local (07:00 UTC). */
    transitionBoundary("America/New_York", -2717650800, null, -1633280400, -238);
    transitionBoundary("US/Eastern", -2717650800, null, -1633280400, -238, "iso8601");
    /* This shared-provider configuration also enables non-ISO calendars.
       The dedicated calendar fixture covers Intl enabled and disabled. */
    if (typeof Intl !== "undefined") {
        transitionBoundary("US/Eastern", -2717650800, null, -1633280400, -238, "gregory");
    }

    /* europe:589-590,2395-2396: raw +0/DST +1 becomes raw +1/DST +0.
       northamerica:184-186: EWT -> EPT preserves raw -5/DST +1. */
    offsetPreservingCandidate("Europe/Lisbon", 717555600, 701830800, 733280400, 3600);
    offsetPreservingCandidate("America/New_York", -769395600, -880218000, -765396000, -14400);

    /* The pinned footer parser's explicit observations end in 2438. These
       witnesses cross that boundary, and then the same boundary +400 years.
       Dates follow US Mar Sun>=8 / Nov Sun>=1 and LH Apr/Oct Sun>=1. */
    const repeatSeconds = 12622780800;
    for (const shift of [0, repeatSeconds]) {
        transitionBoundary("America/New_York", 14795503200 + shift,
                           14774943600 + shift, 14806393200 + shift, -3600);
        transitionBoundary("America/New_York", 14806393200 + shift,
                           14795503200 + shift, 14826952800 + shift, 3600);
        transitionBoundary("Australia/Lord_Howe", 14792427000 + shift,
                           14776700400 + shift, 14808150000 + shift, 1800);
        transitionBoundary("Australia/Lord_Howe", 14808150000 + shift,
                           14792427000 + shift, 14823876600 + shift, -1800);
    }

    /* Portable bound properties do not prescribe a political transition date. */
    const nsLimit = 8640000000000000000000n;
    for (const zone of ["America/New_York", "Australia/Lord_Howe", "UTC", "+05:45", "-04:30"]) {
        assert(new Temporal.ZonedDateTime(nsLimit, zone).getTimeZoneTransition("next"), null);
        assert(new Temporal.ZonedDateTime(-nsLimit, zone).getTimeZoneTransition("previous"), null);
    }
    for (const zone of ["UTC", "Etc/UTC", "Etc/GMT+5", "+05:45", "-04:30"]) {
        for (const ns of [-nsLimit, -1n, 0n, 1n, nsLimit]) {
            const input = new Temporal.ZonedDateTime(ns, zone);
            assert(input.getTimeZoneTransition("next"), null);
            assert(input.getTimeZoneTransition("previous"), null);
        }
    }
    for (const zone of ["America/New_York", "Australia/Lord_Howe"]) {
        const input = new Temporal.ZonedDateTime(nsLimit, zone);
        const previous = input.getTimeZoneTransition("previous");
        assert(previous !== null, true);
        assert(previous.epochNanoseconds < nsLimit, true);
        assert(previous.epochNanoseconds >= -nsLimit, true);
        assert(previous.offsetNanoseconds !==
               new Temporal.ZonedDateTime(previous.epochNanoseconds - 1n, zone).offsetNanoseconds, true);
        const first = new Temporal.ZonedDateTime(-nsLimit, zone).getTimeZoneTransition("next");
        assert(first !== null, true);
        assert(first.epochNanoseconds > -nsLimit, true);
        assert(first.getTimeZoneTransition("previous"), null);
    }
}
