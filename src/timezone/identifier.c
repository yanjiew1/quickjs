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
#include "identifier.h"
#include <limits.h>
#include <string.h>

static int two_digits(const char *text, unsigned int *out)
{
    if (text[0] < '0' || text[0] > '9' || text[1] < '0' || text[1] > '9')
        return QJS_TZ_RANGE;
    *out = (text[0] - '0') * 10 + text[1] - '0';
    return 0;
}

int qjs_timezone_parse_identifier(QJSTimeZoneIdentifier *result,
                                  const char *identifier, size_t length)
{
    QJSTimeZoneIdentifier zone = {0};
    unsigned int hour, minute = 0;
    const char *canonical;
    int error, negative;
    if (!result || !identifier || !length || length >= sizeof(zone.identifier) ||
        memchr(identifier, 0, length))
        return QJS_TZ_RANGE;
    if (identifier[0] == '+' || identifier[0] == '-') {
        if ((length != 3 && length != 5 && length != 6) || two_digits(identifier + 1, &hour) || hour > 23)
            return QJS_TZ_RANGE;
        if (length == 6 && identifier[3] != ':')
            return QJS_TZ_RANGE;
        if (length > 3 && (two_digits(identifier + (length == 6 ? 4 : 3), &minute) || minute > 59))
            return QJS_TZ_RANGE;
        negative = identifier[0] == '-' && (hour || minute);
        zone.is_offset = 1;
        zone.offset_nanoseconds = ((int64_t)hour * 60 + minute) * INT64_C(60000000000);
        if (negative)
            zone.offset_nanoseconds = -zone.offset_nanoseconds;
        zone.identifier[0] = negative ? '-' : '+';
        zone.identifier[1] = '0' + hour / 10;
        zone.identifier[2] = '0' + hour % 10;
        zone.identifier[3] = ':';
        zone.identifier[4] = '0' + minute / 10;
        zone.identifier[5] = '0' + minute % 10;
    } else {
        error = qjs_timezone_lookup(&zone.named, identifier, length);
        if (error)
            return error;
        canonical = qjs_timezone_identifier(zone.named);
        if (strlen(canonical) >= sizeof(zone.identifier))
            return QJS_TZ_DATA;
        strcpy(zone.identifier, canonical);
    }
    *result = zone;
    return 0;
}

int qjs_timezone_identifier_offset(const QJSTimeZoneIdentifier *zone,
                                   int64_t epoch_seconds, int32_t *result)
{
    if (!zone || !result || (!zone->is_offset && !zone->named))
        return QJS_TZ_DATA;
    if (zone->is_offset) {
        *result = zone->offset_nanoseconds / INT64_C(1000000000);
        return 0;
    }
    return qjs_timezone_offset(zone->named, epoch_seconds, result);
}

typedef struct FirstCandidate { int64_t seconds; int found; } FirstCandidate;

static void first_candidate(void *opaque, int64_t seconds)
{
    FirstCandidate *first = opaque;
    if (!first->found) {
        first->seconds = seconds;
        first->found = 1;
    }
}

int qjs_timezone_identifier_compatible(const QJSTimeZoneIdentifier *zone,
                                       int64_t local_seconds, int64_t *result)
{
    FirstCandidate first = {0};
    int32_t before, after;
    int64_t end, offset;
    size_t count;
    int error;
    if (!zone || !result || (!zone->is_offset && !zone->named))
        return QJS_TZ_DATA;
    if (zone->is_offset) {
        offset = zone->offset_nanoseconds / INT64_C(1000000000);
    } else {
        error = qjs_timezone_candidates(zone->named, local_seconds, first_candidate,
                                         &first, &count, &before, &after, &end);
        if (error)
            return error;
        if (count) {
            if (!first.found)
                return QJS_TZ_DATA;
            *result = first.seconds;
            return 0;
        }
        offset = before;
    }
    if ((offset > 0 && local_seconds < INT64_MIN + offset) ||
        (offset < 0 && local_seconds > INT64_MAX + offset))
        return QJS_TZ_RANGE;
    *result = local_seconds - offset;
    return 0;
}
