/*
 * QuickJS native internationalization support
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 * Copyright (c) 2026 Yan-Jie Wang
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
#include "libintl.h"
#include "../temporal/time-zone.h"
#include <unicode/uclean.h>
#include <unicode/icudataver.h>
#include <unicode/uchar.h>
#include <unicode/uloc.h>
#include <unicode/ulocdata.h>
#include <unicode/ucal.h>
#include <string.h>

void intl_backend_initialize(UErrorCode *status)
{
    IntlBackendVersions versions;
    if (U_FAILURE(*status))
        return;
    u_init(status);
    if (U_FAILURE(*status))
        return;
    /* This also requires CLDR and timezone resources from the linked data. */
    intl_backend_versions(&versions, status);
    if (U_SUCCESS(*status) &&
        (versions.icu[0] < 78 ||
         (versions.icu[0] == 78 && versions.icu[1] < 3)))
        *status = U_UNSUPPORTED_ERROR;
}

void intl_backend_versions(IntlBackendVersions *versions, UErrorCode *status)
{
    if (U_FAILURE(*status))
        return;
    memset(versions, 0, sizeof(*versions));
    u_getVersion(versions->icu);
    u_getUnicodeVersion(versions->unicode);
    u_getDataVersion(versions->data, status);
    ulocdata_getCLDRVersion(versions->cldr, status);
    versions->tzdata = ucal_getTZDataVersion(status);
}

const char *intl_backend_default_locale(void)
{
    return uloc_getDefault();
}

int32_t intl_backend_default_time_zone(UChar *buffer, int32_t capacity,
                                       UErrorCode *status)
{
    QJSTemporalZone zone;
    size_t i;
    int32_t required, copy;
    int error;
    if (U_FAILURE(*status))
        return 0;
    if (capacity < 0 || (!buffer && capacity)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    /* Copy one configured host/embedding provider result. Temporal and
       Date use this same pinned Identifier/PrimaryIdentifier policy. */
    error = qjs_temporal_system_zone(&zone);
    if (error) {
        *status = error == QJS_TEMPORAL_ERROR_MEMORY ?
            U_MEMORY_ALLOCATION_ERROR : U_UNSUPPORTED_ERROR;
        return 0;
    }
    required = (int32_t)strlen(zone.identifier);
    copy = required < capacity ? required : capacity;
    for (i = 0; i < (size_t)copy; i++)
        buffer[i] = (unsigned char)zone.identifier[i];
    if (required < capacity)
        buffer[required] = 0;
    if (required > capacity)
        *status = U_BUFFER_OVERFLOW_ERROR;
    else if (required == capacity)
        *status = U_STRING_NOT_TERMINATED_WARNING;
    return required;
}

static int intl_ascii_digit(char value)
{
    return value >= '0' && value <= '9';
}

int intl_backend_parse_offset_time_zone(const char *identifier, size_t length,
                                        char result[7])
{
    const char *digits;
    size_t count;
    int hours, minutes = 0;
    char sign;
    if (length && (identifier[0] == '+' || identifier[0] == '-')) {
        /* The shared system provider already emits canonical +/-HH:MM. */
        if (length != 6 || identifier[3] != ':')
            return -1;
        sign = identifier[0];
        digits = identifier + 1;
        count = length - 1;
    } else {
        if (length < 4 || memcmp(identifier, "GMT", 3) ||
            (identifier[3] != '+' && identifier[3] != '-'))
            return 0;
        sign = identifier[3];
        digits = identifier + 4;
        count = length - 4;
    }
    if (count == 1 || count == 2) {
        if (!intl_ascii_digit(digits[0]) ||
            (count == 2 && !intl_ascii_digit(digits[1])))
            return -1;
        hours = digits[0] - '0';
        if (count == 2)
            hours = hours * 10 + digits[1] - '0';
    } else if (count == 3 || count == 4 || count == 5) {
        size_t hour_digits = count == 3 ? 1 : 2;
        size_t minute_index = hour_digits;
        if (count == 5 || (count == 4 && digits[1] == ':')) {
            hour_digits = count - 3;
            if (digits[hour_digits] != ':')
                return -1;
            minute_index = hour_digits + 1;
        }
        if (!intl_ascii_digit(digits[0]) ||
            (hour_digits == 2 && !intl_ascii_digit(digits[1])) ||
            !intl_ascii_digit(digits[minute_index]) ||
            !intl_ascii_digit(digits[minute_index + 1]))
            return -1;
        hours = digits[0] - '0';
        if (hour_digits == 2)
            hours = hours * 10 + digits[1] - '0';
        minutes = (digits[minute_index] - '0') * 10 +
            digits[minute_index + 1] - '0';
    } else {
        return -1;
    }
    if (hours > 23 || minutes > 59)
        return -1;
    result[0] = hours || minutes ? sign : '+';
    result[1] = '0' + hours / 10;
    result[2] = '0' + hours % 10;
    result[3] = ':';
    result[4] = '0' + minutes / 10;
    result[5] = '0' + minutes % 10;
    result[6] = 0;
    return 1;
}

int intl_remove_unicode_extension(char *tag)
{
    char *part = strchr(tag, '-');
    while (part) {
        char *next = strchr(part + 1, '-');
        size_t length = next ? (size_t)(next - part - 1) : strlen(part + 1);
        if (length == 1) {
            /* A private-use 'u' subtag is not a Unicode extension singleton. */
            if (part[1] == 'x')
                return 0;
            if (part[1] == 'u') {
                char *end = next;
                while (end) {
                    char *following = strchr(end + 1, '-');
                    size_t subtag_length = following ?
                        (size_t)(following - end - 1) : strlen(end + 1);
                    if (subtag_length == 1)
                        break;
                    end = following;
                }
                if (end)
                    memmove(part, end, strlen(end) + 1);
                else
                    *part = 0;
                return 1;
            }
        }
        part = next;
    }
    return 0;
}
