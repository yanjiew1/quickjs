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

/* DurationFormat coercion, options, parts, intrinsic use, realms and GC. */
import * as std from 'std';
import {assert, assertThrows, assertArrayEquals} from './assert.js';

const DF = Intl.DurationFormat;
const fields = ['years', 'months', 'weeks', 'days', 'hours', 'minutes', 'seconds',
                'milliseconds', 'microseconds', 'nanoseconds'];
assert(DF.name, 'DurationFormat');
assert(DF.length, 0);
assertThrows(TypeError, () => DF('en'));
for (const options of [null, true, 1, 1n, '', Symbol()])
    assertThrows(TypeError, () => new DF('en', options));
const digital = new DF('en', {style: 'digital'});
for (const localeMatcher of ['lookup', 'best fit']) {
    assert(new DF({0: 'EN', length: 1}, {localeMatcher}).resolvedOptions().locale, 'en');
    assert(new DF({0: 'EN', length: 0}, {localeMatcher}).resolvedOptions().locale,
           new DF(undefined, {localeMatcher}).resolvedOptions().locale);
}
assert(Object.prototype.toString.call(digital), '[object Intl.DurationFormat]');
assert(digital.format.length, 1);
assert(digital.formatToParts.length, 1);
assert(digital.format, DF.prototype.format);
assertThrows(TypeError, () => new digital.format());
const resolved = digital.resolvedOptions();
assert(resolved.style, 'digital');
assert(resolved.numberingSystem, 'latn');
assert(resolved.hours, 'numeric');
assert(resolved.minutes, '2-digit');
assert(resolved.milliseconds, 'numeric');
assert('fractionalDigits' in resolved, false);
assertArrayEquals(Object.keys(resolved), ['locale', 'numberingSystem', 'style',
    ...fields.flatMap(name => [name, name + 'Display'])]);
for (const name of ['format', 'formatToParts', 'resolvedOptions']) {
    const descriptor = Object.getOwnPropertyDescriptor(DF.prototype, name);
    assert(descriptor.enumerable, false);
    assert(descriptor.writable, true);
    assert(descriptor.configurable, true);
    const length = Object.getOwnPropertyDescriptor(descriptor.value, 'length');
    assert(length.value, name === 'resolvedOptions' ? 0 : 1);
    assert(length.writable, false);
    assert(length.enumerable, false);
    assert(length.configurable, true);
    for (const receiver of [undefined, null, 1, {}, DF.prototype, Object.create(digital), new Proxy(digital, {})])
        assertThrows(TypeError, () => descriptor.value.call(receiver, {get seconds() {throw 'wrong order';}}));
}
assert(digital.format({hours: 1, minutes: 2, seconds: 3, nanoseconds: 1}), '1:02:03.000000001');
assert(digital.format('PT1H2M3.000000001S'), '1:02:03.000000001');
assert(digital.format({minutes: -2}), '-0:02:00');
assert(digital.format({seconds: 9007199254740991, nanoseconds: 999999999}), '0:00:9007199254740991.999999999');
assert(new DF('en', {style: 'long'}).format({years: 1, months: 2}), '1 year, 2 months');
assert(new DF('en', {style: 'digital', fractionalDigits: 3.9}).format({seconds: 1, nanoseconds: 999999999}), '0:00:01.999');
assert(new DF('en', {style: 'digital', hours: '2-digit'}).format({seconds: 1}), '00:00:01');
assert(new DF('en', {seconds: 'numeric', secondsDisplay: 'auto', fractionalDigits: 0}).format({nanoseconds: -1}), '-0');
assert(new DF('en', {milliseconds: 'long', microseconds: 'numeric'}).format({milliseconds: 1, nanoseconds: 1}), '1.000001 milliseconds');
const parts = digital.formatToParts({hours: 1, minutes: 2, seconds: 3});
assert(JSON.stringify(parts), '[{"type":"integer","value":"1","unit":"hour"},{"type":"literal","value":":"},{"type":"integer","value":"02","unit":"minute"},{"type":"literal","value":":"},{"type":"integer","value":"03","unit":"second"}]');
const text = new DF('en', {style: 'long', yearsDisplay: 'always'});
const textParts = text.formatToParts({months: -2});
assert(textParts.map(p => p.value).join(''), '-0 years, 2 months');
assert(textParts.find(p => p.type === 'minusSign').unit, 'year');
assert(textParts.find(p => p.value === ' ').unit, 'year');
assert('unit' in textParts.find(p => p.value === ', '), false);

const negativeFields = Object.fromEntries(fields.map((name, i) => [name, -(i + 1)]));
const negativeLong = new DF('en', {style: 'long'});
assert(negativeLong.format(negativeFields),
       '-1 year, 2 months, 3 weeks, 4 days, 5 hours, 6 minutes, 7 seconds, ' +
       '8 milliseconds, 9 microseconds, 10 nanoseconds');
assertArrayEquals(negativeLong.formatToParts(negativeFields).filter(p => p.type === 'minusSign')
    .map(p => p.unit), ['year']);
assert(digital.format({hours: -1, minutes: -2, seconds: -3, nanoseconds: -999999999}), '-1:02:03.999999999');
assert(digital.format({hours: -0, minutes: -0, seconds: -0}), '0:00:00');
for (const [unit, following, fraction] of [
    ['seconds', 'milliseconds', '000'],
    ['milliseconds', 'microseconds', '000'],
    ['microseconds', 'nanoseconds', '999'],
]) {
    for (const fractionalDigits of [0, 3]) {
        const formatter = new DF('en', {style: 'long', [unit]: 'long',
            [following]: 'numeric', fractionalDigits});
        const magnitude = fractionalDigits ? '1.' + fraction + ' ' + unit : '1 ' + unit.slice(0, -1);
        assert(formatter.format({[unit]: -1, nanoseconds: -999}), '-' + magnitude);
        const later = {days: -1, [unit]: -1, nanoseconds: -999};
        assert(formatter.format(later), '-1 day, ' + magnitude);
        assert(formatter.formatToParts(later).map(p => p.value).join(''), '-1 day, ' + magnitude);
    }
}
const negativeFraction = new DF('en', {style: 'digital', fractionalDigits: 3});
assert(negativeFraction.format({seconds: -1, nanoseconds: -999999999}), '-0:00:01.999');
assert(negativeFraction.format({nanoseconds: -1}), '-0:00:00.000');

const optionOrder = ['localeMatcher', 'numberingSystem', 'style',
    ...fields.flatMap(name => [name, name + 'Display']), 'fractionalDigits'];
let log = [];
new DF('en', new Proxy({}, {get(target, key) {log.push(key);}}));
assertArrayEquals(log, optionOrder);
for (let stop = 0; stop < optionOrder.length; stop++) {
    const order = [], marker = {};
    let caught;
    try {new DF('en', new Proxy({}, {get(target, key) {
        order.push(key); if (order.length === stop + 1) throw marker;
    }}));} catch (error) {caught = error;}
    assert(caught, marker);
    assertArrayEquals(order, optionOrder.slice(0, stop + 1));
}
log = [];
assertThrows(RangeError, () => new DF('en', {get years() {log.push('years'); return 'numeric';},
    get yearsDisplay() {log.push('display');}}));
assertArrayEquals(log, ['years']);
log = [];
assertThrows(RangeError, () => new DF('en', {hours: 'numeric',
    get minutes() {log.push('minutes'); return {toString() {log.push('style'); return 'long';}};},
    get minutesDisplay() {log.push('display'); return {toString() {log.push('display string'); return 'auto';}};},
    get seconds() {throw 'too late';}}));
assertArrayEquals(log, ['minutes', 'style', 'display', 'display string']);
for (const fractionalDigits of [-1, 10, NaN, Infinity])
    assertThrows(RangeError, () => new DF('en', {fractionalDigits}));
for (const value of [undefined, null, true, 1, 1n, Symbol(), {}])
    assertThrows(TypeError, () => digital.format(value));
assertThrows(RangeError, () => digital.format('invalid'));
assertThrows(RangeError, () => digital.format({seconds: 1, minutes: -1}));
assertThrows(RangeError, () => digital.format({seconds: 0.1}));
log = [];
const alphabetical = ['days', 'hours', 'microseconds', 'milliseconds', 'minutes', 'months',
                      'nanoseconds', 'seconds', 'weeks', 'years'];
digital.format(new Proxy({}, {get(target, key) {log.push(key); if (key === 'seconds')
    return {valueOf() {std.gc(); log.push('number'); return 2;}};}}));
assertArrayEquals(log, [...alphabetical.slice(0, 8), 'number', ...alphabetical.slice(8)]);
assert(new DF('en-u-nu-arab', {numberingSystem: 'latn'}).resolvedOptions().locale, 'en');
assert(new DF('en-u-nu-arab').resolvedOptions().numberingSystem, 'arab');
assert(new DF('en', {numberingSystem: 'arab', style: 'digital'}).format({seconds: 1}), '٠:٠٠:٠١');
const arabDigital = new DF('en', {numberingSystem: 'arab', style: 'digital', fractionalDigits: 3});
for (const [duration, expected] of [
    [{hours: -1, minutes: -2, seconds: -3, nanoseconds: -999999999}, '\u061c-١:٠٢:٠٣٫٩٩٩'],
    [{nanoseconds: -1}, '\u061c-٠:٠٠:٠٠٫٠٠٠'],
    [{hours: -0, minutes: -0, seconds: -0}, '٠:٠٠:٠٠٫٠٠٠'],
]) {
    assert(arabDigital.format(duration), expected);
    assert(arabDigital.formatToParts(duration).map(p => p.value).join(''), expected);
}
const arabLong = new DF('en', {numberingSystem: 'arab', style: 'long', yearsDisplay: 'always'});
assert(arabLong.format({months: -2}), '\u061c-٠ years, ٢ months');
assertArrayEquals(arabLong.formatToParts({months: -2}).filter(p => p.type === 'minusSign')
    .map(p => p.value), ['\u061c-']);
for (const numberingSystem of ['arab', 'arabext', 'beng', 'deva', 'fullwide',
                               'hanidec', 'latn', 'mathbold', 'thai'])
    assert(new DF('en', {numberingSystem}).resolvedOptions().numberingSystem, numberingSystem);
assertArrayEquals(DF.supportedLocalesOf(['EN', 'en-u-nu-arab', 'zz']), ['en', 'en-u-nu-arab']);
for (const options of [true, 1, 1n, '', Symbol()])
    assertArrayEquals(DF.supportedLocalesOf(['en'], options), ['en']);
class Sub extends DF {}
assert(new Sub('en') instanceof Sub, true);
log = [];
const newTarget = new Proxy(function Target() {}, {get(target, key, receiver) {
    if (key === 'prototype') {log.push('prototype'); return {};}
    return Reflect.get(target, key, receiver);
}});
Reflect.construct(DF, [{get length() {log.push('locales'); return 0;}},
                      {get localeMatcher() {log.push('options');}}], newTarget);
assertArrayEquals(log, ['prototype', 'locales', 'options']);
const realm = std.createRealm();
const Foreign = realm.global.Intl.DurationFormat;
const foreign = new Foreign('en', {style: 'digital'});
assert(DF.prototype.format.call(foreign, {seconds: 1}), '0:00:01');
const foreignParts = Foreign.prototype.formatToParts.call(digital, {seconds: 1});
assert(Object.getPrototypeOf(foreignParts), realm.global.Array.prototype);
assert(Object.getPrototypeOf(foreignParts[0]), realm.global.Object.prototype);
assert(Object.getPrototypeOf(Foreign.prototype.resolvedOptions.call(digital)), realm.global.Object.prototype);
let caught;
try {Foreign.prototype.format.call(digital, undefined);} catch (error) {caught = error;}
assert(caught instanceof realm.global.TypeError, true);
const Target = realm.evalScript('(function Target(){})');
Target.prototype = null;
assert(Object.getPrototypeOf(Reflect.construct(DF, ['en'], Target)), Foreign.prototype);
if (typeof Temporal !== 'undefined') {
    const duration = new Temporal.Duration(0, 0, 0, 0, 1, 2, 3);
    const negativeDuration = new Temporal.Duration(0, 0, 0, 0, 0, -2, 0, 0, 0, -1);
    Object.defineProperty(duration, 'hours', {get() {throw 'branded getter';}});
    assert(digital.format(duration), '1:02:03');
    const saved = globalThis.Intl;
    const method = DF.prototype.format;
    try {
        globalThis.Intl = null;
        DF.prototype.format = () => {throw 'mutable format';};
        assert(duration.toLocaleString('en', {style: 'digital'}), '1:02:03');
        assert(negativeDuration.toLocaleString('en', {style: 'digital', numberingSystem: 'arab',
            fractionalDigits: 3}), '\u061c-٠:٠٢:٠٠٫٠٠٠');
    } finally {globalThis.Intl = saved; DF.prototype.format = method;}
}
for (let i = 0; i < 20; i++) new DF('en', {get style() {std.gc(); return 'digital';}}).formatToParts({seconds: i});
std.gc();
assert(digital.format({seconds: 1}), '0:00:01');
