/* Constructor/data absence, syntax, fallback and coercion witnesses. */
function eq(actual,expected) { if(actual!==expected)throw Error(`${actual} !== ${expected}`); }
function throws(type,fn) { try { fn(); } catch(e) { if(e instanceof type)return;throw e; }throw Error('missing error'); }
for(const [type,code,name] of [['language','en','English'],['region','US','United States'],['script','Latn','Latin'],['currency','USD','US Dollar'],['calendar','gregory','Gregorian Calendar'],['dateTimeField','year','year']]) {
    const d=new Intl.DisplayNames('en',{type});eq(d.of(code),name);eq(d.resolvedOptions().type,type);
}
for(const style of ['long','short','narrow']) {
    const d=new Intl.DisplayNames('en',{type:'dateTimeField',style});
    eq(d.resolvedOptions().style,style);if(typeof d.of('month')!=='string')throw Error('missing field');
}
eq(new Intl.DisplayNames('en',{type:'currency',fallback:'none'}).of('ZZZ'),undefined);
eq(new Intl.DisplayNames('en',{type:'currency',fallback:'code'}).of('zzz'),'ZZZ');
eq(new Intl.DisplayNames('en',{type:'region',fallback:'none'}).of('AA'),undefined);
eq(new Intl.DisplayNames('en',{type:'calendar',fallback:'code'}).of('foobar'),'foobar');
throws(RangeError,()=>new Intl.DisplayNames('en',{type:'calendar'}).of('gregorian'));
for(const fallback of ['none','code']) {
    throws(RangeError,()=>new Intl.DisplayNames('en',{type:'region',fallback}).of('U'));
    throws(RangeError,()=>new Intl.DisplayNames('en',{type:'dateTimeField',fallback}).of('bogus'));
    throws(RangeError,()=>new Intl.DisplayNames('en',{type:'language',fallback}).of('en-u-nu-latn'));
    throws(RangeError,()=>new Intl.DisplayNames('en',{type:'currency',fallback}).of('US\0'));
}
const language=new Intl.DisplayNames('en',{type:'language',languageDisplay:'dialect'});
eq(language.resolvedOptions().languageDisplay,'dialect');
const standard=new Intl.DisplayNames('en',{type:'language',languageDisplay:'standard'});
eq(standard.resolvedOptions().languageDisplay,'standard');
eq(language.of('en-US'),'American English');
eq(standard.of('en-US'),'English (United States)');
eq(new Intl.DisplayNames('en',{type:'currency'}).of({toString(){return 'USD';}}),'US Dollar');
throws(TypeError,()=>new Intl.DisplayNames('en',{type:'currency'}).of(Symbol()));
throws(TypeError,()=>new Intl.DisplayNames('en'));throws(TypeError,()=>new Intl.DisplayNames('en',{}));
throws(TypeError,()=>Intl.DisplayNames('en',{type:'region'}));throws(TypeError,()=>Intl.DisplayNames.prototype.of.call({},'US'));
let log=[];const options={type:'region'};for(const name of ['localeMatcher','style','fallback','languageDisplay'])Object.defineProperty(options,name,{get(){log.push(name);return undefined;}});
new Intl.DisplayNames('en',options);eq(log.join(','),'localeMatcher,style,fallback,languageDisplay');
log=[];throws(TypeError,()=>new Intl.DisplayNames('en',{get style(){log.push('style');},get type(){log.push('type');},get fallback(){throw Error('late fallback');}}));eq(log.join(','),'style,type');
eq(Intl.DisplayNames.supportedLocalesOf(['en','en-US','fr']).join(','),'en,en-US');
print('native-display-ok');
