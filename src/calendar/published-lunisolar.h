/* Provider contract for pinned published lunisolar month data.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 * No parser, mutable installation, network, or ICU runtime dependency.
 */
#ifndef QUICKJS_CALENDAR_PUBLISHED_LUNISOLAR_H
#define QUICKJS_CALENDAR_PUBLISHED_LUNISOLAR_H
#include "calendar.h"
#include "calendar_month_record.h"

enum {
    QJS_LUNISOLAR_PRIMARY_PMO = 1,
    QJS_LUNISOLAR_PRIMARY_KASI = 2
};

/* Half-open date interval whose fields are backed by the named primary
 * publication. Approximation has no primary span and cannot acquire one.
 * A provider's spans are immutable, sorted, disjoint and independently audited.
 */
typedef struct QJSLunisolarPrimarySpan {
    int32_t first_day, end_day;
    uint8_t authority;
} QJSLunisolarPrimarySpan;

typedef struct QJSLunisolarProvider {
    /* This legacy record container is the composite arithmetic sequence.
     * Its proven bounds certify month continuity, not primary provenance.
     * Primary provenance is stated solely by the separate date spans below.
     */
    const QJSPublishedCalendarTable *arithmetic;
    const QJSLunisolarPrimarySpan *primary_spans;
    size_t primary_span_count;
} QJSLunisolarProvider;

/* Define QJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA only for root review of an
 * independently audited compiled provider. Mandatory Gregorian dates must
 * be covered by PMO (Chinese) or KASI (Dangi) spans. Approximation may complete
 * boundary-year metadata solely for dates outside those Gregorian windows.
 * NULL means unavailable. This contract does not authorize ID discovery.
 */
const QJSLunisolarProvider *qjs_calendar_lunisolar_provider(
    QJSCalendarId calendar);
#endif
