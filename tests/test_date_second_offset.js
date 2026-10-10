/* Date strings retain exact local seconds within the required GMT+HHMM form.
 * Use --expect-new-york-seconds with TZ=America/New_York in ICU or Temporal builds.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
function assert(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ': ' + actual + ' !== ' + expected);
}
const expectNewYorkSeconds = scriptArgs.includes('--expect-new-york-seconds');
const historical = Date.UTC(1880, 0, 1);
const format = /^(Sun|Mon|Tue|Wed|Thu|Fri|Sat) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [0-9]{2} -?[0-9]{4,6} [0-9]{2}:[0-9]{2}:[0-9]{2} GMT[+-][0-9]{4}( \(.+\))?$/;
for (const value of [
    0, -1000, 1000, historical,
    Date.UTC(2024, 0, 15, 12, 34, 56), Date.UTC(2024, 6, 15, 12, 34, 56),
    -62167219200000, -62198755200000, 253402300800000,
    -8640000000000000, 8640000000000000,
]) {
    const date = new Date(value);
    assert(format.test(date.toString()), true, 'required Date string format');
    assert(Date.parse(date.toString()), value, 'local string roundtrip');
    assert(Date.parse(date.toUTCString()), value, 'UTC string roundtrip');
    assert(Date.parse(date.toISOString()), value, 'ISO string roundtrip');
    assert(date.toDateString() + ' ' + date.toTimeString(), date.toString(),
           'date and time parts use the same offset');
    const offset = date.getTimezoneOffset();
    if (Number.isInteger(offset)) {
        const minutes = Math.abs(offset);
        const hh = String(Math.floor(minutes / 60)).padStart(2, '0');
        const mm = String(minutes % 60).padStart(2, '0');
        assert(date.toTimeString().slice(8),
               ' GMT' + (offset > 0 ? '-' : '+') + hh + mm,
               'whole-minute offset bytes');
    }
}
assert(new Date(0).toUTCString(), 'Thu, 01 Jan 1970 00:00:00 GMT', 'UTC bytes');
assert(new Date(0).toISOString(), '1970-01-01T00:00:00.000Z', 'ISO bytes');
assert(Date.parse('1970-01-01T00:20:34+00:20'), 34000, 'ISO minute offset');
assert(Date.parse('1970-01-01'), 0, 'ISO date-only form');

// Only the complete emitted layout with a matching reserved name adds seconds.
const prefix = 'Thu Jan 01 1970 00:20:34 GMT+0020';
for (const text of [
    prefix + ' (ordinary UTC+00:20:34)',
    prefix + ' (UTC+01:20:34)', prefix + ' (UTC-00:20:34)',
    prefix + ' (UTC+00:21:34)', prefix + ' (UTC+00:20:00)',
    prefix + ' (UTC+00:20:60)', prefix + ' (UTC+00:20:34) (ordinary)',
    prefix + ' (utc+00:20:34)', prefix + ' (UTC+00:20:34) ',
    prefix + ' (UTC+00:20:34)\0',
    'Jan 01 1970 00:20:34 GMT+0020 (UTC+00:20:34)',
    'Thu Jan 1 1970 00:20:34 GMT+0020 (UTC+00:20:34)',
]) assert(Date.parse(text), 34000, 'ordinary comment remains ignored');
for (const text of [prefix + ' (UTC+00:20:34', prefix + ' (UTC+00:20:34))'])
    assert(Number.isNaN(Date.parse(text)), true, 'malformed comment stays invalid');

const negative = 'Wed Dec 31 1969 23:39:26 GMT-0020 (UTC-00:20:34)';
assert(Date.parse(negative.replace('GMT-', 'GMT\u2212')), -34000,
       'legacy Unicode GMT minus stays a minute offset');
assert(Date.parse(negative.replace('UTC-', 'UTC\u2212')), -34000,
       'Unicode comment minus is outside the emitted ASCII extension');

const plusZero = 'Wed Dec 31 1969 23:59:40 GMT+0000 (UTC-00:00:20)';
const minusZero = 'Wed Dec 31 1969 23:59:40 GMT-0000 (UTC-00:00:20)';
if (typeof Temporal !== 'undefined') {
    assert(Date.parse(plusZero), 0, 'Stage 4 GMT zero uses the exact name sign');
    assert(Date.parse(minusZero), -20000, 'noncanonical Stage 4 GMT sign stays ignored');
} else if (typeof Intl !== 'undefined') {
    assert(Date.parse(minusZero), 0, 'ECMA262 exact millisecond GMT sign');
    assert(Date.parse(plusZero), -20000, 'mismatched ECMA262 GMT sign stays ignored');
} else {
    assert(Date.parse(plusZero), -20000, 'Date-only comment policy');
    assert(Date.parse(minusZero), -20000, 'Date-only comment policy');
}

if (expectNewYorkSeconds) {
    const date = new Date(historical);
    assert(date.getTimezoneOffset(), 17762 / 60, 'real New York historical seconds');
    assert(date.toString().endsWith(' GMT-0456 (UTC-04:56:02)'), true,
           'New York historical offset name');
    assert(Date.parse(prefix + ' (UTC+00:20:34)'), 0, 'explicit exact offset');
    assert(new Date(prefix + ' (UTC+00:20:34)').getTime(), 0,
           'constructor parses the same exact offset');
}
