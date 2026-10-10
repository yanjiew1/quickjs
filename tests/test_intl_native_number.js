/* Original native NumberFormat integration checks. ECMA402 revision
 * 7ae78cfdf8255468ffc8ebda33dafaea952808dd, reviewed 2026-10-09. */
function eq(actual, expected, label) {
    if (!Object.is(actual, expected)) throw Error(label + ': ' + actual + ' != ' + expected);
}
function ok(value, label) { if (!value) throw Error(label); }
function throws(type, operation, label) {
    try { operation(); } catch (error) { if (error instanceof type) return; throw error; }
    throw Error('did not throw: ' + label);
}
function partsCheck(nf, input) {
    let parts = nf.formatToParts(input);
    eq(parts.map(p => p.value).join(''), nf.format(input), 'parts reconstruct format');
    for (let p of parts) {
        eq(Object.keys(p).join(','), 'type,value', 'ordinary part fields');
        ok(p.value.length > 0, 'nonempty part');
    }
    return parts;
}
function rangeCheck(nf, first, second, equal) {
    let parts = nf.formatRangeToParts(first, second);
    eq(parts.map(p => p.value).join(''), nf.formatRange(first, second), 'range parts reconstruct');
    for (let p of parts) {
        eq(Object.keys(p).join(','), 'type,value,source', 'range part fields');
        ok(['startRange','endRange','shared'].includes(p.source), 'range source');
    }
    if (equal) {
        ok(parts.every(p => p.source === 'shared'), 'identity sources shared');
        ok(parts.some(p => p.type === 'approximatelySign'), 'identity approximate pattern');
    } else {
        ok(parts.some(p => p.source === 'startRange'), 'start source');
        ok(parts.some(p => p.source === 'endRange'), 'end source');
    }
    return parts;
}
let nf = new Intl.NumberFormat('en-US');
eq(nf.format(1234.5), '1,234.5', 'decimal data');
eq(nf.resolvedOptions().numberingSystem, 'latn', 'actual default numbering');
eq(nf.resolvedOptions().locale, 'en-US', 'resolved locale');
eq(nf.format, nf.format, 'bound format cached');
eq(nf.format.name, '', 'bound format name');
eq(nf.format.length, 1, 'bound format arity');
eq(nf.format.call(null, 12), '12', 'bound receiver ignored');
eq(Object.prototype.toString.call(nf), '[object Intl.NumberFormat]', 'tag');
eq(Intl.NumberFormat.supportedLocalesOf(['fr','en-US','en-GB']).join(','), 'en-US,en-GB', 'supported locale matching');
partsCheck(nf, -1234.5);
let plain = new Intl.NumberFormat('en', {useGrouping:false, maximumFractionDigits:100});
eq(plain.format(0.1), '0.1', 'Number shortest decimal');
eq(plain.format('0.100000000000000000001'), '0.100000000000000000001', 'exact decimal string');
eq(plain.format('9007199254740993'), '9007199254740993', 'string beyond exact IEEE integer');
eq(plain.format(9007199254740993n), '9007199254740993', 'BigInt exact');
eq(plain.format('0x20000000000001'), '9007199254740993', 'radix string exact');
eq(plain.format('\u2028 12.345\u2029'), '12.345', 'numeric whitespace');
eq(plain.format(''), '0', 'empty numeric string');
eq(plain.format('-1e-999999999999999999999'), '-0', 'string negative underflow');
eq(plain.format('1e999999999999999999999'), '∞', 'string positive overflow');
eq(plain.format('1\u00002'), 'NaN', 'NUL invalid grammar');
eq(plain.format('1_000'), 'NaN', 'separator invalid numeric string');
let primitiveCalls = [];
eq(plain.format({[Symbol.toPrimitive](hint) { primitiveCalls.push(hint); return '1.25000000000000001'; }}),
    '1.25000000000000001', 'primitive exact string');
eq(primitiveCalls.join(','), 'number', 'ToPrimitive hint');
throws(TypeError, () => nf.format(Symbol()), 'Symbol input');
for (let value of [NaN, Infinity, -Infinity, -0]) partsCheck(nf, value);
let signExpectations = {auto:['-0','0'], never:['0','0'], always:['-0','+0'], exceptZero:['0','0'], negative:['0','0']};
for (let mode of Object.keys(signExpectations)) {
    let signed = new Intl.NumberFormat('en', {signDisplay:mode});
    eq(signed.format(-0), signExpectations[mode][0], 'negative zero ' + mode);
    eq(signed.format(0), signExpectations[mode][1], 'zero ' + mode);
    partsCheck(signed, -2);
}
let rounding = {ceil:['1.3','-1.2'], floor:['1.2','-1.3'], expand:['1.3','-1.3'], trunc:['1.2','-1.2'],
    halfCeil:['1.3','-1.2'], halfFloor:['1.2','-1.3'], halfExpand:['1.3','-1.3'], halfTrunc:['1.2','-1.2'], halfEven:['1.2','-1.2']};
for (let mode of Object.keys(rounding)) {
    let rounded = new Intl.NumberFormat('en', {maximumFractionDigits:1, roundingMode:mode});
    eq(rounded.format('1.25'), rounding[mode][0], 'positive rounding ' + mode);
    eq(rounded.format('-1.25'), rounding[mode][1], 'negative rounding ' + mode);
}
eq(new Intl.NumberFormat('en', {minimumFractionDigits:2, maximumFractionDigits:2, roundingIncrement:5}).format('1.225'), '1.25', 'increment');
eq(new Intl.NumberFormat('en', {minimumFractionDigits:2, trailingZeroDisplay:'stripIfInteger'}).format('2'), '2', 'strip fraction zeros');
for (let priority of ['auto','morePrecision','lessPrecision']) {
    let n = new Intl.NumberFormat('en', {maximumFractionDigits:2, maximumSignificantDigits:3, roundingPriority:priority});
    eq(n.resolvedOptions().roundingPriority, priority, 'roundingPriority');
    partsCheck(n, '12.3456');
}
for (let group of [false, true, 'min2','auto','always','true','false']) {
    let n = new Intl.NumberFormat('en', {useGrouping:group});
    partsCheck(n, '12345.5');
}
let ordering = [], getters = ['localeMatcher','numberingSystem','style','currency','currencyDisplay','currencySign','unit','unitDisplay',
    'notation','minimumIntegerDigits','minimumFractionDigits','maximumFractionDigits','minimumSignificantDigits','maximumSignificantDigits',
    'roundingIncrement','roundingMode','roundingPriority','trailingZeroDisplay','compactDisplay','useGrouping','signDisplay'];
let options = new Proxy({}, {get(target,key) { ordering.push(key); return undefined; }});
new Intl.NumberFormat('en', options);
eq(ordering.join(','), getters.join(','), 'option getter order exactly once');
let conversions = [];
new Intl.NumberFormat('en', {get minimumFractionDigits() { conversions.push('get-min'); return {valueOf(){conversions.push('convert-min');return 1;}};},
    get maximumFractionDigits(){conversions.push('get-max');return 3;}, get roundingMode(){conversions.push('get-mode');return 'halfExpand';}});
eq(conversions.join(','), 'get-min,get-max,get-mode,convert-min', 'deferred digit conversion');
let withExtension = new Intl.NumberFormat('en-u-nu-latn');
eq(withExtension.resolvedOptions().locale, 'en-u-nu-latn', 'supported nu extension retained');
eq(new Intl.NumberFormat('en-u-nu-latn', {numberingSystem:'unknown'}).resolvedOptions().locale, 'en-u-nu-latn', 'unsupported option retains extension');
eq(new Intl.NumberFormat('en-u-nu-unknown').resolvedOptions().locale, 'en', 'unsupported extension removed');
for (let invalid of ['', 'ab', 'ab_cd', 'latn\u0000']) throws(RangeError, () => new Intl.NumberFormat('en', {numberingSystem:invalid}), 'invalid nu');
for (let invalid of [0,3,6,5001]) throws(RangeError, () => new Intl.NumberFormat('en', {roundingIncrement:invalid}), 'invalid increment');
throws(TypeError, () => new Intl.NumberFormat('en', {roundingIncrement:5, maximumSignificantDigits:2}), 'increment incompatible precision');
throws(RangeError, () => new Intl.NumberFormat('en', {minimumFractionDigits:3, maximumFractionDigits:2}), 'fraction order');
throws(TypeError, () => new Intl.NumberFormat('en', {style:'currency'}), 'currency required');
throws(TypeError, () => new Intl.NumberFormat('en', {style:'unit'}), 'unit required');
for (let code of ['US','USDD','U1D','USD\u0000']) throws(RangeError, () => new Intl.NumberFormat('en', {currency:code}), 'currency grammar');
for (let unit of ['not-a-unit','meter-per-second-per-hour','meter\u0000']) throws(RangeError, () => new Intl.NumberFormat('en', {unit}), 'unit grammar');
let jpy = new Intl.NumberFormat('en', {style:'currency',currency:'jpy'});
eq(jpy.resolvedOptions().currency, 'JPY', 'currency uppercase');
eq(jpy.resolvedOptions().maximumFractionDigits, 0, 'JPY source digits');
eq(new Intl.NumberFormat('en', {style:'currency',currency:'ZZZ'}).resolvedOptions().maximumFractionDigits, 2, 'currency DEFAULT row');
eq(new Intl.NumberFormat('en', {style:'currency',currency:'USD'}).format(2), '$2.00', 'USD source symbol');
for (let currencyDisplay of ['symbol','narrowSymbol','code','name']) {
    for (let currencySign of ['standard','accounting']) {
        let n = new Intl.NumberFormat('en', {style:'currency',currency:'USD',currencyDisplay,currencySign});
        ok(partsCheck(n, -2).some(p => p.type === 'currency'), 'currency part ' + currencyDisplay);
        rangeCheck(n, 1, 2, false);
    }
}
let percent = new Intl.NumberFormat('en', {style:'percent'});
eq(percent.format('0.12'), '12%', 'exact percent scaling');
ok(partsCheck(percent, '0.12').some(p => p.type === 'percentSign'), 'percent part');
for (let notation of ['standard','scientific','engineering','compact']) {
    let n = new Intl.NumberFormat('en', {notation});
    let parts = partsCheck(n, '123456.75');
    if (notation === 'scientific' || notation === 'engineering') {
        ok(parts.some(p => p.type === 'exponentSeparator'), 'exponent separator');
        ok(partsCheck(n, '0.00000123').some(p => p.type === 'exponentMinusSign'), 'exponent sign');
    }
    if (notation === 'compact') ok(parts.some(p => p.type === 'compact'), 'compact part');
    rangeCheck(n, 1000, 2000, false);
}
for (let compactDisplay of ['short','long']) {
    let n = new Intl.NumberFormat('en', {notation:'compact',compactDisplay});
    for (let value of [0,1,999,1000,1200,999500,1e9,-999500]) partsCheck(n,value);
}
let sanctioned = ['acre','bit','byte','celsius','centimeter','day','degree','fahrenheit','fluid-ounce','foot','gallon','gigabit','gigabyte',
    'gram','hectare','hour','inch','kilobit','kilobyte','kilogram','kilometer','liter','megabit','megabyte','meter','microsecond','mile',
    'mile-scandinavian','milliliter','millimeter','millisecond','minute','month','nanosecond','ounce','percent','petabyte','pound','second',
    'stone','terabit','terabyte','week','yard','year'];
for (let unitDisplay of ['long','short','narrow']) {
    for (let unit of sanctioned) {
        let n = new Intl.NumberFormat('en', {style:'unit',unit,unitDisplay});
        ok(partsCheck(n, 2).some(p => p.type === 'unit'), 'simple unit ' + unit);
        partsCheck(n, 1);
        for (let compound of ['meter-per-'+unit, unit+'-per-second']) {
            let c = new Intl.NumberFormat('en', {style:'unit',unit:compound,unitDisplay});
            ok(partsCheck(c, 2).some(p => p.type === 'unit'), 'compound unit ' + compound);
        }
    }
}
eq(new Intl.NumberFormat('en', {style:'unit',unit:'meter',unitDisplay:'long'}).format(1), '1 meter', 'singular unit');
eq(new Intl.NumberFormat('en', {style:'unit',unit:'meter',unitDisplay:'long'}).format(2), '2 meters', 'plural unit');
rangeCheck(nf, 1, 2, false);
rangeCheck(nf, 1, 1, true);
rangeCheck(new Intl.NumberFormat('en', {signDisplay:'never'}), -0, 0, true);
rangeCheck(new Intl.NumberFormat('en', {maximumFractionDigits:0}), '1.1', '1.2', true);
rangeCheck(nf, -Infinity, Infinity, false);
throws(TypeError, () => nf.formatRange(1), 'missing range endpoint');
throws(TypeError, () => nf.formatRange(1, undefined), 'undefined range endpoint');
throws(RangeError, () => nf.formatRange(NaN, 2), 'NaN range endpoint');
let coercion = [];
nf.formatRange({valueOf(){coercion.push('first');return 1;}},{valueOf(){coercion.push('second');return 2;}});
eq(coercion.join(','), 'first,second', 'range coercion order');
for (let name of ['formatToParts','formatRange','formatRangeToParts','resolvedOptions'])
    throws(TypeError, () => Intl.NumberFormat.prototype[name].call({}), 'method brand ' + name);
let saved = Intl.NumberFormat;
Intl.NumberFormat = undefined;
eq((1234.5).toLocaleString('en'), '1,234.5', 'intrinsic Number method');
eq((9007199254740993n).toLocaleString('en'), '9,007,199,254,740,993', 'intrinsic BigInt method');
Intl.NumberFormat = saved;
print('native NumberFormat integration checks passed');
