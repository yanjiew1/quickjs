/* SetNumberFormatUnitOptions is exercised without advertising a partial API. */
function assert(value, expected) {
    if (arguments.length < 2) expected = true;
    if (!Object.is(value, expected))
        throw new Error(`expected ${String(expected)}, got ${String(value)}`);
}
function throws(type, fn, exact) {
    try { fn(); } catch (error) {
        assert(error instanceof type);
        if (exact !== undefined) assert(error, exact);
        return;
    }
    throw new Error(`missing ${type.name}`);
}
const read = readNumberUnitOptions;
assert(JSON.stringify(read({})), '{"style":"decimal"}');
assert(JSON.stringify(read({style: 'percent'})), '{"style":"percent"}');
assert(JSON.stringify(read({style: 'currency', currency: 'uSd'})),
    '{"style":"currency","currency":"USD","currencyDisplay":"symbol","currencySign":"standard"}');
assert(JSON.stringify(read({style: 'unit', unit: 'meter'})),
    '{"style":"unit","unit":"meter","unitDisplay":"short"}');
for (const currencyDisplay of ['symbol', 'narrowSymbol', 'code', 'name'])
    assert(read({style: 'currency', currency: 'xxx', currencyDisplay}).currencyDisplay, currencyDisplay);
for (const currencySign of ['standard', 'accounting'])
    assert(read({style: 'currency', currency: 'EUR', currencySign}).currencySign, currencySign);
for (const unitDisplay of ['short', 'narrow', 'long'])
    assert(read({style: 'unit', unit: 'meter-per-second', unitDisplay}).unitDisplay, unitDisplay);
for (const unit of ['meter', 'meter-per-meter', 'kilometer-per-hour', 'mile-scandinavian'])
    assert(read({style: 'unit', unit}).unit, unit);
// Values for inactive styles are still validated, but are not retained.
assert(JSON.stringify(read({currency: 'USD', unit: 'meter', currencyDisplay: 'name',
    currencySign: 'accounting', unitDisplay: 'long'})), '{"style":"decimal"}');
throws(TypeError, () => read({style: 'currency'}));
throws(TypeError, () => read({style: 'unit'}));
for (const currency of ['', 'US', 'USDD', 'U$D', 'US0', 'US\0', 'éUR', null, 123]) {
    throws(RangeError, () => read({currency}));
    throws(RangeError, () => read({style: 'currency', currency}));
}
for (const unit of ['', 'Meter', 'square-meter', 'meter-per', '-per-second',
    'meter-per-second-per-hour', 'meter\0', null, 123]) {
    throws(RangeError, () => read({unit}));
    throws(RangeError, () => read({style: 'unit', unit}));
}
for (const key of ['style', 'currencyDisplay', 'currencySign', 'unitDisplay']) {
    throws(RangeError, () => read({[key]: 'invalid'}));
    throws(TypeError, () => read({[key]: Symbol()}));
}
throws(TypeError, () => read({currency: Symbol()}));
throws(TypeError, () => read({unit: Symbol()}));

const keys = ['style', 'currency', 'currencyDisplay', 'currencySign', 'unit', 'unitDisplay'];
const witness = new Error('abrupt');
for (const fail of keys) {
    const log = [];
    const options = new Proxy({}, {get(target, key) {
        log.push(key);
        if (key === fail) throw witness;
    }});
    throws(Error, () => read(options), witness);
    assert(log.join(), keys.slice(0, keys.indexOf(fail) + 1).join());
}
for (const key of keys) {
    const log = [];
    const options = new Proxy({[key]: {[Symbol.toPrimitive](hint) {
        assert(hint, 'string'); log.push('convert'); throw witness;
    }}}, {get(target, name) {log.push(name); return target[name];}});
    throws(Error, () => read(options), witness);
    assert(log.join(), [...keys.slice(0, keys.indexOf(key) + 1), 'convert'].join());
}
{
    const log = [];
    const values = ['unit', 'USD', 'name', 'accounting', 'meter-per-second', 'long'];
    const options = new Proxy(Object.fromEntries(keys.map((key, index) =>
        [key, {[Symbol.toPrimitive](hint) {
            assert(hint, 'string'); log.push('convert ' + key); return values[index];
        }}])), {get(target, key) {log.push('get ' + key); return target[key];}});
    const result = read(options);
    assert(result.unit, 'meter-per-second');
    assert(result.currency, undefined);
    assert(log.join(), keys.flatMap(key => ['get ' + key, 'convert ' + key]).join());
}
for (const [options, seen] of [
    [{style: 'currency'}, ['style', 'currency']],
    [{currency: 'bad!'}, ['style', 'currency']],
    [{style: 'unit'}, keys.slice(0, 5)],
    [{unit: 'bad!'}, keys.slice(0, 5)]]) {
    const log = [];
    throws(options.style && !options.currency && !options.unit ? TypeError : RangeError,
        () => read(new Proxy(options, {get(target, key) {log.push(key); return target[key];}})));
    assert(log.join(), seen.join());
}
// A later operation remains valid after all abrupt paths.
assert(read({style: 'currency', currency: 'jpy'}).currency, 'JPY');
