/* Native DateTimeFormat adapter; Copyright (c) 2026 Yan-Jie Wang. MIT. */
#ifndef QJS_INTL_PROVIDER_NATIVE_DATE_H
#define QJS_INTL_PROVIDER_NATIVE_DATE_H
#include "provider-native.h"
#include "date-native-data.h"
#include "date-native-zone.h"
#include "date-native-calendar.h"
/* This state lives inside the JS formatter or another durable bank owner.
 * Only the actual data-proof mask is retained; no provider/view/data slices.
 * Bridge opaque pointers refer to this state until bank close. */
typedef struct QJSIntlNativeDateProviderEnvironment {
    QJSIntlDateCalendarBridge bridge;
    uint32_t calendar_mask;
} QJSIntlNativeDateProviderEnvironment;
/* The timezone runtime owner retains the borrowed provider through bank close.
 * Bank handles copy decoded formatter data. No provider structs/data owners. */
QJSIntlStatus qjs_intl_native_provider_date_open(QJSIntlProvider *,
    QJSIntlBytes data_locale, QJSIntlBytes numbering_system,
    const QJSIntlDateBankOptions *, QJSTzProvider *,
    QJSIntlNativeDateProviderEnvironment *, QJSIntlNativeDateBank **);
QJSIntlStatus qjs_intl_native_provider_date_available(QJSIntlProvider *,
                                                     QJSIntlTagList *);
QJSIntlStatus qjs_intl_native_provider_date_key_values(QJSIntlProvider *,
    QJSIntlBytes data_locale, QJSIntlBytes key, QJSIntlTagList *);
/* Query separately from hc resolution because hour12 suppresses hc. */
QJSIntlStatus qjs_intl_native_provider_date_preferred_hour_cycle(
    QJSIntlProvider *, QJSIntlBytes data_locale, unsigned int *);
#endif
