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

/* Collator options/coercion, intrinsic consumers, cached functions and realms. */
import * as std from 'std';
import {assert, assertThrows, assertArrayEquals} from './assert.js';

const Collator = Intl.Collator;
const prototype = Collator.prototype;
const getter = Object.getOwnPropertyDescriptor(prototype, 'compare').get;
assert(Collator.name, 'Collator');
assert(Collator.length, 0);
assert(Object.getPrototypeOf(Collator), Function.prototype);
assert(Object.getPrototypeOf(prototype), Object.prototype);
assert(Object.prototype.toString.call(new Collator('en')), '[object Intl.Collator]');
assert(Object.getOwnPropertyDescriptor(Collator, 'prototype').writable, false);
assert(getter.length, 0);
assert(getter.name, 'get compare');
assert(Object.getOwnPropertyDescriptor(prototype, 'compare').set, undefined);
assert(Collator('en').compare('a', 'b') < 0);
assert(Collator.call(null, 'en').compare('a', 'b') < 0);
assertThrows(TypeError, () => new Collator('en', null));
for (const options of [false, 1, 1n, '', Symbol()])
    assert(new Collator('en', options).compare('a', 'b') < 0);
assertThrows(RangeError, () => new Collator('invalid_tag'));
for (const collation of ['', 'ab', 'phonebook', 'emoji\0junk'])
    assertThrows(RangeError, () => new Collator('en', {collation}));
for (const collation of ['foobar', 'standard', 'search'])
    assert(new Collator('en', {collation}).resolvedOptions().collation, 'default');

const ordinary = new Collator('en');
const compare = ordinary.compare;
assert(compare, ordinary.compare);
assert(compare.name, '');
assert(compare.length, 2);
assert(compare.prototype, undefined);
assert(Object.getPrototypeOf(compare), Function.prototype);
assertThrows(TypeError, () => new compare());
assert(compare.call({compare() {throw 0;}}, 'a', 'b') < 0);
assertArrayEquals(['c', 'a', 'b'].sort(compare), ['a', 'b', 'c']);
assert(compare(), 0);
assert(compare('undefined'), 0);
assertThrows(TypeError, () => compare(Symbol(), 'x'));
assertThrows(TypeError, () => compare('x', Symbol()));
assert(compare('é', 'e\u0301'), 0);
assert(compare('a\0b', 'a') > 0);
assert(compare('\ud800', '\ud801') < 0);
const prefix = 'x'.repeat(512);
assert(compare(prefix + 'é', prefix + 'e\u0301'), 0);

const keys = ['locale', 'usage', 'sensitivity', 'ignorePunctuation', 'collation', 'numeric', 'caseFirst'];
assertArrayEquals(Object.keys(ordinary.resolvedOptions()), keys);
assert(ordinary.resolvedOptions().locale, 'en');
assert(ordinary.resolvedOptions().usage, 'sort');
assert(ordinary.resolvedOptions().sensitivity, 'variant');
assert(ordinary.resolvedOptions().collation, 'default');
assert(new Collator('en', {sensitivity: 'base'}).compare('e', 'é'), 0);
assert(new Collator('en', {sensitivity: 'base'}).compare('e', 'E'), 0);
assert(new Collator('en', {sensitivity: 'accent'}).compare('e', 'é') < 0);
assert(new Collator('en', {sensitivity: 'accent'}).compare('e', 'E'), 0);
assert(new Collator('en', {sensitivity: 'case'}).compare('e', 'é'), 0);
assert(new Collator('en', {sensitivity: 'case', caseFirst: 'upper'}).compare('e', 'E') > 0);
assert(new Collator('en', {caseFirst: 'lower'}).compare('e', 'E') < 0);
assert(new Collator('en', {ignorePunctuation: true}).compare('a-b', 'ab'), 0);
assert(new Collator('en', {ignorePunctuation: true}).compare('a$b', 'ab') !== 0);
assert(new Collator('en', {numeric: true}).compare('2', '10') < 0);
assert(new Collator('en', {numeric: false}).compare('2', '10') > 0);
assert(new Collator('en', {numeric: true}).compare('02', '2'), 0);
assert(new Collator('en', {usage: 'search', sensitivity: 'base'}).compare('L·', 'L') > 0);
assert(new Collator('en', {sensitivity: 'base'}).compare('L·', 'L'), 0);

for (const [request, options, locale, collation, numeric, caseFirst] of [
    ['en-u-co-emoji-kf-upper-kn', {}, 'en-u-co-emoji-kf-upper-kn', 'emoji', true, 'upper'],
    ['en-u-co-emoji', {collation: 'EMOJI'}, 'en-u-co-emoji', 'emoji', false, 'false'],
    ['en-u-co-emoji', {collation: 'foobar'}, 'en-u-co-emoji', 'emoji', false, 'false'],
    ['en-u-co-phonebk', {collation: 'pinyin'}, 'en', 'default', false, 'false'],
    ['en-u-co-emoji', {collation: 'eor'}, 'en', 'eor', false, 'false'],
    ['en-u-kn', {numeric: false}, 'en', 'default', false, 'false'],
    ['en-u-kn-false', {numeric: false}, 'en-u-kn-false', 'default', false, 'false'],
    ['en-u-kf-lower', {caseFirst: 'upper'}, 'en', 'default', false, 'upper'],
    ['en-u-co-emoji', {usage: 'search'}, 'en', 'default', false, 'false'],
]) {
    const resolved = new Collator(request, options).resolvedOptions();
    assert(resolved.locale, locale);
    assert(resolved.collation, collation);
    assert(resolved.numeric, numeric);
    assert(resolved.caseFirst, caseFirst);
}
assertArrayEquals(Collator.supportedLocalesOf(['EN-u-co-emoji', 'en-x-test', 'qaa']),
    ['en-u-co-emoji', 'en-x-test']);

// Numeric is ToBoolean, not ToString or ToPrimitive. Getter order is part of
// the contract even when options later resolve to an implementation default.
let log = [];
const numericObject = {toString() {throw 0;}, valueOf() {throw 0;}};
const options = new Proxy({}, {get(target, key) {
    log.push(key);
    return key === 'numeric' ? numericObject : undefined;
}});
assert(new Collator('en', options).resolvedOptions().numeric, true);
assertArrayEquals(log, ['usage', 'localeMatcher', 'collation', 'numeric', 'caseFirst',
                       'sensitivity', 'ignorePunctuation']);
log = [];
const requested = {get length() {log.push('length'); return 1;},
    get 0() {log.push('locale'); return {toString() {log.push('tag'); return 'en';}};}};
const Target = new Proxy(function Target() {}, {get(target, key, receiver) {
    if (key === 'prototype') log.push('prototype');
    return Reflect.get(target, key, receiver);
}});
Reflect.construct(Collator, [requested, options], Target);
assertArrayEquals(log, ['prototype', 'length', 'locale', 'tag', 'usage', 'localeMatcher',
                       'collation', 'numeric', 'caseFirst', 'sensitivity', 'ignorePunctuation']);

const marker = {};
function throwsMarker(callback) {
    let caught;
    try {callback();} catch (error) {caught = error;}
    assert(caught, marker);
}
for (const key of ['usage', 'localeMatcher', 'collation', 'numeric', 'caseFirst',
                   'sensitivity', 'ignorePunctuation']) {
    const names = ['usage', 'localeMatcher', 'collation', 'numeric', 'caseFirst',
                   'sensitivity', 'ignorePunctuation'];
    log = [];
    throwsMarker(() => new Collator('en', new Proxy({}, {get(target, name) {
        log.push(name);
        if (name === key) throw marker;
        return undefined;
    }})));
    assertArrayEquals(log, names.slice(0, names.indexOf(key) + 1));
}
log = [];
assert(compare({toString() {log.push('x'); return 'e\u0301';}},
               {toString() {log.push('y'); std.gc(); return 'é';}}), 0);
assertArrayEquals(log, ['x', 'y']);
log = [];
throwsMarker(() => compare({toString() {throw marker;}}, {toString() {log.push('y'); return '';}}));
assertArrayEquals(log, []);

for (const receiver of [undefined, null, 1, {}, prototype, new Proxy(ordinary, {})]) {
    assertThrows(TypeError, () => getter.call(receiver));
    assertThrows(TypeError, () => prototype.resolvedOptions.call(receiver));
}
// Collator does not implement NumberFormat's legacy chaining behavior.
const legacy = Object.create(prototype);
Object.defineProperty(Collator, Symbol.hasInstance, {value() {throw 0;}, configurable: true});
assert(Collator.call(legacy, 'en') !== legacy);
delete Collator[Symbol.hasInstance];
class Derived extends Collator {}
const derived = new Derived('en');
assert(Object.getPrototypeOf(derived), Derived.prototype);
assert(derived.compare('a', 'b') < 0);

// String.localeCompare coerces both strings before constructor options, and
// calls private intrinsics even when exposed namespace/getter properties change.
log = [];
String.prototype.localeCompare.call({toString() {log.push('left'); return 'a';}},
    {toString() {log.push('right'); return 'b';}}, 'en', options);
assertArrayEquals(log, ['left', 'right', 'usage', 'localeMatcher', 'collation',
                       'numeric', 'caseFirst', 'sensitivity', 'ignorePunctuation']);
const compareDescriptor = Object.getOwnPropertyDescriptor(prototype, 'compare');
try {
    Intl.Collator = function () {throw marker;};
    Object.defineProperty(prototype, 'compare', {get() {throw marker;}, configurable: true});
    assert('2'.localeCompare('10', 'en', {numeric: true}) < 0);
} finally {
    Intl.Collator = Collator;
    Object.defineProperty(prototype, 'compare', compareDescriptor);
}
assertThrows(TypeError, () => String.prototype.localeCompare.call(null, 'a'));
assertThrows(TypeError, () => 'a'.localeCompare(Symbol()));

assertArrayEquals(new Intl.Locale('en').getCollations(), ['emoji', 'eor']);
assertArrayEquals(new Intl.Locale('qaa').getCollations(), ['emoji', 'eor']);
assertArrayEquals(new Intl.Locale('en-u-co-foobar').getCollations(), ['foobar']);
const types = Intl.supportedValuesOf('collation');
assert(types.includes('emoji') && types.includes('eor'));
for (const collation of new Intl.Locale('en').getCollations())
    assert(new Collator('en', {collation}).resolvedOptions().collation, collation);
if (Collator.supportedLocalesOf('de').length) {
    assert(new Collator('de', {sensitivity: 'base', collation: 'phonebk'}).compare('ä', 'ae'), 0);
    assert(new Intl.Locale('de').getCollations().includes('phonebk'));
    assert(new Collator('sv', {sensitivity: 'base'}).compare('ä', 'z') > 0);
    assertArrayEquals(['AE', 'Ä'].sort(new Collator('de', {usage: 'search'}).compare), ['AE', 'Ä']);
    assert(new Collator('de-u-co-phonebk', {collation: 'pinyin'}).resolvedOptions().locale,
        'de-u-co-phonebk');
}
// Defaults follow the resolved locale, including the English-only profile's
// fallback; the requested Thai name alone must not force Thai punctuation.
assert(new Collator('en').resolvedOptions().ignorePunctuation, false);
const thai = new Collator('th').resolvedOptions();
assert(thai.ignorePunctuation, Collator.supportedLocalesOf('th').length > 0);

const realm = std.createRealm();
const Other = realm.evalScript('Intl.Collator');
const OtherTypeError = realm.evalScript('TypeError');
const OtherFunctionPrototype = realm.evalScript('Function.prototype');
const foreignGetter = realm.evalScript('Object.getOwnPropertyDescriptor(Intl.Collator.prototype, "compare").get');
assert(foreignGetter.call(ordinary), compare);
const foreignCompare = foreignGetter.call(new Collator('en'));
assert(Object.getPrototypeOf(foreignCompare), OtherFunctionPrototype);
assertThrows(OtherTypeError, () => foreignCompare(Symbol()));
const localCompare = getter.call(new Other('en'));
assertThrows(TypeError, () => localCompare(Symbol()));
const Fallback = realm.evalScript('(function Target() {})');
Fallback.prototype = null;
assert(Object.getPrototypeOf(Reflect.construct(Collator, ['en'], Fallback)), Other.prototype);

let retained;
{
    let temporary = new Collator('en');
    retained = temporary.compare;
    temporary = null;
}
std.gc();
assert(retained('a', 'b') < 0);
retained = null;
for (let i = 0; i < 20; i++) new Collator('en').compare;
std.gc();
