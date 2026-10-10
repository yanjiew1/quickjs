/* Private DurationFormat adapter. Copyright (c) 2026 Yan-Jie Wang. */
#ifndef QJS_INTL_PROVIDER_NATIVE_DURATION_H
#define QJS_INTL_PROVIDER_NATIVE_DURATION_H
#include "provider-native-number.h"
#include "duration-native-data.h"
/* All inputs have been resolved by the frontend. Open copies every needed
 * Number/Cardinal/List/clock slice; the handle does not retain provider/view.
 * Its runtime allocator opaque must outlive the handle and its results. */
QJSIntlStatus qjs_intl_native_provider_duration_clock(QJSIntlProvider *,
    QJSIntlBytes locale, QJSIntlBytes numbering, uint8_t *two_digit_hours);
QJSIntlStatus qjs_intl_native_provider_duration_open(QJSIntlProvider *,
    QJSIntlBytes locale, QJSIntlBytes numbering,
    const QJSIntlDurationOptions *, QJSIntlNativeDuration **);
QJSIntlStatus qjs_intl_native_provider_duration_available(QJSIntlProvider *);
QJSIntlStatus qjs_intl_native_provider_duration_key_values(QJSIntlProvider *,
    QJSIntlBytes locale, QJSIntlBytes key, QJSIntlTagList *);
#endif
