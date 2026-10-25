/* ICU4X identity over the explicit shared IANA TZif rules companion.
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
#include "../icu4x/timezone.h"
#include <string.h>

int qjs_timezone_backend_lookup(const QJSTimeZone **result, const char *identifier, size_t length)
{
    QJSICU4XZoneName name;
    int error = qjs_icu4x_timezone_normalize((const uint8_t *)identifier, length, &name);

    if (error == QJS_ICU4X_UNSUPPORTED) {
        /* ICU4X classifies the real IANA Factory zone as unknown. Use actual
           admitted IANA data for such names, never a fabricated UTC payload. */
        return qjs_iana_timezone_lookup(result, identifier, length);
    }
    if (error == QJS_ICU4X_MEMORY)
        return QJS_TZ_MEMORY;
    if (error == QJS_ICU4X_BACKEND)
        return QJS_TZ_DATA;
    if (error)
        return QJS_TZ_RANGE;
    /* ICU4X also retains obsolete aliases absent from current IANA data.
       Admit the normalized spelling itself, not its unrelated canonical name. */
    return qjs_iana_timezone_lookup(result, name.name, strlen(name.name));
}

const char *qjs_timezone_backend_data_version(void)
{
    return qjs_iana_timezone_version();
}

size_t qjs_timezone_backend_count(void)
{
    return qjs_iana_timezone_count();
}

const QJSTimeZone *qjs_timezone_backend_at(size_t index)
{
    return qjs_iana_timezone_at(index);
}

int qjs_timezone_backend_offset(const QJSTimeZoneRules *rules, int64_t seconds, int32_t *offset)
{
    return qjs_tzif_offset(&rules->tzif, seconds, offset);
}

int qjs_timezone_backend_info(const QJSTimeZoneRules *rules, int64_t seconds,
                              QJSTimeZoneInfo *result)
{
    return qjs_tzif_info(&rules->tzif, seconds, result);
}

int qjs_timezone_backend_info_is_constant(const QJSTimeZoneRules *rules, int64_t first,
                                          int64_t last, int *constant)
{
    return qjs_tzif_info_is_constant(&rules->tzif, first, last, constant);
}

int qjs_timezone_backend_candidates(const QJSTimeZoneRules *rules, int64_t wall,
                                    QJSTimeZoneVisit *visit, void *opaque, size_t *count,
                                    int32_t *gap_before, int32_t *gap_after, int64_t *gap_end)
{
    return qjs_tzif_candidates(&rules->tzif, wall, visit, opaque, count, gap_before, gap_after,
                               gap_end);
}

int qjs_timezone_backend_transition(const QJSTimeZoneRules *rules, int64_t seconds, int next,
                                    int inclusive, int64_t *result, int *found)
{
    return qjs_tzif_transition(&rules->tzif, seconds, next, inclusive, result, found);
}
