/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* NumberFormat options, exact values, bound functions, ranges, legacy and realms. */
import * as std from 'std';
import {assert, assertThrows, assertArrayEquals} from './assert.js';

const NumberFormat = Intl.NumberFormat;
assert(NumberFormat.name, 'NumberFormat');
assert(NumberFormat.length, 0);
assert(Object.getPrototypeOf(NumberFormat), Function.prototype);
assert(Object.getPrototypeOf(NumberFormat.prototype), Object.prototype);
assert(Object.prototype.toString.call(new NumberFormat('en')), '[object Intl.NumberFormat]');
assert(NumberFormat('en').format(1234.5), '1,234.5');
assert(NumberFormat.call(null, 'en').format(2), '2');
assertThrows(TypeError, () => new NumberFormat('en', null));
for (const options of [false, 1, 1n, '', Symbol()])
    assert(new NumberFormat('en', options).format(1), '1');
assertThrows(RangeError, () => new NumberFormat('invalid_tag'));
const english = new NumberFormat('en');
const bound = english.format;
assert(bound, english.format);
assert(bound.name, '');
assert(bound.length, 1);
assert(bound.prototype, undefined);
assertThrows(TypeError, () => new bound());
assert(bound.call(null, 1234.5), '1,234.5');
assert(bound.call({format() {throw 0;}}, 3), '3');
assertArrayEquals([1, 2, 3].map(bound), ['1', '2', '3']);
assert(bound(), 'NaN');
assert(bound(NaN), 'NaN');
assert(bound(Infinity), '∞');
assert(bound(-Infinity), '-∞');
assert(bound(-0), '-0');
assertThrows(TypeError, () => bound(Symbol()));
assert(bound('9007199254740993'), '9,007,199,254,740,993');
assert(bound(9007199254740993n), '9,007,199,254,740,993');
let log = [];
assert(bound({[Symbol.toPrimitive](hint) {log.push(hint); return '1.25';}}), '1.25');
assertArrayEquals(log, ['number']);
assert(new NumberFormat('en', {style: 'percent'}).format(.25), '25%');
assert(new NumberFormat('en', {style: 'currency', currency: 'uSd'}).format(12), '$12.00');
assert(new NumberFormat('en', {style: 'currency', currency: 'USD', currencySign: 'accounting'}).format(-12), '($12.00)');
assert(new NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'name'}).format(2), '2.00 US dollars');
assert(new NumberFormat('en', {style: 'unit', unit: 'meter-per-second', unitDisplay: 'long'}).format(2), '2 meters per second');
assert(new NumberFormat('en', {notation: 'scientific'}).format(1234), '1.234E3');
assert(new NumberFormat('en', {notation: 'engineering'}).format(12345), '12.345E3');

// Match ICU/CLDR's selected digits for exponents, including zero and characters
// outside the BMP. Keep exponent signs/separators and part boundaries separate.
for (const [notation, cases] of [
    ['scientific', [[1e25, '٢٥', '𝟐𝟓'], [1e-25, '٢٥', '𝟐𝟓'], [0, '٠', '𝟎']]],
    ['engineering', [[1e25, '٢٤', '𝟐𝟒'], [1e-25, '٢٧', '𝟐𝟕'], [0, '٠', '𝟎']]],
]) {
    for (const [number, arab, mathbold] of cases) {
        for (const [numberingSystem, expected] of [['arab', arab], ['mathbold', mathbold]]) {
            const f = new NumberFormat('en', {notation, numberingSystem});
            const parts = f.formatToParts(number);
            const exponent = parts.filter(p => p.type === 'exponentInteger');

            assert(parts.map(p => p.value).join(''), f.format(number));
            assert(exponent.length, 1);
            assert(exponent[0].value, expected);
            assert(parts.some(p => p.type === 'exponentMinusSign'), number > 0 && number < 1);
        }
    }
}

assert(new NumberFormat('en', {notation: 'compact'}).format(1234), '1.2K');
assert(new NumberFormat('en', {maximumFractionDigits: 0, roundingMode: 'ceil'}).format(1.1), '2');
assert(new NumberFormat('en', {minimumFractionDigits: 2, trailingZeroDisplay: 'stripIfInteger'}).format(1), '1');
for (const notation of ['standard', 'scientific', 'engineering', 'compact']) {
    for (const style of ['decimal', 'percent', 'currency', 'unit']) {
        const f = new NumberFormat('en', {notation, style, currency: 'USD', unit: 'meter-per-second'});
        for (const value of [0, -0, -1234.5, 1234567n, '9007199254740993', NaN, Infinity]) {
            const parts = f.formatToParts(value);
            assert(parts.map(p => p.value).join(''), f.format(value));
            for (const part of parts) assertArrayEquals(Object.keys(part), ['type', 'value']);
        }
        const parts = f.formatRangeToParts(1, 2);
        assert(parts.map(p => p.value).join(''), f.formatRange(1, 2));
        assert(parts.some(p => p.source === 'startRange'));
        assert(parts.some(p => p.source === 'endRange'));
        for (const part of parts) assertArrayEquals(Object.keys(part), ['type', 'value', 'source']);
    }
}
const expectedKeys = ['locale', 'numberingSystem', 'style', 'minimumIntegerDigits',
    'minimumFractionDigits', 'maximumFractionDigits', 'useGrouping', 'notation',
    'signDisplay', 'roundingIncrement', 'roundingMode', 'roundingPriority', 'trailingZeroDisplay'];
const resolved = english.resolvedOptions();
assertArrayEquals(Object.keys(resolved), expectedKeys);
assert(resolved.locale, 'en');
assert(resolved.numberingSystem, 'latn');
assert(resolved.useGrouping, 'auto');
assert(resolved.minimumFractionDigits, 0);
assert(resolved.maximumFractionDigits, 3);
assert(resolved.roundingMode, 'halfExpand');
assert(resolved.roundingPriority, 'auto');
assert(new NumberFormat('en', {useGrouping: false}).resolvedOptions().useGrouping, false);
assert(new NumberFormat('en', {maximumSignificantDigits: 4}).resolvedOptions().minimumFractionDigits, undefined);
assert(new NumberFormat('en', {notation: 'compact'}).resolvedOptions().roundingPriority, 'morePrecision');
assert(new NumberFormat('en', {style: 'currency', currency: 'JPY'}).resolvedOptions().maximumFractionDigits, 0);
assert(new NumberFormat('en', {style: 'currency', currency: 'JPY', notation: 'scientific'}).resolvedOptions().maximumFractionDigits, 3);
for (const [request, option, locale, system] of [
    ['en-u-nu-arab', undefined, 'en-u-nu-arab', 'arab'],
    ['en-u-nu-arab', 'ARAB', 'en-u-nu-arab', 'arab'],
    ['en-u-nu-arab', 'latn', 'en', 'latn'],
    ['en-u-nu-arab', 'foobar', 'en-u-nu-arab', 'arab'],
    ['en-u-nu-foobar', undefined, 'en', 'latn'],
    ['en', 'mathbold', 'en', 'mathbold']]) {
    const f = new NumberFormat(request, {numberingSystem: option});
    assert(f.resolvedOptions().locale, locale);
    assert(f.resolvedOptions().numberingSystem, system);
}
assert(new NumberFormat('en', {numberingSystem: 'mathbold'}).format(12), '𝟏𝟐');
assertArrayEquals(NumberFormat.supportedLocalesOf(['EN-u-nu-arab', 'en-x-test', 'zz']),
    ['en-u-nu-arab', 'en-x-test']);
for (const numberingSystem of ['', 'la', 'latn\0arab'])
    assertThrows(RangeError, () => new NumberFormat('en', {numberingSystem}));
assert(english.formatRange(1, 2), '1–2');
assert(english.formatRange(2, 1), '2–1');
assert(english.formatRange(1, 1), '~1');
assert(english.formatRange(Infinity, Infinity), '~∞');
assert(english.formatRangeToParts(1, 1).every(p => p.source === 'shared'));
assert(english.formatRangeToParts(1, 1)[0].type, 'approximatelySign');
for (const method of ['formatRange', 'formatRangeToParts']) {
    for (const args of [[], [1], [undefined, 1], [1, undefined]])
        assertThrows(TypeError, () => english[method](...args));
    log = [];
    assertThrows(TypeError, () => english[method]({valueOf() {log.push('start'); return 1;}}, undefined));
    assertArrayEquals(log, []);
    assertThrows(RangeError, () => english[method](NaN, {valueOf() {log.push('end'); return 1;}}));
    assertArrayEquals(log, ['end']);
    log = [];
    english[method]({[Symbol.toPrimitive](hint) {log.push('start ' + hint); return 1;}},
        {[Symbol.toPrimitive](hint) {log.push('end ' + hint); return 2;}});
    assertArrayEquals(log, ['start number', 'end number']);
}
const getter = Object.getOwnPropertyDescriptor(NumberFormat.prototype, 'format').get;
for (const receiver of [undefined, null, 1, {}, NumberFormat.prototype, new Proxy(english, {})]) {
    assertThrows(TypeError, () => getter.call(receiver));
    assertThrows(TypeError, () => NumberFormat.prototype.resolvedOptions.call(receiver));
    for (const method of ['formatToParts', 'formatRange', 'formatRangeToParts'])
        assertThrows(TypeError, () => NumberFormat.prototype[method].call(receiver, 1, 2));
}
// Normative optional legacy chaining uses intrinsic identity and fixed descriptors.
const legacy = Object.create(NumberFormat.prototype);
assert(NumberFormat.call(legacy, 'en', {style: 'percent'}), legacy);
assert(legacy.format(.25), '25%');
assert(legacy.resolvedOptions().style, 'percent');
const symbols = Object.getOwnPropertySymbols(legacy);
assert(symbols.length, 1);
assert(symbols[0].description, 'IntlLegacyConstructedSymbol');
const descriptor = Object.getOwnPropertyDescriptor(legacy, symbols[0]);
assert(descriptor.writable, false);
assert(descriptor.enumerable, false);
assert(descriptor.configurable, false);
assertThrows(TypeError, () => NumberFormat.call(legacy, 'en'));
for (const method of ['formatToParts', 'formatRange', 'formatRangeToParts'])
    assertThrows(TypeError, () => legacy[method](1, 2));
const old = Intl.NumberFormat;
Intl.NumberFormat = function () {throw 0;};
assert(legacy.format(.5), '50%');
assert(legacy.resolvedOptions().style, 'percent');
Intl.NumberFormat = old;
Object.defineProperty(NumberFormat, Symbol.hasInstance, {value() {throw 0;}, configurable: true});
assert(NumberFormat.call({}, 'en').format(1), '1');
assert(legacy.format(.5), '50%');
delete NumberFormat[Symbol.hasInstance];
class Derived extends NumberFormat {}
const derived = new Derived('en');
assert(Object.getPrototypeOf(derived), Derived.prototype);
assert(derived.format(1), '1');
const realm = std.createRealm();
const Other = realm.evalScript('Intl.NumberFormat');
const other = new Other('en');
assert(NumberFormat.prototype.resolvedOptions.call(other).locale, 'en');
assert(getter.call(other)(2), '2');
const Fallback = realm.evalScript('(function Target() {});');
Fallback.prototype = null;
const fallback = Reflect.construct(NumberFormat, ['en'], Fallback);
assert(Object.getPrototypeOf(fallback), Other.prototype);
let detached;
{
    let temporary = new NumberFormat('en');
    detached = temporary.format;
    temporary = null;
}
std.gc();
assert(detached(1234), '1,234');
detached = null;
for (let i = 0; i < 20; i++) new NumberFormat('en').format(1);
std.gc();

const OtherTypeError = realm.evalScript('TypeError');
const foreignBound = realm.evalScript('new Intl.NumberFormat("en").format');
assertThrows(OtherTypeError, () => foreignBound(Symbol()));
// A borrowed getter's realm, not the formatter instance's realm, owns the callback.
const localBound = getter.call(new Other('en'));
assertThrows(TypeError, () => localBound(Symbol()));
const foreignGetter = realm.evalScript('Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, "format").get');
const fromForeignGetter = foreignGetter.call(new NumberFormat('en'));
assertThrows(OtherTypeError, () => fromForeignGetter(Symbol()));
const constructorKeys = ['localeMatcher', 'numberingSystem', 'style', 'currency',
    'currencyDisplay', 'currencySign', 'unit', 'unitDisplay', 'notation',
    'minimumIntegerDigits', 'minimumFractionDigits', 'maximumFractionDigits',
    'minimumSignificantDigits', 'maximumSignificantDigits', 'roundingIncrement',
    'roundingMode', 'roundingPriority', 'trailingZeroDisplay', 'compactDisplay',
    'useGrouping', 'signDisplay'];
log = [];
const target = new Proxy(function () {}, {get(object, key) {
    if (key === 'prototype') log.push('prototype');
    return Reflect.get(object, key);
}});
Reflect.construct(NumberFormat, [[{toString() {log.push('locale'); return 'en';}}],
    new Proxy({}, {get(object, key) {log.push(key);}})], target);
assertArrayEquals(log, ['prototype', 'locale', ...constructorKeys]);

assert((1234.5).toLocaleString('en'), '1,234.5');
assert((1234n).toLocaleString('en'), '1,234');
assert(Object(1234n).toLocaleString('en'), '1,234');
assert(new Number(1234.5).toLocaleString('en'), '1,234.5');
assert((9007199254740993n).toLocaleString('en'), '9,007,199,254,740,993');
assert((-0).toLocaleString('en'), '-0');
assert((.25).toLocaleString('en', {style: 'percent'}), '25%');
const numberLocale = Number.prototype.toLocaleString;
const bigintLocale = BigInt.prototype.toLocaleString;
for (const [method, receiver] of [[numberLocale, 1n], [numberLocale, {}],
    [bigintLocale, 1], [bigintLocale, {}]]) {
    log = [];
    assertThrows(TypeError, () => method.call(receiver, {get length() {log.push('locale');}}));
    assertArrayEquals(log, []);
}
Intl.NumberFormat = function () {throw 0;};
Intl.NumberFormat.prototype = {format() {throw 0;}};
assert((1234).toLocaleString('en'), '1,234');
assert((1234n).toLocaleString('en'), '1,234');
Intl.NumberFormat = NumberFormat;
