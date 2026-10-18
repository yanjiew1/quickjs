/* Platform identity discovery and explicit default-zone fallback policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "system-zone.h"
#include "timezone.h"
#include "../calendar/iso.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#ifdef CONFIG_TIMEZONE_NATIVE
#include "windows-zone.h"
#endif
#endif

static int copy_identifier(char *result, size_t capacity, const char *name)
{
    size_t length = strlen(name);

    if (length >= capacity)
        return QJS_TZ_RANGE;
    memcpy(result, name, length + 1);
    return 0;
}

static int format_offset(char *result, size_t capacity, int64_t seconds)
{
    char text[7];
    unsigned minutes;

    if (seconds <= -86400 || seconds >= 86400 || seconds % 60)
        return QJS_TZ_RANGE;
    text[0] = seconds < 0 ? '-' : '+';
    minutes = (unsigned)(seconds < 0 ? -seconds : seconds) / 60;
    text[1] = '0' + minutes / 60 / 10;
    text[2] = '0' + minutes / 60 % 10;
    text[3] = ':';
    text[4] = '0' + minutes % 60 / 10;
    text[5] = '0' + minutes % 60 % 10;
    text[6] = 0;
    return copy_identifier(result, capacity, text);
}

static int ascii_letter(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int number(const char **cursor, int maximum, int *result)
{
    const char *p = *cursor;
    int value = 0, count = 0;

    while (*p >= '0' && *p <= '9') {
        if (++count > 2)
            return QJS_TZ_RANGE;
        value = value * 10 + *p++ - '0';
    }
    if (!count || value > maximum)
        return QJS_TZ_RANGE;
    *result = value;
    *cursor = p;
    return 0;
}

int qjs_timezone_parse_environment(char *result, size_t capacity,
                                    const char *value)
{
    const QJSTimeZone *zone;
    const char *p, *start;
    int sign = 1, hours, minutes = 0, seconds = 0, error;

    if (!value)
        return QJS_TZ_RANGE;
    if (*value == ':')
        value++;
    if (!*value)
        return copy_identifier(result, capacity, "UTC");
    error = qjs_timezone_lookup(&zone, value, strlen(value));
    if (!error)
        return copy_identifier(result, capacity,
                                qjs_timezone_primary_identifier(zone));
    if (error != QJS_TZ_RANGE)
        return error;
    p = value;
    if (*p == '<') {
        start = ++p;
        while (ascii_letter(*p) || (*p >= '0' && *p <= '9') ||
               *p == '+' || *p == '-')
            p++;
        if (p - start < 3 || *p++ != '>')
            return QJS_TZ_RANGE;
    } else {
        start = p;
        while (ascii_letter(*p))
            p++;
        if (p - start < 3)
            return QJS_TZ_RANGE;
    }
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    if (number(&p, 23, &hours))
        return QJS_TZ_RANGE;
    if (*p == ':') {
        p++;
        if (number(&p, 59, &minutes))
            return QJS_TZ_RANGE;
        if (*p == ':') {
            p++;
            if (number(&p, 59, &seconds))
                return QJS_TZ_RANGE;
        }
    }
    if (*p)
        return QJS_TZ_RANGE; /* No host parsing of general DST expressions. */
    /* POSIX specifies the amount added to local time to obtain UTC. */
    return format_offset(result, capacity,
                          -sign * (hours * 3600 + minutes * 60 + seconds));
}

static int host_offset(int64_t *result)
{
#ifdef _WIN32
    TIME_ZONE_INFORMATION zone;
    DWORD status = GetTimeZoneInformation(&zone);
    int64_t bias;

    if (status == TIME_ZONE_ID_INVALID)
        return QJS_TZ_DATA;
    bias = zone.Bias;
    if (status == TIME_ZONE_ID_STANDARD)
        bias += zone.StandardBias;
    else if (status == TIME_ZONE_ID_DAYLIGHT)
        bias += zone.DaylightBias;
    *result = -bias * 60;
#else
    time_t now = time(NULL);
    struct tm local, utc;
    QJSISODate local_date, utc_date;
    int64_t local_days, utc_days;

    tzset();
    if (now == (time_t)-1 || !localtime_r(&now, &local) || !gmtime_r(&now, &utc))
        return QJS_TZ_DATA;
    if ((int64_t)local.tm_year + 1900 > INT32_MAX ||
        (int64_t)utc.tm_year + 1900 > INT32_MAX)
        return QJS_TZ_RANGE;
    local_date = (QJSISODate){local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
    utc_date = (QJSISODate){utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday};
    if (qjs_iso_date_to_days(&local_days, local_date) ||
        qjs_iso_date_to_days(&utc_days, utc_date))
        return QJS_TZ_DATA;
    *result = (local_days - utc_days) * 86400 +
        (int64_t)(local.tm_hour - utc.tm_hour) * 3600 +
        (local.tm_min - utc.tm_min) * 60 + local.tm_sec - utc.tm_sec;
#endif
    return 0;
}

int qjs_timezone_system_default(char *result, size_t capacity,
                                int host_offset_fallback)
{
    int64_t seconds;
    char selected[256];
    int error = QJS_TZ_RANGE;
#ifdef _WIN32
#ifdef CONFIG_TIMEZONE_NATIVE
    char name[256];

    if (!qjs_tz_windows_system_identifier(name, sizeof(name)))
        error = qjs_timezone_parse_environment(selected, sizeof(selected), name);
#endif
#else
    error = qjs_timezone_parse_environment(selected, sizeof(selected), getenv("TZ"));
#endif
    if (!error)
        return copy_identifier(result, capacity, selected);
    if (error != QJS_TZ_RANGE)
        return error;
    if (host_offset_fallback && !host_offset(&seconds) &&
        !format_offset(selected, sizeof(selected), seconds))
        return copy_identifier(result, capacity, selected);
    return copy_identifier(result, capacity, "UTC");
}
