/* Calendar arithmetic to native date-formatting fields; no localized data.
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
#ifndef QJS_INTL_DATETIME_CHRONOLOGY_H
#define QJS_INTL_DATETIME_CHRONOLOGY_H
#include "fields.h"
#include "../../calendar/types.h"

typedef struct QJSIntlDateCalendarFields {
    QJSIntlDateNumericFields numeric;
    int32_t arithmetic_year, era_year;
    QJSCalendarId era_calendar;
    unsigned int weekday, in_leap_year, leap_month;
    unsigned int has_era, has_cyclic_year, cyclic_year;
    char month_code[5], month_key[3], cyclic_key[3], era_code[32];
} QJSIntlDateCalendarFields;

/* Convert a local epoch day through the selected calendar arithmetic facade.
   Epoch day zero is 1970-01-01. Own all code buffers; allocate/borrow nothing,
   reset output on failure. No timezone conversion or localized-name lookup
   occurs here; numeric clock fields are zero for the
   caller to fill from the same local-time calculation.

   Preserve arithmetic and era years separately. Supply unnormalized y to the
   renderer: modern Japanese and Ethiopic use era_year, other calendars year.
   The renderer alone applies its prescribed nonpositive-year adjustment.
   ISO's explicit Gregorian era presentation and pre-adoption Japanese select
   GREGORY as era source without changing calendar arithmetic. Mandatory
   Gregorian formatting uses core ISO arithmetic even without native calendars.
   Chinese/Dangi expose related/cyclic year, nominal numeric month and an
   independent leap flag.
   Hebrew numeric month remains ordinal; its raw name key is independently
   fixed by monthCode. Raw era-key mapping remains in era-data. */
QJSIntlStatus qjs_intl_date_calendar_fields(QJSCalendarId calendar, int64_t epoch_day,
                                           QJSIntlDateCalendarFields *out);
#endif
