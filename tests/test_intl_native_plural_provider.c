/* Actual canonical Number data -> PluralRules c/e boundary witnesses.
 * Copyright (c) 2026 Yan-Jie Wang. No substitute reader/provider facade. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../src/intl/provider-native-plural.h"
typedef struct Heap {size_t live;} Heap;
static void *allocate(void *opaque,size_t n)
{
    Heap *h=opaque;void *p=malloc(n);if(p)h->live++;return p;
}
static void release(void *opaque,void *p)
{
    Heap *h=opaque;if(p){assert(h->live);h->live--;free(p);}
}
static void *resize(void *opaque,void *p,size_t n)
{
    if(!p)return allocate(opaque,n);
    assert(n);return realloc(p,n);
}
static QJSIntlBytes text(const char *s){return (QJSIntlBytes){s,strlen(s)};}
static QJSIntlDecimalOptions digits(unsigned int fraction)
{
    QJSIntlDecimalOptions o={0};
    o.minimum_integer_digits=1;o.maximum_fraction_digits=fraction;
    o.minimum_significant_digits=1;o.maximum_significant_digits=21;
    o.rounding_increment=1;o.rounding_mode=QJS_INTL_ROUND_HALF_EXPAND;
    o.maximum_output_length=4096;return o;
}
static void check(const QJSIntlAllocator *a,QJSIntlNativeNumber *number,
                  const char *input,unsigned int fraction,int32_t exponent,
                  const char *expected_raw,QJSIntlNativePlural *rule)
{
    QJSIntlDecimal exact={0};QJSIntlDecimalResult raw={0};
    QJSIntlDecimalOptions o=digits(fraction);
    QJSIntlPluralCategory category;int32_t actual=INT32_MAX;
    assert(qjs_intl_decimal_parse(a,text(input),&exact)==QJS_INTL_OK);
    assert(qjs_intl_decimal_format_numeric(a,&exact,&o,&raw)==QJS_INTL_OK);
    assert(!strcmp(raw.text,expected_raw));
    assert(qjs_intl_native_number_notation_exponent(number,&raw,&actual)==QJS_INTL_OK);
    assert(actual==exponent && !strcmp(raw.text,expected_raw));
    if(rule) {
        JSIntlPluralOperands operands={raw.text,raw.length,actual};
        assert(qjs_intl_native_plural_select_operands(rule,&operands,&category)==QJS_INTL_OK);
        assert(category==QJS_INTL_PLURAL_ONE); /* n=1000, e=3; no mantissa rewrite */
    }
    qjs_intl_decimal_result_clear(a,&raw);qjs_intl_decimal_clear(a,&exact);
}
#endif
int main(void)
{
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
    Heap heap={0};QJSIntlAllocator a={&heap,allocate,resize,release};
    QJSIntlProviderConfig config={0};QJSIntlProvider *provider=NULL;
    QJSIntlNativeNumber *numbers[4][2]={{0}};
    QJSIntlNativePlural *rule=NULL;
    QJSIntlPluralOptions po={0};QJSIntlNumberOptions no={0};
    const QJSIntlPluralRule rules[]={
        {QJS_INTL_PLURAL_ONE,{"n = 1000 and e = 3",sizeof("n = 1000 and e = 3")-1}},
        {QJS_INTL_PLURAL_OTHER,{NULL,0}}
    };
    const QJSIntlPluralRulesData data={rules,2,NULL,0};
    unsigned int notation,display;
    config.backend=QJS_INTL_BACKEND_NATIVE;config.allocator=a;
    config.default_locale=text("en-US");config.default_time_zone=text("UTC");
    assert(qjs_intl_provider_new(&config,&provider)==QJS_INTL_OK && provider);
    po.digits=digits(3);
    assert(qjs_intl_native_plural_open(&a,&po,&data,&rule)==QJS_INTL_OK);
    no.digits=digits(3);no.maximum_output_length=4096;
    for(notation=0;notation<4;notation++)for(display=0;display<2;display++) {
        no.notation=(QJSIntlNumberNotation)notation;
        no.compact_display=(QJSIntlCompactDisplay)display;
        assert(qjs_intl_native_provider_number_open(provider,text("en"),text("latn"),
            &no,&numbers[notation][display])==QJS_INTL_OK);
    }
    qjs_intl_provider_free(provider); /* Every notation handle owns its snapshot. */
    for(notation=0;notation<4;notation++)for(display=0;display<2;display++) {
        QJSIntlNativeNumber *n=numbers[notation][display];
        check(&a,n,"-0",3,0,"0",NULL);
        check(&a,n,"999.999",3,notation==1?2:0,"999.999",NULL);
        check(&a,n,"0.001",3,(notation==1||notation==2)?-3:0,"0.001",NULL);
        check(&a,n,"999.9",0,notation?3:0,"1000",notation?rule:NULL);
        check(&a,n,"1000000",3,notation?6:0,"1000000",NULL);
        qjs_intl_native_number_close(n);
    }
    qjs_intl_native_plural_close(rule);assert(!heap.live);
#endif
    return 0;
}
