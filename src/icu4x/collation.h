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
#ifndef QJS_ICU4X_COLLATION_H
#define QJS_ICU4X_COLLATION_H
#include <stddef.h>
#include <stdint.h>
#include "bridge.h"

typedef struct QJSICU4XCollator QJSICU4XCollator;
typedef struct QJSICU4XCollationOptions {
    uint32_t sensitivity, case_first, numeric, ignore_punctuation;
} QJSICU4XCollationOptions;

/* Option values follow intl/collation/policy.h. Usage is sort=0, search=1.
   The stock-data search policy is documented in README.md. Counted strings
   preserve embedded NUL; lone surrogates have U+FFFD collation weights.
   Failure preserves outputs. Handles retain their own immutable payloads.
   Enumeration spans borrow the callback; callbacks return bridge statuses. */
int qjs_icu4x_collation_types(void *, int (*emit)(void *, const uint8_t *, size_t));
int qjs_icu4x_collation_supported(const uint8_t *locale, size_t locale_length, const uint8_t *name,
                                  size_t name_length, uint32_t *);
int qjs_icu4x_collation_defaults(const uint8_t *locale, size_t, uint32_t usage,
                                 QJSICU4XCollationOptions *);
int qjs_icu4x_collation_check_version(const uint8_t *, size_t);
int qjs_icu4x_collator_open(const uint8_t *locale, size_t, uint32_t usage, const uint8_t *name,
                            size_t name_length, QJSICU4XCollationOptions, QJSICU4XCollator **);
void qjs_icu4x_collator_close(QJSICU4XCollator *);
int qjs_icu4x_collator_compare(const QJSICU4XCollator *, const uint16_t *, size_t, const uint16_t *,
                               size_t, int32_t *);
#endif
