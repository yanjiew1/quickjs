/* Impossible lunisolar years are rejected before calculating moon data. */
function throwsRange(callback) {
    try { callback(); } catch (error) {
        if (error instanceof RangeError) return;
        throw error;
    }
    throw Error("expected RangeError");
}
for (const calendar of ["chinese", "dangi"]) {
    for (const year of [-999999, 999999, -271822, 275761]) {
        for (const overflow of ["constrain", "reject"]) {
            throwsRange(() => Temporal.PlainMonthDay.from({
                calendar, year, monthCode: "M06L", day: 30
            }, { overflow }));
        }
    }
    const date = Temporal.PlainMonthDay.from({
        calendar, year: 1938, monthCode: "M07L", day: 30
    }, { overflow: "reject" });
    if (date.monthCode !== "M07L" || date.day !== 30)
        throw Error("supported reference date must remain available");
}
