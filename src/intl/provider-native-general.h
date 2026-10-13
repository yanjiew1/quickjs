/* Private native Locale information bridge; no embedding ABI addition. */
#ifndef QJS_INTL_PROVIDER_NATIVE_GENERAL_H
#define QJS_INTL_PROVIDER_NATIVE_GENERAL_H
#include "native-locale-info.h"
#include "provider-native.h"
QJSIntlStatus qjs_intl_native_provider_locale_info_get(const QJSIntlAllocator *,
    const QJSIntlDataView *, QJSIntlProvider *, const QJSIntlLocaleInfoRequest *,
    QJSIntlLocaleInfoField, QJSIntlLocaleInfoResult *);
#endif
