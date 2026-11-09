/* Explicit field adaptations shared by native single-date constructors.
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
#ifndef QJS_INTL_DATETIME_PROGRAM_ADAPT_H
#define QJS_INTL_DATETIME_PROGRAM_ADAPT_H
#include "pattern-info.h"
#include "source-candidate.h"

enum {
    QJS_INTL_DATE_ADAPT_HOUR_WIDTH = 1,
    QJS_INTL_DATE_ADAPT_HOUR_FAMILY = 2,
    QJS_INTL_DATE_ADAPT_MINUTE_WIDTH = 4,
    QJS_INTL_DATE_ADAPT_SECOND_WIDTH = 8,
    QJS_INTL_DATE_ADAPT_ZONE_APPEND = 16,
    QJS_INTL_DATE_ADAPT_ZONE_EXTRACT = 32,
    QJS_INTL_DATE_ADAPT_PERIOD_LAYOUT = 64,
    QJS_INTL_DATE_ADAPT_PERIOD_WIDTH = 128,
    QJS_INTL_DATE_ADAPT_YEAR_WIDTH = 256,
    QJS_INTL_DATE_ADAPT_MONTH_WIDTH = 512,
    QJS_INTL_DATE_ADAPT_DAY_WIDTH = 1024,
    QJS_INTL_DATE_ADAPT_ERA_WIDTH = 2048,
    QJS_INTL_DATE_ADAPT_WEEKDAY_WIDTH = 4096
};

enum {
    QJS_INTL_DATE_NONCALENDAR_YEAR_WIDTH = 255,
    QJS_INTL_DATE_SKELETON_UNMATCHED = 10000
};

/* Year, month, day, era and weekday skeleton widths; zero means absent. */
void qjs_intl_date_skeleton_widths(QJSIntlBytes skeleton, unsigned char widths[5]);
/* Distance considers the complete key, including unrequested fields. */
int qjs_intl_date_skeleton_distance(QJSIntlBytes skeleton, const QJSIntlDateTimeComponents *request,
                                    QJSIntlDateHourCycle cycle, unsigned char widths[5]);
/* Compare whole skeletons only when they select the same source text. */
int qjs_intl_date_best_alias(const QJSIntlDateSourceCandidates *pool,
                             const QJSIntlDateSourceCandidate *selected,
                             const QJSIntlDateTimeComponents *request,
                             QJSIntlDateHourCycle cycle, unsigned char widths[5]);

/* Copy an admitted ordinary program to an exact component vector. Permit only
   explicitly allowed numeric date and HOUR/MINUTE/SECOND width changes, plus the requested
   hour family (twelve is 0/1). PERIOD_WIDTH additionally permits textual b/B
   width changes; the caller must prove source policy and name availability.
   Month changes stay within numeric or textual widths; era and weekday changes
   are textual. Related and cyclic years are unchanged.
   Other component differences are UNSUPPORTED. Preserve year kinds, period
   symbol, other nonnumeric fields and literals. Reproject before
   returning; h11/h24 presentation remains with the renderer. Own copied storage,
   clear via pattern_clear with the same allocator. Require unchanged inputs and
   fresh disjoint outputs; reset both on failure, release partial copies. */
QJSIntlStatus qjs_intl_date_program_adapt(const QJSIntlAllocator *allocator,
                                        const QJSIntlDatePattern *source,
                                        const QJSIntlDateTimeComponents *components,
                                        unsigned int allowed_widths, unsigned int twelve,
                                        QJSIntlDatePattern *out, unsigned int *adaptations);
#endif
