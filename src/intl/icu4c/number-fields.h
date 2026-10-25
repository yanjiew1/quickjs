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
#ifndef QJS_INTL_ICU4C_NUMBER_FIELDS_H
#define QJS_INTL_ICU4C_NUMBER_FIELDS_H
#include "common.h"
#include "../number/number.h"
#include <unicode/uformattedvalue.h>

enum { QJS_ICU4C_FIELD_START = 1, QJS_ICU4C_FIELD_END = 2 };
typedef struct QJSICU4CNumberFields {
    const uint16_t *text; /* Borrow ICU unless a representation repair is needed. */
    uint16_t *owned_text;
    size_t length;
    int16_t *fields;
    uint8_t *flags;
} QJSICU4CNumberFields;

/* ICU number spans overlap: integer envelopes include grouping fields. Flatten
   those fields with specific-field precedence, retaining range origins.
   Result owns its maps and any repaired text copy. Otherwise text borrows the
   live ICU result. Close with the opening allocator. */
QJSIntlStatus qjs_icu4c_number_fields(const QJSIntlAllocator *, const UFormattedValue *,
                                      size_t limit, QJSICU4CNumberFields *);
void qjs_icu4c_number_fields_clear(const QJSIntlAllocator *, QJSICU4CNumberFields *);
/* Copy one ICU numeric result into the common owned text/parts result. */
QJSIntlStatus qjs_icu4c_number_parts(const QJSIntlAllocator *, const UFormattedValue *,
                                     const QJSIntlMathematicalValue *, QJSIntlNumberStyle,
                                     size_t limit, QJSIntlFormatted *);
QJSIntlPartType qjs_icu4c_number_part_type(int field, const QJSIntlMathematicalValue *,
                                           QJSIntlNumberStyle);
#endif
