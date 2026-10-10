/* Authored fixtures for root execution; not run during source preparation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, project LICENSE.
 */
#include "../src/calendar/calendar.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static int64_t iso(int year, int month, int day)
{
    int64_t epoch;
    assert(qjs_calendar_to_epoch_day(QJS_CAL_ISO8601, year, month, day, &epoch) == 0);
    return epoch;
}
static void expect(QJSCalendarId id, int64_t epoch, int year, int month, int day,
                   const char *code, const char *era, int era_year)
{
    QJSCalendarDate date;
    int64_t inverse;
    assert(qjs_calendar_from_epoch_day(id, epoch, &date) == 0);
    assert(date.year == year && date.month == month && date.day == day);
    assert(strcmp(date.month_code, code) == 0);
    assert(date.has_era == (era != NULL));
    if (era) assert(strcmp(date.era, era) == 0 && date.era_year == era_year);
    assert(qjs_calendar_to_epoch_day(id, year, month, day, &inverse) == 0);
    assert(inverse == epoch);
}
static void absolute_dates(void)
{
    expect(QJS_CAL_ISO8601, 0, 1970,1,1,"M01",NULL,0);
    expect(QJS_CAL_GREGORY, 0, 1970,1,1,"M01","ce",1970);
    expect(QJS_CAL_BUDDHIST, 0, 2513,1,1,"M01","be",2513);
    expect(QJS_CAL_ROC, 0, 59,1,1,"M01","roc",59);
    expect(QJS_CAL_JAPANESE, 0, 1970,1,1,"M01","showa",45);
    expect(QJS_CAL_COPTIC, 0, 1686,4,23,"M04","am",1686);
    expect(QJS_CAL_ETHIOPIC, 0, 1962,4,23,"M04","am",1962);
    expect(QJS_CAL_ETHIOAA, 0, 7462,4,23,"M04","aa",7462);
    expect(QJS_CAL_INDIAN, 0, 1891,10,11,"M10","shaka",1891);
    expect(QJS_CAL_HEBREW, 0, 5730,4,23,"M04","am",5730);
    expect(QJS_CAL_ISLAMIC_CIVIL, 0, 1389,10,22,"M10","ah",1389);
    expect(QJS_CAL_ISLAMIC_TBLA, 0, 1389,10,23,"M10","ah",1389);
    /* Persian remains a direct conversion candidate until primary proof. */
    expect(QJS_CAL_PERSIAN, 0, 1348,10,11,"M10","ap",1348);
    expect(QJS_CAL_ISLAMIC_CIVIL, iso(622,7,19), 1,1,1,"M01","ah",1);
    expect(QJS_CAL_ISLAMIC_TBLA, iso(622,7,18), 1,1,1,"M01","ah",1);
    expect(QJS_CAL_ISLAMIC_UMALQURA, iso(1882,11,12), 1300,1,1,"M01","ah",1300);
    expect(QJS_CAL_GREGORY, iso(0,1,1), 0,1,1,"M01","bce",1);
    expect(QJS_CAL_ROC, iso(1911,1,1), 0,1,1,"M01","broc",1);
    expect(QJS_CAL_JAPANESE, iso(1872,12,31), 1872,12,31,"M12","ce",1872);
    expect(QJS_CAL_JAPANESE, iso(1873,1,1), 1873,1,1,"M01","meiji",6);
    expect(QJS_CAL_JAPANESE, iso(1989,1,7), 1989,1,7,"M01","showa",64);
    expect(QJS_CAL_JAPANESE, iso(1989,1,8), 1989,1,8,"M01","heisei",1);
    expect(QJS_CAL_JAPANESE, iso(2019,4,30), 2019,4,30,"M04","heisei",31);
    expect(QJS_CAL_JAPANESE, iso(2019,5,1), 2019,5,1,"M05","reiwa",1);
}
static void hebrew_ordinals(void)
{
    int months, days, ordinal, new_month;
    int32_t new_year;
    char code[5];
    assert(qjs_calendar_month_info(QJS_CAL_HEBREW,5784,6,&months,&days,code) == 0);
    assert(months == 13 && days == 30 && strcmp(code,"M05L") == 0);
    assert(qjs_calendar_month_info(QJS_CAL_HEBREW,5784,7,&months,&days,code) == 0);
    assert(days == 29 && strcmp(code,"M06") == 0);
    ordinal = 99;
    assert(qjs_calendar_month_ordinal(QJS_CAL_HEBREW,5783,"M05L",0,&ordinal) == QJS_CAL_RANGE);
    assert(ordinal == 99);
    assert(qjs_calendar_month_ordinal(QJS_CAL_HEBREW,5783,"M05L",1,&ordinal) == 0 && ordinal == 6);
    assert(qjs_calendar_add_months(QJS_CAL_HEBREW,5784,5,1,&new_year,&new_month) == 0);
    assert(new_year == 5784 && new_month == 6);
    assert(qjs_calendar_add_months(QJS_CAL_HEBREW,5784,5,2,&new_year,&new_month) == 0);
    assert(new_year == 5784 && new_month == 7);
    assert(qjs_calendar_add_months(QJS_CAL_HEBREW,5784,6,235,&new_year,&new_month) == 0);
    assert(new_year == 5803 && new_month == 6);
    assert(qjs_calendar_add_months(QJS_CAL_HEBREW,5803,6,-235,&new_year,&new_month) == 0);
    assert(new_year == 5784 && new_month == 6);
}
static void full_domain(void)
{
    static const int64_t samples[] = {
        QJS_CAL_MIN_EPOCH_DAY, -100000000, -719528, -1, 0, 19723,
        99999999, QJS_CAL_MAX_EPOCH_DAY
    };
    QJSCalendarDate date;
    int id;
    size_t sample;
    for (id = 0; id < QJS_CAL_COUNT; id++) {
        if (id == QJS_CAL_CHINESE || id == QJS_CAL_DANGI) continue;
        for (sample = 0; sample < sizeof(samples)/sizeof(samples[0]); sample++) {
            int64_t inverse;
            assert(qjs_calendar_from_epoch_day((QJSCalendarId)id,samples[sample],&date) == 0);
            assert(date.month >= 1 && date.month <= date.months_in_year);
            assert(date.day >= 1 && date.day <= date.days_in_month);
            assert(qjs_calendar_to_epoch_day((QJSCalendarId)id,date.year,date.month,date.day,&inverse) == 0);
            assert(inverse == samples[sample]);
        }
    }
}
static void umalqura_boundaries(void)
{
    int year, month, months, days;
    char code[5];
    for (year = 1300; year <= 1600; year++) {
        for (month = 1; month <= 12; month++) {
            int64_t first, last;
            QJSCalendarDate date;
            assert(qjs_calendar_month_info(QJS_CAL_ISLAMIC_UMALQURA,year,month,&months,&days,code) == 0);
            assert(qjs_calendar_to_epoch_day(QJS_CAL_ISLAMIC_UMALQURA,year,month,1,&first) == 0);
            assert(qjs_calendar_to_epoch_day(QJS_CAL_ISLAMIC_UMALQURA,year,month,days,&last) == 0);
            assert(last - first == days - 1);
            assert(qjs_calendar_from_epoch_day(QJS_CAL_ISLAMIC_UMALQURA,last,&date) == 0);
            assert(date.year == year && date.month == month && date.day == days);
        }
    }
    for (year = 1299; year <= 1601; year += 302) {
        for (month = 1; month <= 12; month++) {
            int64_t civil, umalqura;
            assert(qjs_calendar_to_epoch_day(QJS_CAL_ISLAMIC_CIVIL,year,month,1,&civil) == 0);
            assert(qjs_calendar_to_epoch_day(QJS_CAL_ISLAMIC_UMALQURA,year,month,1,&umalqura) == 0);
            assert(civil == umalqura);
        }
    }
}
static void errors_and_support(void)
{
    QJSCalendarDate before, after;
    QJSCalendarId id = QJS_CAL_ROC;
    int64_t epoch = 123;
    int32_t year = 123;
    int month = 123;
    memset(&before,0x5a,sizeof(before)); after = before;
    assert(qjs_calendar_from_epoch_day(QJS_CAL_GREGORY,QJS_CAL_MAX_EPOCH_DAY+1,&after) == QJS_CAL_RANGE);
    assert(memcmp(&before,&after,sizeof(before)) == 0);
    assert(qjs_calendar_to_epoch_day(QJS_CAL_GREGORY,2023,2,29,&epoch) == QJS_CAL_RANGE && epoch == 123);
    assert(qjs_calendar_add_months(QJS_CAL_HEBREW,5784,6,INT64_MAX,&year,&month) == QJS_CAL_RANGE);
    assert(year == 123 && month == 123);
    assert(qjs_calendar_add_months(QJS_CAL_GREGORY,2024,1,INT64_MIN,&year,&month) == QJS_CAL_RANGE);
    assert(year == 123 && month == 123);
#ifndef QJS_CAL_CHINESE_AUTHORITY_VERIFIED
    assert(!qjs_calendar_is_supported(QJS_CAL_CHINESE));
#else
    assert(qjs_calendar_is_supported(QJS_CAL_CHINESE));
#endif
#ifndef QJS_CAL_DANGI_AUTHORITY_VERIFIED
    assert(!qjs_calendar_is_supported(QJS_CAL_DANGI));
#else
    assert(qjs_calendar_is_supported(QJS_CAL_DANGI));
#endif
#ifndef QJS_CAL_PERSIAN_AUTHORITY_VERIFIED
    assert(!qjs_calendar_is_supported(QJS_CAL_PERSIAN));
#endif
    assert(qjs_calendar_from_identifier(&id,"islamicc",8) == 0 && id == QJS_CAL_ISLAMIC_CIVIL);
    assert(qjs_calendar_from_identifier(&id,"ethiopic-amete-alem",sizeof("ethiopic-amete-alem")-1) == 0 && id == QJS_CAL_ETHIOAA);
#ifndef QJS_CAL_CHINESE_AUTHORITY_VERIFIED
    assert(qjs_calendar_from_identifier(&id,"chinese",7) == QJS_CAL_UNSUPPORTED && id == QJS_CAL_ETHIOAA);
#else
    assert(qjs_calendar_from_identifier(&id,"chinese",7) == QJS_CAL_OK && id == QJS_CAL_CHINESE);
#endif
}
int main(void)
{
    absolute_dates(); hebrew_ordinals(); full_domain();
    umalqura_boundaries(); errors_and_support();
    return 0;
}
