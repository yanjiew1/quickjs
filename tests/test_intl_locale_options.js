/* Shared ResolveOptions/ResolveLocale policy with authored service capabilities. */
function assert(value, expected = true) {
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
function check(locales, options, locale, value, key = 'nu', requestIndex = 0) {
    const result = readLocaleOptions(locales, options, key);
    assert(result.locale, locale);
    assert(result.value, value);
    assert(result.requestIndex, requestIndex);
}
check(undefined, undefined, 'en-US', 'latn', 'nu', -1);
check('zz-u-nu-arab', undefined, 'en-US', 'latn', 'nu', -1);
check('en', undefined, 'en', 'latn');
check('en-US-u-nu-arab', undefined, 'en-US-u-nu-arab', 'arab');
check('en-u-nu-arab', {numberingSystem: 'arab'}, 'en-u-nu-arab', 'arab');
check('en-u-nu-arab', {numberingSystem: 'ARAB'}, 'en-u-nu-arab', 'arab');
check('en-u-nu-arab', {numberingSystem: 'latn'}, 'en', 'latn');
check('en-u-nu-arab', {numberingSystem: 'foobar'}, 'en-u-nu-arab', 'arab');
check('en-u-nu-arab', {numberingSystem: 'native'}, 'en-u-nu-arab', 'arab');
check('en', {numberingSystem: 'mathbold'}, 'en', 'mathbold');
check('en', {numberingSystem: 'foobar'}, 'en', 'latn');
check('en-u-nu-foobar', undefined, 'en', 'latn');
check('en-u-nu', undefined, 'en', 'latn');
check('en-u-nu-true', undefined, 'en', 'latn');
check('en-u-attr-ca-gregory-nu-arab-x-test', undefined, 'en-u-nu-arab', 'arab');
check('en-Latn-US-u-nu-arab', undefined, 'en-u-nu-arab', 'arab');
check(['zz-u-nu-arab', 'en-u-nu-mathbold'], undefined,
    'en-u-nu-mathbold', 'mathbold', 'nu', 1);
check('en-u-nu-arab-nu-latn', undefined, 'en-u-nu-arab', 'arab');
for (const localeMatcher of ['lookup', 'best fit'])
    check('en-u-nu-arab', {localeMatcher}, 'en-u-nu-arab', 'arab');
check('en', 5, 'en', 'latn');
check('en', {numberingSystem: null}, 'en', 'latn');
check('en', {numberingSystem: {toString() { return 'ARAB'; }}}, 'en', 'arab');
for (const numberingSystem of ['', 'la', 'latn-', 'latn\0arab', 'ééé'])
    throws(RangeError, () => readLocaleOptions('en', {numberingSystem}));
throws(TypeError, () => readLocaleOptions('en', {numberingSystem: Symbol()}));
throws(TypeError, () => readLocaleOptions('en', null));
throws(RangeError, () => readLocaleOptions('en', {localeMatcher: 'invalid'}));

// The common policy also handles aliases and empty Boolean keyword values.
check('en-u-ca-islamic-civil', {calendar: 'islamicc'},
    'en-u-ca-islamic-civil', 'islamic-civil', 'ca');
check('en-u-ca-gregory', {calendar: 'iso8601'}, 'en', 'iso8601', 'ca');
check('en-u-kn', undefined, 'en-u-kn', 'true', 'kn');
check('en-u-kn', {numeric: 'TRUE'}, 'en-u-kn', 'true', 'kn');
check('en-u-kn', {numeric: 'false'}, 'en', 'false', 'kn');
check('en-u-kn-false', undefined, 'en-u-kn-false', 'false', 'kn');

const witness = new Error('abrupt');
for (const fail of ['localeMatcher', 'numberingSystem']) {
    const log = [];
    const options = new Proxy({}, {get(target, key) {
        log.push(key);
        if (key === fail) throw witness;
    }});
    throws(Error, () => readLocaleOptions('en', options), witness);
    assert(log.join(), fail === 'localeMatcher' ? fail : 'localeMatcher,numberingSystem');
}
{
    const log = [];
    const locales = {get length() {log.push('length'); return 1;},
        get 0() {log.push('locale'); return 'en-u-nu-arab';}};
    const options = new Proxy({numberingSystem: {[Symbol.toPrimitive](hint) {
        assert(hint, 'string'); log.push('convert'); return 'LATN';
    }}}, {get(target, key) {log.push(key); return target[key];}});
    check(locales, options, 'en', 'latn');
    assert(log.join(), 'length,locale,localeMatcher,numberingSystem,convert');
}
throws(Error, () => readLocaleOptions('en', {numberingSystem: {
    toString() {throw witness;}
}}), witness);
throws(Error, () => readLocaleOptions('en-u-nu-error'));
throws(Error, () => readLocaleOptions('en', {numberingSystem: 'error'}));
// Failure does not retain partial results or poison the next resolution.
check('en-u-nu-arab', undefined, 'en-u-nu-arab', 'arab');
