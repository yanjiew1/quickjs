/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Source-only integration fixture: actual shared decimal/plural/Number/List.
 */
#include "intl/duration-native.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct Memory { size_t calls, fail_at, live; } Memory;
static void *am(void *opaque, size_t size)
{
    Memory *m = opaque; void *p;
    assert(size);
    if (++m->calls == m->fail_at) return NULL;
    p = malloc(size); if (p) m->live++; return p;
}
static void af(void *opaque, void *p)
{
    Memory *m = opaque;
    if (p) { assert(m->live); m->live--; free(p); }
}
static QJSIntlBytes bytes(const char *s) { QJSIntlBytes b = {s,strlen(s)}; return b; }
typedef struct Context { QJSIntlNativePlural *plural; unsigned arabic; size_t opened, closed; } Context;
static QJSIntlStatus open_number(void *opaque, const QJSIntlAllocator *a,
    const QJSIntlNumberOptions *o, QJSIntlNativeNumber **out)
{
    static const char *const symbols[] = {".",",","+","-","%","E","INF","NaN",".",","};
    static const char *const units[] = {"year","month","week","day","hour","minute","second","millisecond","microsecond","nanosecond"};
    static const char *const shorts[] = {"yr","mo","wk","day","hr","min","sec","ms","us","ns"};
    static const char *const narrows[] = {"y","m","w","d","h","m","s","ms","us","ns"};
    Context *c = opaque; QJSIntlNumberData d; QJSIntlStatus s;
    char one[100], other[100]; size_t i, unit = 0;
    memset(&d,0,sizeof(d));
    for (i=0;i<10;i++) { d.digits[i]=(c->arabic ? 0x660u : (uint32_t)'0')+(uint32_t)i; d.symbols[i]=bytes(symbols[i]); }
    d.minimum_grouping_digits=1; d.pattern.primary_group=d.pattern.secondary_group=3;
    d.pattern.zero=bytes("{number}"); d.pattern.negative=bytes("{minusSign}{number}"); d.pattern.positive=bytes("{plusSign}{number}");
    d.cardinal=c->plural;
    if (o->style==QJS_INTL_NUMBER_UNIT) {
        for (unit=0;unit<10;unit++) if (o->unit.length==strlen(units[unit]) && !memcmp(o->unit.data,units[unit],o->unit.length)) break;
        assert(unit<10);
        if (o->unit_display==QJS_INTL_UNIT_LONG) {
            snprintf(one,sizeof(one),"{number} %s",units[unit]);
            snprintf(other,sizeof(other),"{number} %ss",units[unit]);
        } else if (o->unit_display==QJS_INTL_UNIT_SHORT) {
            snprintf(one,sizeof(one),"{number} %s",shorts[unit]); snprintf(other,sizeof(other),"%s",one);
        } else {
            snprintf(one,sizeof(one),"{number}%s",narrows[unit]); snprintf(other,sizeof(other),"%s",one);
        }
        for (i=0;i<6;i++) d.unit_patterns[i]=bytes(i==QJS_INTL_PLURAL_ONE ? one:other);
    }
    s=qjs_intl_native_number_open(a,o,&d,out);
    if (s==QJS_INTL_OK) c->opened++;
    return s;
}
static void close_number(void *opaque, QJSIntlNativeNumber *n)
{
    Context *c=opaque; c->closed++; qjs_intl_native_number_close(n);
}
static QJSIntlDurationData data(Context *c)
{
    static const uint16_t colon[]={':'};
    QJSIntlDurationData d; size_t i;
    memset(&d,0,sizeof(d)); d.number.open_opaque=c; d.number.close_opaque=c;
    d.number.open=open_number; d.number.close=close_number;
    for (i=0;i<4;i++) d.list.base[i]=bytes("{0}, {1}");
    d.hour_minute_separator.data=colon; d.hour_minute_separator.length=1;
    d.minute_second_separator=d.hour_minute_separator; return d;
}
static QJSIntlDurationOptions options(int digital)
{
    QJSIntlDurationOptions o; size_t i;
    memset(&o,0,sizeof(o)); o.style=digital?QJS_INTL_DURATION_DIGITAL:QJS_INTL_DURATION_LONG;
    o.maximum_output_length=1024; o.fractional_digits=-1;
    for (i=0;i<10;i++) o.units[i].style=QJS_INTL_DURATION_UNIT_LONG;
    if (digital) {
        o.units[4].style=QJS_INTL_DURATION_NUMERIC;
        o.units[5].style=o.units[6].style=QJS_INTL_DURATION_TWO_DIGIT;
        for(i=7;i<10;i++)o.units[i].style=QJS_INTL_DURATION_FRACTIONAL;
    }
    return o;
}
static QJSIntlDurationRecord record(void)
{
    QJSIntlDurationRecord r; size_t i;
    memset(&r,0,sizeof(r)); for(i=0;i<10;i++)r.magnitudes[i]=bytes("0"); return r;
}
static void expect(const QJSIntlNativeDuration *d,const QJSIntlAllocator *a,
                   QJSIntlDurationRecord *r,const char *text)
{
    QJSIntlFormatted f={0}; size_t i,pos=0;
    assert(qjs_intl_native_duration_format(d,r,&f)==QJS_INTL_OK);
    assert(f.length==strlen(text));
    for(i=0;i<f.length;i++)assert(f.text[i]==(unsigned char)text[i]);
    for(i=0;i<f.part_count;i++) {
        assert(f.parts[i].start==pos && f.parts[i].end>pos);
        assert(f.parts[i].source==QJS_INTL_SOURCE_SINGLE);
        if(f.parts[i].type==QJS_INTL_PART_INTEGER || f.parts[i].type==QJS_INTL_PART_FRACTION ||
           f.parts[i].type==QJS_INTL_PART_UNIT || f.parts[i].type==QJS_INTL_PART_MINUS_SIGN)
            assert(f.parts[i].unit.data && f.parts[i].unit.length);
        pos=f.parts[i].end;
    }
    assert(pos==f.length); qjs_intl_native_duration_result_clear(a,&f);
}
static void digital_cases(const QJSIntlAllocator *a,Context *c)
{
    QJSIntlDurationOptions o=options(1), resolved;
    QJSIntlDurationData x=data(c); QJSIntlDurationRecord r=record(); QJSIntlNativeDuration *d;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    assert(qjs_intl_native_duration_resolved_options(d,&resolved)==QJS_INTL_OK);
    assert(resolved.fractional_digits==-1 && resolved.units[5].style==QJS_INTL_DURATION_TWO_DIGIT);
    expect(d,a,&r,"");
    r.sign=1;r.magnitudes[4]=bytes("1");r.magnitudes[6]=bytes("2");expect(d,a,&r,"1:00:02");
    r.magnitudes[4]=bytes("1000");expect(d,a,&r,"1000:00:02");
    r.magnitudes[4]=bytes("0");r.magnitudes[6]=bytes("0");r.magnitudes[7]=bytes("1001");
    r.magnitudes[8]=bytes("987");r.magnitudes[9]=bytes("999");expect(d,a,&r,"01.001987999");
    r.sign=-1;expect(d,a,&r,"-01.001987999");
    r.magnitudes[4]=bytes("2");expect(d,a,&r,"-2:00:01.001987999");
    qjs_intl_native_duration_close(d);
    o.fractional_digits=0;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();r.sign=-1;r.magnitudes[9]=bytes("1");expect(d,a,&r,"-00");
    qjs_intl_native_duration_close(d);
    o.fractional_digits=9;o.units[4].always=o.units[5].always=o.units[6].always=1;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();expect(d,a,&r,"0:00:00.000000000");qjs_intl_native_duration_close(d);
    o=options(1);o.units[0].always=1;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();r.sign=-1;r.magnitudes[6]=bytes("3");expect(d,a,&r,"-0 years, 03");qjs_intl_native_duration_close(d);
}
static void fraction_widths(const QJSIntlAllocator *a,Context *c)
{
    static const char *const expected[]={"01","01.2","01.23","01.234","01.2345",
        "01.23456","01.234567","01.2345678","01.23456789","01.234567890"};
    QJSIntlDurationOptions o=options(1);QJSIntlDurationData x=data(c);QJSIntlDurationRecord r=record();QJSIntlNativeDuration *d;
    int i;
    r.sign=1;r.magnitudes[6]=bytes("1");r.magnitudes[7]=bytes("234");r.magnitudes[8]=bytes("567");r.magnitudes[9]=bytes("890");
    for(i=0;i<=9;i++) {
        o.fractional_digits=(int8_t)i;
        assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
        expect(d,a,&r,expected[i]);qjs_intl_native_duration_close(d);
    }
    o.fractional_digits=-1;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();r.sign=1;r.magnitudes[7]=bytes("9007199254740991");
    expect(d,a,&r,"9007199254740.991");qjs_intl_native_duration_close(d);
}
static void textual_cases(const QJSIntlAllocator *a,Context *c)
{
    QJSIntlDurationOptions o=options(0);QJSIntlDurationData x=data(c);QJSIntlDurationRecord r=record();QJSIntlNativeDuration *d;
    size_t i;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r.sign=1;for(i=0;i<10;i++)r.magnitudes[i]=bytes("1");
    expect(d,a,&r,"1 year, 1 month, 1 week, 1 day, 1 hour, 1 minute, 1 second, 1 millisecond, 1 microsecond, 1 nanosecond");
    r.sign=-1;expect(d,a,&r,"-1 year, 1 month, 1 week, 1 day, 1 hour, 1 minute, 1 second, 1 millisecond, 1 microsecond, 1 nanosecond");
    r=record();r.sign=1;r.magnitudes[0]=bytes("1000");expect(d,a,&r,"1,000 years");qjs_intl_native_duration_close(d);
    for(i=7;i<10;i++)o.units[i].style=QJS_INTL_DURATION_FRACTIONAL;
    o.fractional_digits=2;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();r.sign=1;r.magnitudes[6]=bytes("1");r.magnitudes[7]=bytes("999");expect(d,a,&r,"1.99 seconds");
    qjs_intl_native_duration_close(d);
    o=options(0);o.units[9].style=QJS_INTL_DURATION_FRACTIONAL;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();r.sign=1;r.magnitudes[8]=bytes("2");r.magnitudes[9]=bytes("3456");expect(d,a,&r,"5.456 microseconds");qjs_intl_native_duration_close(d);
    o=options(0);o.style=QJS_INTL_DURATION_SHORT;for(i=0;i<10;i++)o.units[i].style=QJS_INTL_DURATION_UNIT_SHORT;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();r.sign=1;r.magnitudes[4]=bytes("2");r.magnitudes[6]=bytes("3");expect(d,a,&r,"2 hr, 3 sec");qjs_intl_native_duration_close(d);
    o.style=QJS_INTL_DURATION_NARROW;for(i=0;i<10;i++)o.units[i].style=QJS_INTL_DURATION_UNIT_NARROW;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);expect(d,a,&r,"2h, 3s");qjs_intl_native_duration_close(d);
}
static void lifetime_and_invalid(const QJSIntlAllocator *a,Context *c)
{
    QJSIntlDurationOptions o=options(1);QJSIntlDurationData x=data(c);QJSIntlDurationRecord r=record();QJSIntlNativeDuration *d;
    QJSIntlFormatted f={0};uint16_t separators[2]={0xd83d,0xdd53};
    x.hour_minute_separator.data=separators;x.hour_minute_separator.length=2;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    separators[0]='x';separators[1]='y';r.sign=1;r.magnitudes[4]=bytes("1");r.magnitudes[5]=bytes("2");
    assert(qjs_intl_native_duration_format(d,&r,&f)==QJS_INTL_OK);
    assert(f.length==5 && f.text[1]==0xd83d && f.text[2]==0xdd53);
    qjs_intl_native_duration_close(d);assert(f.parts[0].unit.length==4 && !memcmp(f.parts[0].unit.data,"hour",4));
    qjs_intl_native_duration_result_clear(a,&f);qjs_intl_native_duration_result_clear(a,&f);
    o.units[7].always=1;assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_INVALID_ARGUMENT && !d);
    o=options(1);o.units[5].style=QJS_INTL_DURATION_NUMERIC;assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_INVALID_ARGUMENT);
    o=options(1);assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();r.sign=1;assert(qjs_intl_native_duration_format(d,&r,&f)==QJS_INTL_INVALID_ARGUMENT && !f.text && !f.parts);
    r.sign=0;r.magnitudes[0]=bytes("01");assert(qjs_intl_native_duration_format(d,&r,&f)==QJS_INTL_INVALID_ARGUMENT);
    r.magnitudes[0]=bytes("1e0");assert(qjs_intl_native_duration_format(d,&r,&f)==QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_native_duration_close(d);
    o=options(1);o.maximum_output_length=6;
    assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);
    r=record();r.sign=1;r.magnitudes[4]=bytes("1111");r.magnitudes[6]=bytes("222");
    assert(qjs_intl_native_duration_format(d,&r,&f)==QJS_INTL_OVERFLOW && !f.text && !f.parts && !f.length && !f.part_count);
    qjs_intl_native_duration_close(d);
}
static void failures(QJSIntlAllocator *a,Context *c,Memory *m)
{
    QJSIntlDurationOptions o=options(1);QJSIntlDurationData x=data(c);QJSIntlDurationRecord r=record();QJSIntlNativeDuration *d;
    QJSIntlFormatted f={0};size_t base=m->live,allocations,k,held;
    m->calls=0;assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);allocations=m->calls;
    qjs_intl_native_duration_close(d);assert(m->live==base);
    for(k=1;k<=allocations;k++) {
        m->calls=0;m->fail_at=k;
        assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_NO_MEMORY && !d);
        assert(m->live==base);
    }
    m->fail_at=0;assert(qjs_intl_native_duration_open(a,&o,&x,&d)==QJS_INTL_OK);held=m->live;
    r.sign=-1;r.magnitudes[0]=bytes("2");r.magnitudes[4]=bytes("1");r.magnitudes[6]=bytes("2");r.magnitudes[9]=bytes("999");
    m->calls=0;assert(qjs_intl_native_duration_format(d,&r,&f)==QJS_INTL_OK);allocations=m->calls;
    qjs_intl_native_duration_result_clear(a,&f);assert(m->live==held);
    for(k=1;k<=allocations;k++) {
        m->calls=0;m->fail_at=k;
        assert(qjs_intl_native_duration_format(d,&r,&f)==QJS_INTL_NO_MEMORY);
        assert(!f.text && !f.parts && !f.length && !f.part_count && m->live==held);
    }
    m->fail_at=0;qjs_intl_native_duration_close(d);assert(m->live==base);
}
int main(void)
{
    Memory memory={0};QJSIntlAllocator a={&memory,am,NULL,af};Context c={0};
    QJSIntlPluralOptions po;QJSIntlPluralRule rules[2];QJSIntlPluralRulesData pd;
    memset(&po,0,sizeof(po));po.digits.minimum_integer_digits=1;po.digits.maximum_fraction_digits=3;
    po.digits.minimum_significant_digits=1;po.digits.maximum_significant_digits=21;
    po.digits.rounding_increment=1;po.digits.rounding_mode=QJS_INTL_ROUND_HALF_EXPAND;po.digits.maximum_output_length=1024;
    rules[0].category=QJS_INTL_PLURAL_ONE;rules[0].relation=bytes("i = 1 and v = 0");
    rules[1].category=QJS_INTL_PLURAL_OTHER;rules[1].relation=bytes("");
    memset(&pd,0,sizeof(pd));pd.rules=rules;pd.rule_count=2;
    assert(qjs_intl_native_plural_open(&a,&po,&pd,&c.plural)==QJS_INTL_OK);
    digital_cases(&a,&c);fraction_widths(&a,&c);textual_cases(&a,&c);lifetime_and_invalid(&a,&c);failures(&a,&c,&memory);
    c.arabic=1;
    { QJSIntlDurationOptions o=options(1);QJSIntlDurationData x=data(&c);QJSIntlDurationRecord r=record();QJSIntlNativeDuration *d;QJSIntlFormatted f={0};
      r.sign=1;r.magnitudes[4]=bytes("1");r.magnitudes[5]=bytes("2");
      assert(qjs_intl_native_duration_open(&a,&o,&x,&d)==QJS_INTL_OK);
      assert(qjs_intl_native_duration_format(d,&r,&f)==QJS_INTL_OK);
      assert(f.length==4 && f.text[0]==0x661 && f.text[1]==':' && f.text[2]==0x660 && f.text[3]==0x662);
      qjs_intl_native_duration_result_clear(&a,&f);qjs_intl_native_duration_close(d); }
    assert(c.opened==c.closed);qjs_intl_native_plural_close(c.plural);assert(!memory.live);return 0;
}
