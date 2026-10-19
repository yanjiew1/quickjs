/* RelativeTimeFormat options, observable coercion, parts, realms and GC. */
import * as std from 'std';
import {assert, assertThrows, assertArrayEquals} from './assert.js';

const RTF = Intl.RelativeTimeFormat;
assert(RTF.name, 'RelativeTimeFormat');
assert(RTF.length, 0);
assert(Object.getPrototypeOf(RTF), Function.prototype);
assert(Object.getPrototypeOf(RTF.prototype), Object.prototype);
assertThrows(TypeError, () => RTF('en'));
assertThrows(TypeError, () => RTF.call({}, 'en'));
assertThrows(TypeError, () => new RTF('en', null));
for (const options of [true, 1, 1n, '', Symbol()])
    assert(new RTF('en', options).format(1, 'day'), 'in 1 day');
for (const localeMatcher of ['lookup', 'best fit']) {
    assert(new RTF({0: 'EN', length: 1}, {localeMatcher}).resolvedOptions().locale, 'en');
    assert(new RTF({0: 'EN', length: 0}, {localeMatcher}).resolvedOptions().locale,
           new RTF(undefined, {localeMatcher}).resolvedOptions().locale);
}
const rtf = new RTF('en');
assert(Object.prototype.toString.call(rtf), '[object Intl.RelativeTimeFormat]');
assert(rtf.format.length, 2);
assert(rtf.formatToParts.length, 2);
assert(rtf.format, RTF.prototype.format);
assertThrows(TypeError, () => rtf.format.call(undefined, 1, 'day'));
assertThrows(TypeError, () => new rtf.format());
assertArrayEquals(Object.keys(rtf.resolvedOptions()), ['locale', 'style', 'numeric', 'numberingSystem']);
assert(JSON.stringify(rtf.resolvedOptions()), '{"locale":"en","style":"long","numeric":"always","numberingSystem":"latn"}');
for (const name of ['format', 'formatToParts', 'resolvedOptions']) {
    const d = Object.getOwnPropertyDescriptor(RTF.prototype, name);
    assert(d.enumerable, false);
    assert(d.configurable, true);
    assert(d.writable, true);
    for (const receiver of [undefined, null, 1, {}, RTF.prototype, Object.create(rtf), new Proxy(rtf, {})])
        assertThrows(TypeError, () => d.value.call(receiver, 1, 'day'));
}
for (const key of ['style', 'numeric'])
    assertThrows(RangeError, () => new RTF('en', {[key]: 'invalid'}));
for (const numberingSystem of ['ab', 'latn\0', 'x-foo', 'a'.repeat(9)])
    assertThrows(RangeError, () => new RTF('en', {numberingSystem}));
for (const style of ['long', 'short', 'narrow']) {
    const formatter = new RTF('en', {style});
    assert(formatter.resolvedOptions().style, style);
    assert(typeof formatter.format(2, 'years'), 'string');
}
let log = [];
const options = new Proxy({}, {get(target, key) {log.push(key);}});
new RTF('en', options);
assertArrayEquals(log, ['localeMatcher', 'numberingSystem', 'style', 'numeric']);
for (let stop = 0; stop < log.length; stop++) {
    const order = [], marker = {};
    const access = new Proxy({}, {get(target, key) {order.push(key); if (order.length === stop + 1) throw marker;}});
    let caught;
    try {new RTF('en', access);} catch (e) {caught = e;}
    assert(caught, marker);
    assertArrayEquals(order, log.slice(0, stop + 1));
}
assert(rtf.format(-0, 'day'), '0 days ago');
assert(rtf.format(0, 'day'), 'in 0 days');
assert(rtf.format(1.0005, 'day'), 'in 1 day');
assert(rtf.format(-1, 'day'), '1 day ago');
const auto = new RTF('en', {numeric: 'auto'});
assert(auto.format(-1, 'day'), 'yesterday');
assert(auto.format(-0, 'day'), 'today');
assert(auto.format(1, 'day'), 'tomorrow');
assert(auto.format(1.0001, 'day'), 'in 1 day');
for (const unit of ['second', 'minute', 'hour', 'day', 'week', 'month', 'quarter', 'year']) {
    assert(rtf.format(2, unit), rtf.format(2, unit + 's'));
    const parts = rtf.formatToParts(1234.5, unit + 's');
    assert(parts.map(part => part.value).join(''), rtf.format(1234.5, unit));
    for (const part of parts) {
        assertArrayEquals(Object.keys(part), part.unit === undefined ? ['type', 'value'] : ['type', 'value', 'unit']);
        if (part.type !== 'literal') assert(part.unit, unit);
        if (part.unit !== undefined) assert(part.unit, unit);
    }
}
assert(JSON.stringify(auto.formatToParts(0, 'day')), '[{"type":"literal","value":"today"}]');
for (const method of ['format', 'formatToParts']) {
    for (const unit of [undefined, '', 'Day', 'day\0', ' day', 'dayss', 1, 1n])
        assertThrows(RangeError, () => rtf[method](1, unit));
    for (const value of [1n, Symbol()]) {
        let touched = false;
        assertThrows(TypeError, () => rtf[method](value, {toString() {touched = true; return 'day';}}));
        assert(touched, false);
    }
    assertThrows(TypeError, () => rtf[method](1, Symbol()));
    assertThrows(TypeError, () => rtf[method](NaN, Symbol()));
    for (const value of [undefined, NaN, Infinity, -Infinity]) {
        let touched = false;
        assertThrows(RangeError, () => rtf[method](value, {toString() {touched = true; return 'day';}}));
        assert(touched, true);
    }
    log = [];
    rtf[method]({[Symbol.toPrimitive](hint) {log.push(hint); return '2';}},
                {[Symbol.toPrimitive](hint) {log.push(hint); return 'days';}});
    assertArrayEquals(log, ['number', 'string']);
    const marker = {};
    let caught;
    try {rtf[method](NaN, {toString() {throw marker;}});} catch (e) {caught = e;}
    assert(caught, marker);
    log = [];
    assertThrows(TypeError, () => RTF.prototype[method].call({}, {valueOf() {log.push(1);}}, 'day'));
    assertArrayEquals(log, []);
}
assert(new RTF('en-u-nu-arab').resolvedOptions().locale, 'en-u-nu-arab');
assert(new RTF('en-u-nu-arab', {numberingSystem: 'arab'}).resolvedOptions().locale, 'en-u-nu-arab');
assert(new RTF('en-u-nu-arab', {numberingSystem: 'latn'}).resolvedOptions().locale, 'en');
assert(new RTF('en-u-nu-arab', {numberingSystem: 'unknown'}).resolvedOptions().numberingSystem, 'arab');
assert(new RTF('en-u-nu-native').resolvedOptions().numberingSystem, 'latn');
assert(new RTF('en', {numberingSystem: 'arab'}).format(1, 'day'), 'in ١ day');
assertArrayEquals(RTF.supportedLocalesOf(['EN', 'en-u-nu-arab', 'zz']), ['en', 'en-u-nu-arab']);
class Sub extends RTF {}
const sub = new Sub('en');
assert(sub instanceof Sub, true);
assert(sub.format(2, 'day'), 'in 2 days');
log = [];
const newTarget = new Proxy(function Target() {}, {
    get(target, key, receiver) {
        if (key === 'prototype') {log.push('prototype'); return {};}
        return Reflect.get(target, key, receiver);
    }
});
Reflect.construct(RTF, [{get length() {log.push('locales'); return 0;}},
                       {get localeMatcher() {log.push('options');}}], newTarget);
assertArrayEquals(log, ['prototype', 'locales', 'options']);
const prototypeError = {};
const throwingTarget = new Proxy(function Target() {}, {get() {throw prototypeError;}});
let caughtPrototype;
try {Reflect.construct(RTF, [{get length() {throw 'wrong order';}}], throwingTarget);}
catch (e) {caughtPrototype = e;}
assert(caughtPrototype, prototypeError);
const realm = std.createRealm();
const Foreign = realm.global.Intl.RelativeTimeFormat;
const foreign = new Foreign('en');
assert(RTF.prototype.format.call(foreign, 2, 'day'), 'in 2 days');
let error;
try {Foreign.prototype.format.call(rtf, 1n, 'day');} catch (e) {error = e;}
assert(error instanceof realm.global.TypeError, true);
const foreignParts = Foreign.prototype.formatToParts.call(rtf, 2, 'day');
assert(Object.getPrototypeOf(foreignParts), realm.global.Array.prototype);
assert(Object.getPrototypeOf(foreignParts[0]), realm.global.Object.prototype);
assert(Object.getPrototypeOf(Foreign.prototype.resolvedOptions.call(rtf)), realm.global.Object.prototype);
const Target = realm.evalScript('(function Target(){})');
Target.prototype = null;
const fallback = Reflect.construct(RTF, ['en'], Target);
assert(Object.getPrototypeOf(fallback), Foreign.prototype);
for (let i = 0; i < 50; i++) new RTF('en', {numeric: i % 2 ? 'auto' : 'always'}).formatToParts(i, 'day');
std.gc();
assert(rtf.format(2, 'day'), 'in 2 days');

assert(RTF.prototype.format.call(new RTF('en'),
    {valueOf() {std.gc(); assert(auto.format(-1, 'day'), 'yesterday'); return 2;}},
    {toString() {std.gc(); return 'days';}}), 'in 2 days');
