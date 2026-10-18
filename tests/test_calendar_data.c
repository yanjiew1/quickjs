/* Publication pack validation, endian-independent views and failed queries. */
#include "../src/calendar/data.h"
#include "../src/calendar/arithmetic.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}

static void reject(const uint8_t *bytes, size_t size)
{
    QJSCalendarData result = qjs_native_calendar_data;

    assert(qjs_calendar_data_open(&result, bytes, size) == QJS_CAL_BACKEND);
    assert(result.bytes == qjs_native_calendar_data.bytes);
    assert(result.size == qjs_native_calendar_data.size);
    assert(result.count == qjs_native_calendar_data.count);
}

static void check_years(const QJSCalendarData *data, QJSCalendarId calendar,
                        int first, int last, int first_day, int end_day)
{
    QJSCalendarYear actual, embedded, saved = {123, 456, 789, 12, 0};
    int year, expected = first_day;

    for (year = first; year <= last; year++) {
        int month, days = 0;

        assert(!qjs_calendar_data_year(data, calendar, year, &actual));
        assert(!qjs_calendar_data_year(&qjs_native_calendar_data, calendar, year, &embedded));
        assert(actual.first_day == expected && actual.first_day == embedded.first_day);
        assert(actual.end_day == embedded.end_day);
        assert(actual.month_lengths == embedded.month_lengths);
        assert(actual.months == 12 && !actual.leap_month);
        for (month = 0; month < actual.months; month++)
            days += 29 + ((actual.month_lengths >> (2 * month)) & 3);
        assert(actual.end_day - actual.first_day == days);
        expected = actual.end_day;
    }
    assert(expected == end_day);
    actual = saved;
    assert(qjs_calendar_data_year(data, calendar, INT32_MIN, &actual) == QJS_CAL_RANGE);
    assert(actual.first_day == saved.first_day && actual.end_day == saved.end_day);
    assert(actual.month_lengths == saved.month_lengths && actual.months == saved.months);
    assert(qjs_calendar_data_year(data, calendar, INT32_MAX, &actual) == QJS_CAL_RANGE);
    assert(actual.first_day == saved.first_day);
}

static void lunar_metadata(void)
{
    /* Synthetic 384-day years: Chinese year -1 repeats M02; Dangi year
       1000000 repeats M12. These are format fixtures, not published dates. */
    static const uint8_t binary[] = {
        0x51, 0x4a, 0x43, 0x41, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x1e, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x80,
        0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x28, 0x00, 0x0f, 0x42, 0x40, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x80,
        0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x6a, 0x00, 0x00, 0x00, 0x00,
        0x73, 0x79, 0x6e, 0x74, 0x68, 0x65, 0x74, 0x69, 0x63, 0x00, 0x73, 0x79, 0x6e, 0x74, 0x68, 0x65,
        0x74, 0x69, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11, 0x0d, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11, 0x0d, 0x0c, 0x00, 0x00,
    };
    QJSCalendarData data;
    QJSCalendarYear year;
    uint8_t corrupt[sizeof(binary)];

    assert(!qjs_calendar_data_open(&data, binary, sizeof(binary)));
    assert(!qjs_calendar_data_year(&data, QJS_CAL_CHINESE, -1, &year));
    assert(year.months == 13 && year.leap_month == 2 && year.end_day == 384);
    assert(!qjs_calendar_data_year(&data, QJS_CAL_DANGI, 1000000, &year));
    assert(year.months == 13 && year.leap_month == 12);
    memcpy(corrupt, binary, sizeof(binary));
    corrupt[125] = 0; /* A 13-month year needs a repeated nominal month. */
    reject(corrupt, sizeof(corrupt));
    corrupt[125] = 13;
    reject(corrupt, sizeof(corrupt));
    memcpy(corrupt, binary, sizeof(binary));
    corrupt[71]++; /* First year 1000001 exceeds the arithmetic contract. */
    reject(corrupt, sizeof(corrupt));
}

int main(void)
{
    const QJSCalendarData *compiled = &qjs_native_calendar_data;
    QJSCalendarData opened, unaligned;
    QJSCalendarYear saved = {1, 2, 3, 4, 5}, result = saved;
    uint8_t *copy = malloc(compiled->size + 1);
    size_t i;
    /* Header and first index fields, including signed year limits. */
    const unsigned bad_offsets[] = {0, 4, 8, 12, 16, 20, 24, 28,
                                    32, 36, 40, 44, 48, 52, 56, 60};

    assert(copy);
    lunar_metadata();
    assert(!qjs_calendar_data_open(&opened, compiled->bytes, compiled->size));
    assert(opened.count == 2);
    check_years(&opened, QJS_CAL_PERSIAN, 1206, 1498, -52150, 54866);
    check_years(&opened, QJS_CAL_ISLAMIC_UMALQURA, 1300, 1600, -31826, 74839);
    assert(!strcmp(qjs_calendar_data_version(&opened, QJS_CAL_PERSIAN), "tehran-1206-1498"));
    assert(!qjs_calendar_data_version(&opened, QJS_CAL_ISO8601));
    assert(qjs_calendar_data_year(&opened, QJS_CAL_ISO8601, 2026, &result) == QJS_CAL_UNSUPPORTED);
    assert(result.first_day == saved.first_day && result.months == saved.months);
    for (i = 0; i < compiled->size; i++)
        reject(compiled->bytes, i);
    for (i = 0; i < sizeof(bad_offsets) / sizeof(bad_offsets[0]); i++) {
        memcpy(copy, compiled->bytes, compiled->size);
        copy[bad_offsets[i]] ^= 0x80;
        reject(copy, compiled->size);
    }
    for (i = 0; i < 12; i++) {
        memcpy(copy, compiled->bytes, compiled->size);
        copy[u32(copy + 48) + i] ^= 0x80;
        reject(copy, compiled->size);
    }
    memcpy(copy + 1, compiled->bytes, compiled->size);
    assert(!qjs_calendar_data_open(&unaligned, copy + 1, compiled->size));
    check_years(&unaligned, QJS_CAL_PERSIAN, 1206, 1498, -52150, 54866);
    check_years(&unaligned, QJS_CAL_ISLAMIC_UMALQURA, 1300, 1600, -31826, 74839);
    free(copy);
    return 0;
}
