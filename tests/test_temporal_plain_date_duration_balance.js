/* PlainDate duration time units carry whole 24-hour days. */
function same(actual, expected) {
    if (!Object.is(actual, expected))
        throw Error(String(actual) + " != " + expected);
}
function check(date, duration, added, subtracted) {
    same(date.add(duration).toString(), added);
    same(date.subtract(duration).toString(), subtracted);
}

const date = new Temporal.PlainDate(2000, 5, 2);
const units = [
    ["hours", 24], ["minutes", 1440], ["seconds", 86400],
    ["milliseconds", 86400000], ["microseconds", 86400000000],
    ["nanoseconds", 86400000000000]
];
for (const [unit, day] of units) {
    check(date, { [unit]: 1 }, "2000-05-02", "2000-05-02");
    check(date, { [unit]: -1 }, "2000-05-02", "2000-05-02");
    check(date, { [unit]: day }, "2000-05-03", "2000-05-01");
    check(date, { [unit]: -day }, "2000-05-01", "2000-05-03");
    check(date, { [unit]: day + 1 }, "2000-05-03", "2000-05-01");
    check(date, { [unit]: -day - 1 }, "2000-05-01", "2000-05-03");
}
const combined = {
    days: 1, hours: 24, minutes: 1440, seconds: 86400,
    milliseconds: 86400000, microseconds: 86400000000,
    nanoseconds: 86400000000000
};
check(date, combined, "2000-05-09", "2000-04-25");
check(date, Temporal.Duration.from(combined), "2000-05-09", "2000-04-25");
check(date, Temporal.Duration.from(combined).negated(),
      "2000-04-25", "2000-05-09");
check(date, "P1DT24H1440M86400S", "2000-05-06", "2000-04-28");
check(date, "-P1DT24H1440M86400S", "2000-04-28", "2000-05-06");
for (const duration of ["PT24.567890123H", "PT1440.567890123M"]) {
    check(date, duration, "2000-05-03", "2000-05-01");
    check(date, "-" + duration, "2000-05-01", "2000-05-03");
}
check(date, {
    hours: 23, minutes: 59, seconds: 59,
    milliseconds: 999, microseconds: 999, nanoseconds: 999
}, "2000-05-02", "2000-05-02");
check(date, { days: 1, nanoseconds: 1 }, "2000-05-03", "2000-05-01");
check(date, { days: -1, nanoseconds: -1 }, "2000-05-01", "2000-05-03");
check(new Temporal.PlainDate(2020, 2, 29), { hours: 48 },
      "2020-03-02", "2020-02-27");

const epoch = new Temporal.PlainDate(1970, 1, 1);
const maximum = "+275760-09-13", minimum = "-271821-04-19";
for (const duration of [
    "PT2400000023H59M59.999999999S",
    { hours: 2400000023, nanoseconds: 3599999999999 },
    "PT144000001439M59.999999999S",
    { minutes: 144000001439, nanoseconds: 59999999999 },
    "PT8640000086399.999999999S",
    { seconds: 8640000086399, nanoseconds: 999999999 }
]) {
    same(epoch.add(duration).toString(), maximum);
    same(epoch.subtract(Temporal.Duration.from(duration).negated()).toString(),
         maximum);
}
for (const duration of [
    "PT2400000047H59M59.999999999S",
    { hours: 2400000047, nanoseconds: 3599999999999 },
    "PT144000002879M59.999999999S",
    { minutes: 144000002879, nanoseconds: 59999999999 },
    "PT8640000172799.999999999S",
    { seconds: 8640000172799, nanoseconds: 999999999 }
]) {
    same(epoch.subtract(duration).toString(), minimum);
    same(epoch.add(Temporal.Duration.from(duration).negated()).toString(),
         minimum);
}
const minDate = new Temporal.PlainDate(-271821, 4, 19);
const maxDate = new Temporal.PlainDate(275760, 9, 13);
for (const duration of [
    "PT4800000047H59M59.999999999S",
    { hours: 4800000047, minutes: 59, seconds: 59,
      milliseconds: 999, microseconds: 999, nanoseconds: 999 },
    "PT288000002879M59.999999999S",
    { minutes: 288000002879, seconds: 59,
      milliseconds: 999, microseconds: 999, nanoseconds: 999 },
    "PT17280000172799.999999998S",
    { seconds: 17280000172799, nanoseconds: 999999998 }
]) {
    const positive = Temporal.Duration.from(duration);
    const negative = positive.negated();
    same(minDate.add(positive).toString(), maximum);
    same(minDate.subtract(negative).toString(), maximum);
    same(maxDate.subtract(positive).toString(), minimum);
    same(maxDate.add(negative).toString(), minimum);
}
