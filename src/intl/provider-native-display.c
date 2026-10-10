/* Explicit en/en-US development admission; one shared immutable view.
 * Copyright (c) 2026 Yan-Jie Wang. */
#include "provider-native-display.h"
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
QJSIntlStatus qjs_intl_native_provider_display_open(QJSIntlProvider *p,
    QJSIntlBytes locale,const QJSIntlDisplayNamesOptions *o,QJSIntlNativeDisplayNames **out)
{
    QJSIntlStatus status;
    uint32_t index;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out=NULL;
    if (!o) return QJS_INTL_INVALID_ARGUMENT;
    status=english(p,locale,&index);
    if (status != QJS_INTL_OK) return status;
    return qjs_intl_native_display_names_open(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p),index,o,out);
}
QJSIntlStatus qjs_intl_native_provider_display_available(QJSIntlProvider *p)
{
    QJSIntlDisplayNamesOptions o;
    QJSIntlNativeDisplayNames *h=NULL;
    QJSIntlStatus status;
    unsigned int type,style,dialect;
    memset(&o,0,sizeof(o)); o.fallback_code=1;
    for (type=0;type<6;type++) for (style=0;style<3;style++)
        for (dialect=0;dialect<2;dialect++) {
            o.type=(QJSIntlDisplayNamesType)type;
            o.style=(QJSIntlDisplayNamesStyle)style;
            o.language_dialect=dialect;
            status=qjs_intl_native_provider_display_open(p,(QJSIntlBytes){"en",2},&o,&h);
            qjs_intl_native_display_names_close(h);h=NULL;
            if (status != QJS_INTL_OK) return status;
        }
    return QJS_INTL_OK;
}
