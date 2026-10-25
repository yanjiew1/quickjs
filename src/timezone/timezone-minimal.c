/* Empty named-zone backend; mandatory UTC belongs to timezone.c.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#include "timezone-backend.h"

int qjs_timezone_backend_lookup(const QJSTimeZone **result,
                                const char *identifier, size_t length)
{
    return QJS_TZ_RANGE;
}

const char *qjs_timezone_backend_data_version(void)
{
    return "minimal";
}

size_t qjs_timezone_backend_count(void)
{
    return 0;
}

const QJSTimeZone *qjs_timezone_backend_at(size_t index)
{
    return NULL;
}

/* No rule handles exist in this configuration. Reaching a rule operation
   indicates an internal contract violation, not an unsupported user zone. */
int qjs_timezone_backend_offset(const QJSTimeZoneRules *rules, int64_t seconds,
                                int32_t *offset)
{
    return QJS_TZ_DATA;
}

int qjs_timezone_backend_info(const QJSTimeZoneRules *rules, int64_t seconds,
                              QJSTimeZoneInfo *result)
{
    return QJS_TZ_DATA;
}

int qjs_timezone_backend_info_is_constant(const QJSTimeZoneRules *rules, int64_t first,
                                          int64_t last, int *constant)
{
    return QJS_TZ_DATA;
}

int qjs_timezone_backend_candidates(const QJSTimeZoneRules *rules, int64_t wall,
                                    QJSTimeZoneVisit *visit, void *opaque,
                                    size_t *count, int32_t *gap_before,
                                    int32_t *gap_after, int64_t *gap_end)
{
    return QJS_TZ_DATA;
}

int qjs_timezone_backend_transition(const QJSTimeZoneRules *rules,
                                    int64_t seconds, int next, int inclusive,
                                    int64_t *result, int *found)
{
    return QJS_TZ_DATA;
}
