/* Native interval matching and ownership over validated CLDR source layers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
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
   extra range context. Preserve ordinary year/period semantics and represent the
   changed field at both endpoints. Never resurrect blocked/empty leaves.
   Missing or unrenderable candidates leave the corresponding fallback slot.
   period_spacing is zero for exact source literals, or source-proved h/m/s
   suffix bits from period-spacing.h. Apply only quote-aware isolated a spacing
   in owned programs; preserve all other literals and immutable raw sources.

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
                                             unsigned int period_spacing,
                                             const QJSIntlAllocator *allocator,
                                             QJSIntlDateIntervalSet *out);
void qjs_intl_date_interval_set_clear(QJSIntlDateIntervalSet *set);
/* Borrow the selected program until clear; NULL means full-endpoint fallback. */
const QJSIntlDateIntervalProgram *qjs_intl_date_interval_set_get(const QJSIntlDateIntervalSet *set,
                                                               QJSIntlDateRangeField field);
#endif
