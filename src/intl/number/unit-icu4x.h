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
#ifndef QJS_INTL_NUMBER_UNIT_ICU4X_H
#define QJS_INTL_NUMBER_UNIT_ICU4X_H
#include "unit.h"
#include "../plural/rules.h"

typedef struct QJSIntlICU4XUnitData {
    QJSIntlUnitData units;
    QJSIntlUnitGrammarData grammar;
    uint32_t root;
} QJSIntlICU4XUnitData;
typedef struct QJSIntlICU4XNumberUnit QJSIntlICU4XNumberUnit;

/* Borrow the immutable scoped companion; validators release temporary memory. */
QJSIntlStatus qjs_icu4x_number_unit_data_open(const QJSIntlAllocator *, QJSIntlICU4XUnitData *);
/* Retain selected foreign patterns/rules at construction. No service or caller
   text is retained; the immutable compiled data has process lifetime. */
QJSIntlStatus qjs_icu4x_number_unit_open(const QJSIntlAllocator *, const QJSIntlICU4XUnitData *,
                                         QJSIntlBytes locale, QJSIntlUnitWidth,
                                         QJSIntlBytes identifier, QJSIntlICU4XNumberUnit **);
void qjs_icu4x_number_unit_close(QJSIntlICU4XNumberUnit *);
QJSIntlStatus qjs_icu4x_number_unit_format(const QJSIntlICU4XNumberUnit *,
                                           const QJSIntlPluralOperands *, uint32_t shift, int exact,
                                           const QJSIntlFormatted *, size_t, QJSIntlFormatted *);
#endif
