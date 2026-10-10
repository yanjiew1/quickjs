/* Acquired primary records are the field oracle; no astronomical comparison.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, project LICENSE. */
#include "../src/calendar/calendar.h"
#include "../src/calendar/internal.h"
#include "../src/calendar/published-lunisolar.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); ++failures; } } while (0)

static void audit_days(QJSCalendarId id, int64_t start, int64_t end)
{
    const QJSLunisolarProvider *p = qjs_calendar_lunisolar_provider(id);
    const QJSPublishedCalendarTable *t = p->arithmetic;
    size_t index = 0, year_start, next_year, j;
    int64_t day, inverse = 0;
    unsigned long audited = 0;
    for (day = start; day < end; day++) {
        const QJSPublishedCalendarMonth *m;
        QJSCalendarDate ordinary = {0}, intl = {0};
        char code[5];
        int count = 0, length = 0, ordinal;
        while (index + 1 < t->count && t->months[index + 1].epoch_day <= day) ++index;
        m = &t->months[index];
        year_start = index;
        while (year_start && t->months[year_start - 1].lunar_year == m->lunar_year) --year_start;
        next_year = index + 1;
        while (next_year < t->count && t->months[next_year].lunar_year == m->lunar_year) ++next_year;
        CHECK(next_year < t->count);
        if (next_year >= t->count) break;
        ordinal = (int)(index - year_start + 1);
        snprintf(code, sizeof(code), "M%02u", (unsigned)m->month);
        if (m->is_leap) { code[3] = 'L'; code[4] = 0; }
        CHECK(qjs_calendar_from_epoch_day(id, day, &ordinary) == QJS_CAL_OK);
        CHECK(qjs_calendar_from_epoch_day_for_intl(id, day, &intl) == QJS_CAL_OK);
        CHECK(memcmp(&ordinary, &intl, sizeof(ordinary)) == 0);
        CHECK(ordinary.year == m->lunar_year && ordinary.month == ordinal &&
              ordinary.day == day - m->epoch_day + 1 && !strcmp(ordinary.month_code, code));
        CHECK(ordinary.day_of_year == day - t->months[year_start].epoch_day + 1 &&
              ordinary.days_in_month == m->length &&
              ordinary.months_in_year == (int)(next_year - year_start) &&
              ordinary.days_in_year == t->months[next_year].epoch_day - t->months[year_start].epoch_day &&
              ordinary.in_leap_year == (next_year - year_start == 13) &&
              !ordinary.has_era && !ordinary.era[0] && !ordinary.era_year);
        CHECK(qjs_calendar_to_epoch_day(id, ordinary.year, ordinary.month, ordinary.day, &inverse) == QJS_CAL_OK && inverse == day);
        CHECK(qjs_calendar_month_info(id, ordinary.year, ordinary.month, &count, &length, code) == QJS_CAL_OK &&
              count == ordinary.months_in_year && length == ordinary.days_in_month && !strcmp(code, ordinary.month_code));
        CHECK(qjs_calendar_month_ordinal(id, ordinary.year, ordinary.month_code, 0, &ordinal) == QJS_CAL_OK && ordinal == ordinary.month);
        /* Every traversed month start must join without reusing a nominal index. */
        if (ordinary.day == 1 && index + 1 < next_year) {
            int32_t result_year = 0; int result_month = 0;
            CHECK(qjs_calendar_add_months(id, ordinary.year, ordinary.month, 1, &result_year, &result_month) == QJS_CAL_OK);
            CHECK(result_year == ordinary.year && result_month == ordinary.month + 1);
        }
        for (j = 0; j < p->primary_span_count; j++)
            if (p->primary_spans[j].first_day <= day && day < p->primary_spans[j].end_day) break;
        CHECK(j < p->primary_span_count);
        ++audited;
    }
    CHECK(audited == (unsigned long)(end - start));
    printf("audited %s %lu acquired mandatory days and inverses\n", qjs_calendar_identifier(id), audited);
}

static void boundary_months(void)
{
    int32_t year = 0;
    int month = 0;
    int64_t day = 0;
    QJSCalendarDate value;
    CHECK(qjs_calendar_add_months(QJS_CAL_CHINESE, 1899, 12, 1, &year, &month) == QJS_CAL_OK);
    CHECK(year == 1900 && month == 1);
    CHECK(qjs_calendar_add_months(QJS_CAL_CHINESE, 1900, 1, -1, &year, &month) == QJS_CAL_OK);
    CHECK(year == 1899 && month == 12);
    CHECK(qjs_calendar_to_epoch_day(QJS_CAL_CHINESE, 1900, 1, 1, &day) == QJS_CAL_OK && day == -25537);
    CHECK(qjs_calendar_add_months(QJS_CAL_CHINESE, 2100, 12, 1, &year, &month) == QJS_CAL_OK);
    CHECK(year == 2101 && month == 1);
    CHECK(qjs_calendar_to_epoch_day(QJS_CAL_CHINESE, 2101, 1, 1, &day) == QJS_CAL_OK && day == 47875);
    CHECK(qjs_calendar_add_months(QJS_CAL_CHINESE, 2101, 1, -1, &year, &month) == QJS_CAL_OK);
    CHECK(year == 2100 && month == 12);
    CHECK(qjs_calendar_from_epoch_day(QJS_CAL_CHINESE, 47874, &value) == QJS_CAL_OK);
    CHECK(value.year == 2100 && value.month == 12 && value.day == 29);
}

int main(void)
{
    QJSCalendarId parsed = QJS_CAL_ISO8601;
    const QJSLunisolarProvider *chinese = qjs_calendar_lunisolar_provider(QJS_CAL_CHINESE);
    const QJSPublishedCalendarTable *t = chinese->arithmetic;
#ifdef QJS_CAL_CHINESE_AUTHORITY_VERIFIED
    CHECK(qjs_calendar_is_supported(QJS_CAL_CHINESE));
    CHECK(qjs_calendar_from_identifier(&parsed, "chinese", 7) == QJS_CAL_OK && parsed == QJS_CAL_CHINESE);
#else
    CHECK(!qjs_calendar_is_supported(QJS_CAL_CHINESE));
    CHECK(qjs_calendar_from_identifier(&parsed, "chinese", 7) == QJS_CAL_UNSUPPORTED && parsed == QJS_CAL_ISO8601);
#endif
#ifdef QJS_CAL_DANGI_AUTHORITY_VERIFIED
    CHECK(qjs_calendar_is_supported(QJS_CAL_DANGI));
    CHECK(qjs_calendar_from_identifier(&parsed, "dangi", 5) == QJS_CAL_OK && parsed == QJS_CAL_DANGI);
#else
    CHECK(!qjs_calendar_is_supported(QJS_CAL_DANGI));
#endif
#ifdef QJS_CAL_PERSIAN_AUTHORITY_VERIFIED
    CHECK(qjs_calendar_is_supported(QJS_CAL_PERSIAN));
    CHECK(qjs_calendar_from_identifier(&parsed, "persian", 7) == QJS_CAL_OK && parsed == QJS_CAL_PERSIAN);
#else
    CHECK(!qjs_calendar_is_supported(QJS_CAL_PERSIAN));
#endif
    audit_days(QJS_CAL_DANGI, -25567, 29585);
    CHECK(t->count == 2499 && t->months[0].lunar_year == 1899);
    CHECK(t->months[t->count - 1].epoch_day == 47875 &&
          t->months[t->count - 1].lunar_year == 2101 &&
          t->months[t->count - 1].month == 1 &&
          !t->months[t->count - 1].is_leap && !t->months[t->count - 1].length);
    audit_days(QJS_CAL_CHINESE, -25567, 47847);
    boundary_months();
    return failures ? 1 : 0;
}
