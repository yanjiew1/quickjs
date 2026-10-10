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
 * Calendar classes, mutable fields, caches, and timezones are replaced by
 * allocation-free C primitives and a small per-call cache. The arithmetic
 * year and no-era output follow Era/MonthCode 5833eae6. Strict month/year
 * invariants and bounded searches report BACKEND rather than invent fields.
 * This approximation alone does NOT meet the proposal's published-data
 * requirement for Chinese 1900..2100 / Dangi 1900..2050. Both IDs must remain
 * unadvertised until data provenance and validation gates are completed.
 * This source candidate has not been compiled or executed by its author.
 */
#include "lunisolar.h"
#include "astronomy.h"
#include "internal.h"
#ifdef QJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA
#include "published-lunisolar.h"
#endif
#include <math.h>
#include <stddef.h>
#include <string.h>

#define DAY_MS QJS_CAL_ASTRO_DAY_MS
#define HOUR_MS 3600000.0
#define SYNODIC_GAP 25
#define CACHE_SIZE 8
#ifdef QJS_CAL_LUNISOLAR_APPROXIMATION_REVIEW
#define APPROXIMATION_REVIEW 1
#else
#define APPROXIMATION_REVIEW 0
#endif

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

typedef struct LunarYear {
    int32_t year;
    int months, days;
    int64_t first[14];
    char code[13][5];
} LunarYear;

static void make_code(char code[5], int nominal, int leap);

static int valid_calendar(QJSCalendarId calendar)
{
    return calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI;
}

static int valid_year(int32_t year)
{
    return year >= QJS_CAL_MIN_YEAR && year <= QJS_CAL_MAX_YEAR;
}

static int published_last_iso_year(QJSCalendarId calendar)
{
    return calendar == QJS_CAL_CHINESE ? 2100 : 2050;
}

static int requires_published_day(QJSCalendarId calendar, int64_t day)
{
    return day >= qjs_calendar_gregorian_to_epoch_day_unchecked(1900, 1, 1) &&
        day < qjs_calendar_gregorian_to_epoch_day_unchecked(
            published_last_iso_year(calendar) + 1, 1, 1);
}

#ifdef QJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA
static const QJSPublishedCalendarTable *provider_arithmetic(QJSCalendarId calendar)
{
    const QJSLunisolarProvider *provider = qjs_calendar_lunisolar_provider(calendar);
    uint8_t authority = calendar == QJS_CAL_CHINESE ?
        QJS_LUNISOLAR_PRIMARY_PMO : QJS_LUNISOLAR_PRIMARY_KASI;
    size_t index;
    if (!provider || !provider->arithmetic ||
        !provider->primary_spans || !provider->primary_span_count ||
        provider->primary_span_count > 10000)
        return NULL;
    for (index = 0; index < provider->primary_span_count; index++) {
        const QJSLunisolarPrimarySpan *span = &provider->primary_spans[index];
        if (span->authority != authority || span->first_day >= span->end_day ||
            (index && provider->primary_spans[index - 1].end_day > span->first_day))
            return NULL;
    }
    return provider->arithmetic;
}

static int primary_covers_interval(QJSCalendarId calendar,
                                  int64_t first, int64_t end)
{
    const QJSLunisolarProvider *provider = qjs_calendar_lunisolar_provider(calendar);
    int64_t mandatory_first =
        qjs_calendar_gregorian_to_epoch_day_unchecked(1900, 1, 1);
    int64_t mandatory_end = qjs_calendar_gregorian_to_epoch_day_unchecked(
        published_last_iso_year(calendar) + 1, 1, 1);
    size_t index;
    if (end <= first || !provider_arithmetic(calendar))
        return 0;
    if (first < mandatory_first)
        first = mandatory_first;
    if (end > mandatory_end)
        end = mandatory_end;
    if (end <= first)
        return 1;
    for (index = 0; index < provider->primary_span_count && first < end; index++) {
        const QJSLunisolarPrimarySpan *span = &provider->primary_spans[index];
        if (span->end_day <= first)
            continue;
        if (span->first_day > first)
            return 0;
        first = span->end_day;
    }
    return first >= end;
}

static int published_year(LunarContext *context, int32_t year, LunarYear *result)
{
    const QJSPublishedCalendarTable *table =
        provider_arithmetic(context->calendar);
    LunarYear value;
    size_t lower = 0, upper, start, index;
    int expected_nominal = 1, leap_count = 0, count = 0;
    int64_t next_day = 0;
    if (!table || !table->months || !table->count)
        return QJS_CAL_BACKEND;
    upper = table->count;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2;
        if (table->months[middle].lunar_year < year)
            lower = middle + 1;
        else
            upper = middle;
    }
    start = lower;
    memset(&value, 0, sizeof(value));
    value.year = year;
    for (index = start; index < table->count &&
            table->months[index].lunar_year == year; index++) {
        const QJSPublishedCalendarMonth *month = &table->months[index];
        if (count == 13 || month->month < 1 || month->month > 12 ||
            month->is_leap > 1 || (month->length != 29 && month->length != 30) ||
            month->epoch_day < table->proven_first_day ||
            (int64_t)month->epoch_day + month->length > table->proven_end_day ||
            !primary_covers_interval(context->calendar, month->epoch_day,
                (int64_t)month->epoch_day + month->length) ||
            (count && month->epoch_day != next_day))
            return QJS_CAL_BACKEND;
        if (month->is_leap) {
            if (!count || month->month != expected_nominal - 1 || ++leap_count > 1)
                return QJS_CAL_BACKEND;
        } else {
            if (month->month != expected_nominal)
                return QJS_CAL_BACKEND;
            ++expected_nominal;
        }
        value.first[count] = month->epoch_day;
        make_code(value.code[count], month->month, month->is_leap);
        next_day = (int64_t)month->epoch_day + month->length;
        ++count;
    }
    /* Exact next M01 must be evidenced, not guessed from the last length.
     * Partial initial/final source years cannot yield complete year fields. */
    if ((count != 12 && count != 13) || count != 12 + leap_count ||
        expected_nominal != 13 || index >= table->count ||
        table->months[index].lunar_year != year + 1 ||
        table->months[index].month != 1 || table->months[index].is_leap ||
        table->months[index].epoch_day != next_day ||
        ((table->months[index].length == 29 || table->months[index].length == 30) ?
            next_day + table->months[index].length > table->proven_end_day :
            (table->months[index].length != 0 || index + 1 != table->count ||
             next_day != table->proven_end_day)) ||
        !primary_covers_interval(context->calendar, next_day, next_day + 1) ||
        next_day - value.first[0] < 353 || next_day - value.first[0] > 385)
        return QJS_CAL_BACKEND;
    value.first[count] = next_day;
    value.months = count;
    value.days = (int)(next_day - value.first[0]);
    *result = value;
    return QJS_CAL_OK;
}

static int published_year_containing_day(LunarContext *context, int64_t day,
                                        int32_t *result)
{
    const QJSPublishedCalendarTable *table =
        provider_arithmetic(context->calendar);
    size_t lower = 0, upper;
    const QJSPublishedCalendarMonth *month;
    if (!table || !table->months || !table->count ||
        day < table->proven_first_day || day >= table->proven_end_day)
        return QJS_CAL_BACKEND;
    upper = table->count;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2;
        if (table->months[middle].epoch_day <= day)
            lower = middle + 1;
        else
            upper = middle;
    }
    if (!lower)
        return QJS_CAL_BACKEND;
    month = &table->months[lower - 1];
    if ((month->length != 29 && month->length != 30) ||
        day >= (int64_t)month->epoch_day + month->length ||
        !primary_covers_interval(context->calendar, month->epoch_day,
            (int64_t)month->epoch_day + month->length) ||
        !valid_year(month->lunar_year))
        return QJS_CAL_BACKEND;
    *result = month->lunar_year;
    return QJS_CAL_OK;
}

static int published_added_month(LunarContext *context, int64_t first_day,
                                int64_t delta, int64_t *result_day,
                                int32_t *result_year)
{
    const QJSPublishedCalendarTable *table =
        provider_arithmetic(context->calendar);
    size_t lower = 0, upper;
    int64_t target;
    if (!table || !table->months || !table->count || table->count > 30000000)
        return QJS_CAL_BACKEND;
    upper = table->count;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2;
        if (table->months[middle].epoch_day < first_day)
            lower = middle + 1;
        else
            upper = middle;
    }
    if (lower >= table->count || table->months[lower].epoch_day != first_day)
        return QJS_CAL_BACKEND;
    target = (int64_t)lower + delta;
    if (target < 0 || target >= (int64_t)table->count)
        return QJS_CAL_BACKEND;
    /* A validated source is contiguous; check the complete traversed span
     * with at most the published dataset's few thousand month records.
     * This guards against an audit accidentally linking a sparse table. */
    {
        size_t a = target < (int64_t)lower ? (size_t)target : lower;
        size_t b = target > (int64_t)lower ? (size_t)target : lower;
        size_t i;
        for (i = a; i < b; i++) {
            const QJSPublishedCalendarMonth *month = &table->months[i];
            if ((month->length != 29 && month->length != 30) ||
                !primary_covers_interval(context->calendar, month->epoch_day,
                    (int64_t)month->epoch_day + month->length) ||
                (int64_t)month->epoch_day + month->length !=
                    table->months[i + 1].epoch_day)
                return QJS_CAL_BACKEND;
        }
    }
    *result_day = table->months[(size_t)target].epoch_day;
    *result_year = table->months[(size_t)target].lunar_year;
    return QJS_CAL_OK;
}
#endif

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
    day = qjs_calendar_gregorian_to_epoch_day_unchecked(year, 12, 1);
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
    int64_t before, after, first, second, eleventh, found;
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
    if (found < qjs_calendar_gregorian_to_epoch_day_unchecked(year, 1, 1) ||
        found >= qjs_calendar_gregorian_to_epoch_day_unchecked(year + 1, 1, 1))
        return QJS_CAL_BACKEND;
    cache_store(context->new_year, &context->new_year_count, year, found);
    *result = found;
    return QJS_CAL_OK;
}

static int month_at_day(LunarContext *context, int64_t day, LunarMonth *result)
{
    int32_t gyear = qjs_calendar_gregorian_year_from_epoch_day(day);
    int64_t before, after, first, last, moon, previous;
    int has_leap, prior_leap, no_term, nominal, leap;
    int error = winter_solstice(context, gyear, &after);
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

static void make_code(char code[5], int nominal, int leap)
{
    code[0] = 'M';
    code[1] = (char)('0' + nominal / 10);
    code[2] = (char)('0' + nominal % 10);
    code[3] = leap ? 'L' : '\0';
    code[4] = '\0';
}

static int build_year(LunarContext *context, int32_t year, LunarYear *result)
{
    LunarYear value;
    int64_t first, end, moon, next;
    int count = 0, expected_nominal = 1, leap_count = 0;
    int error;
#ifdef QJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA
    error = published_year(context, year, result);
    if (!error)
        return QJS_CAL_OK;
#endif
    /* A composite provider may complete a boundary lunar year with explicitly
     * approximate months outside the mandatory Gregorian window. Its required
     * dates still need primary spans. Failed joins never permit a whole-year
     * astronomical fallback that would approximate mandated Gregorian days. */
    if (!APPROXIMATION_REVIEW && year >= 1899 &&
        year <= published_last_iso_year(context->calendar))
        return QJS_CAL_BACKEND;
    error = new_year(context, year, &first);
    if (error)
        return error;
    error = new_year(context, year + 1, &end);
    if (error)
        return error;
    if (end - first < 353 || end - first > 385)
        return QJS_CAL_BACKEND;
    memset(&value, 0, sizeof(value));
    value.year = year;
    value.days = (int)(end - first);
    moon = first;
    while (moon < end && count < 13) {
        LunarMonth info;
        error = month_at_day(context, moon, &info);
        if (error)
            return error;
        if (info.first_day != moon)
            return QJS_CAL_BACKEND;
        if (info.leap) {
            if (!count || info.nominal != expected_nominal - 1 || ++leap_count > 1)
                return QJS_CAL_BACKEND;
        } else {
            if (info.nominal != expected_nominal)
                return QJS_CAL_BACKEND;
            ++expected_nominal;
        }
        value.first[count] = moon;
        make_code(value.code[count], info.nominal, info.leap);
        error = new_moon_near(context, (double)(moon + SYNODIC_GAP), 1, &next);
        if (error)
            return error;
        if (next - moon < 29 || next - moon > 30 || next > end)
            return QJS_CAL_BACKEND;
        moon = next;
        ++count;
    }
    if (moon != end || (count != 12 && count != 13) ||
        expected_nominal != 13 || count != 12 + leap_count)
        return QJS_CAL_BACKEND;
    value.first[count] = end;
    value.months = count;
    *result = value;
    return QJS_CAL_OK;
}

static int year_containing_day(LunarContext *context, int64_t day,
                               int32_t *result)
{
    int32_t year;
    int64_t first;
    int error;
#ifdef QJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA
    error = published_year_containing_day(context, day, result);
    if (!error)
        return QJS_CAL_OK;
#endif
    if (!APPROXIMATION_REVIEW && requires_published_day(context->calendar, day))
        return QJS_CAL_BACKEND;
    year = qjs_calendar_gregorian_year_from_epoch_day(day);
    error = new_year(context, year, &first);
    if (error)
        return error;
    if (day < first)
        --year;
    if (!valid_year(year))
        return QJS_CAL_RANGE;
    *result = year;
    return QJS_CAL_OK;
}

static int from_epoch_day(QJSCalendarId calendar, int64_t epoch_day,
                            QJSCalendarDate *result, int for_intl)
{
    LunarContext context = {0};
    LunarYear year;
    QJSCalendarDate value;
    int32_t arithmetic_year;
    int month, error;
    if (!valid_calendar(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (!result ||
        epoch_day < (for_intl ? QJS_CAL_INTL_MIN_EPOCH_DAY : QJS_CAL_MIN_EPOCH_DAY) ||
        epoch_day > (for_intl ? QJS_CAL_INTL_MAX_EPOCH_DAY : QJS_CAL_MAX_EPOCH_DAY))
        return QJS_CAL_RANGE;
    context.calendar = calendar;
    error = year_containing_day(&context, epoch_day, &arithmetic_year);
    if (error)
        return error;
    error = build_year(&context, arithmetic_year, &year);
    if (error)
        return error;
    if (!(year.first[0] <= epoch_day && epoch_day < year.first[year.months]))
        return QJS_CAL_BACKEND;
    for (month = 0; month < year.months && epoch_day >= year.first[month + 1]; month++)
        ;
    memset(&value, 0, sizeof(value));
    value.year = arithmetic_year;
    value.month = month + 1;
    value.day = (int32_t)(epoch_day - year.first[month] + 1);
    memcpy(value.month_code, year.code[month], sizeof(value.month_code));
    value.day_of_year = (int32_t)(epoch_day - year.first[0] + 1);
    value.days_in_month = (int32_t)(year.first[month + 1] - year.first[month]);
    value.days_in_year = year.days;
    value.months_in_year = year.months;
    value.in_leap_year = year.months == 13;
    /* has_era=0, empty era, and era_year=0 are intentional normative fields. */
    *result = value;
    return QJS_CAL_OK;
}


int qjs_calendar_lunisolar_from_epoch_day(QJSCalendarId calendar,
                                        int64_t epoch_day,
                                        QJSCalendarDate *result)
{
    return from_epoch_day(calendar, epoch_day, result, 0);
}
int qjs_calendar_lunisolar_from_epoch_day_for_intl(QJSCalendarId calendar,
                                                int64_t epoch_day,
                                                QJSCalendarDate *result)
{
    return from_epoch_day(calendar, epoch_day, result, 1);
}

static int to_epoch_day(QJSCalendarId calendar, int32_t year,
                       int month, int day, int bounded, int64_t *result)
{
    LunarContext context = {0};
    LunarYear value;
    int64_t epoch_day;
    int error;
    if (!valid_calendar(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (!result || !valid_year(year) || month < 1 || month > 13 || day < 1 || day > 30)
        return QJS_CAL_RANGE;
    context.calendar = calendar;
    error = build_year(&context, year, &value);
    if (error)
        return error;
    if (month > value.months || day > value.first[month] - value.first[month - 1])
        return QJS_CAL_RANGE;
    epoch_day = value.first[month - 1] + day - 1;
    if (bounded && (epoch_day < QJS_CAL_MIN_EPOCH_DAY || epoch_day > QJS_CAL_MAX_EPOCH_DAY))
        return QJS_CAL_RANGE;
    *result = epoch_day;
    return QJS_CAL_OK;
}

int qjs_calendar_lunisolar_to_epoch_day(QJSCalendarId calendar, int32_t year,
                                      int month, int day, int64_t *result)
{
    return to_epoch_day(calendar, year, month, day, 1, result);
}

int qjs_calendar_lunisolar_to_epoch_day_unbounded(QJSCalendarId calendar,
                                      int32_t year, int month, int day,
                                      int64_t *result)
{
    return to_epoch_day(calendar, year, month, day, 0, result);
}

int qjs_calendar_lunisolar_month_info(QJSCalendarId calendar, int32_t year,
                                    int month, int *months, int *days,
                                    char month_code[5])
{
    LunarContext context = {0};
    LunarYear value;
    int error;
    if (!valid_calendar(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (!months || !days || !month_code || !valid_year(year) || month < 1 || month > 13)
        return QJS_CAL_RANGE;
    context.calendar = calendar;
    error = build_year(&context, year, &value);
    if (error)
        return error;
    if (month > value.months)
        return QJS_CAL_RANGE;
    *months = value.months;
    *days = (int)(value.first[month] - value.first[month - 1]);
    memcpy(month_code, value.code[month - 1], 5);
    return QJS_CAL_OK;
}

int qjs_calendar_lunisolar_add_months(QJSCalendarId calendar, int32_t year,
                                    int month, int64_t delta,
                                    int32_t *result_year, int *result_month)
{
    LunarContext context = {0};
    LunarYear source, target;
    int32_t target_year;
    int64_t source_number, wanted_number, candidate_number, candidate_day;
    double source_time, candidate_time, search_time;
    int target_month, adjustment, error;
    if (!valid_calendar(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (!result_year || !result_month || !valid_year(year) || month < 1 || month > 13)
        return QJS_CAL_RANGE;
    /* At most 13 months per year within the finite internal year domain.
     * Check before adding to the source index, so INT64_MIN/MAX never overflow. */
    if (delta < -INT64_C(26000013) || delta > INT64_C(26000013))
        return QJS_CAL_RANGE;
    context.calendar = calendar;
    error = build_year(&context, year, &source);
    if (error)
        return error;
    if (month > source.months)
        return QJS_CAL_RANGE;
    if (delta == 0) {
        *result_year = year;
        *result_month = month;
        return QJS_CAL_OK;
    }
#ifdef QJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA
    error = published_added_month(&context, source.first[month - 1], delta,
                                  &candidate_day, &target_year);
    if (!error) {
        if (!valid_year(target_year))
            return QJS_CAL_BACKEND;
        error = build_year(&context, target_year, &target);
        if (error)
            return error;
        for (target_month = 0; target_month < target.months; target_month++) {
            if (target.first[target_month] == candidate_day) {
                *result_year = target_year;
                *result_month = target_month + 1;
                return QJS_CAL_OK;
            }
        }
        return QJS_CAL_BACKEND;
    }
#endif
    error = days_to_millis(&context, (double)source.first[month - 1], &source_time);
    if (error)
        return error;
    error = qjs_calendar_astro_mean_lunation_number(source_time, &source_number);
    if (error)
        return error;
    wanted_number = source_number + delta;
    /* The unwrapped model's mean rate avoids accumulated SYNODIC_MONTH
     * drift. The approximation is only a search seed; verify its lunation
     * number and then locate the identical month start in a validated year. */
    search_time = source_time + ((double)delta - 0.5) *
                    qjs_calendar_astro_mean_lunation_days() * DAY_MS;
    for (adjustment = 0; adjustment < 4; adjustment++) {
        error = qjs_calendar_astro_new_moon(search_time, 1, &candidate_time);
        if (error)
            return error;
        error = qjs_calendar_astro_mean_lunation_number(candidate_time, &candidate_number);
        if (error)
            return error;
        if (candidate_number == wanted_number)
            break;
        search_time += (double)(wanted_number - candidate_number) *
                         qjs_calendar_astro_mean_lunation_days() * DAY_MS;
    }
    if (adjustment == 4)
        return QJS_CAL_BACKEND;
    error = millis_to_days(&context, candidate_time, &candidate_day);
    if (error)
        return error;
    error = year_containing_day(&context, candidate_day, &target_year);
    if (error)
        return error;
    error = build_year(&context, target_year, &target);
    if (error)
        return error;
    for (target_month = 0; target_month < target.months; target_month++) {
        if (target.first[target_month] == candidate_day) {
            *result_year = target_year;
            *result_month = target_month + 1;
            return QJS_CAL_OK;
        }
    }
    return QJS_CAL_BACKEND;
}
