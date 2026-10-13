/* Private native PluralRules adapter. Copyright (c) 2026 Yan-Jie Wang. */
#ifndef QJS_INTL_PROVIDER_NATIVE_PLURAL_H
#define QJS_INTL_PROVIDER_NATIVE_PLURAL_H
#include "provider-native.h"
#include "plural-native-data.h"
#include "provider-native-number.h"
QJSIntlStatus qjs_intl_native_provider_plural_open(QJSIntlProvider *,QJSIntlBytes,
    const QJSIntlPluralOptions *,QJSIntlNativePlural **);
QJSIntlStatus qjs_intl_native_provider_plural_available(QJSIntlProvider *);
#endif
