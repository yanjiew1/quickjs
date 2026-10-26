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
#ifndef QJS_INTL_NUMBER_UNIT_NATIVE_H
#define QJS_INTL_NUMBER_UNIT_NATIVE_H
#include "unit.h"
#include "../plural/data.h"

typedef struct QJSIntlNativeNumberUnit {
    QJSIntlNumberUnit composition;
    QJSIntlPluralData plurals;
} QJSIntlNativeNumberUnit;

/* Native source-rule binding. Borrow unchanged unit/grammar/plural owners
   from one pack; retain no caller text and allocate nothing. Clear outputs on
   errors. Format final rounded operands with notation expansion and exact
   -1/0/1; NULL operands mean nonfinite OTHER and require exact=-1, shift=0. */
QJSIntlStatus qjs_intl_number_unit_native_open(const QJSIntlUnitData *, const QJSIntlPluralData *,
                                               const QJSIntlUnitGrammarData *, uint32_t locale,
                                               uint32_t root, QJSIntlUnitWidth, QJSIntlBytes,
                                               QJSIntlNativeNumberUnit *);
QJSIntlStatus
qjs_intl_number_unit_native_format(const QJSIntlNativeNumberUnit *, const QJSIntlAllocator *,
                                   const QJSIntlPluralOperands *, uint32_t decimal_shift, int exact,
                                   const QJSIntlFormatted *body, size_t limit, QJSIntlFormatted *);
#endif
