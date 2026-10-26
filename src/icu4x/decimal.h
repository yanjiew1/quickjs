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
#ifndef QJS_ICU4X_DECIMAL_H
#define QJS_ICU4X_DECIMAL_H
#include <stddef.h>
#include <stdint.h>
#include "bridge.h"

typedef struct QJSICU4XCompact QJSICU4XCompact;
/* Private immutable compact-pattern metadata. This lookup does not format,
   scale or round the input. The caller supplies its already-rounded magnitude.
   Failed calls preserve outputs; close(NULL) is safe. */
int qjs_icu4x_compact_open(const uint8_t *, size_t, uint32_t long_style, QJSICU4XCompact **);
void qjs_icu4x_compact_close(QJSICU4XCompact *);
int qjs_icu4x_compact_exponent(const QJSICU4XCompact *, int32_t, int32_t *);

/* Resolved public decimal data, independent of numeric magnitude. The handle
   owns the payloads; UTF-8 spans borrow it until close. Field order is decimal,
   group, minus-prefix/suffix, plus-prefix/suffix, resolved numbering system.
   This supplies the common exact C body formatter without converting the
   mathematical value through ICU4X's bounded i16 decimal representation. */
typedef struct QJSICU4XDecimal QJSICU4XDecimal;
typedef struct QJSICU4XDecimalText {
    const uint8_t *data;
    size_t length;
} QJSICU4XDecimalText;
typedef struct QJSICU4XDecimalSymbols {
    uint32_t digits[10];
    uint32_t primary, secondary, minimum;
    QJSICU4XDecimalText fields[7];
} QJSICU4XDecimalSymbols;
/* Empty system requests the locale default; an explicit unknown/algorithmic
   system returns UNSUPPORTED rather than silently accepting fallback digits.
   On failure preserve outputs. Close(NULL) is harmless. */
int qjs_icu4x_decimal_open(const uint8_t *locale, size_t locale_length, const uint8_t *system,
                           size_t system_length, QJSICU4XDecimal **);
void qjs_icu4x_decimal_close(QJSICU4XDecimal *);
int qjs_icu4x_decimal_symbols(const QJSICU4XDecimal *, QJSICU4XDecimalSymbols *);
/* Numeric digit systems from real payload IDs, not algorithmic metadata.
   Sorted unique callback spans borrow the call; preserve the first callback
   error. Locale admission belongs to the complete NumberFormat provider:
   explicit symbol rows omit locales legitimately inheriting root symbols. */
typedef int QJSICU4XDecimalEmit(void *, const uint8_t *, size_t);
int qjs_icu4x_decimal_systems(void *, QJSICU4XDecimalEmit *);
#endif
