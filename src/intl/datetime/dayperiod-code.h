/* Shared CLDR vocabulary; independent of names and clock-rule policy.
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
#ifndef QJS_INTL_DATETIME_DAYPERIOD_CODE_H
#define QJS_INTL_DATETIME_DAYPERIOD_CODE_H
#include "../common/types.h"

/* Version-1 wire order, matching tools/intl/dayperiod_codes.py. */
typedef enum QJSIntlDayPeriodCode {
    QJS_INTL_DAYPERIOD_AM,
    QJS_INTL_DAYPERIOD_PM,
    QJS_INTL_DAYPERIOD_MIDNIGHT,
    QJS_INTL_DAYPERIOD_NOON,
    QJS_INTL_DAYPERIOD_MORNING1,
    QJS_INTL_DAYPERIOD_MORNING2,
    QJS_INTL_DAYPERIOD_AFTERNOON1,
    QJS_INTL_DAYPERIOD_AFTERNOON2,
    QJS_INTL_DAYPERIOD_EVENING1,
    QJS_INTL_DAYPERIOD_EVENING2,
    QJS_INTL_DAYPERIOD_NIGHT1,
    QJS_INTL_DAYPERIOD_NIGHT2,
    QJS_INTL_DAYPERIOD_CODE_COUNT
} QJSIntlDayPeriodCode;

/* Borrow static ASCII names. Unknown code yields empty bytes; unknown name -1. */
QJSIntlBytes qjs_intl_dayperiod_code_name(uint32_t code);
int qjs_intl_dayperiod_code_find(QJSIntlBytes name);
#endif
