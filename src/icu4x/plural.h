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
#ifndef QJS_ICU4X_PLURAL_H
#define QJS_ICU4X_PLURAL_H
#include <stddef.h>
#include <stdint.h>
#include "bridge.h"

typedef struct QJSICU4XPlural QJSICU4XPlural;
typedef struct QJSICU4XRuleBytes {
    const uint8_t *data;
    size_t length;
} QJSICU4XRuleBytes;
typedef struct QJSICU4XPluralData {
    QJSICU4XRuleBytes rules[6];
    uint32_t mask;
    uint8_t ranges[36];
} QJSICU4XPluralData;

/* Rules and range categories come only from public ICU4X data. Category order
   matches QJSIntlPluralCategory. Snapshot slices borrow the live immutable
   handle; no source input is retained. Failed calls preserve output. */
int qjs_icu4x_plural_open(const uint8_t *, size_t, uint32_t ordinal, QJSICU4XPlural **);
int qjs_icu4x_plural_check_version(const uint8_t *, size_t);
void qjs_icu4x_plural_close(QJSICU4XPlural *);
int qjs_icu4x_plural_data(const QJSICU4XPlural *, QJSICU4XPluralData *);
#endif
