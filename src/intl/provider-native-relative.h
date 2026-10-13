/* Private native RelativeTimeFormat adapter. Copyright (c) 2026 Yan-Jie Wang. */
#ifndef QJS_INTL_PROVIDER_NATIVE_RELATIVE_H
#define QJS_INTL_PROVIDER_NATIVE_RELATIVE_H
#include "provider-native.h"
#include "relative-native-data.h"
#include "provider-native-number.h"
QJSIntlStatus qjs_intl_native_provider_relative_open(QJSIntlProvider *,QJSIntlBytes,
    const QJSIntlRelativeOptions *,const QJSIntlRelativeNumberBridge *,QJSIntlNativeRelative **);
QJSIntlStatus qjs_intl_native_provider_relative_available(QJSIntlProvider *);
#endif
