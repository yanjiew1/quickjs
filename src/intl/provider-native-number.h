/* Private NumberFormat adapter. Copyright (c) 2026 Yan-Jie Wang. */
#ifndef QJS_INTL_PROVIDER_NATIVE_NUMBER_H
#define QJS_INTL_PROVIDER_NATIVE_NUMBER_H
#include "provider-native.h"
#include "number-native-data.h"
/* Provider view accessor is the single shared Collator/Segmenter owner.
 * Returned view is borrowed while provider is alive. Number open snapshots
 * all needed data; the resulting handle does not retain provider/view. */
const QJSIntlDataView *qjs_intl_native_provider_view(const QJSIntlProvider *);
QJSIntlStatus qjs_intl_native_provider_number_open(QJSIntlProvider *,
    QJSIntlBytes locale, QJSIntlBytes numbering, const QJSIntlNumberOptions *, QJSIntlNativeNumber **);
QJSIntlStatus qjs_intl_native_provider_number_currency_digits(QJSIntlProvider *,
    QJSIntlBytes currency, unsigned int *);
QJSIntlStatus qjs_intl_native_provider_number_key_values(QJSIntlProvider *,
    QJSIntlBytes locale, QJSIntlBytes key, QJSIntlTagList *);
QJSIntlStatus qjs_intl_native_provider_number_available(QJSIntlProvider *);
#endif
