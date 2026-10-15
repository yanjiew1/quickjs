/* Private native RelativeTimeFormat data adapter. Copyright (c) 2026 Yan-Jie Wang. */
#include "provider-native-relative.h"
#include "native-data-validation-internal.h"
QJSIntlStatus qjs_intl_native_provider_relative_open(QJSIntlProvider *p,
    QJSIntlBytes locale,const QJSIntlRelativeOptions *o,
    const QJSIntlRelativeNumberBridge *bridge,QJSIntlNativeRelative **out)
{
    QJSIntlStatus status;uint32_t index;
    if(!out)return QJS_INTL_INVALID_ARGUMENT;
    *out=NULL;
    if(!o || !bridge)return QJS_INTL_INVALID_ARGUMENT;
    status=qjs_intl_native_provider_locale_row(p,locale,&index);if(status!=QJS_INTL_OK)return status;
    return qjs_intl_native_relative_open_data_with_validator(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p),index,o,bridge,qjs_intl_native_provider_validate_compiled_data,p,out);
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
QJSIntlStatus qjs_intl_native_provider_relative_locale_available(QJSIntlProvider *p,
    QJSIntlBytes locale, uint32_t row)
{
    QJSIntlNumberOptions number={0};QJSIntlRelativeOptions o={0};
    QJSIntlNativeNumber *n=NULL;QJSIntlNativeRelative *h=NULL;
    QJSIntlRelativeNumberBridge bridge;
    QJSIntlStatus status;unsigned int style,numeric;
    QJSIntlBytes numbering;
    status=qjs_intl_native_provider_default_numbering(p,row,&numbering);
    if(status!=QJS_INTL_OK)return status;
    number.grouping=QJS_INTL_NUMBER_GROUP_AUTO;
    number.digits.minimum_integer_digits=1;number.digits.maximum_fraction_digits=3;
    number.digits.minimum_significant_digits=1;number.digits.maximum_significant_digits=21;
    number.digits.rounding_increment=1;number.digits.rounding_mode=QJS_INTL_ROUND_HALF_EXPAND;
    number.digits.maximum_output_length=number.maximum_output_length=1048576;
    status=qjs_intl_native_provider_number_open(p,locale,
        numbering,&number,&n);
    if(status!=QJS_INTL_OK)return status;
    bridge=(QJSIntlRelativeNumberBridge){n,format_rounded,clear_rounded};
    o.maximum_output_length=1048576;
    for(style=0;style<3;style++)for(numeric=0;numeric<2;numeric++) {
        o.style=(QJSIntlRelativeStyle)style;o.numeric=(QJSIntlRelativeNumeric)numeric;
        status=qjs_intl_native_provider_relative_open(p,locale,&o,&bridge,&h);
        qjs_intl_native_relative_close(h);h=NULL;if(status!=QJS_INTL_OK)goto done;
    }
done:
    qjs_intl_native_number_close(n);return status;
}
