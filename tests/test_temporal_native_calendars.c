/* Shared-provider adapter contracts, authored for root execution only.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, project LICENSE. */
#include "../src/temporal/calendar.h"
#include "../src/temporal/civil.h"
#include "../src/calendar/calendar.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static QJSTemporalCalendarFields fields(int year, const char *code, int day)
{
    QJSTemporalCalendarFields value = {0};
    value.present = QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_MONTH_CODE |
                    QJS_TEMPORAL_FIELD_DAY;
    value.year = year; value.day = day;
    strcpy(value.month_code, code);
    return value;
}
static void equal_date(QJSTemporalISODate value, int year, int month, int day)
{
    assert(value.year == year && value.month == month && value.day == day);
}
static void golden_dates(void)
{
    static const struct {
        QJSTemporalCalendar id;
        int year, month, day;
        const char *code, *era;
        int era_year;
    } values[] = {
        {QJS_TEMPORAL_CAL_BUDDHIST,2513,1,1,"M01","be",2513},
        {QJS_TEMPORAL_CAL_COPTIC,1686,4,23,"M04","am",1686},
        {QJS_TEMPORAL_CAL_ETHIOPIC,1962,4,23,"M04","am",1962},
        {QJS_TEMPORAL_CAL_ETHIOAA,7462,4,23,"M04","aa",7462},
        {QJS_TEMPORAL_CAL_GREGORY,1970,1,1,"M01","ce",1970},
        {QJS_TEMPORAL_CAL_HEBREW,5730,4,23,"M04","am",5730},
        {QJS_TEMPORAL_CAL_INDIAN,1891,10,11,"M10","shaka",1891},
        {QJS_TEMPORAL_CAL_ISLAMIC_CIVIL,1389,10,22,"M10","ah",1389},
        {QJS_TEMPORAL_CAL_ISLAMIC_TBLA,1389,10,23,"M10","ah",1389},
        {QJS_TEMPORAL_CAL_JAPANESE,1970,1,1,"M01","showa",45},
        {QJS_TEMPORAL_CAL_PERSIAN,1348,10,11,"M10","ap",1348},
        {QJS_TEMPORAL_CAL_ROC,59,1,1,"M01","roc",59},
    };
    size_t i;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        QJSTemporalCalendarDate record;
        QJSTemporalISODate result;
        QJSTemporalCalendarFields input = fields(values[i].year, values[i].code, values[i].day);
        assert(!qjs_temporal_calendar_fields(values[i].id,
                    (QJSTemporalISODate){1970,1,1}, &record));
        assert(record.year == values[i].year && record.month == values[i].month &&
               record.day == values[i].day && record.day_of_week == 4);
        assert(!strcmp(record.era,values[i].era) && record.era_year == values[i].era_year);
        assert(!record.has_week && record.days_in_week == 7);
        assert(!qjs_temporal_calendar_date_from_fields(values[i].id, &input,
                     QJS_TEMPORAL_OVERFLOW_REJECT, &result));
        equal_date(result,1970,1,1);
    }
}
static void aliases_and_eras(void)
{
    QJSTemporalCalendar id;
    QJSTemporalISODate result;
    QJSTemporalCalendarDate record;
    QJSTemporalCalendarFields input = fields(2019,"M05",1);
    int32_t year = 123;
    assert(!qjs_temporal_calendar_from_identifier(&id,"ISLAMICC",8));
    assert(id == QJS_TEMPORAL_CAL_ISLAMIC_CIVIL);
    assert(!qjs_temporal_calendar_from_identifier(&id,"ETHIOPIC-AMETE-ALEM",19));
    assert(id == QJS_TEMPORAL_CAL_ETHIOAA);
    assert(qjs_temporal_calendar_from_identifier(&id,"islamic",7) == QJS_TEMPORAL_ERROR_RANGE);
    strcpy(input.era,"heisei"); input.era_year = 31;
    input.present |= QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR;
    assert(!qjs_temporal_calendar_date_from_fields(QJS_TEMPORAL_CAL_JAPANESE,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&result));
    equal_date(result,2019,5,1);
    assert(!qjs_temporal_calendar_fields(QJS_TEMPORAL_CAL_JAPANESE,result,&record));
    assert(!strcmp(record.era,"reiwa") && record.era_year == 1);
    input.year = 2018;
    assert(qjs_temporal_calendar_date_from_fields(QJS_TEMPORAL_CAL_JAPANESE,&input,
             QJS_TEMPORAL_OVERFLOW_REJECT,&result) == QJS_TEMPORAL_ERROR_RANGE);
    input.present &= ~QJS_TEMPORAL_FIELD_ERA;
    assert(qjs_temporal_calendar_date_from_fields(QJS_TEMPORAL_CAL_JAPANESE,&input,
             QJS_TEMPORAL_OVERFLOW_REJECT,&result) == QJS_TEMPORAL_ERROR_MISSING);
    assert(!qjs_calendar_year_from_era(QJS_CAL_ETHIOPIC,"aa",5500,&year) && year == 0);
    assert(!qjs_calendar_year_from_era(QJS_CAL_GREGORY,"bc",1,&year) && year == 0);
    assert(!qjs_calendar_year_from_era(QJS_CAL_JAPANESE,"reiwa",0,&year) && year == 2018);
    year = 123;
    assert(qjs_calendar_year_from_era(QJS_CAL_GREGORY,"bad",1,&year) == QJS_CAL_RANGE);
    assert(year == 123);
}
static void regulation_and_arithmetic(void)
{
    QJSTemporalISODate leap, result, before;
    QJSTemporalCalendarDate record;
    QJSTemporalDateDuration duration = {1,0,0,0}, difference;
    QJSTemporalCalendarFields input = fields(1403,"M12",30);
    int id;
    assert(!qjs_temporal_calendar_date_from_fields(QJS_TEMPORAL_CAL_PERSIAN,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&leap));
    equal_date(leap,2025,3,20);
    assert(!qjs_temporal_calendar_date_add(QJS_TEMPORAL_CAL_PERSIAN,leap,duration,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result));
    assert(!qjs_temporal_calendar_fields(QJS_TEMPORAL_CAL_PERSIAN,result,&record));
    assert(record.year == 1404 && record.month == 12 && record.day == 29);
    before = result;
    assert(qjs_temporal_calendar_date_add(QJS_TEMPORAL_CAL_PERSIAN,leap,duration,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&result) == QJS_TEMPORAL_ERROR_RANGE);
    assert(!memcmp(&result,&before,sizeof(result)));
    input = fields(5784,"M05L",30);
    assert(!qjs_temporal_calendar_date_from_fields(QJS_TEMPORAL_CAL_HEBREW,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&leap));
    assert(!qjs_temporal_calendar_date_add(QJS_TEMPORAL_CAL_HEBREW,leap,duration,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result));
    assert(!qjs_temporal_calendar_fields(QJS_TEMPORAL_CAL_HEBREW,result,&record));
    assert(record.year == 5785 && !strcmp(record.month_code,"M06") && record.day == 29);
    input = fields(5783,"M05L",1); input.month = 7;
    input.present |= QJS_TEMPORAL_FIELD_MONTH;
    assert(qjs_temporal_calendar_date_from_fields(QJS_TEMPORAL_CAL_HEBREW,&input,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result) == QJS_TEMPORAL_ERROR_RANGE);
    input.month = 6;
    assert(!qjs_temporal_calendar_date_from_fields(QJS_TEMPORAL_CAL_HEBREW,&input,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result));
    assert(qjs_temporal_calendar_date_from_fields(QJS_TEMPORAL_CAL_HEBREW,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&result) == QJS_TEMPORAL_ERROR_RANGE);
    for (id = QJS_TEMPORAL_CAL_CHINESE; id <= QJS_TEMPORAL_CAL_DANGI;
         id += QJS_TEMPORAL_CAL_DANGI - QJS_TEMPORAL_CAL_CHINESE) {
        input = fields(2020,"M04",30);
        assert(!qjs_temporal_calendar_date_from_fields((QJSTemporalCalendar)id,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&leap));
        duration = (QJSTemporalDateDuration){0,1,0,0};
        assert(!qjs_temporal_calendar_date_add((QJSTemporalCalendar)id,leap,duration,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result));
        assert(!qjs_temporal_calendar_fields((QJSTemporalCalendar)id,result,&record));
        assert(!strcmp(record.month_code,"M04L") && record.months_in_year == 13 && record.day == 29);
        assert(qjs_temporal_calendar_date_add((QJSTemporalCalendar)id,leap,duration,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&result) == QJS_TEMPORAL_ERROR_RANGE);
        assert(!qjs_temporal_calendar_date_until((QJSTemporalCalendar)id,leap,result,
                    QJS_TEMPORAL_MONTH,&difference));
        assert(difference.months == 0 && difference.days == 29);
    }
    duration = (QJSTemporalDateDuration){INT64_MAX,0,0,0};
    assert(qjs_temporal_calendar_date_add(QJS_TEMPORAL_CAL_GREGORY,
                    (QJSTemporalISODate){2024,1,1},duration,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result) == QJS_TEMPORAL_ERROR_RANGE);
    duration = (QJSTemporalDateDuration){0,INT64_MIN,0,0};
    assert(qjs_temporal_calendar_date_add(QJS_TEMPORAL_CAL_HEBREW,leap,duration,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result) == QJS_TEMPORAL_ERROR_RANGE);
}
static void reference_dates(void)
{
    QJSTemporalISODate result;
    QJSTemporalCalendarFields input = fields(-271821,"M01",1);
    QJSCalendarDate date;
    int64_t day = 123;
    int supported = 123, id;
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_TEMPORAL_CAL_GREGORY,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&result));
    equal_date(result,1972,1,1);
    input = fields(-271821,"M04",99);
    assert(!qjs_temporal_calendar_year_month_from_fields(QJS_TEMPORAL_CAL_GREGORY,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&result));
    equal_date(result,-271821,4,1);
    assert(qjs_calendar_to_epoch_day(QJS_CAL_GREGORY,-271821,4,1,&day) == QJS_CAL_RANGE);
    assert(day == 123);
    assert(!qjs_calendar_to_epoch_day_unbounded(QJS_CAL_GREGORY,-271821,4,1,&day));
    assert(!qjs_calendar_from_epoch_day_unbounded(QJS_CAL_GREGORY,day,&date));
    assert(date.year == -271821 && date.month == 4 && date.day == 1);
    assert(qjs_calendar_from_epoch_day(QJS_CAL_GREGORY,day,&date) == QJS_CAL_RANGE);
    for (id = QJS_TEMPORAL_CAL_BUDDHIST; id < QJS_TEMPORAL_CAL_COUNT; id++) {
        input = fields(999999,"M01",1);
        assert(qjs_temporal_calendar_month_day_from_fields((QJSTemporalCalendar)id,&input,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result) == QJS_TEMPORAL_ERROR_RANGE);
    }
    assert(!qjs_calendar_year_has_supported_date(QJS_CAL_CHINESE,2020,&supported) && supported);
    assert(!qjs_calendar_year_has_supported_date(QJS_CAL_CHINESE,999999,&supported) && !supported);
    for (id = QJS_TEMPORAL_CAL_CHINESE; id <= QJS_TEMPORAL_CAL_DANGI;
         id += QJS_TEMPORAL_CAL_DANGI - QJS_TEMPORAL_CAL_CHINESE) {
        input = fields(0,"M11L",20); input.present &= ~QJS_TEMPORAL_FIELD_YEAR;
        assert(!qjs_temporal_calendar_month_day_from_fields((QJSTemporalCalendar)id,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&result));
        assert(result.year == 2034);
        input = fields(0,"M01L",1); input.present &= ~QJS_TEMPORAL_FIELD_YEAR;
        assert(qjs_temporal_calendar_month_day_from_fields((QJSTemporalCalendar)id,&input,
                    QJS_TEMPORAL_OVERFLOW_REJECT,&result) == QJS_TEMPORAL_ERROR_RANGE);
        assert(!qjs_temporal_calendar_month_day_from_fields((QJSTemporalCalendar)id,&input,
                    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&result));
        assert(result.year == 1972);
    }
}
static void extreme_lunar_construction(void)
{
    /* Official PlainDate/from/extreme-dates.js requires successful creation
     * at these years without asserting astronomical accuracy. These probes
     * are mandatory; a BACKEND failure identifies a shared-provider gap. */
    static const int years[] = {-250000,250000};
    int id;
    size_t i;
    for (id = QJS_TEMPORAL_CAL_CHINESE; id <= QJS_TEMPORAL_CAL_DANGI;
         id += QJS_TEMPORAL_CAL_DANGI - QJS_TEMPORAL_CAL_CHINESE) {
        for (i = 0; i < sizeof(years)/sizeof(years[0]); i++) {
            QJSTemporalCalendarFields input = fields(years[i],"M01",1);
            QJSTemporalISODate result;
            assert(!qjs_temporal_calendar_date_from_fields((QJSTemporalCalendar)id,
                        &input,QJS_TEMPORAL_OVERFLOW_REJECT,&result));
            assert(qjs_temporal_iso_date_within_limits(result));
        }
    }
}
static void equal_iso(QJSTemporalISODate a, QJSTemporalISODate b)
{
    assert(a.year == b.year && a.month == b.month && a.day == b.day);
}
static void lunar_transition_arithmetic(void)
{
    static const int years[] = {
        -250001,-250000,-249999,-10002,-10001,-10000,-9999,-9998,
        9998,9999,10000,10001,10002,249999,250000,250001
    };
    static const int64_t edges[] = {QJS_CAL_MIN_EPOCH_DAY,QJS_CAL_MAX_EPOCH_DAY};
    int id;
    size_t y, e;
    for (id = QJS_TEMPORAL_CAL_CHINESE; id <= QJS_TEMPORAL_CAL_DANGI;
         id += QJS_TEMPORAL_CAL_DANGI - QJS_TEMPORAL_CAL_CHINESE) {
        QJSTemporalCalendar cal = (QJSTemporalCalendar)id;
        for (y = 0; y < sizeof(years)/sizeof(years[0]); y++) {
            QJSTemporalCalendarFields input = fields(years[y],"M01",1);
            QJSTemporalISODate start, end, rebuilt;
            QJSTemporalCalendarDate record;
            QJSTemporalDateDuration duration, difference;
            assert(!qjs_temporal_calendar_date_from_fields(cal,&input,
                        QJS_TEMPORAL_OVERFLOW_REJECT,&start));
            for (int sign = -1; sign <= 1; sign += 2) {
                duration = (QJSTemporalDateDuration){0,sign,0,0};
                assert(!qjs_temporal_calendar_date_add(cal,start,duration,
                            QJS_TEMPORAL_OVERFLOW_REJECT,&end));
                assert(!qjs_temporal_calendar_date_until(cal,start,end,
                            QJS_TEMPORAL_MONTH,&difference));
                assert(difference.years == 0 && difference.months == sign &&
                       difference.weeks == 0 && difference.days == 0);
                assert(!qjs_temporal_calendar_date_add(cal,start,difference,
                            QJS_TEMPORAL_OVERFLOW_REJECT,&rebuilt));
                equal_iso(end,rebuilt);
                duration = (QJSTemporalDateDuration){0,-sign,0,0};
                assert(!qjs_temporal_calendar_date_add(cal,end,duration,
                            QJS_TEMPORAL_OVERFLOW_REJECT,&rebuilt));
                equal_iso(start,rebuilt);
                duration = (QJSTemporalDateDuration){sign,0,0,0};
                assert(!qjs_temporal_calendar_date_add(cal,start,duration,
                            QJS_TEMPORAL_OVERFLOW_REJECT,&end));
                assert(!qjs_temporal_calendar_fields(cal,end,&record));
                assert(record.year == years[y] + sign && record.day == 1 &&
                       !strcmp(record.month_code,"M01"));
                assert(!qjs_temporal_calendar_date_until(cal,start,end,
                            QJS_TEMPORAL_YEAR,&difference));
                assert(difference.years == sign && difference.months == 0 &&
                       difference.weeks == 0 && difference.days == 0);
                assert(!qjs_temporal_calendar_date_add(cal,start,difference,
                            QJS_TEMPORAL_OVERFLOW_REJECT,&rebuilt));
                equal_iso(end,rebuilt);
            }
        }
        for (e = 0; e < sizeof(edges)/sizeof(edges[0]); e++) {
            QJSCalendarDate gregorian;
            QJSTemporalISODate start, end, rebuilt;
            QJSTemporalDateDuration duration = {0,e ? -1 : 1,0,0}, difference;
            assert(!qjs_calendar_from_epoch_day(QJS_CAL_GREGORY,edges[e],&gregorian));
            start = (QJSTemporalISODate){gregorian.year,gregorian.month,gregorian.day};
            assert(!qjs_temporal_calendar_date_add(cal,start,duration,
                        QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&end));
            assert(!qjs_temporal_calendar_date_until(cal,start,end,
                        QJS_TEMPORAL_MONTH,&difference));
            assert(!qjs_temporal_calendar_date_add(cal,start,difference,
                        QJS_TEMPORAL_OVERFLOW_CONSTRAIN,&rebuilt));
            equal_iso(end,rebuilt);
        }
    }
}

int main(void)
{
#ifndef CONFIG_ICU
    golden_dates(); aliases_and_eras(); regulation_and_arithmetic(); reference_dates();
    extreme_lunar_construction(); lunar_transition_arithmetic();
#endif
    return 0;
}
