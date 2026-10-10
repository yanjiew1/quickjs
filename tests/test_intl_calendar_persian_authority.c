/* Original authority and boundary fixtures for root execution.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, project LICENSE.
 */
#include "../src/calendar/calendar.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "persian-years.inc"

static int64_t iso(int year, int month, int day)
{
    int64_t value;
    assert(qjs_calendar_to_epoch_day(QJS_CAL_ISO8601,year,month,day,&value) == QJS_CAL_OK);
    return value;
}
static void hard_date(int year, int gy, int gd, int leap)
{
    int64_t start, end;
    QJSCalendarDate date;
    assert(qjs_calendar_to_epoch_day(QJS_CAL_PERSIAN,year,1,1,&start) == 0);
    assert(start == iso(gy,3,gd));
    assert(qjs_calendar_from_epoch_day(QJS_CAL_PERSIAN,start,&date) == 0);
    assert(date.year == year && date.month == 1 && date.day == 1);
    assert(date.days_in_year == 365 + leap && date.in_leap_year == leap);
    assert(date.has_era && !strcmp(date.era,"ap") && date.era_year == year);
    assert(qjs_calendar_to_epoch_day(QJS_CAL_PERSIAN,year,12,29+leap,&end) == 0);
    assert(end - start == 364 + leap);
}
int main(void)
{
    int year, index;
    QJSCalendarDate date;
    int64_t epoch, inverse;
    hard_date(1206,1827,22,0);
    hard_date(1210,1831,21,1);
    hard_date(1280,1901,21,1);
    hard_date(1309,1930,21,1);
    hard_date(1403,2024,20,1);
    hard_date(1404,2025,21,0);
    hard_date(1469,2090,20,1);
    hard_date(1470,2091,21,0);
    hard_date(1498,2119,21,1);
    /* Test every captured year's first/final day, all months, and every day.
     * Hard-coded dates above and separate Test262 host parity guard the data. */
    for (year = 1206; year <= 1498; year++) {
        int month, days, months, sum = 0;
        char code[5];
        index = year - 1206;
        assert(qjs_persian_authority_starts[index+1] - qjs_persian_authority_starts[index] ==
               365 + (qjs_persian_authority_stars[index] != 0));
        for (month=1; month<=12; month++) {
            assert(qjs_calendar_month_info(QJS_CAL_PERSIAN,year,month,&months,&days,code) == 0);
            assert(months == 12 && days == (month <= 6 ? 31 : month <= 11 ? 30 :
                                          29 + (qjs_persian_authority_stars[index] != 0)));
            assert(strlen(code) == 3);
            sum += days;
        }
        assert(sum == qjs_persian_authority_starts[index+1] - qjs_persian_authority_starts[index]);
        for (epoch=qjs_persian_authority_starts[index]; epoch<qjs_persian_authority_starts[index+1]; epoch++) {
            assert(qjs_calendar_from_epoch_day(QJS_CAL_PERSIAN,epoch,&date) == 0);
            assert(date.year == year && date.days_in_year == sum);
            assert(date.day_of_year == epoch - qjs_persian_authority_starts[index] + 1);
            assert(date.in_leap_year == (qjs_persian_authority_stars[index] != 0));
            assert(qjs_calendar_to_epoch_day(QJS_CAL_PERSIAN,date.year,date.month,date.day,&inverse) == 0);
            assert(inverse == epoch);
        }
    }
    /* Last captured star proves a full 366-day year; no 1499 row is captured. */
    assert(qjs_persian_authority_starts[293] == iso(2120,3,21));
    assert(qjs_calendar_to_epoch_day(QJS_CAL_PERSIAN,1499,1,1,&epoch) == 0);
    assert(epoch == qjs_persian_authority_starts[293]);
    assert(qjs_calendar_from_epoch_day(QJS_CAL_PERSIAN,epoch-1,&date) == 0);
    assert(date.year == 1498 && date.month == 12 && date.day == 30);
    assert(qjs_calendar_from_epoch_day(QJS_CAL_PERSIAN,epoch,&date) == 0);
    assert(date.year == 1499 && date.month == 1 && date.day == 1);
    /* Table/approximation seam closure on both sides. */
    assert(qjs_calendar_to_epoch_day(QJS_CAL_PERSIAN,1206,1,1,&epoch) == 0);
    assert(qjs_calendar_from_epoch_day(QJS_CAL_PERSIAN,epoch-1,&date) == 0);
    assert(date.year == 1205 && date.month == 12);
    inverse = 123;
    assert(qjs_calendar_to_epoch_day(QJS_CAL_PERSIAN,1470,12,30,&inverse) == QJS_CAL_RANGE);
    assert(inverse == 123);
    assert(qjs_calendar_to_epoch_day(QJS_CAL_PERSIAN,1498,12,31,&inverse) == QJS_CAL_RANGE);
    assert(inverse == 123);
#ifndef QJS_CAL_PERSIAN_AUTHORITY_VERIFIED
    assert(!qjs_calendar_is_supported(QJS_CAL_PERSIAN));
#else
    assert(qjs_calendar_is_supported(QJS_CAL_PERSIAN));
#endif
    return 0;
}
