/* Public ICU C API witness for ICU-23007 and negative-year postponements. */
#include <stdio.h>
#include <unicode/ucal.h>

static int failed;
static void check(int value, int expected, const char *name, int year)
{
    if (value != expected) {
        fprintf(stderr, "%s year %d: %d != %d\n",
                name, year, value, expected);
        failed = 1;
    }
}
int main(void)
{
    static const UChar utc[] = {'U', 'T', 'C', 0};
    static const int cheshvan[] = {29,30,30,29,29,30,30,29,29,30,29};
    static const int kislev[] = {30,30,30,29,30,30,30,30,29,30,30};
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *calendar = ucal_open(utc, 3, "en_US@calendar=hebrew",
                                   UCAL_DEFAULT, &status);
    int year, month;
    if (!calendar || U_FAILURE(status)) return 2;
    for (year = 0; year <= 10; year++) {
        for (month = 1; month <= 2; month++) {
            ucal_clear(calendar);
            ucal_set(calendar, UCAL_EXTENDED_YEAR, year);
            ucal_set(calendar, UCAL_MONTH, month);
            ucal_set(calendar, UCAL_DATE, 1);
            (void)ucal_get(calendar, UCAL_DATE, &status);
            check(ucal_getLimit(calendar, UCAL_DATE, UCAL_ACTUAL_MAXIMUM,
                                 &status),
                  month == 1 ? cheshvan[year] : kislev[year],
                  month == 1 ? "Cheshvan" : "Kislev", year);
        }
    }
    for (year = -40; year <= 10; year++) {
        int weekday, length;
        UDate start;
        ucal_clear(calendar);
        ucal_set(calendar, UCAL_EXTENDED_YEAR, year);
        ucal_set(calendar, UCAL_MONTH, 0);
        ucal_set(calendar, UCAL_DATE, 1);
        start = ucal_getMillis(calendar, &status);
        ucal_setMillis(calendar, start, &status);
        check(ucal_get(calendar, UCAL_EXTENDED_YEAR, &status), year,
              "round trip year", year);
        check(ucal_get(calendar, UCAL_MONTH, &status), 0,
              "round trip month", year);
        check(ucal_get(calendar, UCAL_DATE, &status), 1,
              "round trip day", year);
        weekday = ucal_get(calendar, UCAL_DAY_OF_WEEK, &status);
        if (weekday == UCAL_SUNDAY || weekday == UCAL_WEDNESDAY ||
            weekday == UCAL_FRIDAY) {
            fprintf(stderr, "forbidden Rosh Hashanah weekday in year %d\n",
                    year);
            failed = 1;
        }
        length = ucal_getLimit(calendar, UCAL_DAY_OF_YEAR,
                               UCAL_ACTUAL_MAXIMUM, &status);
        if (length != 353 && length != 354 && length != 355 &&
            length != 383 && length != 384 && length != 385) {
            fprintf(stderr, "invalid Hebrew year %d length %d\n", year,
                    length);
            failed = 1;
        }
    }
    ucal_close(calendar);
    if (U_FAILURE(status)) {
        fprintf(stderr, "ICU status: %s\n", u_errorName(status));
        return 2;
    }
    return failed;
}
