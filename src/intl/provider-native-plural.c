/* Private native PluralRules data adapter. Copyright (c) 2026 Yan-Jie Wang. */
#include "provider-native-plural.h"
#include <string.h>
static QJSIntlStatus english(QJSIntlProvider *p, QJSIntlBytes locale, uint32_t *out)
{
    const QJSIntlDataView *v = qjs_intl_native_provider_view(p);
    QJSIntlDataSection s;
    QJSIntlDataSlice tag;
    uint32_t i;
    if (!p || !v || !locale.data) return QJS_INTL_INVALID_ARGUMENT;
    if (!((locale.length == 2 && !memcmp(locale.data,"en",2)) ||
          (locale.length == 5 && !memcmp(locale.data,"en-US",5))))
        return QJS_INTL_UNSUPPORTED;
    if (qjs_intl_data_section(v,QJS_INTL_DATA_LOCALE,&s) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    for (i=0;i<s.record_count;i++) {
        if (qjs_intl_data_record_string(v,&s,i,0,&tag) != QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (tag.length == 2 && !memcmp(tag.data,"en",2)) { *out=i; return QJS_INTL_OK; }
    }
    return QJS_INTL_UNSUPPORTED;
}
QJSIntlStatus qjs_intl_native_provider_plural_open(QJSIntlProvider *p,
    QJSIntlBytes locale,const QJSIntlPluralOptions *o,QJSIntlNativePlural **out)
{
    QJSIntlStatus status;uint32_t index;
    if(!out)return QJS_INTL_INVALID_ARGUMENT;
    *out=NULL;
    if(!o)return QJS_INTL_INVALID_ARGUMENT;
    status=english(p,locale,&index);if(status!=QJS_INTL_OK)return status;
    return qjs_intl_native_plural_open_data(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p),index,o,out);
}
QJSIntlStatus qjs_intl_native_provider_plural_available(QJSIntlProvider *p)
{
    QJSIntlPluralOptions o={0};QJSIntlNativePlural *h=NULL;
    QJSIntlNumberOptions number={0};QJSIntlNativeNumber *n=NULL;
    QJSIntlStatus status;unsigned int type,notation,display;
    o.digits.minimum_integer_digits=1;o.digits.maximum_fraction_digits=3;
    o.digits.minimum_significant_digits=1;o.digits.maximum_significant_digits=21;
    o.digits.rounding_increment=1;o.digits.rounding_mode=QJS_INTL_ROUND_HALF_EXPAND;
    o.digits.maximum_output_length=1048576;
    for(type=0;type<2;type++) {
        o.type=(QJSIntlPluralType)type;
        status=qjs_intl_native_provider_plural_open(p,(QJSIntlBytes){"en",2},&o,&h);
        qjs_intl_native_plural_close(h);h=NULL;if(status!=QJS_INTL_OK)return status;
    }
    number.digits=o.digits;number.maximum_output_length=1048576;
    for(notation=1;notation<4;notation++)for(display=0;display<2;display++) {
        number.notation=(QJSIntlNumberNotation)notation;
        number.compact_display=(QJSIntlCompactDisplay)display;
        status=qjs_intl_native_provider_number_open(p,(QJSIntlBytes){"en",2},
            (QJSIntlBytes){"latn",4},&number,&n);
        qjs_intl_native_number_close(n);n=NULL;if(status!=QJS_INTL_OK)return status;
    }
    return QJS_INTL_OK;
}
