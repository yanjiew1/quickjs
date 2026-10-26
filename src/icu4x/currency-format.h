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
#ifndef QJS_ICU4X_CURRENCY_FORMAT_H
#define QJS_ICU4X_CURRENCY_FORMAT_H
#include "decimal.h"

typedef struct QJSICU4XCurrencyNameHandle QJSICU4XCurrencyNameHandle;
typedef struct QJSICU4XCurrencyName {
    QJSICU4XDecimalText name;
    QJSICU4XDecimalText literals[3]; /* Prefix, infix, suffix around two placeholders. */
    uint32_t first;                  /* 0: number first; 1: currency name first. */
} QJSICU4XCurrencyName;

/* Immutable selected ICU4X currency-name and plural pattern payloads. The
   caller has already rounded/classified the amount exactly. Category is the
   common zero/one/two/few/many/other order; exact is -1, 0 or 1 for an ordinary
   value or exact mathematical zero/one, independent of visible fraction zeros.
   These arguments select metadata only; no amount is rounded or converted.
   All returned UTF-8 spans borrow the handle until close. Failures preserve
   outputs. Literal braces stay literal; pattern items are never reparsed. */
int qjs_icu4x_currency_name_open(const uint8_t *locale, size_t locale_length, const uint8_t *code,
                                 size_t code_length, QJSICU4XCurrencyNameHandle **);
void qjs_icu4x_currency_name_close(QJSICU4XCurrencyNameHandle *);
int qjs_icu4x_currency_name(const QJSICU4XCurrencyNameHandle *, uint32_t category, int32_t exact,
                            QJSICU4XCurrencyName *);
#endif
