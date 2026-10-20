/* Native interval matching and ownership over validated CLDR source layers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_INTERVAL_SELECT_H
#define QJS_INTL_DATETIME_INTERVAL_SELECT_H
#include "interval-data.h"
#include "interval-program.h"
#include "text.h"

typedef enum QJSIntlDateRangeField {
    QJS_INTL_DATE_RANGE_ERA,
    QJS_INTL_DATE_RANGE_YEAR,
    QJS_INTL_DATE_RANGE_MONTH,
    QJS_INTL_DATE_RANGE_DAY,
    QJS_INTL_DATE_RANGE_AM_PM,
    QJS_INTL_DATE_RANGE_DAY_PERIOD,
    QJS_INTL_DATE_RANGE_HOUR,
    QJS_INTL_DATE_RANGE_MINUTE,
    QJS_INTL_DATE_RANGE_SECOND,
    QJS_INTL_DATE_RANGE_FRACTION,
    QJS_INTL_DATE_RANGE_FIELD_COUNT
} QJSIntlDateRangeField;

typedef struct QJSIntlDateIntervalSet {
    QJSIntlAllocator allocator;
    QJSIntlDateIntervalProgram *records;
    size_t count;
    uint8_t indexes[QJS_INTL_DATE_RANGE_FIELD_COUNT]; /* One-based, zero missing. */
} QJSIntlDateIntervalSet;

/* Last comparison field for a validated component record. A fieldless record
   retains Default-only comparison through milliseconds. */
QJSIntlDateRangeField qjs_intl_date_range_last_field(const QJSIntlDateTimeComponents *components);

/* Resolve exact ordinary-alternate source chains, score usable skeletons in
   stable source order, compile improving per-field candidates and admit names.
   Require every ordinary visible field in the actual interval union, allowing
   extra range context. Preserve ordinary period semantics and represent the
   changed field at both endpoints. Never resurrect blocked/empty leaves.
   Missing or unrenderable candidates leave the corresponding fallback slot.

   Locale/calendar indexes belong to this interval owner. Names and its cache
   remain unchanged except for cached finite-domain proofs; they use their own
   indexes and the supplied arithmetic calendar. Own all returned programs,
   independent of source/name views and ordinary pattern storage. Opening
   allocates no result when the set is empty. Require malloc/free and fresh
   disjoint output; reset errors. Clear with the retained allocator. */
QJSIntlStatus qjs_intl_date_interval_set_open(const QJSIntlDateIntervalData *data,
                                             uint32_t locale, uint32_t calendar,
                                             const QJSIntlDatePattern *ordinary,
                                             QJSIntlDateHourCycle cycle,
                                             QJSCalendarId arithmetic_calendar,
                                             const QJSIntlDateTextData *names,
                                             QJSIntlDateNameAdmission *admission,
                                             const QJSIntlAllocator *allocator,
                                             QJSIntlDateIntervalSet *out);
void qjs_intl_date_interval_set_clear(QJSIntlDateIntervalSet *set);
/* Borrow the selected program until clear; NULL means full-endpoint fallback. */
const QJSIntlDateIntervalProgram *qjs_intl_date_interval_set_get(const QJSIntlDateIntervalSet *set,
                                                               QJSIntlDateRangeField field);
#endif
