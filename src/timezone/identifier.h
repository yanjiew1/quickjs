/* Normalized named/fixed timezone identities shared by Date, Intl and Temporal.
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
#ifndef QUICKJS_TIMEZONE_IDENTIFIER_H
#define QUICKJS_TIMEZONE_IDENTIFIER_H
#include "timezone.h"

typedef struct QJSTimeZoneIdentifier {
    char identifier[256];
    int64_t offset_nanoseconds; /* Offset identifiers have whole-minute precision. */
    int is_offset;
    const QJSTimeZone *named; /* Borrowed immutable named handle, NULL for offsets. */
} QJSTimeZoneIdentifier;

/* Parse an accepted named alias or ASCII +/-HH, +/-HHmm, +/-HH:mm. Preserve
   canonical alias identity; runtime default policy may select its primary ID.
   Normalize negative zero to +00:00, which remains distinct from named UTC.
   No allocation. Preserve output on all failures; return QJS_TZ_* errors. */
int qjs_timezone_parse_identifier(QJSTimeZoneIdentifier *result,
                                  const char *identifier, size_t length);
int qjs_timezone_identifier_offset(const QJSTimeZoneIdentifier *zone,
                                   int64_t epoch_seconds, int32_t *result);
/* Resolve mathematical-floor local seconds with compatible disambiguation:
   earliest candidate in overlaps, offset-before-transition in gaps. Callers
   retain fractional seconds and enforce their Date/Temporal domain afterward.
   Preserve output on failure; named backend bounds still apply. */
int qjs_timezone_identifier_compatible(const QJSTimeZoneIdentifier *zone,
                                       int64_t local_seconds, int64_t *result);
#endif
