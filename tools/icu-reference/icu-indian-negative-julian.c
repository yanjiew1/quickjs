/* Public ICU C API witness for truncation of negative Indian Julian dates. */
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
    static const int years[] = {-271899, -6000, -4792, -4791, -4790,
                                -1, 0, 1, 1946, 275682};
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *indian = ucal_open(utc, 3, "en_US@calendar=indian",
                                UCAL_DEFAULT, &status);
    UCalendar *gregorian = ucal_open(utc, 3, "en_US@calendar=gregorian",
                                   UCAL_DEFAULT, &status);
    unsigned i;
    int month;
    if (!indian || !gregorian || U_FAILURE(status)) return 2;
    ucal_setGregorianChange(gregorian, -1e16, &status);
    for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
        int year = years[i], gregorianYear = year + 78;
        int leap = gregorianYear % 4 == 0 &&
            (gregorianYear % 100 != 0 || gregorianYear % 400 == 0);
        for (month = 0; month < 12; month++) {
            int offset = 0;
            UDate actual, expected;
            if (month > 0) {
                int longMonths = month - 1;
                if (longMonths > 5) longMonths = 5;
                offset = 30 + leap + longMonths * 31;
                if (month > 6) offset += (month - 6) * 30;
            }
            ucal_clear(gregorian);
            ucal_set(gregorian, UCAL_EXTENDED_YEAR, gregorianYear);
            ucal_set(gregorian, UCAL_MONTH, 2);
            ucal_set(gregorian, UCAL_DATE, leap ? 21 : 22);
            ucal_add(gregorian, UCAL_DATE, offset, &status);
            expected = ucal_getMillis(gregorian, &status);

            ucal_clear(indian);
            ucal_set(indian, UCAL_EXTENDED_YEAR, year);
            ucal_set(indian, UCAL_MONTH, month);
            ucal_set(indian, UCAL_DATE, 1);
            actual = ucal_getMillis(indian, &status);
            if (actual != expected) {
                fprintf(stderr, "Indian %d month %d shift %.0f days\n",
                        year, month + 1, (actual - expected) / 86400000.0);
                failed = 1;
            }
            ucal_setMillis(indian, expected, &status);
            check(ucal_get(indian, UCAL_EXTENDED_YEAR, &status), year,
                  "round trip year", year);
            check(ucal_get(indian, UCAL_MONTH, &status), month,
                  "round trip month", year);
            check(ucal_get(indian, UCAL_DATE, &status), 1,
                  "round trip day", year);
        }
    }
    ucal_clear(indian);
    ucal_set(indian, UCAL_EXTENDED_YEAR, -271899);
    ucal_set(indian, UCAL_MONTH, 0);
    ucal_set(indian, UCAL_DATE, 29);
    if (ucal_getMillis(indian, &status) != -8640000086400000.0) {
        fprintf(stderr, "minimum Temporal PlainDate conversion mismatch\n");
        failed = 1;
    }
    ucal_close(indian);
    ucal_close(gregorian);
    if (U_FAILURE(status)) {
        fprintf(stderr, "ICU status: %s\n", u_errorName(status));
        return 2;
    }
    return failed;
}
