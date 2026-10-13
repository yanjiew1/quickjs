/* Original native PluralRules witnesses; pinned ECMA-402 reviewed 2026-10-09. */
function eq(actual,expected) { if(actual!==expected)throw Error(`${actual} !== ${expected}`); }
function throws(type,fn) { try { fn(); } catch(e) { if(e instanceof type)return;throw e; }throw Error('missing error'); }
const p=new Intl.PluralRules('en');
eq(p.select(1),'one');eq(p.select(0),'other');eq(p.select(NaN),'other');eq(p.select(Infinity),'other');
eq(p.select(9007199254740993n),'other');
const ordinal=new Intl.PluralRules('en',{type:'ordinal'});
for(const [x,y] of [[21,'one'],[22,'two'],[23,'few'],[11,'other'],[12,'other'],[13,'other']])eq(ordinal.select(x),y);
eq(ordinal.select(9007199254740993n),'few');eq(ordinal.select('9007199254740993'),'few');
eq(new Intl.PluralRules('en',{minimumFractionDigits:2}).select(1),'other');
eq(new Intl.PluralRules('en',{minimumFractionDigits:2,trailingZeroDisplay:'stripIfInteger'}).select(1),'one');
eq(new Intl.PluralRules('en',{maximumFractionDigits:100}).select('1.0000000000000000000000000000000000000001'),'other');
eq(new Intl.PluralRules('en',{maximumSignificantDigits:21}).select('1.0000000000000000000000000000000000000001'),'one');
for(const [mode,x,y] of [['ceil',-1.0004,'one'],['floor',-1.0004,'other'],['floor',1.0004,'one'],['ceil',1.0004,'other']])eq(new Intl.PluralRules('en',{roundingMode:mode}).select(x),y);
for(const [priority,y] of [['morePrecision','other'],['lessPrecision','one']])eq(new Intl.PluralRules('en',{minimumFractionDigits:2,maximumFractionDigits:2,maximumSignificantDigits:1,roundingPriority:priority}).select(1.01),y);
eq(p.selectRange(1,1),'one');eq(p.selectRange(1,-1),'one');eq(p.selectRange(1,2),'other');
eq(p.selectRange(2,1),'other');eq(p.selectRange(Infinity,Infinity),'other');
throws(TypeError,()=>p.selectRange(1));throws(TypeError,()=>p.select(Symbol()));
throws(RangeError,()=>p.selectRange(NaN,2));
let log=[];const end={valueOf(){log.push('end');return 1;}};
throws(RangeError,()=>p.selectRange(NaN,end));eq(log.join(','),'end');
for(const notation of ['standard','scientific','engineering','compact']) {
    const q=new Intl.PluralRules('en',{notation});
    eq(q.resolvedOptions().notation,notation);eq(q.select(1000),'other');
    eq(q.selectRange(1000,-1000),'other');
}
log=[];const names=['localeMatcher','type','notation','compactDisplay','minimumIntegerDigits','minimumFractionDigits','maximumFractionDigits','minimumSignificantDigits','maximumSignificantDigits','roundingIncrement','roundingMode','roundingPriority','trailingZeroDisplay'];
const options={};for(const name of names)Object.defineProperty(options,name,{get(){log.push(name);return undefined;}});
new Intl.PluralRules('en',options);eq(log.join(','),names.join(','));
throws(TypeError,()=>Intl.PluralRules());throws(TypeError,()=>Intl.PluralRules.prototype.select.call({},1));
throws(TypeError,()=>new Intl.PluralRules('en',{roundingIncrement:2,maximumSignificantDigits:3}));
throws(RangeError,()=>new Intl.PluralRules('en',{minimumFractionDigits:3,maximumFractionDigits:1}));
eq(Intl.PluralRules.supportedLocalesOf(['en','en-US','fr']).join(','),'en,en-US');
print('native-plural-ok');
