/* Direct binary64 rounding, tense, parts and observable coercion witnesses. */
function eq(actual,expected) { if(actual!==expected)throw Error(`${actual} !== ${expected}`); }
function throws(type,fn) { try { fn(); } catch(e) { if(e instanceof type)return;throw e; }throw Error('missing error'); }
const r=new Intl.RelativeTimeFormat('en');
eq(r.format(1,'day'),'in 1 day');eq(r.format(-1,'days'),'-1 day ago');
eq(r.format(-0.0001,'day'),'-0 days ago');
eq(r.format(-0,'day'),'0 days ago');eq(r.format(0,'day'),'in 0 days');
eq(r.format(1.0005,'day'),'in 1 day');
/* Direct binary64 and shortest Number routes have distinct half boundaries. */
eq(new Intl.NumberFormat('en',{maximumFractionDigits:3}).format(1.0005),'1.001');
eq(r.format(2.675,'day'),'in 2.675 days');
eq(new Intl.RelativeTimeFormat('en',{numeric:'auto'}).format(-0,'day'),'today');
eq(new Intl.RelativeTimeFormat('en',{numeric:'auto'}).format(-1,'day'),'yesterday');
eq(new Intl.RelativeTimeFormat('en',{numeric:'auto'}).format(1.0000000000000002,'day'),'in 1 day');
for(const style of ['long','short','narrow']) {
    const q=new Intl.RelativeTimeFormat('en-u-nu-latn',{style});
    eq(q.resolvedOptions().style,style);eq(q.resolvedOptions().numberingSystem,'latn');
    for(const unit of ['second','minute','hour','day','week','month','quarter','year']) {
        const parts=q.formatToParts(1234.5,unit);
        eq(parts.map(x=>x.value).join(''),q.format(1234.5,unit));
        for(const part of parts) {
            if(part.type!=='literal')eq(part.unit,unit);
            if('unit' in part)eq(part.unit,unit);
        }
    }
}
let log=[];const value={valueOf(){log.push('value');return Infinity;}};
const unit={toString(){log.push('unit');return 'day';}};
throws(RangeError,()=>r.format(value,unit));eq(log.join(','),'value,unit');
log=[];throws(TypeError,()=>r.format(1n,{toString(){log.push('unit');return 'day';}}));eq(log.length,0);
throws(TypeError,()=>r.format(Symbol(),'day'));throws(TypeError,()=>r.format(1,Symbol()));
throws(RangeError,()=>r.format(1,'day\0'));throws(RangeError,()=>r.format(1,'DAY'));
throws(TypeError,()=>Intl.RelativeTimeFormat());throws(TypeError,()=>Intl.RelativeTimeFormat.prototype.format.call({},1,'day'));
log=[];const options={};for(const name of ['localeMatcher','numberingSystem','style','numeric'])Object.defineProperty(options,name,{get(){log.push(name);return undefined;}});
new Intl.RelativeTimeFormat('en',options);eq(log.join(','),'localeMatcher,numberingSystem,style,numeric');
eq(new Intl.RelativeTimeFormat('en-u-nu-foobar').resolvedOptions().numberingSystem,'latn');
eq(new Intl.RelativeTimeFormat('en-u-nu-latn',{numberingSystem:'foobar'}).resolvedOptions().locale,'en-u-nu-latn');
throws(RangeError,()=>new Intl.RelativeTimeFormat('en',{numberingSystem:'a'}));
eq(Intl.RelativeTimeFormat.supportedLocalesOf(['en','en-US','zz']).join(','),'en,en-US');
for (const locale of ['de', 'fr', 'pl', 'ar', 'fa'])
    eq(Intl.RelativeTimeFormat.supportedLocalesOf([locale])[0], locale);
eq(new Intl.RelativeTimeFormat('fr').format(1, 'day'), 'dans 1 jour');
eq(new Intl.RelativeTimeFormat('pl').format(1000, 'second'), 'za 1000 sekund');
eq(new Intl.RelativeTimeFormat('pl', {numeric:'auto'}).format(-1, 'day'), 'wczoraj');
const persian = new Intl.RelativeTimeFormat('fa');
eq(persian.resolvedOptions().numberingSystem, 'arabext');
for (const locale of ['de', 'fr', 'pl', 'ar', 'fa']) {
    for (const style of ['long', 'short', 'narrow']) {
        const relative = new Intl.RelativeTimeFormat(locale, {style});
        for (const value of [-1234.5, -1, -0, 0, 1, 1234.5]) {
            const parts = relative.formatToParts(value, 'day');
            eq(parts.map(part => part.value).join(''), relative.format(value, 'day'));
            if (value < 0 && parts.some(part => part.type === 'integer'))
                eq(parts.some(part => part.type === 'minusSign'), true);
        }
    }
}
/* CLDR Arabic singular text has no numeric placeholder to carry a sign. */
eq(new Intl.RelativeTimeFormat('ar').format(-1, 'day'), 'قبل يوم واحد');
eq(new Intl.RelativeTimeFormat('ar').formatToParts(-1, 'day').every(part => part.type === 'literal'), true);
print('native-relative-ok');
