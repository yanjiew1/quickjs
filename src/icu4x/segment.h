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
#ifndef QJS_ICU4X_SEGMENT_H
#define QJS_ICU4X_SEGMENT_H
#include <stddef.h>
#include <stdint.h>
#include "bridge.h"

typedef struct QJSICU4XSegment QJSICU4XSegment;
typedef struct QJSICU4XBoundarySink {
    void *opaque;
    int (*emit)(void *, size_t, int32_t);
} QJSICU4XBoundarySink;

/* Private synchronous boundary. Handles own public ICU4X segmenters. Inputs
   remain borrowed, callback errors stop traversal, and failed open preserves
   *out. Boundaries are original UTF-16 indices, not UTF-8/scalar positions. */
int qjs_icu4x_segment_check(const uint8_t *, size_t, const uint8_t *cldr, size_t);
int qjs_icu4x_segment_open(const uint8_t *, size_t, uint32_t, QJSICU4XSegment **);
void qjs_icu4x_segment_close(QJSICU4XSegment *);
int qjs_icu4x_segment_boundaries(const QJSICU4XSegment *, const uint16_t *, size_t,
                                 QJSICU4XBoundarySink);
#endif
