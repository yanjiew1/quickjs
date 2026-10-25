/* Mandatory UTC policy shared by all compile-time timezone backends.
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
#include <string.h>

static const QJSTimeZone utc = {"UTC", "UTC", NULL};

int qjs_timezone_lookup(const QJSTimeZone **result, const char *identifier,
                        size_t length)
{
    if (length == 3 && (identifier[0] == 'U' || identifier[0] == 'u') &&
        (identifier[1] == 'T' || identifier[1] == 't') &&
        (identifier[2] == 'C' || identifier[2] == 'c')) {
        *result = &utc;
        return 0;
    }
    return qjs_timezone_backend_lookup(result, identifier, length);
}

const char *qjs_timezone_identifier(const QJSTimeZone *zone)
{
    return zone->identifier;
}

const char *qjs_timezone_primary_identifier(const QJSTimeZone *zone)
{
    return zone->primary;
}

int qjs_timezone_is_primary(const QJSTimeZone *zone)
{
    return !strcmp(zone->identifier, zone->primary);
}

const char *qjs_timezone_data_version(void)
{
    return qjs_timezone_backend_data_version();
}

size_t qjs_timezone_count(void)
{
    return qjs_timezone_backend_count() + 1;
}

const QJSTimeZone *qjs_timezone_at(size_t index)
{
    size_t count = qjs_timezone_backend_count();
    const QJSTimeZone *zone;

    if (index > count)
        return NULL;
    /* Before UTC's insertion, backend[index] is the requested entry. After
       it, backend[index - 1] is. This preserves sorted enumeration in O(1)
       without requiring backends to duplicate or index the core UTC entry. */
    if (index < count) {
        zone = qjs_timezone_backend_at(index);
        if (strcmp(zone->identifier, "UTC") < 0)
            return zone;
    }
    if (index) {
        zone = qjs_timezone_backend_at(index - 1);
        if (strcmp(zone->identifier, "UTC") > 0)
            return zone;
    }
    return &utc;
}

int qjs_timezone_offset(const QJSTimeZone *zone, int64_t epoch_seconds,
                        int32_t *offset_seconds)
{
    if (zone->rules)
        return qjs_timezone_backend_offset(zone->rules, epoch_seconds,
                                           offset_seconds);
    *offset_seconds = 0;
    return 0;
}

int qjs_timezone_info(const QJSTimeZone *zone, int64_t epoch_seconds,
                      QJSTimeZoneInfo *result)
{
    if (zone->rules)
        return qjs_timezone_backend_info(zone->rules, epoch_seconds, result);
    result->offset_seconds = 0;
    result->is_dst = 0;
    return 0;
}

int qjs_timezone_info_is_constant(const QJSTimeZone *zone, int64_t first,
                                  int64_t last, int *constant)
{
    if (!zone || !constant)
        return QJS_TZ_DATA;
    if (first > last)
        return QJS_TZ_RANGE;
    if (zone->rules)
        return qjs_timezone_backend_info_is_constant(zone->rules, first, last, constant);
    *constant = 1;
    return QJS_TZ_OK;
}

int qjs_timezone_candidates(const QJSTimeZone *zone, int64_t local_seconds,
                            QJSTimeZoneVisit *visit, void *opaque,
                            size_t *count, int32_t *gap_before,
                            int32_t *gap_after, int64_t *gap_end)
{
    if (zone->rules)
        return qjs_timezone_backend_candidates(zone->rules, local_seconds,
                                               visit, opaque, count, gap_before,
                                               gap_after, gap_end);
    visit(opaque, local_seconds);
    *count = 1;
    *gap_before = *gap_after = 0;
    return 0;
}

int qjs_timezone_transition(const QJSTimeZone *zone, int64_t epoch_seconds,
                            int next, int inclusive,
                            int64_t *transition_seconds, int *found)
{
    if (zone->rules)
        return qjs_timezone_backend_transition(zone->rules, epoch_seconds, next,
                                               inclusive, transition_seconds,
                                               found);
    *found = 0;
    return 0;
}
