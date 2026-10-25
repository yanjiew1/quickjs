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
#ifndef QJS_INTL_ICU4C_NUMBERING_H
#define QJS_INTL_ICU4C_NUMBERING_H
#include "common.h"

typedef struct QJSICU4CNumbering {
    QJSIntlAllocator allocator;
    QJSIntlBytes *names;
    size_t count;
    QJSIntlBytes *defaults; /* Borrow names owned by this same view. */
    size_t locale_count;
} QJSICU4CNumbering;

/* Decimal digit systems and each module locale's default. Own all returned
   names; queries never allocate or retain temporary ICU handles. Reused by
   NumberFormat, RelativeTimeFormat, DateTimeFormat and DurationFormat. */
QJSIntlStatus qjs_icu4c_numbering_open(const QJSIntlAllocator *, const QJSICU4CLocales *,
                                       QJSICU4CNumbering *);
void qjs_icu4c_numbering_clear(QJSICU4CNumbering *);
QJSIntlStatus qjs_icu4c_numbering_default(const QJSICU4CNumbering *, size_t locale, QJSIntlBytes *);
QJSIntlStatus qjs_icu4c_numbering_supported(const QJSICU4CNumbering *, QJSIntlBytes, int *);
#endif
