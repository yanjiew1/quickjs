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

/* Shared SetNumberFormatDigitOptions tests. The C driver exposes only this
 * internal operation, so services need not be advertised before completion. */
function assert(value, expected) {
    if (arguments.length < 2)
        expected = true;
    if (!Object.is(value, expected))
        throw new Error(`expected ${String(expected)}, got ${String(value)}`);
}
function throws(type, fn, exact) {
    try {
        fn();
    } catch (error) {
        assert(error instanceof type);
        if (exact !== undefined)
            assert(error, exact);
        return;
    }
    throw new Error(`missing ${type.name}`);
}
function read(options = {}, minimum = 0, maximum = 3, compact = false) {
    return readDigitOptions(options, minimum, maximum, compact);
}
function check(actual, expected) {
    for (const key of Object.keys(expected))
        assert(actual[key], expected[key]);
}
const fraction = 0, significant = 1, more = 2, less = 3;
check(read(), { minimumIntegerDigits: 1, minimumFractionDigits: 0,
    maximumFractionDigits: 3, minimumSignificantDigits: undefined,
    maximumSignificantDigits: undefined, roundingIncrement: 1,
    roundingMode: 6, roundingType: fraction, trailingZeroDisplay: 0,
    computedRoundingPriority: 0 });
check(read({}, 0, 3, true), { minimumFractionDigits: 0, maximumFractionDigits: 0,
    minimumSignificantDigits: 1, maximumSignificantDigits: 2,
    roundingType: more, computedRoundingPriority: 1 });
check(read({ minimumFractionDigits: 2 }, 0, 3, true), {
    minimumFractionDigits: 2, maximumFractionDigits: 3, roundingType: fraction });
check(read({ maximumSignificantDigits: 4 }), {
    minimumFractionDigits: undefined, maximumFractionDigits: undefined,
    minimumSignificantDigits: 1, maximumSignificantDigits: 4, roundingType: significant });
for (const [priority, type, computed] of [
    ['morePrecision', more, 1], ['lessPrecision', less, 2]]) {
    check(read({ roundingPriority: priority }), { minimumFractionDigits: 0,
        maximumFractionDigits: 3, minimumSignificantDigits: 1,
        maximumSignificantDigits: 21, roundingType: type, computedRoundingPriority: computed });
}
check(read({ maximumFractionDigits: 0 }, 2, 2), {
    minimumFractionDigits: 0, maximumFractionDigits: 0 });
check(read({ minimumFractionDigits: 5 }), { maximumFractionDigits: 5 });
check(read({ minimumIntegerDigits: true, minimumFractionDigits: null,
    maximumFractionDigits: '2' }), { minimumIntegerDigits: 1,
    minimumFractionDigits: 0, maximumFractionDigits: 2 });
check(read({ minimumFractionDigits: { valueOf() { return '1.9'; } } }), {
    minimumFractionDigits: 1 });
check(read({ minimumIntegerDigits: 1.9, minimumFractionDigits: 2.9,
    roundingIncrement: 2.9 }), { minimumIntegerDigits: 1,
    minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 2 });
throws(RangeError, () => read({ minimumIntegerDigits: 21.1 }));
throws(RangeError, () => read({ maximumFractionDigits: 100.1 }));
throws(RangeError, () => read({ minimumFractionDigits: 3, maximumFractionDigits: 2 }));
throws(RangeError, () => read({ minimumSignificantDigits: 3, maximumSignificantDigits: 2 }));
for (const value of [NaN, Infinity, -Infinity, -1, 101, Symbol('digits'), 2n])
    throws(typeof value === 'symbol' || typeof value === 'bigint' ? TypeError : RangeError,
        () => read({ minimumFractionDigits: value }));
for (const increment of [1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000])
    check(read({ roundingIncrement: increment }), { roundingIncrement: increment,
        maximumFractionDigits: increment === 1 ? 3 : 0 });
for (const increment of [0, 3, 4999, 5001, NaN])
    throws(RangeError, () => read({ roundingIncrement: increment }));
throws(TypeError, () => read({ roundingIncrement: 5, maximumSignificantDigits: 3 }));
throws(TypeError, () => read({ roundingIncrement: 5, roundingPriority: 'morePrecision' }));
throws(RangeError, () => read({ roundingIncrement: 5, minimumFractionDigits: 1,
    maximumFractionDigits: 2 }));
for (const [index, mode] of ['ceil', 'floor', 'expand', 'trunc', 'halfCeil', 'halfFloor',
    'halfExpand', 'halfTrunc', 'halfEven'].entries())
    assert(read({ roundingMode: mode }).roundingMode, index);
assert(read({ trailingZeroDisplay: 'stripIfInteger' }).trailingZeroDisplay, 1);
for (const key of ['roundingMode', 'roundingPriority', 'trailingZeroDisplay']) {
    for (const value of ['', 'invalid', 'auto\0'])
        throws(RangeError, () => read({ [key]: value }));
    throws(TypeError, () => read({ [key]: Symbol() }));
}

const keys = ['minimumIntegerDigits', 'minimumFractionDigits', 'maximumFractionDigits',
    'minimumSignificantDigits', 'maximumSignificantDigits', 'roundingIncrement',
    'roundingMode', 'roundingPriority', 'trailingZeroDisplay'];
const witness = new Error('abrupt');
for (const fail of keys) {
    const log = [];
    const options = new Proxy({}, { get(target, key) {
        log.push(key);
        if (key === fail) throw witness;
    } });
    throws(Error, () => read(options), witness);
    assert(log.join(), keys.slice(0, keys.indexOf(fail) + 1).join());
}
{
    const log = [];
    const raw = {};
    for (const key of keys.slice(0, 6)) {
        raw[key] = { [Symbol.toPrimitive](hint) {
            assert(hint, 'number');
            log.push('convert ' + key);
            return key === 'roundingIncrement' ? 1 : key.endsWith('IntegerDigits') ? 1 : 2;
        } };
    }
    raw.roundingPriority = { [Symbol.toPrimitive](hint) {
        assert(hint, 'string'); log.push('convert roundingPriority'); return 'morePrecision';
    } };
    const options = new Proxy(raw, { get(target, key) { log.push('get ' + key); return target[key]; } });
    read(options);
    assert(log.join(), ['get minimumIntegerDigits', 'convert minimumIntegerDigits',
        ...keys.slice(1, 6).map(key => 'get ' + key), 'convert roundingIncrement',
        'get roundingMode', 'get roundingPriority', 'convert roundingPriority',
        'get trailingZeroDisplay', 'convert minimumSignificantDigits',
        'convert maximumSignificantDigits', 'convert minimumFractionDigits',
        'convert maximumFractionDigits'].join());
}
{
    const poison = { valueOf() { throw witness; } };
    check(read({ minimumFractionDigits: poison, maximumFractionDigits: Symbol(),
        minimumSignificantDigits: 2 }), { roundingType: significant });
    const log = [];
    const options = new Proxy({ minimumIntegerDigits: poison }, {
        get(target, key) { log.push(key); return target[key]; }
    });
    throws(Error, () => read(options), witness);
    assert(log.join(), 'minimumIntegerDigits');
    const incrementLog = [];
    throws(RangeError, () => read(new Proxy({ roundingIncrement: 3 }, {
        get(target, key) { incrementLog.push(key); return target[key]; }
    })));
    assert(incrementLog.join(), keys.slice(0, 6).join());
}
{
    const conversionOrder = ['minimumSignificantDigits', 'maximumSignificantDigits',
        'minimumFractionDigits', 'maximumFractionDigits'];
    for (const fail of conversionOrder) {
        const log = [];
        const raw = { roundingPriority: 'morePrecision' };
        for (const key of conversionOrder) {
            raw[key] = { valueOf() {
                log.push('convert ' + key);
                if (key === fail) throw witness;
                return 2;
            } };
        }
        const options = new Proxy(raw, {
            get(target, key) { log.push('get ' + key); return target[key]; }
        });
        throws(Error, () => read(options), witness);
        assert(log.join(), [...keys.map(key => 'get ' + key),
            ...conversionOrder.slice(0, conversionOrder.indexOf(fail) + 1)
                .map(key => 'convert ' + key)].join());
    }
}
