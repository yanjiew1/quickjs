/*
 * Copyright (C) 2007-2014, International Business Machines Corporation
 * and others. All Rights Reserved. (Chinese calendar)
 * Copyright (C) 2013, International Business Machines Corporation
 * and others. All Rights Reserved. (Dangi calendar)
 * Copyright (C) 2016 and later: Unicode, Inc. and others.
 * Original Chinese modification history: 9/18/2007 ajmacher,
 * ported from Java ChineseCalendar.
 * SPDX-License-Identifier: Unicode-3.0
 *
 * C translation of ICU 78.3 source/i18n/chnsecal.cpp and dangical.cpp;
 * the STANDARD_TIME transition interpretation is from tzrule.cpp.
 * SHA-256 chnsecal.cpp:
 * 625e63321f41d106b77134c177fe5907baf1507a646427066751c29509b25140
 * SHA-256 dangical.cpp:
 * bd8ee11c51e523175a86959b7c7467df7e5cd08075e9fefa396176343e5866b8
 * Copyright/permission notices are preserved in LICENSE-ICU beside this source.
 *
 * Allocation-free C approximation. Required PMO/KASI dates must be supplied
 * separately by the publication pack; this model alone does not authorize
 * advertising Chinese or Dangi support. No user/default timezone is read.
 */
#include "lunisolar.h"
#include "astronomy.h"
#include "iso.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>

#define DAY_MS QJS_CAL_ASTRO_DAY_MS
#define HOUR_MS 3600000.0
#define SYNODIC_GAP 25
#define CACHE_SIZE 8

typedef struct LunarCacheEntry {
    int32_t year;
    int64_t day;
} LunarCacheEntry;

typedef struct LunarContext {
    QJSCalendarId calendar;
    LunarCacheEntry solstice[CACHE_SIZE], new_year[CACHE_SIZE];
    unsigned solstice_count, new_year_count;
} LunarContext;

typedef struct LunarMonth {
    int nominal, leap;
    int64_t first_day;
} LunarMonth;

static int valid_calendar(QJSCalendarId calendar)
{
    return calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI;
}

int qjs_calendar_lunisolar_zone_offset(QJSCalendarId calendar, double utc_ms,
                                     int32_t *result_ms)
{
    int offset = 8;
    if (!valid_calendar(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (!result_ms || !isfinite(utc_ms) ||
        fabs(utc_ms) > QJS_CAL_ASTRO_MAX_ABS_MS)
        return QJS_CAL_RANGE;
    if (calendar == QJS_CAL_DANGI) {
        /* These are ICU's ad hoc astronomy rules, not civil Korean history.
         * dangical.cpp uses (year-1970)*365*DAY_MS in STANDARD_TIME.
         * tzrule.cpp getUTC subtracts the PREVIOUS rule's raw offset.
         * Keep the approximate transition dates and that subtraction exactly.
         * No 1908, 1954, or 1961 civil transition is present in this model. */
        const double start1897 = (1897.0 - 1970.0) * 365.0 * DAY_MS - 8.0 * HOUR_MS;
        const double start1898 = (1898.0 - 1970.0) * 365.0 * DAY_MS - 7.0 * HOUR_MS;
        const double start1912 = (1912.0 - 1970.0) * 365.0 * DAY_MS - 8.0 * HOUR_MS;
        if (utc_ms >= start1912)
            offset = 9;
        else if (utc_ms >= start1898)
            offset = 8;
        else if (utc_ms >= start1897)
            offset = 7;
    }
    *result_ms = offset * 3600000;
    return QJS_CAL_OK;
}

static int days_to_millis(LunarContext *context, double day, double *result)
{
    double time = day * DAY_MS;
    int32_t offset;
    int error = qjs_calendar_lunisolar_zone_offset(context->calendar, time, &offset);
    if (error)
        return error;
    /* Preserve ICU's intentional approximation: query the zone at the local
     * midnight value interpreted as UTC, then subtract the resulting offset. */
    *result = time - offset;
    return QJS_CAL_OK;
}

static int millis_to_days(LunarContext *context, double time, int64_t *result)
{
    int32_t offset;
    double day;
    int error = qjs_calendar_lunisolar_zone_offset(context->calendar, time, &offset);
    if (error)
        return error;
    day = floor((time + offset) / DAY_MS);
    if (!isfinite(day) || day < -400000000.0 || day > 400000000.0)
        return QJS_CAL_BACKEND;
    *result = (int64_t)day;
    return QJS_CAL_OK;
}

static int cache_lookup(const LunarCacheEntry *cache, unsigned count,
                         int32_t year, int64_t *day)
{
    unsigned i;
    for (i = 0; i < count && i < CACHE_SIZE; i++) {
        if (cache[i].year == year) {
            *day = cache[i].day;
            return 1;
        }
    }
    return 0;
}

static void cache_store(LunarCacheEntry *cache, unsigned *count,
                        int32_t year, int64_t day)
{
    /* A full cache simply stops accepting entries; it has no global state. */
    if (*count < CACHE_SIZE) {
        cache[*count].year = year;
        cache[*count].day = day;
        ++*count;
    }
}

static int winter_solstice(LunarContext *context, int32_t year, int64_t *result)
{
    double time, event;
    int64_t day;
    int error;
    if (cache_lookup(context->solstice, context->solstice_count, year, result))
        return QJS_CAL_OK;
    /* ICU starts on Dec 1, since Dec 15 misses some medieval solstices in
     * its fixed-epoch model. Gregorian metadata can extend past public days. */
    if (qjs_iso_date_to_days(&day, (QJSISODate){year, 12, 1}))
        return QJS_CAL_RANGE;
    error = days_to_millis(context, (double)day, &time);
    if (error)
        return error;
    error = qjs_calendar_astro_solar_time(time, QJS_CAL_ASTRO_PI * 1.5, 1, &event);
    if (error)
        return error;
    error = millis_to_days(context, event, &day);
    if (error)
        return error;
    cache_store(context->solstice, &context->solstice_count, year, day);
    *result = day;
    return QJS_CAL_OK;
}

static int new_moon_near(LunarContext *context, double day, int after,
                         int64_t *result)
{
    double time, event;
    int64_t found;
    int error = days_to_millis(context, day, &time);
    if (error)
        return error;
    error = qjs_calendar_astro_new_moon(time, after, &event);
    if (error)
        return error;
    error = millis_to_days(context, event, &found);
    if (error)
        return error;
    *result = found;
    return QJS_CAL_OK;
}

static int synodic_months_between(int64_t first, int64_t last)
{
    double value = (double)(last - first) / QJS_CAL_ASTRO_SYNODIC_MONTH;
    return (int)(value + (value >= 0.0 ? 0.5 : -0.5));
}

static int major_solar_term(LunarContext *context, int64_t day, int *result)
{
    double time, longitude;
    int term;
    int error = days_to_millis(context, (double)day, &time);
    if (error)
        return error;
    error = qjs_calendar_astro_sun_longitude(time, &longitude);
    if (error)
        return error;
    term = ((int)(6.0 * longitude / QJS_CAL_ASTRO_PI) + 2) % 12;
    *result = term < 1 ? term + 12 : term;
    return QJS_CAL_OK;
}

static int no_major_solar_term(LunarContext *context, int64_t moon, int *result)
{
    int first, last;
    int64_t next;
    int error = major_solar_term(context, moon, &first);
    if (error)
        return error;
    error = new_moon_near(context, (double)(moon + SYNODIC_GAP), 1, &next);
    if (error)
        return error;
    if (next - moon < 29 || next - moon > 30)
        return QJS_CAL_BACKEND;
    error = major_solar_term(context, next, &last);
    if (error)
        return error;
    *result = first == last;
    return QJS_CAL_OK;
}

static int leap_month_between(LunarContext *context, int64_t first,
                              int64_t last, int *result)
{
    int iteration;
    /* The callers bracket at most 13 months. ICU's open-ended backwards
     * loop is bounded here and every moon must strictly decrease. */
    for (iteration = 0; last >= first && iteration < 15; iteration++) {
        int no_term;
        int64_t previous;
        int error = no_major_solar_term(context, last, &no_term);
        if (error)
            return error;
        if (no_term) {
            *result = 1;
            return QJS_CAL_OK;
        }
        error = new_moon_near(context, (double)(last - SYNODIC_GAP), 0, &previous);
        if (error)
            return error;
        if (last - previous < 29 || last - previous > 30)
            return QJS_CAL_BACKEND;
        last = previous;
    }
    if (last >= first)
        return QJS_CAL_BACKEND;
    *result = 0;
    return QJS_CAL_OK;
}

static int new_year(LunarContext *context, int32_t year, int64_t *result)
{
    int64_t before, after, first, second, eleventh, found, minimum, maximum;
    int no_term1, no_term2;
    int error;
    if (cache_lookup(context->new_year, context->new_year_count, year, result))
        return QJS_CAL_OK;
    error = winter_solstice(context, year - 1, &before);
    if (error)
        return error;
    error = winter_solstice(context, year, &after);
    if (error)
        return error;
    if (after - before < 364 || after - before > 367)
        return QJS_CAL_BACKEND;
    error = new_moon_near(context, (double)(before + 1), 1, &first);
    if (error)
        return error;
    error = new_moon_near(context, (double)(first + SYNODIC_GAP), 1, &second);
    if (error)
        return error;
    error = new_moon_near(context, (double)(after + 1), 0, &eleventh);
    if (error)
        return error;
    found = second;
    if (synodic_months_between(first, eleventh) == 12) {
        error = no_major_solar_term(context, first, &no_term1);
        if (error)
            return error;
        error = no_major_solar_term(context, second, &no_term2);
        if (error)
            return error;
        if (no_term1 || no_term2) {
            error = new_moon_near(context, (double)(second + SYNODIC_GAP), 1, &found);
            if (error)
                return error;
        }
    }
    /* Arithmetic year is Gregorian year containing M01. The fixed tropical
     * model is not silently allowed to attach a different year at extremes. */
    if (qjs_iso_date_to_days(&minimum, (QJSISODate){year, 1, 1}) ||
        qjs_iso_date_to_days(&maximum, (QJSISODate){year + 1, 1, 1}))
        return QJS_CAL_RANGE;
    if (found < minimum || found >= maximum)
        return QJS_CAL_BACKEND;
    cache_store(context->new_year, &context->new_year_count, year, found);
    *result = found;
    return QJS_CAL_OK;
}

static int month_at_day(LunarContext *context, int64_t day, LunarMonth *result)
{
    QJSISODate date;
    int32_t gyear;
    int64_t before, after, first, last, moon, previous;
    int has_leap, prior_leap, no_term, nominal, leap, error;

    if (qjs_iso_date_from_days(&date, day))
        return QJS_CAL_RANGE;
    gyear = date.year;
    error = winter_solstice(context, gyear, &after);
    if (error)
        return error;
    if (day < after) {
        error = winter_solstice(context, gyear - 1, &before);
    } else {
        before = after;
        error = winter_solstice(context, gyear + 1, &after);
    }
    if (error)
        return error;
    if (!(before <= day && day < after))
        return QJS_CAL_BACKEND;
    error = new_moon_near(context, (double)(before + 1), 1, &first);
    if (error)
        return error;
    error = new_moon_near(context, (double)(after + 1), 0, &last);
    if (error)
        return error;
    error = new_moon_near(context, (double)(day + 1), 0, &moon);
    if (error)
        return error;
    if (moon > day || day - moon > 29)
        return QJS_CAL_BACKEND;
    has_leap = synodic_months_between(first, last) == 12;
    nominal = synodic_months_between(first, moon);
    leap = 0;
    if (has_leap) {
        error = leap_month_between(context, first, moon, &prior_leap);
        if (error)
            return error;
        nominal -= prior_leap;
        error = no_major_solar_term(context, moon, &no_term);
        if (error)
            return error;
        if (no_term) {
            error = new_moon_near(context, (double)(moon - SYNODIC_GAP), 0, &previous);
            if (error)
                return error;
            error = leap_month_between(context, first, previous, &prior_leap);
            if (error)
                return error;
            leap = !prior_leap;
        }
    }
    if (nominal < 1)
        nominal += 12;
    if (nominal < 1 || nominal > 12)
        return QJS_CAL_BACKEND;
    result->nominal = nominal;
    result->leap = leap;
    result->first_day = moon;
    return QJS_CAL_OK;
}

int qjs_cal_lunisolar_approximate_year(QJSCalendarId calendar, int32_t year,
                                      QJSCalendarYear *result)
{
    LunarContext context = {0};
    QJSCalendarYear value = {0};
    int64_t first, end, moon, next;
    int expected_nominal = 1, count = 0, leap_count = 0, error;

    if (!valid_calendar(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    context.calendar = calendar;
    error = new_year(&context, year, &first);
    if (error)
        return error;
    error = new_year(&context, year + 1, &end);
    if (error)
        return error;
    if (first < INT32_MIN || end > INT32_MAX ||
        end - first < 353 || end - first > 385)
        return QJS_CAL_BACKEND;
    moon = first;
    while (moon < end && count < 13) {
        LunarMonth info;

        error = month_at_day(&context, moon, &info);
        if (error)
            return error;
        if (info.first_day != moon)
            return QJS_CAL_BACKEND;
        if (info.leap) {
            if (!count || info.nominal != expected_nominal - 1 || ++leap_count > 1)
                return QJS_CAL_BACKEND;
            value.leap_month = info.nominal;
        } else {
            if (info.nominal != expected_nominal)
                return QJS_CAL_BACKEND;
            expected_nominal++;
        }
        error = new_moon_near(&context, (double)(moon + SYNODIC_GAP), 1, &next);
        if (error)
            return error;
        if (next - moon < 29 || next - moon > 30 || next > end)
            return QJS_CAL_BACKEND;
        value.month_lengths |= (uint32_t)(next - moon - 29) << (2 * count);
        moon = next;
        count++;
    }
    if (moon != end || (count != 12 && count != 13) ||
        expected_nominal != 13 || count != 12 + leap_count)
        return QJS_CAL_BACKEND;
    value.first_day = first;
    value.end_day = end;
    value.months = count;
    *result = value;
    return QJS_CAL_OK;
}
