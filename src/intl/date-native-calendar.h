/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_NATIVE_CALENDAR_H
#define QJS_INTL_DATE_NATIVE_CALENDAR_H
#include "date-native.h"
#include "calendar/calendar.h"

/* Shared calendar arithmetic is owned by src/calendar, never copied here.
 * resolve_names validates real calendar data and maps canonical era/monthCode
 * to this formatter's name IDs. Numeric month remains the ordinal month.
 * supports_data must also gate real patterns/names for the calendar. Neither
 * this bridge nor arithmetic support enables an Intl calendar advertisement.
 */
typedef struct QJSIntlDateCalendarBridge {
    QJSIntlDateEnvironment upstream; /* zone callback and opaque remain borrowed */
    void *names_opaque;
    int (*supports_data)(void *, QJSCalendarId);
    QJSIntlStatus (*resolve_names)(void *, QJSCalendarId,
        const QJSCalendarDate *, unsigned int *era_index,
        unsigned int *month_name_index);
} QJSIntlDateCalendarBridge;
/* bridge must outlive every native DateTimeFormat using the returned env. */
QJSIntlStatus qjs_intl_native_date_calendar_environment(
    QJSIntlDateCalendarBridge *, QJSIntlDateEnvironment *);
/* Stable collector-owned CLDR name IDs. This callback neither borrows a
 * data view nor supplies any locale text; supports_data remains mandatory.
 * Both result indices stay unchanged on failure. opaque is unused.
 */
QJSIntlStatus qjs_intl_native_date_calendar_names(void *, QJSCalendarId,
    const QJSCalendarDate *, unsigned int *era_index,
    unsigned int *month_name_index);
#endif
