/* Complete single-instant date text over prepared programs and shared rules.
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
#ifndef QJS_INTL_DATETIME_FORMAT_H
#define QJS_INTL_DATETIME_FORMAT_H
#include "local-time.h"
#include "text.h"
#include "zone-name.h"
#include "../../timezone/identifier.h"

typedef struct QJSIntlDateFormatData {
    QJSIntlDateTextData text;
    QJSIntlDateZoneData zones;
    const QJSIntlDateOffset *offset;
} QJSIntlDateFormatData;

/* Render an admitted ordinary program from floor epoch seconds and a positive
   nanosecond remainder. Chronology/clock/name/zone fields use the same instant.
   The frontend owns coercion, range limits and Temporal plain-value semantics:
   for plain values encode the local ISO fields as UTC and supply named UTC.
   This operation never discovers a host zone or reads mutable JS state.

   Data views, prepared offset and NumberFormat handles remain unchanged and
   use this allocator; they outlive the call. Output owns independent UTF-16 and
   typed parts in token order. Require malloc/free and a nonzero UTF-16 bound;
   fresh disjoint output is reset on failure. Clear with formatted_clear. */
QJSIntlStatus qjs_intl_date_format(const QJSIntlAllocator *allocator,
                                  const QJSIntlDatePattern *pattern, QJSCalendarId calendar,
                                  int64_t epoch_seconds, uint32_t nanosecond,
                                  QJSIntlDateHourCycle cycle,
                                  const QJSIntlDateNumberFormats *numbers,
                                  const QJSIntlDateFormatData *data,
                                  const QJSTimeZoneIdentifier *zone,
                                  size_t maximum_output_length, QJSIntlFormatted *out);
#endif
