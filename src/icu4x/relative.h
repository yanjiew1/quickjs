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
#ifndef QJS_ICU4X_RELATIVE_H
#define QJS_ICU4X_RELATIVE_H
#include <stdint.h>
#include <stddef.h>
#include "bridge.h"

typedef struct QJSICU4XRelative QJSICU4XRelative;
typedef struct QJSICU4XRelativeText {
    const uint8_t *data;
    size_t length;
} QJSICU4XRelativeText;
typedef struct QJSICU4XRelativePattern {
    QJSICU4XRelativeText prefix, suffix;
    uint32_t has_number;
} QJSICU4XRelativePattern;
/* Width, unit and plural category follow the shared C relative/plural enums.
   The past argument is Boolean (1=past, 0=future), not RelativeTense.
   Pattern/literal spans borrow the owned handle. Failures preserve outputs.
   Literal lookup is exact, including offset zero for either Number zero sign;
   a missing offset returns UNSUPPORTED, without numeric fallback here. */
int qjs_icu4x_relative_open(const uint8_t *, size_t, uint32_t width, QJSICU4XRelative **);
void qjs_icu4x_relative_close(QJSICU4XRelative *);
int qjs_icu4x_relative_pattern(const QJSICU4XRelative *, uint32_t unit, uint32_t past,
                               uint32_t category, QJSICU4XRelativePattern *);
int qjs_icu4x_relative_literal(const QJSICU4XRelative *, uint32_t unit, int32_t offset,
                               QJSICU4XRelativeText *);
int qjs_icu4x_relative_check_version(const uint8_t *, size_t);
#endif
