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
#ifndef QJS_ICU4X_ZONE_NAMES_H
#define QJS_ICU4X_ZONE_NAMES_H
#include "datetime-names.h"
typedef struct QJSICU4XZoneNames QJSICU4XZoneNames;
/* Exact source payloads only; failure preserves outputs. All text borrows the
   retained handle. Root maps to und. Checksums tie names to public periods. */
int qjs_icu4x_zone_names_open(const uint8_t *, size_t, QJSICU4XZoneNames **);
void qjs_icu4x_zone_names_close(QJSICU4XZoneNames *);
/* metazone=0 reads a zone override. metazone=1 uses representative IANA zone
   and epoch seconds inside a C-proven membership. Width long=1/short=2;
   kind generic=0/standard=1/daylight=2. No additional source/type fallback. */
int qjs_icu4x_zone_names_text(const QJSICU4XZoneNames *, const uint8_t *, size_t,
                              int64_t epoch_seconds, uint32_t metazone, uint32_t width,
                              uint32_t kind, QJSICU4XDateText *);
#endif
