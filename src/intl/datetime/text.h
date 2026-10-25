/* Native date field text; arithmetic, source selection and zones are separate.
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
#ifndef QJS_INTL_DATETIME_TEXT_H
#define QJS_INTL_DATETIME_TEXT_H
#include "chronology.h"
#include "number-formats.h"
#include "pattern-info.h"
#include "calendar-identity.h"
#include "dayperiod.h"
#include "era-data.h"

typedef struct QJSIntlDateTextData {
    const QJSIntlCalendarNameData *names;
    const QJSIntlLocaleMetadata *metadata;
    const QJSIntlCalendarEraData *eras;
    const QJSIntlDayPeriodData *periods;
    const QJSIntlDayPeriodRuleList *period_rules;
    uint32_t locale, calendar; /* Indexes in names, not other source owners. */
    unsigned int use_midnight; /* Explicit 00:00 point-label policy; 0 or 1. */
} QJSIntlDateTextData;

/* Zero-initialize once for one unchanged locale/calendar/data/rule binding.
   Numeric-only calls without lunar month templates need not bind the cache. Cache only name
   proofs, not arithmetic, numeric or zone capabilities. */
typedef struct QJSIntlDateNameAdmission {
    const QJSIntlDateTextData *data;
    QJSCalendarId calendar;
    unsigned char fields[10][7];
} QJSIntlDateNameAdmission;

/* Prove every reachable name selection for the program using the renderer's
   own selectors. Allocate nothing; missing names return UNSUPPORTED, while
   operational/data errors propagate. Numeric-only programs do not require
   optional era/day-period metadata. Check both normal and twelve programs.
   A cache is optional and must not be rebound to another immutable input. */
QJSIntlStatus qjs_intl_date_names_admit(const QJSIntlDatePattern *pattern,
                                       QJSCalendarId calendar, const QJSIntlDateTextData *data,
                                       QJSIntlDateNameAdmission *cache);

/* Render an admitted ordinary non-zone program to owned UTF-16 and date parts.
   Supply chronology's unchanged calendar fields, with clock fields filled from
   the same local-time conversion. subsecond is a separate 0/1 fact about the
   full local instant (including precision finer than milliseconds); it must be
   true when millisecond is nonzero. Use resolved FORMAT rules for B, not the
   selection-rule list. Absent rules use the documented AM/PM fallback.

   All owners and NumberFormat instances remain immutable and outlive this
   operation. Numeric y<=0 becomes 1-y, while relatedYear stays signed.
   Truncate fractional milliseconds before localization; two-digit fields
   retain the last two Unicode code points of the whole formatted value. Lunar months apply the exact inherited leap template to formatted
   digits or names, without guessed width/context fallbacks. Numeric/all
   decoration follows CLDR's regular M/L handling; the current ECMA numeric
   pseudocode does not explicitly include this established calendar behavior.
   Hebrew Adar names use the year-specific name, not a lunar leap prefix.
   Era lookup uses explicit canonical-code mappings; ISO retains its localized
   names while historical Japanese uses Gregorian era labels. Unavailable,
   blocked or empty names/templates and zones return UNSUPPORTED. This is a
   rendering kernel, not the public service's locale admission/fallback policy.

   Require malloc/free, fresh disjoint output and a nonzero UTF-16 output bound.
   Inputs are unchanged; output is reset on errors. Clear the independently
   owned text/parts with formatted_clear and the same allocator. */
QJSIntlStatus qjs_intl_date_text_format(const QJSIntlAllocator *allocator,
                                       const QJSIntlDatePattern *pattern,
                                       const QJSIntlDateCalendarFields *local,
                                       unsigned int subsecond, QJSIntlDateHourCycle cycle,
                                       const QJSIntlDateNumberFormats *numbers,
                                       const QJSIntlDateTextData *data,
                                       size_t maximum_output_length, QJSIntlFormatted *out);
#endif
