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
#ifndef QJS_ICU4X_DURATION_H
#define QJS_ICU4X_DURATION_H
#include <stddef.h>
#include <stdint.h>
#include "bridge.h"

typedef struct QJSICU4XDuration QJSICU4XDuration;
typedef struct QJSICU4XDurationDigital {
    const uint8_t *separator;
    size_t length;
    uint32_t hour_width;
} QJSICU4XDurationDigital;

/* Public selected ICU4X digital-duration metadata, borrowing the retained
   handle. hms hour width is raw CLDR preference, not a unit-option override.
   Failure preserves output; NULL close is harmless. */
int qjs_icu4x_duration_open(const uint8_t *, size_t, QJSICU4XDuration **);
void qjs_icu4x_duration_close(QJSICU4XDuration *);
int qjs_icu4x_duration_digital(const QJSICU4XDuration *, QJSICU4XDurationDigital *);
#endif
