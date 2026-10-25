/* Logical collation coordinates shared by root readers and tailoring.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_COLLATION_COORDINATES_H
#define QJS_INTL_COLLATION_COORDINATES_H
#include "../common/types.h"

/* The root owner defines ordinary/ignorable bands and common aliases. */
typedef struct QJSIntlCollationRootLimits {
    uint32_t primary_max, secondary_max, tertiary_max;
    uint32_t secondary_common, tertiary_common;
    uint32_t secondary_boundary, tertiary_boundary;
} QJSIntlCollationRootLimits;

enum {
    QJS_INTL_COLLATION_LOWER,
    QJS_INTL_COLLATION_MIXED,
    QJS_INTL_COLLATION_UPPER
};

typedef struct QJSIntlCollationCoordinate {
    uint32_t weights[4]; /* Root or stable tailored IDs, not frozen ranks. */
    uint32_t root_primary; /* Original root anchor for grouping/variable policy. */
    unsigned char case_value, variable, implicit;
} QJSIntlCollationCoordinate;
#endif
