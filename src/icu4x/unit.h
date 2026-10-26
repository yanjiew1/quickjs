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
#ifndef QJS_ICU4X_UNIT_H
#define QJS_ICU4X_UNIT_H
#include <stddef.h>
#include <stdint.h>
#include "bridge.h"

typedef struct QJSICU4XUnit QJSICU4XUnit;
typedef struct QJSICU4XUnitText {
    const uint8_t *data;
    size_t length;
} QJSICU4XUnitText;
typedef struct QJSICU4XUnitPattern {
    QJSICU4XUnitText prefix, suffix;
    uint32_t has_number;
} QJSICU4XUnitPattern;

/* Exact source identity chosen by C unit overlays, not requester fallback.
   Group is the full CLDR identifier (e.g. length-meter); root maps to und.
   Width follows QJSIntlUnitWidth (long=0, short=1, narrow=2), not unitDisplay.
   UNSUPPORTED means no public payload at that source/width/unit. Category uses
   the shared six grammatical counts, never numeric exact0/1 or source cases.
   Failure preserves outputs. Prepared spans borrow the handle until close. */
int qjs_icu4x_unit_open(const uint8_t *source, size_t, const uint8_t *group, size_t, uint32_t width,
                        QJSICU4XUnit **);
void qjs_icu4x_unit_close(QJSICU4XUnit *);
int qjs_icu4x_unit_pattern(const QJSICU4XUnit *, uint32_t category, QJSICU4XUnitPattern *);
int qjs_icu4x_unit_check_version(const uint8_t *, size_t);
#endif
