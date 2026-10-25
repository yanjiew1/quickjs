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
#include "local-time.h"
#include <string.h>

QJSIntlStatus qjs_intl_date_local_time(QJSCalendarId calendar, int64_t epoch_seconds,
                                      uint32_t nanosecond, const QJSTimeZone *named,
                                      int32_t offset_minutes, QJSIntlDateLocalTime *out)
{
    QJSIntlDateLocalTime result = {0};
    QJSTimeZoneInfo zone = {0};
    QJSIntlStatus status;
    int64_t seconds, day, clock;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (unsigned int)calendar >= QJS_CAL_COUNT || nanosecond >= UINT32_C(1000000000) ||
        (named ? offset_minutes != 0 : offset_minutes <= -1440 || offset_minutes >= 1440))
        return QJS_INTL_INVALID_ARGUMENT;
    if (named) {
        int error = qjs_timezone_info(named, epoch_seconds, &zone);
        if (error)
            return error == QJS_TZ_RANGE ? QJS_INTL_INVALID_ARGUMENT :
                   error == QJS_TZ_UNSUPPORTED ? QJS_INTL_UNSUPPORTED :
                   error == QJS_TZ_MEMORY ? QJS_INTL_NO_MEMORY : QJS_INTL_DATA_ERROR;
        if (zone.offset_seconds <= -86400 || zone.offset_seconds >= 86400 ||
            (unsigned int)zone.is_dst > 1)
            return QJS_INTL_DATA_ERROR;
    } else {
        zone.offset_seconds = offset_minutes * 60;
    }
    if ((zone.offset_seconds > 0 && epoch_seconds > INT64_MAX - zone.offset_seconds) ||
        (zone.offset_seconds < 0 && epoch_seconds < INT64_MIN - zone.offset_seconds))
        return QJS_INTL_OVERFLOW;
    seconds = epoch_seconds + zone.offset_seconds;
    day = seconds / 86400;
    clock = seconds % 86400;
    if (clock < 0) {
        day--;
        clock += 86400;
    }
    status = qjs_intl_date_calendar_fields(calendar, day, &result.fields);
    if (status)
        return status;
    result.fields.numeric.hour = (unsigned int)(clock / 3600);
    result.fields.numeric.minute = (unsigned int)(clock / 60 % 60);
    result.fields.numeric.second = (unsigned int)(clock % 60);
    result.fields.numeric.millisecond = nanosecond / 1000000;
    result.epoch_day = day;
    result.offset_seconds = zone.offset_seconds;
    result.nanosecond = nanosecond;
    result.subsecond = nanosecond != 0;
    result.is_dst = zone.is_dst;
    *out = result;
    return QJS_INTL_OK;
}
