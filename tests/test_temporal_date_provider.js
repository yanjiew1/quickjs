/* Date and Temporal share the host zone, exact offsets and wall-time policy.
 * Run separately with TZ=America/New_York and TZ=UTC; no Intl dependency. */
function assert(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ': ' + actual + ' !== ' + expected);
}
const zone = Temporal.Now.timeZoneId();
function compareFields(milliseconds) {
    const date = new Date(milliseconds);
    const zoned = new Temporal.ZonedDateTime(BigInt(milliseconds) * 1000000n, zone);
    assert(date.getFullYear(), zoned.year, 'year');
    assert(date.getMonth() + 1, zoned.month, 'month');
    assert(date.getDate(), zoned.day, 'day');
    assert(date.getDay(), zoned.dayOfWeek % 7, 'weekday');
    assert(date.getHours(), zoned.hour, 'hour');
    assert(date.getMinutes(), zoned.minute, 'minute');
    assert(date.getSeconds(), zoned.second, 'second');
    assert(date.getMilliseconds(), zoned.millisecond, 'millisecond');
    assert(date.getTimezoneOffset(), -zoned.offsetNanoseconds / 60000000000, 'offset');
}
for (const milliseconds of [
    -1, 0, 1,
    Date.UTC(1880, 0, 1, 0, 0, 0, 999), // Historical offsets can contain seconds.
    Date.UTC(2024, 0, 15, 12, 34, 56, 789),
    Date.UTC(2024, 6, 15, 12, 34, 56, 789),
    Date.UTC(2024, 2, 10, 6, 59, 59, 999),
    Date.UTC(2024, 2, 10, 7, 0),
    Date.UTC(2024, 10, 3, 5, 59, 59, 999),
    Date.UTC(2024, 10, 3, 6, 0),
]) compareFields(milliseconds);

for (const [year, month, day, hour, minute, iso] of [
    [2024, 1, 15, 12, 34, '2024-01-15T12:34:00.000'],
    [2024, 3, 10, 2, 30, '2024-03-10T02:30:00.000'], // Gap: compatible uses later.
    [2024, 11, 3, 1, 30, '2024-11-03T01:30:00.000'], // Overlap: compatible uses earlier.
]) {
    const expected = Temporal.ZonedDateTime.from({
        timeZone: zone, year, month, day, hour, minute,
    });
    const milliseconds = Number(expected.epochMilliseconds);
    assert(new Date(year, month - 1, day, hour, minute).getTime(), milliseconds, 'constructor UTC');
    assert(Date.parse(iso), milliseconds, 'parse UTC');
    const changed = new Date(year, month - 1, day, 0, 0);
    assert(changed.setHours(hour, minute, 0, 0), milliseconds, 'setHours UTC');
    compareFields(milliseconds);
}
// Local setters must all use the same offset provider as local getters.
const original = new Date(2024, 0, 15, 12, 34, 56, 789);
const setters = [
    ['setFullYear', [2024, 0, 15]], ['setMonth', [0, 15]], ['setDate', [15]],
    ['setHours', [12, 34, 56, 789]], ['setMinutes', [34, 56, 789]],
    ['setSeconds', [56, 789]], ['setMilliseconds', [789]],
    ['setYear', [2024]],
];
for (const [method, arguments_] of setters) {
    const date = new Date(original.getTime());
    assert(date[method](...arguments_), original.getTime(), method);
}
// Named-zone alias copies retain the same native snapshot.
const primary = new Temporal.ZonedDateTime(1710054000000000000n, 'America/New_York');
assert(primary.withTimeZone('US/Eastern').offsetNanoseconds, primary.offsetNanoseconds, 'alias copy');
