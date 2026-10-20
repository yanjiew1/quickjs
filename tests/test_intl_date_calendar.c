/* Real calendar boundaries and independent numeric/name identities.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/chronology.h"
#include "../src/calendar/calendar.h"
#include <assert.h>
#include <string.h>

static QJSIntlDateCalendarFields fields(QJSCalendarId calendar, int64_t day)
{
    QJSIntlDateCalendarFields result;
    assert(!qjs_intl_date_calendar_fields(calendar, day, &result));
    return result;
}

static int era_domain_contains(QJSCalendarId calendar, const char *code)
{
    const char *name = "unchanged", *previous;
    size_t i, j;
    int found = 0, error;
    for (i = 0; ; i++) {
        previous = name;
        error = qjs_calendar_era_at(calendar, i, &name);
        if (error) {
            assert(error == QJS_CAL_UNSUPPORTED && name == previous);
            return found;
        }
        assert(name && *name);
        if (!strcmp(name, code))
            found = 1;
        for (j = 0; j < i; j++) {
            const char *other;
            assert(!qjs_calendar_era_at(calendar, j, &other));
            assert(strcmp(name, other));
        }
    }
}

static QJSIntlDateCalendarFields calendar_date(QJSCalendarId calendar, int year, int month)
{
    int64_t day;
    assert(!qjs_calendar_to_epoch_day(calendar, year, month, 1, &day));
    return fields(calendar, day);
}

static void hebrew_months(void)
{
    static const char *const common[] = {
        "1", "2", "3", "4", "5", "7", "8", "9", "10", "11", "12", "13",
    };
    static const char *const leap[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
    };
    unsigned int i, which;
    for (which = 0; which < 2; which++) {
        unsigned int count = which ? 13 : 12;
        for (i = 0; i < count; i++) {
            QJSIntlDateCalendarFields value = calendar_date(QJS_CAL_HEBREW, 5783 + which, i + 1);
            assert(value.numeric.month == i + 1 && value.in_leap_year == which);
            assert(!strcmp(value.month_key, which ? leap[i] : common[i]));
            assert(value.leap_month == (which && i == 5));
        }
    }
}

static void lunar_years_and_months(void)
{
    static const QJSCalendarId calendars[] = {QJS_CAL_CHINESE, QJS_CAL_DANGI};
    static const struct { int year; unsigned int cycle; } years[] = {
        {-57, 60}, {-56, 1}, {0, 57}, {4, 1}, {1984, 1}, {2043, 60}, {2044, 1},
    };
    unsigned int c, i;
    for (c = 0; c < sizeof(calendars) / sizeof(calendars[0]); c++) {
        QJSCalendarId calendar = calendars[c];
        for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
            QJSIntlDateCalendarFields value = calendar_date(calendar, years[i].year, 1);
            assert(value.has_cyclic_year && !value.has_era && !value.era_code[0]);
            assert(value.cyclic_year == years[i].cycle);
            assert(value.numeric.related_year == years[i].year);
            assert(value.numeric.year == years[i].year);
        }
        /* In 2023 both calendars repeat nominal month 2, then advance to 3. */
        for (i = 2; i <= 4; i++) {
            QJSIntlDateCalendarFields value = calendar_date(calendar, 2023, i);
            assert(value.numeric.month == (i == 4 ? 3u : 2u));
            assert(value.leap_month == (i == 3) && value.in_leap_year);
            assert(!strcmp(value.month_key, i == 4 ? "3" : "2"));
            assert(value.numeric.related_year == 2023 && value.cyclic_year == 40);
        }
    }
}

static void signed_years(void)
{
    static const QJSCalendarId calendars[] = {
        QJS_CAL_BUDDHIST, QJS_CAL_COPTIC, QJS_CAL_ETHIOAA, QJS_CAL_HEBREW,
        QJS_CAL_INDIAN, QJS_CAL_PERSIAN, QJS_CAL_ROC, QJS_CAL_ISLAMIC_CIVIL,
        QJS_CAL_ISLAMIC_TBLA, QJS_CAL_ISLAMIC_UMALQURA,
    };
    unsigned int i;
    int year;
    for (i = 0; i < sizeof(calendars) / sizeof(calendars[0]); i++) {
        for (year = -1; year <= 1; year++) {
            QJSIntlDateCalendarFields value = calendar_date(calendars[i], year, 1);
            assert(value.numeric.year == year && value.arithmetic_year == year);
            assert(value.has_era && era_domain_contains(calendars[i], value.era_code));
        }
    }
}

static void bounds_and_errors(void)
{
    QJSIntlDateCalendarFields value, zero = {0};
    int64_t day;
    int years[] = {QJS_CAL_MIN_YEAR, QJS_CAL_MAX_YEAR};
    unsigned int i;
    for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
        assert(!qjs_calendar_to_epoch_day(QJS_CAL_ISO8601, years[i], 1, 1, &day));
        value = fields(QJS_CAL_ISO8601, day);
        assert(value.numeric.year == years[i] && value.numeric.month == 1 && value.numeric.day == 1);
        value = fields(QJS_CAL_GREGORY, day);
        assert(value.numeric.year == years[i] && value.has_era);
    }
    memset(&value, 0xff, sizeof(value));
    assert(qjs_intl_date_calendar_fields((QJSCalendarId)-1, 0, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&value, &zero, sizeof(value)));
    assert(qjs_intl_date_calendar_fields(QJS_CAL_ISO8601, INT64_MIN, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&value, &zero, sizeof(value)));
}

int main(void)
{
    QJSIntlDateCalendarFields value, copy, zero = {0};
    unsigned int i;
    static const struct { int64_t day; int year; const char *era; } japanese[] = {
        {-35429, 1872, "ce"}, {-35428, 6, "meiji"}, {-20975, 45, "meiji"},
        {-20974, 1, "taisho"}, {-15714, 15, "taisho"}, {-15713, 1, "showa"},
        {6946, 64, "showa"}, {6947, 1, "heisei"}, {18016, 31, "heisei"}, {18017, 1, "reiwa"},
    };
    value = fields(QJS_CAL_ISO8601, -719528);
    assert(value.numeric.year == 0 && value.era_year == 1 && !strcmp(value.era_code, "bce"));
    assert(value.era_calendar == QJS_CAL_GREGORY && value.weekday == 6);
    value = fields(QJS_CAL_GREGORY, -719893);
    assert(value.numeric.year == -1 && value.era_year == 2 && !strcmp(value.era_code, "bce"));
    assert(fields(QJS_CAL_GREGORY, 0).weekday == 4);
    assert(fields(QJS_CAL_GREGORY, -1).weekday == 3);
    assert(fields(QJS_CAL_GREGORY, -4).weekday == 0);
    assert(fields(QJS_CAL_GREGORY, -5).weekday == 6);
    if (qjs_calendar_is_supported(QJS_CAL_JAPANESE)) {
        hebrew_months();
        lunar_years_and_months();
        signed_years();
        for (i = 0; i < sizeof(japanese) / sizeof(japanese[0]); i++) {
            value = fields(QJS_CAL_JAPANESE, japanese[i].day);
            assert(value.numeric.year == japanese[i].year && !strcmp(value.era_code, japanese[i].era));
            assert(value.era_calendar == (i ? QJS_CAL_JAPANESE : QJS_CAL_GREGORY));
            assert(era_domain_contains(QJS_CAL_JAPANESE, value.era_code));
        }
        value = fields(QJS_CAL_ROC, -21550);
        assert(value.numeric.year == 0 && value.era_year == 1 && !strcmp(value.era_code, "broc"));
        value = fields(QJS_CAL_ETHIOPIC, -716732);
        assert(value.arithmetic_year == 0 && value.numeric.year == 5500 && !strcmp(value.era_code, "aa"));
        assert(era_domain_contains(QJS_CAL_ETHIOPIC, value.era_code));
        value = fields(QJS_CAL_ETHIOPIC, -2725973);
        assert(value.arithmetic_year == -5501 && value.numeric.year == -1);
        value = fields(QJS_CAL_ETHIOPIC, -2725607);
        assert(value.arithmetic_year == -5500 && value.numeric.year == 0);
        value = fields(QJS_CAL_JAPANESE, -719893);
        assert(value.numeric.year == -1 && value.era_calendar == QJS_CAL_GREGORY);
        assert(!strcmp(value.era_code, "bce"));
        value = calendar_date(QJS_CAL_HEBREW, 5783, 6);
        assert(value.numeric.month == 6 && !strcmp(value.month_code, "M06") && !strcmp(value.month_key, "7"));
        value = calendar_date(QJS_CAL_HEBREW, 5784, 6);
        assert(value.numeric.month == 6 && value.leap_month && !strcmp(value.month_key, "6"));
        value = calendar_date(QJS_CAL_HEBREW, 5784, 7);
        assert(value.numeric.month == 7 && !value.leap_month && value.in_leap_year && !strcmp(value.month_key, "7"));
        value = fields(QJS_CAL_CHINESE, 19762);
        assert(value.numeric.related_year == 2023 && value.cyclic_year == 40 && !value.has_era);
        value = fields(QJS_CAL_CHINESE, 19763);
        assert(value.numeric.related_year == 2024 && value.cyclic_year == 41 && !strcmp(value.cyclic_key, "41"));
        copy = value;
        value = fields(QJS_CAL_GREGORY, 0);
        assert(!strcmp(copy.cyclic_key, "41") && copy.numeric.related_year == 2024);
        for (i = 0; i < QJS_CAL_COUNT; i++) {
            QJSCalendarDate date;
            assert(!qjs_calendar_from_epoch_day(i, 0, &date));
            assert(era_domain_contains(i, date.era) == date.has_era);
            value = fields(i, 0);
            assert(value.weekday == 4 && value.arithmetic_year == date.year && value.numeric.day == (unsigned int)date.day);
            assert(!value.numeric.hour && !value.numeric.minute && !value.numeric.second && !value.numeric.millisecond);
        }
    } else {
        memset(&value, 0xff, sizeof(value));
        assert(qjs_intl_date_calendar_fields(QJS_CAL_JAPANESE, 0, &value) == QJS_INTL_UNSUPPORTED);
        assert(!memcmp(&value, &zero, sizeof(value)));
    }
    memset(&value, 0xff, sizeof(value));
    assert(qjs_intl_date_calendar_fields(QJS_CAL_COUNT, 0, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&value, &zero, sizeof(value)));
    assert(qjs_intl_date_calendar_fields(QJS_CAL_ISO8601, INT64_MAX, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&value, &zero, sizeof(value)));
    assert(qjs_intl_date_calendar_fields(QJS_CAL_ISO8601, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    bounds_and_errors();
    {
        const char *name = "unchanged";
        assert(qjs_calendar_era_at(QJS_CAL_COUNT, 0, &name) == QJS_CAL_RANGE);
        assert(!strcmp(name, "unchanged"));
        assert(qjs_calendar_era_at(QJS_CAL_ISO8601, 0, &name) == QJS_CAL_UNSUPPORTED);
        assert(!strcmp(name, "unchanged"));
        assert(qjs_calendar_era_at(QJS_CAL_ISO8601, 0, NULL) == QJS_CAL_RANGE);
    }
    return 0;
}
