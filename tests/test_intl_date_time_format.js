/* DateTimeFormat options, coercion, parts, intrinsic realms and bound lifetime. */
import * as std from 'std';
import {assert, assertThrows, assertArrayEquals} from './assert.js';

const DTF = Intl.DateTimeFormat;
const utc = {timeZone: 'UTC'};
assert(DTF.name, 'DateTimeFormat');
assert(DTF.length, 0);
assert(Object.getPrototypeOf(DTF), Function.prototype);
assert(Object.getPrototypeOf(DTF.prototype), Object.prototype);
assert(Object.prototype.toString.call(new DTF('en', utc)), '[object Intl.DateTimeFormat]');
for (const options of [{}, {year: 'numeric'}, {dateStyle: 'short'},
    {minute: 'numeric', second: 'numeric'}, {dayPeriod: 'short'},
    {dateStyle: 'long', hourCycle: 'h24'}, {year: 'numeric', hour12: false}]) {
    const resolved = new DTF('en', {...utc, ...options}).resolvedOptions();
    assert(Object.hasOwn(resolved, 'hourCycle'), false);
    assert(Object.hasOwn(resolved, 'hour12'), false);
}
for (const hourCycle of ['h11', 'h12', 'h23', 'h24']) {
    for (const options of [{hour: 'numeric'}, {timeStyle: 'short'},
        {dateStyle: 'short', timeStyle: 'long'}]) {
        const resolved = new DTF('en', {...utc, ...options, hourCycle}).resolvedOptions();
        assert(resolved.hourCycle, hourCycle);
        assert(resolved.hour12, hourCycle === 'h11' || hourCycle === 'h12');
    }
}
assert(DTF('en', utc).format(0), new DTF('en', utc).format(0));
assertThrows(TypeError, () => new DTF('en', null));
assertThrows(RangeError, () => new DTF('invalid_tag'));
for (const options of [false, 1, 1n, '', Symbol()])
    assert(typeof new DTF('en', options).format(0), 'string');

const f = new DTF('en', {...utc, year: 'numeric', month: '2-digit', day: '2-digit',
    hour: '2-digit', minute: '2-digit', second: '2-digit', fractionalSecondDigits: 3,
    hourCycle: 'h23'});
const bound = f.format;
assert(bound, f.format);
assert(bound.name, '');
assert(bound.length, 1);
assert(bound.prototype, undefined);
assertThrows(TypeError, () => new bound());
assert(bound.call(null, 0), f.format(0));
assert(f.format(-.9), f.format(0));
assert(f.format('0'), f.format(0));
for (const value of [NaN, Infinity, -Infinity, 8640000000000001])
    assertThrows(RangeError, () => f.format(value));
for (const value of [1n, Symbol()])
    assertThrows(TypeError, () => f.format(value));
assert(typeof f.format(), 'string');
let log = [];
assert(f.format({[Symbol.toPrimitive](hint) {log.push(hint); return 0;}}), f.format(0));
assertArrayEquals(log, ['number']);
for (const value of [0, -1, 123456, 1719835200123]) {
    const parts = f.formatToParts(value);
    assert(parts.map(p => p.value).join(''), f.format(value));
    for (const part of parts)
        assertArrayEquals(Object.keys(part), ['type', 'value']);
}

// Fractional precision extends a whole minute/second pattern; it adds no hour.
for (const digits of [1, 2, 3]) {
    const formatter = new DTF('en', {...utc, minute: 'numeric', second: 'numeric',
        fractionalSecondDigits: digits});
    assert(formatter.format(123456), '02:03.' + '456'.slice(0, digits));
    assert(formatter.resolvedOptions().fractionalSecondDigits, digits);
    assertArrayEquals(formatter.formatToParts(123456).map(p => p.type),
        ['minute', 'literal', 'second', 'literal', 'fractionalSecond']);
}
for (const [hourCycle, text] of [['h11', '0'], ['h12', '12'], ['h23', '0'], ['h24', '24']]) {
    const formatter = new DTF('en', {...utc, hour: 'numeric', hourCycle});
    assert(formatter.formatToParts(0).find(p => p.type === 'hour').value, text);
    assert(formatter.resolvedOptions().hourCycle, hourCycle);
}
const overridden = new DTF('en-u-hc-h11', {...utc, hour: 'numeric', hour12: false, hourCycle: 'h12'});
assert(overridden.resolvedOptions().hourCycle, 'h23');
assert(overridden.resolvedOptions().locale, 'en');
assertThrows(RangeError, () => new DTF('en', {...utc, hour12: true, hourCycle: 'invalid'}));
assertThrows(TypeError, () => new DTF('en', {dateStyle: 'full', year: 'numeric'}));
assertThrows(RangeError, () => new DTF('en', {timeZone: '+24:00'}));
assertThrows(RangeError, () => new DTF('en', {fractionalSecondDigits: 0}));
const styled = new DTF('en', {...utc, dateStyle: 'full', timeStyle: 'short'}).resolvedOptions();
assert(styled.dateStyle, 'full');
assert(styled.timeStyle, 'short');
assert(styled.year, undefined);
assert(styled.hour, undefined);

// Date/time composition retains the whole source programs without adding the
// weekday present in the specification's mandatory mixed-format subsets.
const mixed = new DTF('en', {...utc, year: 'numeric', month: 'numeric', day: 'numeric',
    hour: 'numeric', minute: '2-digit', second: '2-digit', fractionalSecondDigits: 2});
assert(mixed.resolvedOptions().weekday, undefined);
assert(mixed.formatToParts(1719835200123).some(part => part.type === 'weekday'), false);
assert(mixed.formatToParts(1719835200123).find(part => part.type === 'fractionalSecond').value, '12');

// Both real cyclic-only and numeric related-year sources exist. The native
// policy prefers a numeric year without changing U into another field kind.
for (const calendar of ['chinese', 'dangi']) {
    const lunisolar = new DTF('en', {...utc, calendar});
    if (lunisolar.resolvedOptions().calendar !== calendar)
        continue; // An ISO-only arithmetic build cannot select these calendars.
    const parts = lunisolar.formatToParts(Date.UTC(2000, 0, 1));
    assert(parts.find(part => part.type === 'relatedYear').value, '1999');
    assert(parts.find(part => part.type === 'month').value, '11');
    assert(parts.find(part => part.type === 'day').value, '25');
    const range = lunisolar.formatRangeToParts(Date.UTC(2000, 0, 1), Date.UTC(1900, 0, 1));
    assertArrayEquals(range.filter(part => part.type === 'relatedYear').map(part => part.value),
        ['1999', '1899']);
}

const getter = Object.getOwnPropertyDescriptor(DTF.prototype, 'format').get;
for (const receiver of [undefined, null, 1, {}, DTF.prototype, Object.create(f), new Proxy(f, {})]) {
    assertThrows(TypeError, () => getter.call(receiver));
    assertThrows(TypeError, () => DTF.prototype.resolvedOptions.call(receiver));
    assertThrows(TypeError, () => DTF.prototype.formatToParts.call(receiver, {valueOf() {throw 0;}}));
}
// Legacy chaining shares the intrinsic symbol with NumberFormat, not globals.
const legacy = Object.create(DTF.prototype);
assert(DTF.call(legacy, 'en', utc), legacy);
assert(legacy.format(0), new DTF('en', utc).format(0));
const numberLegacy = Object.create(Intl.NumberFormat.prototype);
Intl.NumberFormat.call(numberLegacy, 'en');
const key = Object.getOwnPropertySymbols(legacy)[0];
assert(key, Object.getOwnPropertySymbols(numberLegacy)[0]);
const descriptor = Object.getOwnPropertyDescriptor(legacy, key);
assert(descriptor.writable, false);
assert(descriptor.enumerable, false);
assert(descriptor.configurable, false);
assertThrows(TypeError, () => DTF.call(legacy, 'en'));
assertThrows(TypeError, () => legacy.formatToParts(0));

const optionKeys = ['localeMatcher', 'calendar', 'numberingSystem', 'hour12', 'hourCycle',
    'timeZone', 'weekday', 'era', 'year', 'month', 'day', 'dayPeriod', 'hour', 'minute',
    'second', 'fractionalSecondDigits', 'timeZoneName', 'formatMatcher', 'dateStyle', 'timeStyle'];
log = [];
const target = new Proxy(function () {}, {get(object, key) {
    if (key === 'prototype') log.push('prototype');
    return Reflect.get(object, key);
}});
Reflect.construct(DTF, [[{toString() {log.push('locale'); return 'en';}}],
    new Proxy({}, {get(object, key) {log.push(key);}})], target);
assertArrayEquals(log, ['prototype', 'locale', ...optionKeys]);

const realm = std.createRealm();
const Other = realm.evalScript('Intl.DateTimeFormat');
const other = new Other('en', utc);
assert(DTF.prototype.resolvedOptions.call(other).locale, 'en');
const foreignGetter = realm.evalScript('Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, "format").get');
const foreignBound = foreignGetter.call(new DTF('en', utc));
const ForeignTypeError = realm.evalScript('TypeError');
assertThrows(ForeignTypeError, () => foreignBound(Symbol()));
let detached;
{
    let temporary = new DTF('en', utc);
    detached = temporary.format;
    temporary = null;
}
std.gc();
assert(detached(0), new DTF('en', utc).format(0));
detached = null;
std.gc();

// Invalid Date and wrong-brand receivers fail before locales/options are read.
const poison = {get length() {throw 'locales read';}};
assert(new Date(NaN).toLocaleString(poison), 'Invalid Date');
assertThrows(TypeError, () => Date.prototype.toLocaleString.call({}, poison));
assertThrows(TypeError, () => new Date(0).toLocaleDateString('en', {timeStyle: 'short'}));
assertThrows(TypeError, () => new Date(0).toLocaleTimeString('en', {dateStyle: 'short'}));
const date = new Date(1719835200123);
const ordinary = new DTF('en', {...utc, year: 'numeric', month: 'numeric', day: 'numeric'});
assert(date.toLocaleDateString('en', utc), ordinary.format(+date));
Intl.DateTimeFormat = function () {throw 'global constructor read';};
assert(date.toLocaleDateString('en', utc), ordinary.format(+date));
Intl.DateTimeFormat = DTF;

if (typeof Temporal !== 'undefined') {
    // A date-only public view still retains the effective cycle for the
    // proposal's PlainTime record; hiding options must not erase that state.
    const dateOnly = new DTF('en', {...utc, hourCycle: 'h23'});
    assert(Object.hasOwn(dateOnly.resolvedOptions(), 'hourCycle'), false);
    assert(dateOnly.formatToParts(new Temporal.PlainTime(13, 24, 35))
        .find(part => part.type === 'hour').value, '13');
    const plain = Temporal.PlainDateTime.from('2024-07-01T12:34:56.123456789');
    assert(f.format(plain), f.format(Date.UTC(2024, 6, 1, 12, 34, 56, 123)));
    const full = new DTF('en', {...utc, dateStyle: 'full', timeStyle: 'long'});
    assert(full.format(Temporal.PlainDate.from('2024-07-01')), 'Monday, July 1, 2024');
    assertThrows(TypeError, () => new DTF('en', {...utc, year: 'numeric'}).format(plain.toPlainTime()));
    const ym = Temporal.PlainYearMonth.from('2024-07');
    const md = Temporal.PlainMonthDay.from('07-01');
    // The proposal requires exact calendars for these two reference-date types.
    assertThrows(RangeError, () => ym.toLocaleString('en', utc));
    assertThrows(RangeError, () => md.toLocaleString('en', utc));
    assert(typeof ym.toLocaleString('en-u-ca-iso8601', utc), 'string');
    assert(typeof md.toLocaleString('en', {...utc, calendar: 'iso8601'}), 'string');
    assert(plain.toLocaleString('en', {...utc, hour: 'numeric', hourCycle: 'h23'}),
        new DTF('en', {...utc, hour: 'numeric', hourCycle: 'h23'}).format(plain));
    const zoned = Temporal.ZonedDateTime.from('2024-07-01T12:00Z[UTC]');
    assertThrows(TypeError, () => f.format(zoned));
    assert(typeof zoned.toLocaleString('en'), 'string');
    assertThrows(TypeError, () => zoned.toLocaleString('en', {timeZone: {toString() {throw 0;}}}));
}

// Match the staging String/internalUsage witness on a supported locale. Its
// German exact-output variant remains part of the full native Test262 run.
{
    const value = 24 * 60 * 60 * 1000;
    const expected = new DTF('en', utc).format(value);
    const descriptor = Object.getOwnPropertyDescriptor(String.prototype, Symbol.split);
    try {
        for (const replacement of ['', 'x-foo', 'de-u-co', 'en-US']) {
            String.prototype[Symbol.split] = function() { return [replacement]; };
            assert(new DTF('en', utc).format(value), expected);
        }
    } finally {
        if (descriptor)
            Object.defineProperty(String.prototype, Symbol.split, descriptor);
        else
            delete String.prototype[Symbol.split];
    }
}
