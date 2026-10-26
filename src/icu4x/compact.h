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
#ifndef QJS_ICU4X_COMPACT_H
#define QJS_ICU4X_COMPACT_H
#include "decimal.h"

typedef struct QJSICU4XCompactPatterns QJSICU4XCompactPatterns;
typedef struct QJSICU4XCompactScale {
    uint32_t magnitude, exponent;
} QJSICU4XCompactScale;
typedef struct QJSICU4XCompactPattern {
    QJSICU4XDecimalText prefix, suffix;
    uint32_t has_number;
} QJSICU4XCompactPattern;

/* Public positive compact pattern metadata, not a numeric formatter. Selected
   locale/system lookup follows ICU4X's exact-nu then unqualified fallback.
   Ascending rows omit equivalent later patterns and leading noncompact forms;
   below the first threshold, the caller uses an implicit exponent-zero body.
   Scales always use default/other metadata, independent of label selection.
   Category/exact follow currency-format.h; no actual amount crosses this API.
   A literal-only pattern can omit the number entirely. Prefix/suffix retain
   public literal text; the outer style handles sign/percent/currency policy.
   Failed outputs stay unchanged; text spans borrow the immutable handle. */
int qjs_icu4x_compact_patterns_open(const uint8_t *locale, size_t locale_length,
                                    const uint8_t *system, size_t system_length,
                                    uint32_t long_style, QJSICU4XCompactPatterns **);
void qjs_icu4x_compact_patterns_close(QJSICU4XCompactPatterns *);
int qjs_icu4x_compact_patterns_count(const QJSICU4XCompactPatterns *, uint32_t *);
int qjs_icu4x_compact_patterns_scale(const QJSICU4XCompactPatterns *, uint32_t index,
                                     QJSICU4XCompactScale *);
int qjs_icu4x_compact_patterns_get(const QJSICU4XCompactPatterns *, uint32_t index,
                                   uint32_t category, int32_t exact, QJSICU4XCompactPattern *);
#endif
