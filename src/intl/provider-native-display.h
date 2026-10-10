/* Private native DisplayNames adapter. Copyright (c) 2026 Yan-Jie Wang. */
#ifndef QJS_INTL_PROVIDER_NATIVE_DISPLAY_H
#define QJS_INTL_PROVIDER_NATIVE_DISPLAY_H
#include "provider-native.h"
#include "display-names-native.h"
QJSIntlStatus qjs_intl_native_provider_display_open(QJSIntlProvider *,
    QJSIntlBytes, const QJSIntlDisplayNamesOptions *, QJSIntlNativeDisplayNames **);
QJSIntlStatus qjs_intl_native_provider_display_available(QJSIntlProvider *);
#endif
