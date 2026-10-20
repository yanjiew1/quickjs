/* One instant drives timezone rules, calendar identity and full clock precision.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/local-time.h"
#include "../src/calendar/calendar.h"
#include <assert.h>
#include <string.h>

static QJSIntlDateLocalTime local(QJSCalendarId calendar, int64_t second, uint32_t nano,
                                  const QJSTimeZone *zone, int32_t minutes)
{
    QJSIntlDateLocalTime result;
    assert(!qjs_intl_date_local_time(calendar, second, nano, zone, minutes, &result));
    assert(result.nanosecond == nano && result.subsecond == (nano != 0));
    assert(result.fields.numeric.millisecond == nano / 1000000);
    return result;
}

static void expect(const QJSIntlDateLocalTime *value, int year, unsigned int month, unsigned int day,
                    unsigned int hour, unsigned int minute, unsigned int second, int offset, unsigned int dst)
{
    const QJSIntlDateNumericFields *n = &value->fields.numeric;
    assert(n->year == year && n->month == month && n->day == day && n->hour == hour &&
           n->minute == minute && n->second == second && value->offset_seconds == offset && value->is_dst == dst);
}

static void errors(void)
{
    QJSIntlDateLocalTime result, zero = {0};
    const QJSTimeZone *utc;
    static const struct { int64_t second; uint32_t nano; int minutes; QJSIntlStatus status; } cases[] = {
        {0, 1000000000, 0, QJS_INTL_INVALID_ARGUMENT},
        {0, 0, 1440, QJS_INTL_INVALID_ARGUMENT}, {0, 0, -1440, QJS_INTL_INVALID_ARGUMENT},
        {INT64_MAX, 0, 1, QJS_INTL_OVERFLOW}, {INT64_MIN, 0, -1, QJS_INTL_OVERFLOW},
        {INT64_MAX, 0, 0, QJS_INTL_INVALID_ARGUMENT}, {INT64_MIN, 0, 0, QJS_INTL_INVALID_ARGUMENT},
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_local_time(QJS_CAL_GREGORY, cases[i].second, cases[i].nano, NULL,
                                        cases[i].minutes, &result) == cases[i].status);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    assert(!qjs_timezone_lookup(&utc, "UTC", 3));
    assert(qjs_intl_date_local_time(QJS_CAL_GREGORY, 0, 0, utc, 1, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_date_local_time(QJS_CAL_COUNT, 0, 0, NULL, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_date_local_time(QJS_CAL_GREGORY, 0, 0, NULL, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

static void core(void)
{
    const QJSTimeZone *utc;
    QJSIntlDateLocalTime value, copy;
    assert(!qjs_timezone_lookup(&utc, "UTC", 3));
    value = local(QJS_CAL_GREGORY, -1, 999999999, utc, 0);
    expect(&value, 1969, 12, 31, 23, 59, 59, 0, 0);
    assert(value.epoch_day == -1 && value.fields.weekday == 3);
    copy = value;
    value = local(QJS_CAL_GREGORY, 0, 1, NULL, 0);
    expect(&value, 1970, 1, 1, 0, 0, 0, 0, 0);
    assert(!value.epoch_day && value.fields.weekday == 4 && !value.fields.numeric.millisecond);
    assert(copy.fields.numeric.year == 1969 && !strcmp(copy.fields.month_code, "M12"));
    value = local(QJS_CAL_GREGORY, 60, 0, NULL, 1439);
    expect(&value, 1970, 1, 2, 0, 0, 0, 86340, 0);
    value = local(QJS_CAL_GREGORY, 0, 0, NULL, -1439);
    expect(&value, 1969, 12, 31, 0, 1, 0, -86340, 0);
    value = local(QJS_CAL_GREGORY, 43200, 0, utc, 0);
    assert(!value.subsecond && value.fields.numeric.hour == 12);
    value = local(QJS_CAL_GREGORY, 43200, 1, utc, 0);
    assert(value.subsecond && !value.fields.numeric.millisecond && value.fields.numeric.hour == 12);
    value = local(QJS_CAL_GREGORY, INT64_C(-8640000000000), 0, utc, 0);
    expect(&value, -271821, 4, 20, 0, 0, 0, 0, 0);
    value = local(QJS_CAL_GREGORY, INT64_C(8640000000000), 0, utc, 0);
    expect(&value, 275760, 9, 13, 0, 0, 0, 0, 0);
    value = local(QJS_CAL_GREGORY, INT64_C(8640000000000), 999999999, NULL, 1439);
    expect(&value, 275760, 9, 13, 23, 59, 0, 86340, 0);
}

static void named(void)
{
    const QJSTimeZone *zone;
    QJSIntlDateLocalTime value;
    if (qjs_timezone_count() == 1) {
        assert(qjs_timezone_lookup(&zone, "America/New_York", 16) == QJS_TZ_RANGE);
        return;
    }
    assert(!qjs_timezone_lookup(&zone, "America/New_York", 16));
    value = local(QJS_CAL_GREGORY, 1710053999, 999999999, zone, 0);
    expect(&value, 2024, 3, 10, 1, 59, 59, -18000, 0);
    value = local(QJS_CAL_GREGORY, 1710054000, 0, zone, 0);
    expect(&value, 2024, 3, 10, 3, 0, 0, -14400, 1);
    value = local(QJS_CAL_GREGORY, 1730613599, 0, zone, 0);
    expect(&value, 2024, 11, 3, 1, 59, 59, -14400, 1);
    value = local(QJS_CAL_GREGORY, 1730613600, 0, zone, 0);
    expect(&value, 2024, 11, 3, 1, 0, 0, -18000, 0);
    /* Historical LMT retains seconds, rather than rounding the local clock. */
    value = local(QJS_CAL_GREGORY, -5364662400LL, 123456789, zone, 0);
    assert(value.offset_seconds == -17762 && value.fields.numeric.second == 58);
    assert(!qjs_timezone_lookup(&zone, "Europe/Dublin", 13));
    value = local(QJS_CAL_GREGORY, 1719835200, 0, zone, 0);
    expect(&value, 2024, 7, 1, 13, 0, 0, 3600, 0);
    value = local(QJS_CAL_GREGORY, 1704110400, 0, zone, 0);
    expect(&value, 2024, 1, 1, 12, 0, 0, 0, 1); /* Raw negative-DST designation. */
    if (qjs_calendar_is_supported(QJS_CAL_JAPANESE)) {
        assert(!qjs_timezone_lookup(&zone, "Asia/Tokyo", 10));
        value = local(QJS_CAL_JAPANESE, 1556636399, 0, zone, 0);
        expect(&value, 31, 4, 30, 23, 59, 59, 32400, 0);
        assert(!strcmp(value.fields.era_code, "heisei"));
        value = local(QJS_CAL_JAPANESE, 1556636400, 0, zone, 0);
        expect(&value, 1, 5, 1, 0, 0, 0, 32400, 0);
        assert(!strcmp(value.fields.era_code, "reiwa"));
        assert(!qjs_timezone_lookup(&zone, "Asia/Jerusalem", 14));
        value = local(QJS_CAL_HEBREW, 1694811599, 0, zone, 0);
        assert(value.fields.arithmetic_year == 5783 && value.fields.numeric.hour == 23);
        value = local(QJS_CAL_HEBREW, 1694811600, 0, zone, 0);
        assert(value.fields.arithmetic_year == 5784 && value.fields.numeric.day == 1 &&
               !strcmp(value.fields.month_code, "M01") && !value.fields.numeric.hour);
        assert(!qjs_timezone_lookup(&zone, "Asia/Shanghai", 13));
        value = local(QJS_CAL_CHINESE, 1679414400, 0, zone, 0);
        assert(value.fields.numeric.related_year == 2023 && value.fields.leap_month &&
               !strcmp(value.fields.month_code, "M02L") && !value.fields.numeric.hour);
    }
}

int main(void)
{
    core();
    named();
    errors();
    return 0;
}
