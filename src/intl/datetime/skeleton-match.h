/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_DATETIME_SKELETON_MATCH_H
#define QJS_INTL_DATETIME_SKELETON_MATCH_H
#include "source-candidate.h"

enum {
    QJS_INTL_DATE_NONCALENDAR_YEAR_WIDTH = 255,
    QJS_INTL_DATE_SKELETON_UNMATCHED = 10000
};

/* Source key field presence; UINT_MAX denotes unsupported skeleton symbols. */
unsigned int qjs_intl_date_skeleton_mask(QJSIntlBytes skeleton);
/* Year, month, day, era and weekday skeleton widths; zero means absent. */
void qjs_intl_date_skeleton_widths(QJSIntlBytes skeleton, unsigned char widths[5]);
/* Distance considers the complete key, including unrequested fields. */
int qjs_intl_date_skeleton_distance(QJSIntlBytes skeleton, const QJSIntlDateTimeComponents *request,
                                    QJSIntlDateHourCycle cycle, unsigned char widths[5]);
/* Preserve original key/pattern identity. Complete matches require all fields;
   partial matches maximize coverage and apply CLDR period, distance and
   field-priority rules. SIZE_MAX means no usable source; allocate nothing. */
QJSIntlStatus qjs_intl_date_skeleton_select(const QJSIntlDateSourceCandidates *pool,
                                            const QJSIntlDateTimeComponents *request,
                                            QJSIntlDateHourCycle cycle, int partial,
                                            size_t *selected, QJSIntlDateTimeComponents *basis);
#endif
