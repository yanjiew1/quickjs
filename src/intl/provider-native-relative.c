/* Private native RelativeTimeFormat data adapter. Copyright (c) 2026 Yan-Jie Wang. */
#include "provider-native-relative.h"
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
QJSIntlStatus qjs_intl_native_provider_relative_open(QJSIntlProvider *p,
    QJSIntlBytes locale,const QJSIntlRelativeOptions *o,
    const QJSIntlRelativeNumberBridge *bridge,QJSIntlNativeRelative **out)
{
    QJSIntlStatus status;uint32_t index;
    if(!out)return QJS_INTL_INVALID_ARGUMENT;
    *out=NULL;
    if(!o || !bridge)return QJS_INTL_INVALID_ARGUMENT;
    status=english(p,locale,&index);if(status!=QJS_INTL_OK)return status;
    return qjs_intl_native_relative_open_data(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p),index,o,bridge,out);
}
static QJSIntlStatus format_rounded(void *opaque,const QJSIntlDecimalResult *raw,
                                    QJSIntlFormatted *out)
{
    return qjs_intl_native_number_format_rounded(opaque,raw,out);
}
static void clear_rounded(void *opaque,QJSIntlFormatted *out)
{
    qjs_intl_native_number_result_clear(opaque,out);
}
QJSIntlStatus qjs_intl_native_provider_relative_available(QJSIntlProvider *p)
{
    QJSIntlNumberOptions number={0};QJSIntlRelativeOptions o={0};
    QJSIntlNativeNumber *n=NULL;QJSIntlNativeRelative *h=NULL;
    QJSIntlRelativeNumberBridge bridge;
    QJSIntlStatus status;unsigned int style,numeric;
    number.grouping=QJS_INTL_NUMBER_GROUP_AUTO;
    number.digits.minimum_integer_digits=1;number.digits.maximum_fraction_digits=3;
    number.digits.minimum_significant_digits=1;number.digits.maximum_significant_digits=21;
    number.digits.rounding_increment=1;number.digits.rounding_mode=QJS_INTL_ROUND_HALF_EXPAND;
    number.digits.maximum_output_length=number.maximum_output_length=1048576;
    status=qjs_intl_native_provider_number_open(p,(QJSIntlBytes){"en",2},
        (QJSIntlBytes){"latn",4},&number,&n);
    if(status!=QJS_INTL_OK)return status;
    bridge=(QJSIntlRelativeNumberBridge){n,format_rounded,clear_rounded};
    o.maximum_output_length=1048576;
    for(style=0;style<3;style++)for(numeric=0;numeric<2;numeric++) {
        o.style=(QJSIntlRelativeStyle)style;o.numeric=(QJSIntlRelativeNumeric)numeric;
        status=qjs_intl_native_provider_relative_open(p,(QJSIntlBytes){"en",2},&o,&bridge,&h);
        qjs_intl_native_relative_close(h);h=NULL;if(status!=QJS_INTL_OK)goto done;
    }
done:
    qjs_intl_native_number_close(n);return status;
}
