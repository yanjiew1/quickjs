/* Immutable, endian-independent calendar publication and era records.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#ifndef QUICKJS_CALENDAR_DATA_H
#define QUICKJS_CALENDAR_DATA_H
#include <stddef.h>
#include <stdint.h>
#include "types.h"

typedef struct QJSCalendarData {
    const uint8_t *bytes;
    size_t size;
    uint32_t count;
} QJSCalendarData;

/* The caller retains the immutable byte span for the view's lifetime. Open
   validates the complete pack. Queries require that view (or an equivalently
   validated compiled view), allocate nothing and preserve outputs on failure. */
int qjs_calendar_data_open(QJSCalendarData *result, const uint8_t *bytes,
                           size_t size);
int qjs_calendar_data_year(const QJSCalendarData *data, QJSCalendarId calendar,
                           int32_t year, QJSCalendarYear *result);
/* Required official lunisolar years only: Chinese 1900..2100 and Dangi
   1900..2050. UNSUPPORTED means outside that contract; a missing required row
   is a data error, never permission to silently substitute an approximation. */
int qjs_calendar_official_year(QJSCalendarId calendar, int32_t year,
                                QJSCalendarYear *result);
/* Resolve the same records by actual local epoch day, without trusting a
   foreign calendar's possibly incorrect month/year labels. */
int qjs_calendar_official_date(QJSCalendarId calendar, int64_t epoch_day,
                                QJSCalendarDate *result);
const char *qjs_calendar_data_version(const QJSCalendarData *data,
                                      QJSCalendarId calendar);
/* Era names borrow the same immutable span as the data view. An absent era
   dataset has count zero. Queries preserve outputs on failure. */
size_t qjs_calendar_data_japanese_era_count(const QJSCalendarData *data);
int qjs_calendar_data_japanese_era(const QJSCalendarData *data, size_t index,
                                   QJSCalendarEra *result);
extern const QJSCalendarData qjs_calendar_publication_data;
#endif /* QUICKJS_CALENDAR_DATA_H */
