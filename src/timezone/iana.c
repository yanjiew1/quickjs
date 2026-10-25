/* Shared named-zone admission over immutable generated IANA descriptors.
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
#include "iana.h"

static int folded_compare(const char *canonical, const char *text, size_t length)
{
    size_t i;

    for (i = 0; i < length && canonical[i]; i++) {
        unsigned char a = canonical[i], b = text[i];

        if (a >= 'A' && a <= 'Z')
            a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z')
            b += 'a' - 'A';
        if (a != b)
            return a < b ? -1 : 1;
    }
    if (i < length)
        return -1;
    return canonical[i] ? 1 : 0;
}

int qjs_iana_timezone_lookup(const QJSTimeZone **result, const char *identifier, size_t length)
{
    size_t lo = 0, hi = qjs_iana_timezone_zone_count;

    while (lo < hi) {
        size_t middle = lo + (hi - lo) / 2;
        const QJSTimeZone *zone = qjs_iana_timezone_zones + qjs_iana_timezone_lookup_order[middle];
        int order = folded_compare(zone->identifier, identifier, length);

        if (!order) {
            *result = zone;
            return 0;
        }
        if (order < 0)
            lo = middle + 1;
        else
            hi = middle;
    }
    return QJS_TZ_RANGE;
}

const char *qjs_iana_timezone_version(void)
{
    return qjs_iana_timezone_data_version;
}

size_t qjs_iana_timezone_count(void)
{
    return qjs_iana_timezone_zone_count;
}

const QJSTimeZone *qjs_iana_timezone_at(size_t index)
{
    return index < qjs_iana_timezone_zone_count ? qjs_iana_timezone_zones + index : NULL;
}
