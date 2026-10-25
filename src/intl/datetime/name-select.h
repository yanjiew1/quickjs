/* Native localized field names, separate from chronology and text rendering.
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
#ifndef QJS_INTL_DATETIME_NAME_SELECT_H
#define QJS_INTL_DATETIME_NAME_SELECT_H
#include "name-data.h"
#include "dayperiod-code.h"

typedef enum QJSIntlDateNameState {
    QJS_INTL_DATE_NAME_MISSING,
    QJS_INTL_DATE_NAME_TEXT,
    QJS_INTL_DATE_NAME_BLOCKED,
    QJS_INTL_DATE_NAME_EMPTY
} QJSIntlDateNameState;

typedef struct QJSIntlDateNameSelection {
    QJSIntlDateNameState state;
    QJSIntlCalendarNameEntry entry;
    QJSIntlCalendarNameNode origin;
} QJSIntlDateNameSelection;

typedef struct QJSIntlDatePeriodNameSelection {
    QJSIntlDateNameState primary_state;
    QJSIntlDateNameSelection selected;
} QJSIntlDatePeriodNameSelection;

/* Require an unchanged validated owner and its locale/calendar indexes. Map
   admitted G/M/L/E/e/c/U/a/b/B name symbols and widths to the recorded group,
   then resolve the supplied raw source code with ordinary alt="". Calendar
   chronology supplies that code: ordinal months and canonical era names are
   not necessarily CLDR keys. Numeric/unrenderable fields return UNSUPPORTED.
   leap_year is 0/1, used only for M/L: prefer explicit LEAP_YEAR or STANDARD_YEAR
   respectively, then NO_YEAR only if that exact key is missing. Exhaust each
   key's inheritance before trying the default. BLOCK/EMPTY stop substitution.
   Only missing weekday short names additionally try abbreviated. All other
   sideways/context/calendar inheritance follows prepared aliases, not guesses.
   Missing names return OK/MISSING, distinct from BLOCKED/EMPTY observations.
   Allocate nothing; borrow pack text/origin, require disjoint output and reset
   it on errors. The unchanged pack must outlive all returned slices. */
QJSIntlStatus qjs_intl_date_name_select(const QJSIntlCalendarNameData *data,
                                      uint32_t locale, uint32_t calendar, unsigned int symbol,
                                      size_t width, QJSIntlBytes code, unsigned int leap_year,
                                      QJSIntlDateNameSelection *out);
/* Select a/b/B's primary clock code, then substitute the actual local AM/PM
   code if its translated label is missing, blocked or empty. Retain the primary
   observation even when substitution is unavailable. No clock arithmetic or
   supplemental-rule lookup occurs here. Require fallback to be AM or PM, and
   primary/fallback to be consistent for fixed a/b codes. Width/context/year
   behavior and ownership match name_select. */
QJSIntlStatus qjs_intl_date_period_name_select(const QJSIntlCalendarNameData *data,
                                             uint32_t locale, uint32_t calendar, unsigned int symbol,
                                             size_t width, QJSIntlDayPeriodCode primary,
                                             QJSIntlDayPeriodCode fallback,
                                             QJSIntlDatePeriodNameSelection *out);
#endif
