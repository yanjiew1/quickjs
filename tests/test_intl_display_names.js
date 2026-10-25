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

/* DisplayNames coercion, fallback, descriptors, reentrancy and realms. */
import * as std from 'std';
import {assert, assertThrows, assertArrayEquals} from './assert.js';

const DN = Intl.DisplayNames;
assert(DN.name, 'DisplayNames');
assert(DN.length, 2);
assert(DN.supportedLocalesOf.length, 1);
assert(Object.getPrototypeOf(DN), Function.prototype);
assert(Object.getPrototypeOf(DN.prototype), Object.prototype);
assertThrows(TypeError, () => DN('en', {type: 'language'}));
for (const options of [undefined, null, true, 1, 1n, '', Symbol()])
    assertThrows(TypeError, () => new DN('en', options));
assertThrows(TypeError, () => new DN('en', {}));
assertThrows(TypeError, () => new DN('en', {type: undefined}));
assertThrows(RangeError, () => new DN('bad_tag'));
for (const key of ['localeMatcher', 'style', 'type', 'fallback', 'languageDisplay'])
    assertThrows(RangeError, () => new DN('en', {type: 'region', [key]: 'invalid'}));
for (const options of [true, 1, 1n, '', Symbol()])
    assertArrayEquals(DN.supportedLocalesOf('EN', options), ['en']);
assertThrows(TypeError, () => DN.supportedLocalesOf('en', null));
assertArrayEquals(DN.supportedLocalesOf(['EN', 'en-u-ca-gregory', 'EN']), ['en', 'en-u-ca-gregory']);

const dn = new DN('en-u-ca-gregory', {type: 'language'});
assert(Object.prototype.toString.call(dn), '[object Intl.DisplayNames]');
assert(dn.of.length, 1);
assert(dn.resolvedOptions.length, 0);
assertArrayEquals(Object.keys(dn.resolvedOptions()), ['locale', 'style', 'type', 'fallback', 'languageDisplay']);
assert(JSON.stringify(dn.resolvedOptions()),
       '{"locale":"en","style":"long","type":"language","fallback":"code","languageDisplay":"dialect"}');
for (const method of ['of', 'resolvedOptions']) {
    const descriptor = Object.getOwnPropertyDescriptor(DN.prototype, method);
    assert(descriptor.enumerable, false);
    assert(descriptor.writable, true);
    assert(descriptor.configurable, true);
    assertThrows(TypeError, () => new descriptor.value());
    for (const receiver of [undefined, null, 1, {}, DN.prototype, Object.create(dn), new Proxy(dn, {})]) {
        let touched = false;
        assertThrows(TypeError, () => descriptor.value.call(receiver, {toString() {touched = true; return 'en';}}));
        assert(touched, false);
    }
}
const tag = Object.getOwnPropertyDescriptor(DN.prototype, Symbol.toStringTag);
assert(tag.value, 'Intl.DisplayNames');
assert(tag.writable, false);
assert(tag.enumerable, false);
assert(tag.configurable, true);
for (const receiver of [Intl, DN, new Intl.Locale('en'), new Intl.ListFormat('en'),
                        new Intl.PluralRules('en'), new Intl.NumberFormat('en'),
                        new Intl.RelativeTimeFormat('en'), new Intl.DurationFormat('en')]) {
    assertThrows(TypeError, () => DN.prototype.resolvedOptions.call(receiver));
    assertThrows(TypeError, () => DN.prototype.of.call(receiver, 'en'));
}

let log = [];
new DN('en', new Proxy({}, {get(target, key) {log.push(key); return key === 'type' ? 'region' : undefined;}}));
assertArrayEquals(log, ['localeMatcher', 'style', 'type', 'fallback', 'languageDisplay']);
for (let stop = 0; stop < log.length; stop++) {
    const order = [], marker = {};
    const options = new Proxy({}, {get(target, key) {
        order.push(key);
        if (order.length === stop + 1) throw marker;
        return key === 'type' ? 'region' : undefined;
    }});
    let caught;
    try {new DN('en', options);} catch (e) {caught = e;}
    assert(caught, marker);
    assertArrayEquals(order, log.slice(0, stop + 1));
}
const newTarget = new Proxy(function Target() {}, {get(target, key, receiver) {
    if (key === 'prototype') {log.push('prototype'); return {};}
    return Reflect.get(target, key, receiver);
}});
log = [];
Reflect.construct(DN, [{get length() {log.push('locales'); return 0;}},
                      {get localeMatcher() {log.push('options');}, type: 'region'}], newTarget);
assertArrayEquals(log, ['prototype', 'locales', 'options']);

const codes = {language: 'en', region: 'US', script: 'Latn', currency: 'USD', calendar: 'gregory', dateTimeField: 'year'};
const names = {language: 'English', region: 'United States', script: 'Latin', currency: 'US Dollar',
               calendar: 'Gregorian Calendar', dateTimeField: 'year'};
for (const type of Object.keys(codes)) {
    const formatter = new DN('en', {type});
    assert(formatter.of(codes[type]), names[type]);
    assert(formatter.resolvedOptions().type, type);
    assertArrayEquals(Object.keys(formatter.resolvedOptions()),
                      type === 'language' ? ['locale', 'style', 'type', 'fallback', 'languageDisplay'] :
                                            ['locale', 'style', 'type', 'fallback']);
    assertThrows(TypeError, () => formatter.of(Symbol()));
    const marker = {};
    let caught;
    try {formatter.of({toString() {throw marker;}});} catch (e) {caught = e;}
    assert(caught, marker);
}
assert(dn.of('EN-gb'), 'British English');
assert(new DN('en', {type: 'language', languageDisplay: 'standard'}).of('EN-gb'), 'English (United Kingdom)');
assert(dn.of('iw'), dn.of('he'));
assert(dn.of('en-Qaaa-QQ'), 'English (Qaaa, QQ)');
assert(new DN('en', {type: 'language', fallback: 'none'}).of('en-Qaaa-QQ'), 'English (Qaaa, QQ)');
for (const style of ['short', 'narrow']) {
    assert(new DN('en', {type: 'region', style}).of('us'), 'US');
    assert(new DN('en', {type: 'script', style}).of('lAtN'), 'Latin');
}
assert(new DN('en', {type: 'script'}).of('hans'), 'Simplified Han');
assert(new DN('en', {type: 'region'}).of(419), 'Latin America');
assert(new DN('en', {type: 'currency'}).of('usd'), 'US Dollar');
assert(new DN('en', {type: 'calendar'}).of('ETHIOAA'), 'Ethiopic Amete Alem Calendar');
assert(new DN('en', {type: 'calendar'}).of('islamicc'), 'Hijri Calendar (tabular, civil epoch)');
for (const [type, code, canonical] of [
    ['language', 'ZZ-qaAa-qq', 'zz-Qaaa-QQ'], ['region', 'qq', 'QQ'],
    ['script', 'qaAa', 'Qaaa'], ['currency', 'qqq', 'QQQ'], ['calendar', 'UNKnown', 'unknown']
]) {
    assert(new DN('en', {type}).of(code), canonical);
    assert(new DN('en', {type, fallback: 'none'}).of(code), undefined);
}
const invalid = {
    language: ['', 'root', 'Latn', 'en_US', 'en-u-ca-gregory', 'en-x-test', 'en-1901-1901', 'en\0'],
    region: ['', 'USA', '1', '1234', 'us\0'],
    script: ['', 'Latin', 'L4tn', 'Latn\0'],
    currency: ['', 'US', 'USDD', 'U$D', 'USD\0'],
    calendar: ['', 'ab', 'gregorian', 'islamic_civil', 'foo--bar', 'foo\0'],
    dateTimeField: ['', 'week', 'WeekOfYear', 'dayperiod', 'zone', 'year\0']
};
for (const type of Object.keys(invalid)) {
    const formatter = new DN('en', {type});
    for (const code of invalid[type])
        assertThrows(RangeError, () => formatter.of(code));
}
const fields = ['era', 'year', 'quarter', 'month', 'weekOfYear', 'weekday', 'day', 'dayPeriod',
                'hour', 'minute', 'second', 'timeZoneName'];
for (const style of ['long', 'short', 'narrow']) {
    const formatter = new DN('en', {type: 'dateTimeField', style});
    for (const code of fields)
        assert(typeof formatter.of(code), 'string');
}
assert(new DN('en', {type: 'dateTimeField', style: 'short'}).of('year'), 'yr.');
assert(new DN('en', {type: 'dateTimeField', style: 'narrow'}).of('year'), 'yr');
class Derived extends DN {}
const derived = new Derived('en', {type: 'region'});
assert(Object.getPrototypeOf(derived), Derived.prototype);
assert(derived.of('US'), 'United States');
const realm = std.createRealm();
const Foreign = realm.global.Intl.DisplayNames;
const foreign = new Foreign('en', {type: 'region'});
assert(DN.prototype.of.call(foreign, 'US'), 'United States');
assert(Object.getPrototypeOf(Foreign.prototype.resolvedOptions.call(dn)), realm.global.Object.prototype);
let error;
try {Foreign.prototype.of.call(dn, Symbol());} catch (e) {error = e;}
assert(error instanceof realm.global.TypeError, true);
const Target = realm.evalScript('(function Target(){})');
Target.prototype = null;
assert(Object.getPrototypeOf(Reflect.construct(DN, ['en', {type: 'language'}], Target)), Foreign.prototype);
assert(dn.of({[Symbol.toPrimitive](hint) {assert(hint, 'string'); std.gc(); return 'en';}}), 'English');
for (let i = 0; i < 25; i++) new DN('en', {type: i % 2 ? 'language' : 'calendar'}).of(i % 2 ? 'en' : 'gregory');
std.gc();
assert(dn.of('en'), 'English');
