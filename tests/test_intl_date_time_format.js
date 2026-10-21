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
    // Isolate numeric hour-cycle arithmetic from best-fit source padding.
    const formatter = new DTF('en', {...utc, hour: 'numeric', hourCycle, formatMatcher: 'basic'});
    assert(formatter.formatToParts(0).find(p => p.type === 'hour').value, text);
    assert(formatter.resolvedOptions().hourCycle, hourCycle);
}

// English's real Hms source has HH, while hms has h. Best fit keeps that
// family-specific width; Basic follows its component score, and an explicit
// two-digit request remains two-digit in either policy.
for (const hourCycle of ['h12', 'h23']) {
    for (const formatMatcher of ['basic', 'best fit']) {
        for (const fields of [
            {}, {fractionalSecondDigits: 3},
            {year: 'numeric', month: 'numeric', day: 'numeric'},
            {year: 'numeric', month: 'numeric', day: 'numeric', weekday: 'short', era: 'short', timeZoneName: 'short'},
        ]) {
            for (const hour of ['numeric', '2-digit']) {
                const formatter = new DTF('en', {...utc, ...fields, hourCycle, formatMatcher,
                    hour, minute: 'numeric', second: 'numeric'});
                const width = formatter.resolvedOptions().hour;
                const padded = width === '2-digit';
                assert(width === 'numeric' || width === '2-digit');
                if (hour === '2-digit')
                    assert(width, '2-digit');
                // Bare clocks provide both source families. Fractional/mixed
                // programs may select another truthful representative width.
                if (Object.keys(fields).length === 0)
                    assert(width, hour === '2-digit' || (formatMatcher === 'best fit' && hourCycle === 'h23')
                        ? '2-digit' : 'numeric');
                const value = Date.UTC(2024, 0, 1, 1, 23, 45, 678);
                assert(formatter.formatToParts(value).find(part => part.type === 'hour').value, padded ? '01' : '1');
                if (typeof Temporal !== 'undefined') {
                    // PlainTime selects its own clock-only record, rather
                    // than the ordinary date/clock record of resolvedOptions.
                    const plainPadded = 'fractionalSecondDigits' in fields ? padded :
                        hour === '2-digit' || (formatMatcher === 'best fit' && hourCycle === 'h23');
                    assert(formatter.formatToParts(new Temporal.PlainTime(1, 23, 45, 678))
                        .find(part => part.type === 'hour').value, plainPadded ? '01' : '1');
                }
            }
        }
    }
}
// A bare flexible period uses format-context names/rules, including exact
// noon, without importing an hour or stand-alone-context labels. Midnight
// uses the general period because the API cannot distinguish 00:00 from 24:00.
for (const dayPeriod of ['short', 'long', 'narrow']) {
    const formatter = new DTF('en', {...utc, dayPeriod});
    assert(formatter.resolvedOptions().dayPeriod, dayPeriod);
    assert(formatter.resolvedOptions().hour, undefined);
    for (const [hour, text] of [[0, 'in the morning'],
                              [12, dayPeriod === 'narrow' ? 'n' : 'noon'], [9, 'in the morning']]) {
        const parts = formatter.formatToParts(Date.UTC(2024, 0, 1, hour));
        assertArrayEquals(parts.map(part => part.type), ['dayPeriod']);
        assert(parts[0].value, text);
        assert(formatter.format(Date.UTC(2024, 0, 1, hour)), text);
    }
    assert(formatter.format(Date.UTC(2024, 0, 1, 0, 0, 0, 1)), 'in the morning');
    assert(formatter.format(Date.UTC(2024, 0, 1, 12, 0, 0, 1)), 'in the afternoon');
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

// Root ISO patterns intentionally pad numeric month/day. Native best fit keeps
// that source override rather than choosing a named month to reduce width cost.
const numericIso = new DTF('en', {...utc, calendar: 'iso8601'});
assert(numericIso.resolvedOptions().month, '2-digit');
assert(numericIso.formatToParts(Date.UTC(2024, 8, 1)).find(part => part.type === 'month').value, '09');
const basicIso = new DTF('en', {...utc, calendar: 'iso8601', formatMatcher: 'basic'});
assert(basicIso.resolvedOptions().month, 'short');
const textIso = new DTF('en', {...utc, calendar: 'iso8601', month: 'short'});
assert(textIso.formatToParts(Date.UTC(2024, 8, 1)).find(part => part.type === 'month').value, 'Sep');
const eraClock = new DTF('en', {...utc, era: 'short', year: 'numeric', month: 'numeric', day: 'numeric',
    hour: 'numeric', minute: '2-digit', second: '2-digit'});
assert(eraClock.resolvedOptions().era, 'short');
assert(eraClock.formatToParts(0).find(part => part.type === 'era').value, 'AD');

// Date-plus-zone uses the CLDR Date-Timezone layout. Plain Temporal records
// intentionally omit zones even though the ordinary Date record retains one.
for (const timeZoneName of ['short', 'long', 'shortOffset', 'longOffset', 'shortGeneric', 'longGeneric']) {
    const formatter = new DTF('en', {...utc, timeZoneName});
    assert(formatter.formatToParts(0).some(part => part.type === 'timeZoneName'), true);
    if (typeof Temporal !== 'undefined')
        assert(formatter.formatToParts(Temporal.PlainDate.from('2024-07-01'))
            .some(part => part.type === 'timeZoneName'), false);
}

// A real zone-bearing clock must also remain available when joined to a date.
// This exercises both clock families without prescribing localized punctuation.
for (const hourCycle of ['h12', 'h23']) {
    for (const formatMatcher of ['basic', 'best fit']) {
        const formatter = new DTF('en', {...utc, year: 'numeric', month: 'numeric', day: 'numeric',
            hour: 'numeric', minute: 'numeric', second: 'numeric',
            timeZoneName: 'shortGeneric', hourCycle, formatMatcher});
        const parts = formatter.formatToParts(0);
        for (const field of ['year', 'month', 'day', 'hour', 'minute', 'second', 'timeZoneName'])
            assert(parts.some(part => part.type === field));
        assert(parts.map(part => part.value).join(''), formatter.format(0));
    }
}

// Source Timezone append items also supply specific names and offsets when a
// whole matching clock is absent. Plain Temporal values still omit the zone.
for (const hourCycle of ['h12', 'h23']) {
    for (const timeZoneName of ['short', 'long', 'shortOffset', 'longOffset']) {
        for (const formatMatcher of ['basic', 'best fit']) {
            const formatter = new DTF('en', {...utc, year: 'numeric', month: 'numeric', day: 'numeric',
                hour: 'numeric', minute: 'numeric', second: 'numeric',
                timeZoneName, hourCycle, formatMatcher});
            assert(formatter.resolvedOptions().timeZoneName, timeZoneName);
            const parts = formatter.formatToParts(0);
            assert(parts.some(part => part.type === 'timeZoneName'));
            assert(parts.map(part => part.value).join(''), formatter.format(0));
            if (typeof Temporal !== 'undefined')
                assert(formatter.formatToParts(Temporal.PlainDateTime.from('1970-01-01T00:00'))
                    .some(part => part.type === 'timeZoneName'), false);
        }
    }
}

// Era belongs to the date operand, including a weekday-bearing date joined
// with a clock. Request widths honestly and retain plain Temporal filtering.
for (const hourCycle of ['h12', 'h23']) {
    for (const formatMatcher of ['basic', 'best fit']) {
        for (const era of ['short', 'long', 'narrow']) {
            const formatter = new DTF('en', {...utc, weekday: 'short', era,
                year: 'numeric', month: 'numeric', day: 'numeric',
                hour: 'numeric', minute: 'numeric', second: 'numeric',
                timeZoneName: 'short', hourCycle, formatMatcher});
            assert(formatter.resolvedOptions().era, era);
            const parts = formatter.formatToParts(0);
            assert(parts.some(part => part.type === 'era'));
            assert(parts.some(part => part.type === 'weekday'));
            assert(parts.some(part => part.type === 'timeZoneName'));
            assert(parts.map(part => part.value).join(''), formatter.format(0));
            const before = new Date(0), after = new Date(0);
            before.setUTCFullYear(0);
            after.setUTCFullYear(1);
            const eraPart = value => formatter.formatToParts(value).find(part => part.type === 'era').value;
            assert(eraPart(before) !== eraPart(after));
            if (typeof Temporal !== 'undefined') {
                for (const value of [new Temporal.PlainDate(2024, 1, 2),
                                     new Temporal.PlainDateTime(2024, 1, 2, 12, 34, 56)]) {
                    const plainParts = formatter.formatToParts(value);
                    assert(plainParts.some(part => part.type === 'era'));
                    assert(plainParts.some(part => part.type === 'timeZoneName'), false);
                }
            }
        }
    }
}

// Both real cyclic-only and numeric related-year sources exist. The native
// policy prefers a numeric year without changing U into another field kind.
for (const calendar of ['chinese', 'dangi']) {
    const lunisolar = new DTF('en', {...utc, calendar});
    if (lunisolar.resolvedOptions().calendar !== calendar) {
        // An ISO-only arithmetic build negotiates the English Gregorian
        // default, whose requested era must still be formatted normally.
        const fallback = new DTF('en', {...utc, calendar, era: 'short',
            year: 'numeric', month: 'numeric', day: 'numeric'});
        assert(fallback.resolvedOptions().calendar, 'gregory');
        assert(fallback.formatToParts(0).some(part => part.type === 'era'), true);
        continue;
    }
    // These calendars have no canonical eras. A requested era cannot invent a
    // name or make an otherwise supported date formatter unavailable.
    const noEra = new DTF('en', {...utc, calendar, era: 'short',
        year: 'numeric', month: 'numeric', day: 'numeric'}).formatToParts(0);
    assert(noEra.some(part => part.type === 'era'), false);
    const parts = lunisolar.formatToParts(Date.UTC(2000, 0, 1));
    assert(parts.find(part => part.type === 'relatedYear').value, '1999');
    assert(parts.find(part => part.type === 'month').value, '11');
    assert(parts.find(part => part.type === 'day').value, '25');
    // Numeric leap months keep the nominal number and the inherited CLDR
    // marker in one month part. The ordinal sixth month here is M05L.
    for (const month of ['numeric', '2-digit']) {
        const formatter = new DTF('en', {...utc, calendar, year: 'numeric', month, day: 'numeric'});
        const leapDate = Date.UTC(2047, 6, 22);
        const leapParts = formatter.formatToParts(leapDate);
        // Matchers may choose a different supported width. Check decoration
        // after the actual numeric-width decision rather than forcing one.
        const padded = formatter.resolvedOptions().month === '2-digit';
        assert(leapParts.filter(part => part.type === 'month').length, 1);
        assert(leapParts.find(part => part.type === 'month').value, padded ? '05bis' : '5bis');
        assert(leapParts.map(part => part.value).join(''), formatter.format(leapDate));
        if (typeof Temporal !== 'undefined') {
            const plain = Temporal.PlainDate.from('2047-07-22').withCalendar(calendar);
            assert(plain.monthCode, 'M05L');
            assert(formatter.formatToParts(plain).find(part => part.type === 'month').value, padded ? '05bis' : '5bis');
        }
    }
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
    const onlyEra = new DTF('en', {...utc, era: 'narrow'});
    assert(onlyEra.format(Temporal.PlainDateTime.from('2025-11-04T14:46')).startsWith('11'), true);
    assert(f.format(plain), f.format(Date.UTC(2024, 6, 1, 12, 34, 56, 123)));
    const full = new DTF('en', {...utc, dateStyle: 'full', timeStyle: 'long'});
    assert(full.format(Temporal.PlainDate.from('2024-07-01')), 'Monday, July 1, 2024');
    const wide = full.formatToParts(plain);
    assert(wide.find(part => part.type === 'month').value, 'July');
    assert(wide.find(part => part.type === 'weekday').value, 'Monday');
    assert(wide.some(part => part.type === 'timeZoneName'), false);
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
    const instant = zoned.toInstant();
    const noName = instant.toLocaleString('en', utc);
    const shortName = instant.toLocaleString('en', {...utc, timeZoneName: 'short'});
    const longName = instant.toLocaleString('en', {...utc, timeZoneName: 'long'});
    assert(shortName !== noName);
    assert(longName !== shortName);
    assert(zoned.toLocaleString('en') !== noName);
    assertThrows(TypeError, () => zoned.toLocaleString('en', {timeZone: {toString() {throw 0;}}}));
}

// Fixed zero offsets have GMT's names; numeric offset styles still use their
// localized wrapper. Neither name selection nor formatting changes the ID.
{
    const zonePart = (locale, timeZone, timeZoneName) => new DTF(locale, {
        timeZone, timeZoneName
    }).formatToParts(0).find(part => part.type === 'timeZoneName').value;
    assert(zonePart('en', '+00:00', 'short'), 'GMT');
    assert(zonePart('en', '+00:00', 'long'), 'Greenwich Mean Time');
    assert(zonePart('en', '+00:00', 'shortGeneric'), 'GMT');
    assert(zonePart('en', '+00:00', 'shortOffset'), 'GMT+0');
    assert(zonePart('en', '+00:00', 'longOffset'), 'GMT+00:00');
    assert(zonePart('en', '+01:00', 'short'), 'GMT+1');
    assert(zonePart('en', '-01:00', 'short'), 'GMT-1');
    assert(new DTF('en', {timeZone: '+00:00'}).resolvedOptions().timeZone, '+00:00');
    if (DTF.supportedLocalesOf('fr').length) {
        assert(zonePart('fr', '+00:00', 'short'), 'GMT');
        assert(zonePart('fr', '+00:00', 'shortOffset'), 'UTC+0');
    }
    if (typeof Temporal === 'object') {
        const fixed = new Temporal.ZonedDateTime(0n, '+00:00');
        assert(fixed.toLocaleString('en').includes('GMT'));
        assert(!fixed.toLocaleString('en').includes('+'));
        assert(fixed.timeZoneId, '+00:00');
        assert(new Temporal.ZonedDateTime(0n, '+01:00').toLocaleString('en').includes('GMT+1'));
        assert(new Temporal.ZonedDateTime(0n, '-01:00').toLocaleString('en').includes('GMT-1'));
    }
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
