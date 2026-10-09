/* Temporal month arithmetic obtains limits from the completed target. */
function assert(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw Error(`${message}: ${actual} !== ${expected}`);
}
function assertDate(date, year, monthCode, day) {
    assert(date.year, year, "year");
    assert(date.monthCode, monthCode, "monthCode");
    assert(date.day, day, "day");
}
function throwsRange(callback) {
    try { callback(); } catch (error) {
        if (error instanceof RangeError) return;
        throw error;
    }
    throw Error("expected RangeError");
}

for (const calendar of ["chinese", "dangi"]) {
    const longMonth = Temporal.PlainDate.from({
        calendar, year: 2019, monthCode: "M01", day: 30
    }, { overflow: "reject" });
    const next = longMonth.add({ months: 1 });
    assertDate(next, 2019, "M02", 29);
    throwsRange(() => longMonth.add({ months: 1 }, {
        overflow: "reject"
    }));
    assertDate(longMonth.add({ months: 1, days: 1 }),
               2019, "M03", 1);
    assertDate(next.subtract({ months: 1 }), 2019, "M01", 29);

    const later = Temporal.PlainDate.from({
        calendar, year: 2021, monthCode: "M07", day: 16
    });
    const earlier = Temporal.PlainDate.from({
        calendar, year: 2021, monthCode: "M03", day: 30
    });
    assert(earlier.until(later, { largestUnit: "year" }).toString(),
           "P3M16D", "difference after completing the anchor");
    assert(later.since(earlier, { largestUnit: "year" }).toString(),
           "P3M16D", "since after completing the anchor");

    const dateTime = longMonth.toPlainDateTime("12:34:56");
    const addedTime = dateTime.add({ months: 1 });
    assertDate(addedTime, 2019, "M02", 29);
    assert(addedTime.hour, 12, "date-time hour");

    const zoned = dateTime.toZonedDateTime("UTC");
    assertDate(zoned.add({ months: 1 }), 2019, "M02", 29);
}
