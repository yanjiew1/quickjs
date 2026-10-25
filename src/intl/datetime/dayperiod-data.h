/* Explicit supplemental clock-rule origins, without inheritance or rendering.
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
#ifndef QJS_INTL_DATETIME_DAYPERIOD_DATA_H
#define QJS_INTL_DATETIME_DAYPERIOD_DATA_H
#include "dayperiod-code.h"
#include "../native/data.h"

typedef enum QJSIntlDayPeriodRuleSetKind {
    QJS_INTL_DAYPERIOD_FORMAT,
    QJS_INTL_DAYPERIOD_SELECTION,
    QJS_INTL_DAYPERIOD_KIND_COUNT
} QJSIntlDayPeriodRuleSetKind;

typedef enum QJSIntlDayPeriodRuleForm {
    QJS_INTL_DAYPERIOD_AT,
    QJS_INTL_DAYPERIOD_RANGE
} QJSIntlDayPeriodRuleForm;

typedef struct QJSIntlDayPeriodRule {
    QJSIntlDayPeriodCode code;
    QJSIntlDayPeriodRuleForm form;
    uint32_t start, end; /* Local clock seconds; ranges are half-open and may wrap. */
} QJSIntlDayPeriodRule;

typedef struct QJSIntlDayPeriodData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *rows, *maps, *rules;
    uint32_t row_count, map_count, rule_count;
} QJSIntlDayPeriodData;

/* Borrow an unchanged validated shared owner. Require malloc/free and release
   all validation scratch before return. Follow pack.h's disjoint-output
   contract; clear non-NULL output on failure. Admit typed source facts, not
   locale availability, translated-name completeness or formatter capability. */
QJSIntlStatus qjs_intl_dayperiod_data_open(const QJSIntlData *data,
                                           const QJSIntlAllocator *allocator,
                                           QJSIntlDayPeriodData *out);
/* Zero-based indexes; queries allocate nothing and borrow storage. Missing exact
   origin returns UNSUPPORTED. All non-NULL outputs reset on failure. Inheritance,
   fractional point eligibility, rounding and name fallback belong to consumers. */
QJSIntlStatus qjs_intl_dayperiod_row(const QJSIntlDayPeriodData *data, uint32_t index,
                                     QJSIntlDayPeriodRuleSetKind *kind, QJSIntlBytes *origin,
                                     uint32_t *map);
QJSIntlStatus qjs_intl_dayperiod_find(const QJSIntlDayPeriodData *data,
                                      QJSIntlDayPeriodRuleSetKind kind, QJSIntlBytes origin,
                                      uint32_t *map);
QJSIntlStatus qjs_intl_dayperiod_map(const QJSIntlDayPeriodData *data, uint32_t index,
                                     uint32_t *start, uint32_t *count);
QJSIntlStatus qjs_intl_dayperiod_rule(const QJSIntlDayPeriodData *data, uint32_t index,
                                      QJSIntlDayPeriodRule *out);
#endif
