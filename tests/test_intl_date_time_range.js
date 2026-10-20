/* Date ranges compare civil fields, preserve sources, and coerce before clipping. */
import * as std from 'std';
import {assert, assertThrows, assertArrayEquals} from './assert.js';

const DTF = Intl.DateTimeFormat;
// Configured runs must exercise every promised provider; direct invocation
// without Meson arguments can discover the optional capabilities instead.
function hasCalendar(calendar) {
    if (scriptArgs.length > 1) {
        assert(['iso', 'native'].includes(scriptArgs[1]));
        return scriptArgs[1] === 'native';
    }
    return new DTF('en', {calendar}).resolvedOptions().calendar === calendar;
}
let namedZones;
if (scriptArgs.length > 2) {
    assert(['iso', 'native'].includes(scriptArgs[1]));
    assert(['minimal', 'native'].includes(scriptArgs[2]));
    namedZones = scriptArgs[2] === 'native';
} else {
    try {
        new DTF('en', {timeZone: 'America/New_York'});
        namedZones = true;
    } catch (error) {
        assert(error instanceof RangeError);
        namedZones = false;
    }
}
const date = new DTF('en', {timeZone: 'UTC', year: 'numeric', month: 'numeric', day: 'numeric'});
const first = Date.UTC(2024, 6, 1), last = Date.UTC(2024, 6, 3);
const names = ['formatRange', 'formatRangeToParts'];
for (const name of names) {
    const method = DTF.prototype[name];
    assert(method.name, name);
    assert(method.length, 2);
    assert(method.prototype, undefined);
    assertThrows(TypeError, () => new method(first, last));
    const descriptor = Object.getOwnPropertyDescriptor(DTF.prototype, name);
    assert(descriptor.writable, true);
    assert(descriptor.enumerable, false);
    assert(descriptor.configurable, true);
    for (const receiver of [undefined, null, 1, {}, DTF.prototype, Object.create(date), new Proxy(date, {})])
        assertThrows(TypeError, () => method.call(receiver, {valueOf() {throw 'coerced';}}, last));

    const log = [];
    const start = {[Symbol.toPrimitive](hint) {log.push('start:' + hint); return first;}};
    const end = {[Symbol.toPrimitive](hint) {log.push('end:' + hint); return last;}};
    method.call(date, start, end);
    assertArrayEquals(log, ['start:number', 'end:number']);
    log.length = 0;
    assertThrows(TypeError, () => method.call(date, start, undefined));
    assertArrayEquals(log, []);
    assertThrows(TypeError, () => method.call(date, undefined, end));
    assertArrayEquals(log, []);

    // Both conversions precede TimeClip, so the second abrupt completion wins.
    const marker = {};
    let caught;
    try {
        method.call(date, NaN, {valueOf() {throw marker;}});
    } catch (error) {
        caught = error;
    }
    assert(caught, marker);
    for (const value of [NaN, Infinity, -Infinity, 8640000000000001]) {
        assertThrows(RangeError, () => method.call(date, value, last));
        assertThrows(RangeError, () => method.call(date, first, value));
    }
    for (const value of [1n, Symbol()])
        assertThrows(TypeError, () => method.call(date, value, last));
    assert(typeof date.formatRange(last, first), 'string'); // Descending ranges are valid.
}

function check(formatter, start, end) {
    const parts = formatter.formatRangeToParts(start, end);
    assert(parts.map(part => part.value).join(''), formatter.formatRange(start, end));
    for (const part of parts) {
        assertArrayEquals(Object.keys(part), ['type', 'value', 'source']);
        assert(['shared', 'startRange', 'endRange'].includes(part.source), true);
    }
    return parts;
}
let parts = check(date, first, last);
assert(parts.some(part => part.source === 'startRange'), true);
assert(parts.some(part => part.source === 'endRange'), true);
parts = check(date, first, first);
assert(parts.every(part => part.source === 'shared'), true);
assert(parts.map(part => part.value).join(''), date.format(first));
// A genuine CLDR interval shares month/year, retaining both endpoint days.
const shortDate = new DTF('en', {timeZone: 'UTC', year: 'numeric', month: 'short', day: 'numeric'});
parts = check(shortDate, first, last);
for (const type of ['month', 'year']) {
    const field = parts.filter(part => part.type === type);
    assert(field.length, 1);
    assert(field[0].source, 'shared');
}
assertArrayEquals(parts.filter(part => part.type === 'day').map(part => part.source),
    ['startRange', 'endRange']);
parts = check(date, -.9, 0);
assert(parts.every(part => part.source === 'shared'), true);

// Differences below the displayed fractional precision must collapse.
for (const digits of [1, 2, 3]) {
    const formatter = new DTF('en', {timeZone: 'UTC', minute: 'numeric', second: 'numeric',
        fractionalSecondDigits: digits});
    const step = 10 ** (3 - digits);
    const base = 123000 + step;
    parts = check(formatter, base, base + step - 1);
    assert(parts.every(part => part.source === 'shared'), true);
    parts = check(formatter, base, base + step);
    assert(parts.some(part => part.source !== 'shared'), true);
}
const clock = new DTF('en', {timeZone: 'UTC', hour: 'numeric', minute: '2-digit'});
parts = check(clock, 0, 86400000);
assert(parts.some(part => part.source !== 'shared'), true);

const legacy = Object.create(DTF.prototype);
DTF.call(legacy, 'en', {timeZone: 'UTC'});
for (const name of names)
    assertThrows(TypeError, () => legacy[name](first, last));

const realm = std.createRealm();
const foreignMethod = realm.evalScript('Intl.DateTimeFormat.prototype.formatRangeToParts');
const foreignArray = realm.evalScript('Array.prototype');
const ForeignTypeError = realm.evalScript('TypeError');
assert(Object.getPrototypeOf(foreignMethod.call(date, first, last)), foreignArray);
assertThrows(ForeignTypeError, () => foreignMethod.call({}, first, last));

if (typeof Temporal !== 'undefined') {
    const a = Temporal.PlainDate.from('2024-07-01');
    const b = Temporal.PlainDate.from('2024-07-03');
    check(date, a, b);
    for (const name of names) {
        assertThrows(TypeError, () => date[name](NaN, b));
        assertThrows(TypeError, () => date[name](a, NaN));
        assertThrows(TypeError, () => date[name](a, last));
        assertThrows(TypeError, () => date[name](first, b));
        assertThrows(TypeError, () => date[name](a, b.toPlainDateTime()));
        assertThrows(TypeError, () => date[name](a.toPlainDateTime(), b));
        const zoned = Temporal.ZonedDateTime.from('2024-07-01T00:00Z[UTC]');
        assertThrows(TypeError, () => date[name](zoned, zoned));
    }
    const timeOnly = new DTF('en', {timeZone: 'UTC', hour: 'numeric'});
    assertThrows(TypeError, () => timeOnly.formatRange(a, b));
    if (hasCalendar('japanese')) {
        const japanese = Temporal.PlainDate.from('2024-07-03[u-ca=japanese]');
        // The first PlainDate's null format precedes the second calendar mismatch.
        assertThrows(TypeError, () => timeOnly.formatRange(a, japanese));
    }
    for (const calendar of ['chinese', 'dangi']) {
        if (!hasCalendar(calendar))
            continue;
        const formatter = new DTF('en', {timeZone: 'UTC', calendar, month: 'numeric', day: 'numeric'});
        const normal = Temporal.PlainDate.from({calendar, year: 2023, monthCode: 'M02', day: 1});
        const leap = Temporal.PlainDate.from({calendar, year: 2023, monthCode: 'M02L', day: 1});
        // Same related year, nominal month and day, but distinct month codes.
        assert(check(formatter, normal, leap).some(part => part.source !== 'shared'), true);
    }
    // Plain values retain their clock fields. The named-zone witness also
    // crosses a repeated DST hour; the fixed-offset provider has no DST rules.
    const local = new DTF('en', {timeZone: namedZones ? 'America/Los_Angeles' : '-07:00',
        hour: '2-digit',
        minute: '2-digit', hourCycle: 'h23'});
    parts = check(local, Temporal.PlainDateTime.from('2026-11-01T08:30'),
        Temporal.PlainDateTime.from('2026-11-01T09:30'));
    assert(parts.some(part => part.type === 'hour' && part.value === '08' && part.source === 'startRange'), true);
    assert(parts.some(part => part.type === 'hour' && part.value === '09' && part.source === 'endRange'), true);
}
