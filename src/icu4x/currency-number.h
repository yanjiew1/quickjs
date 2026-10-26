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
#ifndef QJS_ICU4X_CURRENCY_NUMBER_H
#define QJS_ICU4X_CURRENCY_NUMBER_H
#include "bridge.h"
#include "decimal.h"
#include <stddef.h>
#include <stdint.h>

/* CurrencyDigits from selected ICU4X data: regular precision, not cash
   precision/rounding. Canonical uppercase codes; unknown codes use the data
   default. Failed calls preserve all outputs. */
int qjs_icu4x_currency_digits(const uint8_t *, size_t, uint32_t *);

typedef struct QJSICU4XCurrencySymbolHandle QJSICU4XCurrencySymbolHandle;
typedef struct QJSICU4XCurrencySymbol {
    const uint8_t *text;
    size_t length;
    uint32_t starts_with_letter, ends_with_letter, found;
} QJSICU4XCurrencySymbol;
/* narrow is Boolean. Handles own public payloads; UTF-8 output spans borrow
   them until close. Missing symbols have found=0; explicit empty symbols still
   have found=1 and select no-currency patterns in the composing formatter.
   Narrow fallback is resolved by the public payload, not by text comparison.
   No numeric amount is formatted or rounded. Close(NULL) is harmless. */
int qjs_icu4x_currency_symbol_open(const uint8_t *locale, size_t locale_length, const uint8_t *code,
                                   size_t code_length, uint32_t narrow,
                                   QJSICU4XCurrencySymbolHandle **);
void qjs_icu4x_currency_symbol_close(QJSICU4XCurrencySymbolHandle *);
int qjs_icu4x_currency_symbol(const QJSICU4XCurrencySymbolHandle *, QJSICU4XCurrencySymbol *);

/* Public composed currency-specific decimal symbols. An empty system requests
   the unqualified default record; a nonempty system is exact. UNSUPPORTED means the
   public bake has no override payload for that locale/system/currency pair;
   it does not mean the currency or numeric system is globally unsupported.
   All spans borrow the immutable handle until close; failures preserve output.
   The numeric amount, digit repertoire and precision are not changed. */
typedef struct QJSICU4XCurrencyDecimalHandle QJSICU4XCurrencyDecimalHandle;
typedef struct QJSICU4XCurrencyDecimal {
    QJSICU4XDecimalText decimal, group;
    uint32_t primary, secondary, minimum;
} QJSICU4XCurrencyDecimal;
int qjs_icu4x_currency_decimal_open(const uint8_t *locale, size_t locale_length,
                                    const uint8_t *system, size_t system_length,
                                    const uint8_t *code, size_t code_length,
                                    QJSICU4XCurrencyDecimalHandle **);
void qjs_icu4x_currency_decimal_close(QJSICU4XCurrencyDecimalHandle *);
int qjs_icu4x_currency_decimal(const QJSICU4XCurrencyDecimalHandle *, QJSICU4XCurrencyDecimal *);
#endif
