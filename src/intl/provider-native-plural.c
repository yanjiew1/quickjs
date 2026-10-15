/* Private native PluralRules data adapter. Copyright (c) 2026 Yan-Jie Wang. */
#include "provider-native-plural.h"
#include "native-data-validation-internal.h"
#include <string.h>
QJSIntlStatus qjs_intl_native_provider_plural_open(QJSIntlProvider *p,
    QJSIntlBytes locale,const QJSIntlPluralOptions *o,QJSIntlNativePlural **out)
{
    QJSIntlStatus status;uint32_t index;
    if(!out)return QJS_INTL_INVALID_ARGUMENT;
    *out=NULL;
    if(!o)return QJS_INTL_INVALID_ARGUMENT;
    status=qjs_intl_native_provider_locale_row(p,locale,&index);if(status!=QJS_INTL_OK)return status;
    return qjs_intl_native_plural_open_data_with_validator(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p),index,o,qjs_intl_native_provider_validate_compiled_data,p,out);
}
QJSIntlStatus qjs_intl_native_provider_plural_locale_available(QJSIntlProvider *p,
    QJSIntlBytes locale, uint32_t row)
{
    QJSIntlPluralOptions o={0};QJSIntlNativePlural *h=NULL;
    QJSIntlNumberOptions number={0};QJSIntlNativeNumber *n=NULL;
    QJSIntlStatus status;unsigned int type,notation,display;
    QJSIntlBytes numbering;
    status=qjs_intl_native_provider_default_numbering(p,row,&numbering);
    if(status!=QJS_INTL_OK)return status;
    o.digits.minimum_integer_digits=1;o.digits.maximum_fraction_digits=3;
    o.digits.minimum_significant_digits=1;o.digits.maximum_significant_digits=21;
    o.digits.rounding_increment=1;o.digits.rounding_mode=QJS_INTL_ROUND_HALF_EXPAND;
    o.digits.maximum_output_length=1048576;
    for(type=0;type<2;type++) {
        o.type=(QJSIntlPluralType)type;
        status=qjs_intl_native_provider_plural_open(p,locale,&o,&h);
        qjs_intl_native_plural_close(h);h=NULL;if(status!=QJS_INTL_OK)return status;
    }
    number.digits=o.digits;number.maximum_output_length=1048576;
    for(notation=1;notation<4;notation++)for(display=0;display<2;display++) {
        number.notation=(QJSIntlNumberNotation)notation;
        number.compact_display=(QJSIntlCompactDisplay)display;
        status=qjs_intl_native_provider_number_open(p,locale,
            numbering,&number,&n);
        qjs_intl_native_number_close(n);n=NULL;if(status!=QJS_INTL_OK)return status;
    }
    return QJS_INTL_OK;
}
