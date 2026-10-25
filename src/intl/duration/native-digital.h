/* Native LDML digital-duration projection; not a backend service contract.
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
#ifndef QJS_INTL_DURATION_NATIVE_DIGITAL_H
#define QJS_INTL_DURATION_NATIVE_DIGITAL_H
#include "../common/types.h"

typedef struct QJSIntlNativeDurationDigital {
    char *storage;
    QJSIntlBytes hour_minute, minute_second;
    unsigned int hour_width; /* Raw CLDR preference, not an option override. */
} QJSIntlNativeDurationDigital;

/* Allocation-free syntax/scalar validation for backend locale admission. */
QJSIntlStatus qjs_intl_duration_native_digital_validate(QJSIntlBytes pattern);

/* Project the authoritative scalar UTF-8 hms pattern's inter-field literals.
   Recognize ordered h/m/s runs of width 1 or 2 with LDML apostrophe quoting.
   Outer literals cannot be represented by DigitalFormat and are not emitted.
   Keep unequal separators and the raw hour width; do not substitute the
   unrelated numeric timeSeparator symbol. Malformed input is DATA_ERROR,
   not an implicit colon fallback. Require malloc/free; one owned buffer.
   Fresh disjoint output resets on failure and survives input storage. */
QJSIntlStatus qjs_intl_duration_native_digital(const QJSIntlAllocator *, QJSIntlBytes pattern,
                                             QJSIntlNativeDurationDigital *);
void qjs_intl_duration_native_digital_clear(const QJSIntlAllocator *, QJSIntlNativeDurationDigital *);
#endif
